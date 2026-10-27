#include "WatchViewport.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t width,height,stride;
static risc_display_insets_v1 safe_insets;
static uint16_t *pixels;
static bool borrowed,reject_submit,bad_surface;
static unsigned submits,releases;
static risc_display_frame_v1 serial;
static risc_touch_event_v1 event;
static risc_touch_snapshot_v1 snapshot;
static bool native_info(void *ctx,risc_display_info_v1 *out) {
    (void)ctx;*out=(risc_display_info_v1){.api_version=1,.struct_size=sizeof(*out),.width=width,.height=height,
      .supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565),.safe_insets=safe_insets};return true;
}
static bool native_acquire(void *ctx,uint32_t format,risc_display_surface_v1 *out) {
    (void)ctx;assert(format==RISC_DISPLAY_FORMAT_RGB565);if(borrowed)return false;borrowed=true;
    *out=(risc_display_surface_v1){++serial,pixels,width,height,stride,height*stride,RISC_DISPLAY_FORMAT_RGB565};
    if(bad_surface)out->size_bytes=1;
    return true;
}
static void native_release(void *ctx,risc_display_frame_v1 frame) {
    (void)ctx;assert(borrowed&&frame==serial);borrowed=false;releases++;
}
static bool native_submit(void *ctx,risc_display_frame_v1 frame,const risc_display_rect_v1 *r,size_t count,
    const risc_display_present_options_v1 *options,risc_display_present_token_v1 *out) {
    (void)ctx;(void)options;assert(borrowed&&frame==serial);if(reject_submit)return false;
    assert(count==1 && r && r[0].x==0 && r[0].y==0 && r[0].width==width && r[0].height==height);
    borrowed=false;submits++;if(out)*out=frame;return true;
}
static bool native_status(void *ctx,risc_display_present_token_v1 token,risc_display_present_status_v1 *out) {
    (void)ctx;assert(token<=serial);out->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;
}
static uint64_t subscribe(void *ctx){(void)ctx;return 42;}
static bool unsubscribe(void *ctx,uint64_t id){(void)ctx;return id==42;}
static bool poll(void *ctx,size_t n){(void)ctx;return n>0;}
static int32_t next(void *ctx,uint64_t id,risc_touch_event_v1 *out){(void)ctx;if(id!=42)return -1;*out=event;return 1;}
static bool snap(void *ctx,risc_touch_snapshot_v1 *out){(void)ctx;*out=snapshot;return true;}
static const risc_touch_api_v1 touch={1,sizeof(touch),NULL,subscribe,unsubscribe,poll,next,snap};
static const risc_display_output_api_v1 native={.api_version=1,.struct_size=sizeof(native),.get_info=native_info,
 .acquire=native_acquire,.release=native_release,.submit=native_submit,.present_status=native_status};
static void exercise(unsigned w,unsigned h) {
    width=w;height=h;stride=(w+4)*2;pixels=malloc(height*stride);assert(pixels);
    memset(pixels,0xa5,height*stride);borrowed=reject_submit=bad_surface=false;submits=releases=0;
    watch_viewport v={0};assert(watch_viewport_open(&v,&native));
    risc_display_info_v1 i={0};assert(v.display.get_info(v.display.context,&i));assert(i.width==w&&i.height==h);
    risc_display_surface_v1 s={0},other={0};assert(v.display.acquire(v.display.context,RISC_DISPLAY_FORMAT_RGB565,&s));
    assert(s.width==w&&s.height==h);assert(s.pixels==pixels&&s.stride_bytes==stride);
    assert(!v.display.acquire(v.display.context,RISC_DISPLAY_FORMAT_RGB565,&other));
    for(unsigned y=0;y<i.height;y++)for(unsigned x=0;x<w;x++)((uint16_t*)((uint8_t*)s.pixels+y*s.stride_bytes))[x]=(uint16_t)(y*w+x);
    risc_display_present_token_v1 token=0;
    risc_display_rect_v1 invalid={(int32_t)w-1,0,2,1},all={0,0,w,h};assert(!v.display.submit(v.display.context,s.frame,&invalid,1,NULL,&token));
    reject_submit=true;assert(!v.display.submit(v.display.context,s.frame,NULL,0,NULL,&token));assert(borrowed);
    reject_submit=false;assert(v.display.submit(v.display.context,s.frame,&all,1,NULL,&token));assert(!borrowed&&submits==1);
    assert(!v.display.submit(v.display.context,s.frame,NULL,0,NULL,&token));
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w+4;x++) {
        uint16_t actual=pixels[y*(stride/2)+x];
        if(x>=w){assert(actual==0xa5a5);continue;}
        unsigned expected=(uint16_t)(y*w+x);
        assert(actual==expected);
    }
    assert(v.display.acquire(v.display.context,RISC_DISPLAY_FORMAT_RGB565,&s));
    for(unsigned y=0;y<i.height;y++)for(unsigned x=0;x<w;x++)
        {
            unsigned expected=(uint16_t)(y*w+x);
            assert(((uint16_t*)((uint8_t*)s.pixels+y*s.stride_bytes))[x]==expected);
        }
    v.display.release(v.display.context,s.frame);assert(!borrowed&&releases==1);
    bad_surface=true;assert(!v.display.acquire(v.display.context,RISC_DISPLAY_FORMAT_RGB565,&s));assert(!borrowed&&releases==2);bad_surface=false;
    watch_viewport_touch t={0};assert(watch_viewport_touch_open(&t,&v,&touch));assert(t.api.subscribe(t.api.context)==42);
    snapshot=(risc_touch_snapshot_v1){.width=w,.height=h,.sequence=123,.contact_count=1};
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++) {
        uint16_t xx=x,yy=y;assert(watch_viewport_map(&v,&xx,&yy));
        assert(xx==x&&yy==y);
    }
    event=(risc_touch_event_v1){.sequence=7,.kind=RISC_TOUCH_EVENT_DOWN,.id=2,.x=w/2,.y=h/2};
    risc_touch_event_v1 transformed={0};assert(t.api.next(t.api.context,42,&transformed)==1);
    assert(transformed.sequence==7&&transformed.id==2&&transformed.x==w/2&&transformed.y==h/2);
    snapshot.contacts[0]=(risc_touch_contact_v1){.id=2,.x=w/2,.y=h/2};risc_touch_snapshot_v1 mapped;
    assert(t.api.snapshot(t.api.context,&mapped));assert(mapped.width==w&&mapped.height==h&&mapped.sequence==123&&mapped.contacts[0].x==w/2);
    snapshot.width++;assert(!t.api.snapshot(t.api.context,&mapped));snapshot.width--;
    event.x=w;assert(t.api.next(t.api.context,42,&transformed)==-1);
    event.kind=RISC_TOUCH_EVENT_BUTTON_DOWN;event.x=event.y=UINT16_MAX;
    assert(t.api.next(t.api.context,42,&transformed)==1&&transformed.x==UINT16_MAX);
    assert(t.api.unsubscribe(t.api.context,42));
    assert(v.display.acquire(v.display.context,RISC_DISPLAY_FORMAT_RGB565,&s));watch_viewport_close(&v);
    assert(!borrowed&&releases==3&&!v.native);
    watch_viewport_close(&v);free(pixels);
}
static void exercise_safe_area(void) {
    width=height=466;
    /* A provider can conservatively advertise an inscribed rectangle without
     * adding a device identity or a circle field to display.output v1. */
    safe_insets=(risc_display_insets_v1){69,69,69,69};
    watch_viewport v={0};assert(watch_viewport_open(&v,&native));
    assert(v.width==466&&v.height==466&&v.layout.native);
    assert(v.layout.safe.x==69&&v.layout.safe.y==69);
    assert(v.layout.safe.width==328&&v.layout.safe.height==328);
    watch_layout_stack stack;
    assert(watch_layout_vertical(&v.layout,4,24,24,4,&stack));
    const watch_layout_rect rects[]={stack.header,stack.content,stack.footer};
    for(unsigned n=0;n<3;n++) {
        assert(watch_layout_contains(&v.layout.safe,rects[n].x,rects[n].y));
        assert(watch_layout_contains(&v.layout.safe,rects[n].x+rects[n].width-1,rects[n].y+rects[n].height-1));
        for(unsigned corner=0;corner<4;corner++) {
            int x=rects[n].x+((corner&1)?(int)rects[n].width-1:0);
            int y=rects[n].y+((corner&2)?(int)rects[n].height-1:0);
            int dx=2*x-465,dy=2*y-465;
            assert(dx*dx+dy*dy<=466*466);
        }
    }
    risc_display_info_v1 info={0};assert(v.display.get_info(v.display.context,&info));
    assert(!memcmp(&info.safe_insets,&safe_insets,sizeof(safe_insets)));
    /* Insets constrain content layout, not physical coordinates or gestures.
     * Do not clamp edge touches into controls or crop the provider surface. */
    uint16_t x=0,y=0;assert(watch_viewport_map(&v,&x,&y)&&x==0&&y==0);
    x=465;y=465;assert(watch_viewport_map(&v,&x,&y)&&x==465&&y==465);
    watch_viewport_close(&v);
    width=410;height=502;safe_insets=(risc_display_insets_v1){11,13,17,19};
    assert(watch_viewport_open(&v,&native));
    assert(v.layout.safe.x==19&&v.layout.safe.y==11&&v.layout.safe.width==378&&v.layout.safe.height==474);
    watch_viewport_close(&v);
    safe_insets=(risc_display_insets_v1){0,205,0,205};
    assert(!watch_viewport_open(&v,&native)&&!v.native);
    safe_insets=(risc_display_insets_v1){251,0,251,0};
    assert(!watch_viewport_open(&v,&native)&&!v.native);
    safe_insets=(risc_display_insets_v1){UINT16_MAX,0,UINT16_MAX,0};
    assert(!watch_viewport_open(&v,&native)&&!v.native);
    safe_insets=(risc_display_insets_v1){0};
}
int main(void) {
    exercise_safe_area();
    exercise(240,240);exercise(410,502);exercise(502,410);exercise(480,480);exercise(120,150);exercise(241,295);exercise(240,294);exercise(2048,2048);
    width=0;height=502;watch_viewport v={0};assert(!watch_viewport_open(&v,&native));
    width=3000;assert(!watch_viewport_open(&v,&native));
    puts("viewport: direct/native-height/landscape/fractional/final-row/retained pixels/touch/ownership PASS");return 0;
}
