#include "WatchViewport.h"
#include <string.h>

static bool surface_valid(const watch_viewport *v,const risc_display_surface_v1 *s) {
    return s->frame && s->pixels && s->width==v->width && s->height==v->height &&
        s->pixel_format==RISC_DISPLAY_FORMAT_RGB565 && !(s->stride_bytes&1u) &&
        s->stride_bytes>=s->width*2u && s->stride_bytes<=SIZE_MAX/s->height &&
        s->size_bytes>=(size_t)s->stride_bytes*(s->height-1u)+s->width*2u &&
        !((uintptr_t)s->pixels&1u);
}
static bool info(void *ctx,risc_display_info_v1 *out) {
    watch_viewport *v=ctx;
    if(!out || !v->native || !v->native->get_info(v->native->context,out) ||
       out->width!=v->width || out->height!=v->height)return false;
    out->supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565);
    return true;
}
static bool acquire(void *ctx,uint32_t format,risc_display_surface_v1 *out) {
    watch_viewport *v=ctx;
    if(!out || v->lease.frame || format!=RISC_DISPLAY_FORMAT_RGB565)return false;
    risc_display_surface_v1 s={0};
    if(!v->native->acquire(v->native->context,format,&s))return false;
    if(!surface_valid(v,&s)) {
        if(s.frame)v->native->release(v->native->context,s.frame);
        return false;
    }
    v->lease=s;
    *out=s;
    return true;
}
static void release(void *ctx,risc_display_frame_v1 frame) {
    watch_viewport *v=ctx;
    if(!frame || frame!=v->lease.frame)return;
    v->native->release(v->native->context,frame);v->lease=(risc_display_surface_v1){0};
}
static bool submit(void *ctx,risc_display_frame_v1 frame,const risc_display_rect_v1 *rects,size_t count,
        const risc_display_present_options_v1 *options,risc_display_present_token_v1 *token) {
    watch_viewport *v=ctx;
    if(!frame || frame!=v->lease.frame || count>8 || (count&&!rects))return false;
    for(size_t n=0;n<count;n++) {
        const risc_display_rect_v1 *r=rects+n;
        if(r->x<0 || r->y<0 || (uint32_t)r->x>=v->width || (uint32_t)r->y>=v->layout.height ||
           !r->width || !r->height || r->width>v->width-(uint32_t)r->x ||
           r->height>v->layout.height-(uint32_t)r->y)return false;
    }
    /* The application has rasterized directly into the provider's native lease.
     * Forward physical damage and retain ownership when submission is refused. */
    if(!v->native->submit(v->native->context,frame,rects,count,options,token))return false;
    v->lease=(risc_display_surface_v1){0};return true;
}
static bool status(void *ctx,risc_display_present_token_v1 token,risc_display_present_status_v1 *out) {
    watch_viewport *v=ctx;return v->native->present_status(v->native->context,token,out);
}
static bool wait(void *ctx,risc_display_present_token_v1 token,uint32_t ms,risc_display_present_status_v1 *out) {
    watch_viewport *v=ctx;return v->native->wait_present && v->native->wait_present(v->native->context,token,ms,out);
}
static bool brightness(void *ctx,uint16_t value,uint16_t max) {
    watch_viewport *v=ctx;return v->native->set_brightness && v->native->set_brightness(v->native->context,value,max);
}
bool watch_viewport_open(watch_viewport *v,const risc_display_output_api_v1 *native) {
    if(!v || !native || native->api_version!=1 || native->struct_size<sizeof(*native) ||
       !native->get_info || !native->acquire || !native->release || !native->submit || !native->present_status)return false;
    risc_display_info_v1 i={0};
    if(!native->get_info(native->context,&i) || !i.width || !i.height || i.width>WATCH_VIEWPORT_MAX_SIDE ||
       i.height>WATCH_VIEWPORT_MAX_SIDE || !(i.supported_formats&RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565)))return false;
    *v=(watch_viewport){.native=native,.width=i.width,.height=i.height};
    /* Provider-owned content bounds; retain the full native raster and touch
     * coordinate space. No board identity or display-shape inference. */
    const watch_layout_insets safe={i.safe_insets.top,i.safe_insets.right,
        i.safe_insets.bottom,i.safe_insets.left};
    if(!watch_layout_native_open(&v->layout,i.width,i.height,&safe)){*v=(watch_viewport){0};return false;}
    v->display=(risc_display_output_api_v1){1,sizeof(v->display),v,info,acquire,release,submit,status,wait,brightness};
    return true;
}
void watch_viewport_close(watch_viewport *v) {
    if(!v)return;
    if(v->lease.frame && v->native)release(v,v->lease.frame);
    *v=(watch_viewport){0};
}
bool watch_viewport_map(const watch_viewport *v,uint16_t *x,uint16_t *y) {
    if(!v || !v->native || !x || !y || *x>=v->width || *y>=v->height)return false;
    return watch_layout_touch(&v->layout,*x,*y,x,y);
}
static uint64_t touch_subscribe(void *ctx){watch_viewport_touch *t=ctx;return t->native->subscribe(t->native->context);}
static bool touch_unsubscribe(void *ctx,uint64_t id){watch_viewport_touch *t=ctx;return t->native->unsubscribe(t->native->context,id);}
static bool touch_poll(void *ctx,size_t reports){watch_viewport_touch *t=ctx;return t->native->poll(t->native->context,reports);}
static int32_t touch_next(void *ctx,uint64_t id,risc_touch_event_v1 *out) {
    watch_viewport_touch *t=ctx;if(!out)return -2;
    int32_t result=t->native->next(t->native->context,id,out);
    if(result>0 && out->kind!=RISC_TOUCH_EVENT_BUTTON_DOWN && out->kind!=RISC_TOUCH_EVENT_BUTTON_UP &&
       !watch_viewport_map(t->viewport,&out->x,&out->y))return -1;
    return result;
}
static bool touch_snapshot(void *ctx,risc_touch_snapshot_v1 *out) {
    watch_viewport_touch *t=ctx;if(!out)return false;
    risc_touch_snapshot_v1 raw={0};
    if(!t->native->snapshot(t->native->context,&raw) || raw.width!=t->viewport->width ||
       raw.height!=t->viewport->height || raw.contact_count>RISC_TOUCH_MAX_CONTACTS)return false;
    for(unsigned n=0;n<raw.contact_count;n++)
        if(!watch_viewport_map(t->viewport,&raw.contacts[n].x,&raw.contacts[n].y))return false;
    *out=raw;return true;
}
bool watch_viewport_touch_open(watch_viewport_touch *t,const watch_viewport *v,const risc_touch_api_v1 *native) {
    if(!t || !v || !v->native || !native || native->api_version!=1 || native->struct_size<sizeof(*native) ||
       !native->subscribe || !native->unsubscribe || !native->poll || !native->next || !native->snapshot)return false;
    *t=(watch_viewport_touch){.native=native,.viewport=v};
    t->api=(risc_touch_api_v1){1,sizeof(t->api),t,touch_subscribe,touch_unsubscribe,touch_poll,touch_next,touch_snapshot};return true;
}
