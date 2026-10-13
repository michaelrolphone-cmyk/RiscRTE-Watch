#include "WatchViewport.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t width,height,stride;
static uint16_t *pixels;
static bool borrowed,reject_submit,bad_surface;
static unsigned submits,releases;
static risc_display_frame_v1 serial;
static risc_touch_event_v1 event;
static risc_touch_snapshot_v1 snapshot;
static bool native_info(void *ctx,risc_display_info_v1 *out) {
    (void)ctx;*out=(risc_display_info_v1){.api_version=1,.struct_size=sizeof(*out),.width=width,.height=height,
      .supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565)};return true;
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
    if(width!=240 || height!=240)assert(count==0&&!r);
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
    risc_display_info_v1 i={0};assert(v.display.get_info(v.display.context,&i));assert(i.width==240&&i.height==(h*240+w-1)/w);
    risc_display_surface_v1 s={0},other={0};assert(v.display.acquire(v.display.context,RISC_DISPLAY_FORMAT_RGB565,&s));
    assert(s.width==240&&s.height==i.height);assert((s.pixels==pixels)==(w==240&&h==240));
    assert(!v.display.acquire(v.display.context,RISC_DISPLAY_FORMAT_RGB565,&other));
    for(unsigned y=0;y<i.height;y++)for(unsigned x=0;x<240;x++)((uint16_t*)((uint8_t*)s.pixels+y*s.stride_bytes))[x]=(uint16_t)(y*240+x);
    risc_display_present_token_v1 token=0;
    risc_display_rect_v1 invalid={239,0,2,1};assert(!v.display.submit(v.display.context,s.frame,&invalid,1,NULL,&token));
    reject_submit=true;assert(!v.display.submit(v.display.context,s.frame,NULL,0,NULL,&token));assert(borrowed);
    reject_submit=false;assert(v.display.submit(v.display.context,s.frame,NULL,0,NULL,&token));assert(!borrowed&&submits==1);
    assert(!v.display.submit(v.display.context,s.frame,NULL,0,NULL,&token));
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w+4;x++) {
        uint16_t actual=pixels[y*(stride/2)+x];
        if(x>=w){assert(actual==0xa5a5);continue;}
        unsigned expected=(uint16_t)((y*240/w)*240+x*240/w);
        assert(actual==expected);
    }
    assert(v.display.acquire(v.display.context,RISC_DISPLAY_FORMAT_RGB565,&s));
    for(unsigned y=0;y<i.height;y++)for(unsigned x=0;x<240;x++)
        {
            unsigned py=watch_layout_ceil(y*w,240),px=watch_layout_ceil(x*w,240);
            if(py>=h)py=h-1;if(px>=w)px=w-1;
            unsigned expected=(uint16_t)((py*240/w)*240+px*240/w);
            assert(((uint16_t*)((uint8_t*)s.pixels+y*s.stride_bytes))[x]==expected);
        }
    v.display.release(v.display.context,s.frame);assert(!borrowed&&releases==1);
    bad_surface=true;assert(!v.display.acquire(v.display.context,RISC_DISPLAY_FORMAT_RGB565,&s));assert(!borrowed&&releases==2);bad_surface=false;
    watch_viewport_touch t={0};assert(watch_viewport_touch_open(&t,&v,&touch));assert(t.api.subscribe(t.api.context)==42);
    snapshot=(risc_touch_snapshot_v1){.width=w,.height=h,.sequence=123,.contact_count=1};
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++) {
        uint16_t xx=x,yy=y;assert(watch_viewport_map(&v,&xx,&yy));
        assert(xx==x*240/w&&yy==y*240/w);
    }
    event=(risc_touch_event_v1){.sequence=7,.kind=RISC_TOUCH_EVENT_DOWN,.id=2,.x=w/2,.y=h/2};
    risc_touch_event_v1 transformed={0};assert(t.api.next(t.api.context,42,&transformed)==1);
    assert(transformed.sequence==7&&transformed.id==2&&transformed.x==(w/2)*240/w&&transformed.y==(h/2)*240/w);
    snapshot.contacts[0]=(risc_touch_contact_v1){.id=2,.x=w/2,.y=h/2};risc_touch_snapshot_v1 mapped;
    assert(t.api.snapshot(t.api.context,&mapped));assert(mapped.width==240&&mapped.height==i.height&&mapped.sequence==123&&mapped.contacts[0].x==(w/2)*240/w);
    snapshot.width++;assert(!t.api.snapshot(t.api.context,&mapped));snapshot.width--;
    event.x=w;assert(t.api.next(t.api.context,42,&transformed)==-1);
    assert(t.api.unsubscribe(t.api.context,42));
    assert(v.display.acquire(v.display.context,RISC_DISPLAY_FORMAT_RGB565,&s));watch_viewport_close(&v);
    assert(!borrowed&&releases==3&&!v.native&&!v.canvas);
    watch_viewport_close(&v);free(pixels);
}
int main(void) {
    exercise(240,240);exercise(410,502);exercise(502,410);exercise(480,480);exercise(120,150);exercise(241,295);exercise(240,294);exercise(2048,2048);
    width=0;height=502;watch_viewport v={0};assert(!watch_viewport_open(&v,&native));
    width=3000;assert(!watch_viewport_open(&v,&native));
    puts("viewport: direct/native-height/landscape/fractional/final-row/retained pixels/touch/ownership PASS");return 0;
}
