/* Production Bringup renderer and entry point, using independent providers.
 * All time and hardware are modeled; these are host/source regressions only. */
#include "RiscRuntimeV1.h"
#include "WatchViewport.h"
#include "twatch_caps.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <setjmp.h>
#include "../apps/bringup/main.c"

#define TEST_GUARD 64u
#define TEST_FILL 0xa7u
#define TEST_EVENTS 128u
static const char *test_case;
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"%s:%d: %s [%s]\n",__FILE__,__LINE__,#c,test_case?test_case:"setup"); abort(); } } while (0)
typedef struct { uint8_t *allocation; risc_display_surface_v1 surface; size_t capacity; } guarded_surface;
static guarded_surface gs_new(unsigned w,unsigned h) {
    guarded_surface g={0}; unsigned stride=w*2u+14u;
    g.capacity=(size_t)stride*h;
    g.allocation=malloc(TEST_GUARD+g.capacity+TEST_GUARD);CHECK(g.allocation);
    memset(g.allocation,TEST_FILL,TEST_GUARD+g.capacity+TEST_GUARD);
    g.surface=(risc_display_surface_v1){1,g.allocation+TEST_GUARD,w,h,stride,(uint32_t)g.capacity,RISC_DISPLAY_FORMAT_RGB565};return g;
}
static void gs_check(const guarded_surface *g) {
    for(unsigned i=0;i<TEST_GUARD;i++)CHECK(g->allocation[i]==TEST_FILL && g->allocation[TEST_GUARD+g->capacity+i]==TEST_FILL);
    for(unsigned y=0;y<g->surface.height;y++)for(unsigned x=g->surface.width*2u;x<g->surface.stride_bytes;x++)
        CHECK(g->allocation[TEST_GUARD+(size_t)y*g->surface.stride_bytes+x]==TEST_FILL);
}
static uint16_t gs_pixel(const guarded_surface *g,unsigned x,unsigned y) {
    const uint8_t *p=(const uint8_t *)g->surface.pixels+(size_t)y*g->surface.stride_bytes+x*2u;
    return (uint16_t)(p[0]|(unsigned)p[1]<<8);
}
static bool in_rect(const watch_layout_rect *r,unsigned x,unsigned y) {
    return (int64_t)x>=r->x && (int64_t)y>=r->y && (int64_t)x<r->x+r->width && (int64_t)y<r->y+r->height;
}
static void rect_valid(const watch_layout_rect *r,unsigned w,unsigned h) {
    CHECK(r->x>=0 && r->y>=0 && r->width && r->height);
    CHECK((uint64_t)r->x+r->width<=w && (uint64_t)r->y+r->height<=h);
}
static bool rectangles_overlap(const watch_layout_rect *a,const watch_layout_rect *b) {
    return (int64_t)a->x<(int64_t)b->x+b->width && (int64_t)b->x<(int64_t)a->x+a->width &&
        (int64_t)a->y<(int64_t)b->y+b->height && (int64_t)b->y<(int64_t)a->y+a->height;
}
static uint64_t raster_hash(const guarded_surface *g) {
    const unsigned char *p=g->surface.pixels;uint64_t h=UINT64_C(1469598103934665603);
    for(size_t n=0;n<g->capacity;n++){h^=p[n];h*=UINT64_C(1099511628211);}return h;
}
static void write_raster(const guarded_surface *g) {
    const char *directory=getenv("BRINGUP_RASTER_DIR");if(!directory||!*directory)return;
    char path[1024];int n=snprintf(path,sizeof(path),"%s/bringup-%ux%u.ppm",directory,g->surface.width,g->surface.height);CHECK(n>0&&(size_t)n<sizeof(path));
    FILE *file=fopen(path,"wb");CHECK(file);CHECK(fprintf(file,"P6\n%u %u\n255\n",g->surface.width,g->surface.height)>0);
    for(unsigned y=0;y<g->surface.height;y++)for(unsigned x=0;x<g->surface.width;x++){
        unsigned p=gs_pixel(g,x,y);unsigned char rgb[]={(unsigned char)(((p>>11)&31)*255/31),(unsigned char)(((p>>5)&63)*255/63),(unsigned char)((p&31)*255/31)};
        CHECK(fwrite(rgb,1,sizeof(rgb),file)==sizeof(rgb));
    }CHECK(fclose(file)==0);
}
static void renderer_case(unsigned w,unsigned h) {
    test_case="production raster";bringup_layout l={0};CHECK(bringup_layout_open(&l,w,h));
    CHECK(l.width==w && l.height==h && l.body_scale && l.title_scale && l.slop);
    watch_layout_rect regions[]={l.title,l.subtitle,l.bars,l.rows[0],l.rows[1],l.rows[2],l.rows[3],l.rows[4],l.rows[5],l.rows[6],l.motion,l.haptic,l.back};
    for(unsigned a=0;a<sizeof(regions)/sizeof(*regions);a++){
        if (regions[a].height) rect_valid(regions+a,w,h);
        for(unsigned b=a+1;b<sizeof(regions)/sizeof(*regions);b++)if(regions[a].height&&regions[b].height)CHECK(!rectangles_overlap(regions+a,regions+b));
    }
    CHECK((unsigned)l.back.y+l.back.height>h*3u/4u);
    guarded_surface g=gs_new(w,h);
    const char *long_text="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    CHECK(render(&g.surface,&l,UINT32_MAX,long_text,long_text,long_text,long_text,UINT16_MAX,UINT16_MAX,UINT32_MAX));
    gs_check(&g);unsigned white[3]={0};
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){
        unsigned expected=in_rect(&l.motion,x,y)?1:in_rect(&l.haptic,x,y)?2:in_rect(&l.back,x,y)?3:0;
        CHECK((unsigned)bringup_hit(&l,x,y)==expected);uint16_t p=gs_pixel(&g,x,y);
        if(expected){CHECK(p==(expected==3?0x032c:0x2124)||p==0xffff);white[expected-1]+=p==0xffff;}
        else CHECK(p!=0x2124 && p!=0x032c);
        if(p!=0x0841){bool inside=false;for(unsigned r=0;r<sizeof(regions)/sizeof(*regions);r++)inside|=in_rect(regions+r,x,y);CHECK(inside);}
    }
    CHECK(white[0]&&white[1]&&white[2]);CHECK(!bringup_hit(&l,w,h/2));CHECK(!bringup_hit(&l,w/2,h));
    /* Changing one long line may not overwrite any other row or control. */
    guarded_surface other=gs_new(w,h);
    CHECK(render(&other.surface,&l,UINT32_MAX,"A",long_text,long_text,long_text,UINT16_MAX,UINT16_MAX,UINT32_MAX));
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++)if(!in_rect(&l.rows[1],x,y))CHECK(gs_pixel(&other,x,y)==gs_pixel(&g,x,y));
    gs_check(&other);
    risc_display_surface_v1 bad=g.surface;uint64_t hash=raster_hash(&g);
    CHECK(!render(NULL,&l,0,"","","","",0,0,0));
    bad.width=w-1;CHECK(!render(&bad,&l,0,"","","","",0,0,0));bad=g.surface;
    bad.height=h-1;CHECK(!render(&bad,&l,0,"","","","",0,0,0));bad=g.surface;
    bad.stride_bytes=w*2u-2u;CHECK(!render(&bad,&l,0,"","","","",0,0,0));bad=g.surface;
    bad.size_bytes=(h-1u)*g.surface.stride_bytes+w*2u-1u;CHECK(!render(&bad,&l,0,"","","","",0,0,0));bad=g.surface;
    bad.pixel_format=RISC_DISPLAY_FORMAT_GRAY8;CHECK(!render(&bad,&l,0,"","","","",0,0,0));bad=g.surface;
    bad.pixels=NULL;CHECK(!render(&bad,&l,0,"","","","",0,0,0));CHECK(hash==raster_hash(&g));
    gs_check(&g);
    CHECK(render(&g.surface,&l,123,"BATTERY 73% 3950 MV","RTC RAW 14:30:59","XYZ 100 -100 1000 MG","VIBRATION REQUESTED",(uint16_t)(w/2),(uint16_t)(h/2),42));
    gs_check(&g);write_raster(&g);free(g.allocation);free(other.allocation);
    printf("Bringup raster %ux%u: native pixels, bounded text, exact painted hit regions and padded guards PASS\n",w,h);
}

enum optional_mode { OPTIONAL_OK,OPTIONAL_ABSENT,OPTIONAL_INVALID,OPTIONAL_METHOD_MISSING,OPTIONAL_READ_FAIL };
enum display_mode { DISPLAY_OK,DISPLAY_FAIL,DISPLAY_SUPERSEDE,DISPLAY_STATUS_ERROR,DISPLAY_UNKNOWN,DISPLAY_ACQUIRE_FAIL,DISPLAY_SUBMIT_FAIL,DISPLAY_BAD_LEASE,DISPLAY_STUCK_ACTIVE,DISPLAY_STUCK_QUEUED,DISPLAY_STUCK_ERROR,DISPLAY_STUCK_UNKNOWN };
typedef struct { unsigned tick;int result;uint8_t kind,id;uint16_t x,y;uint64_t sequence; } test_event;
static struct {
    unsigned width,height,now,ticks,health_limit,live,acquires[6],releases[6],release_attempts[6],release_count,last_release;
    unsigned submitted,completed,status_calls,queued_seen,active_seen,frame_releases,pulses,stops,motion_reads,battery_reads,rtc_reads;
    unsigned subscribed,snapshots,event_count,event_index,first_delay,later_delay,ready,submitted_at,last_complete_tick;
    unsigned fail_snapshot_at,mismatch_snapshot_at,invalid_count_snapshot_at,invalid_point_snapshot_at,poll_fail_at;
    unsigned first_optional_tick,error_delivered,contact_count;
    risc_touch_contact_v1 contacts[RISC_TOUCH_MAX_CONTACTS];
    unsigned terminal_frames,acquire_attempts,submit_attempts,protocol_events;uint64_t snapshot_sequence;
    bool borrowed,pending,returned,healthy_stop,retained,stop_fail,unsubscribe_fail;
    unsigned unsubscribe_attempts,release_fail_slot,optional_mask;
    enum optional_mode optional;enum display_mode display;uint64_t hash;
    guarded_surface image;bringup_layout layout;test_event events[TEST_EVENTS];
    risc_display_output_api_v1 display_api;risc_touch_api_v1 touch_api;
    risc_battery_gauge_api_v1 battery_api;twatch_rtc_api_v1 rtc_api;twatch_motion_api_v1 motion_api;twatch_haptic_api_v1 haptic_api;
} t;
static jmp_buf retained_return;
static bool expect_retained;
static unsigned fixture_cases;
static bool t_info(void *ctx,risc_display_info_v1 *out){(void)ctx;*out=(risc_display_info_v1){.api_version=1,.struct_size=sizeof(*out),.width=t.width,.height=t.height,.supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565)};return true;}
static bool t_acquire(void *ctx,uint32_t format,risc_display_surface_v1 *out){
    (void)ctx;CHECK(format==RISC_DISPLAY_FORMAT_RGB565);CHECK(!t.borrowed&&!t.pending);t.acquire_attempts++;
    if(t.display==DISPLAY_ACQUIRE_FAIL)return false;
    t.borrowed=true;*out=t.image.surface;out->frame=t.submitted+1;
    if(t.display==DISPLAY_BAD_LEASE)out->width--;
    return true;
}
static void t_frame_release(void *ctx,uint64_t frame){(void)ctx;CHECK(t.borrowed&&!t.pending&&frame==t.submitted+1);t.borrowed=false;t.frame_releases++;gs_check(&t.image);}
static bool t_submit(void *ctx,uint64_t frame,const risc_display_rect_v1 *rects,size_t count,const risc_display_present_options_v1 *options,uint64_t *out){
    (void)ctx;(void)options;CHECK(t.borrowed&&!t.pending&&frame==t.submitted+1);CHECK(!rects&&count==0);gs_check(&t.image);t.submit_attempts++;
    if(t.display==DISPLAY_SUBMIT_FAIL)return false;
    t.borrowed=false;t.pending=true;t.submitted++;t.submitted_at=t.now;t.ready=t.now+(t.submitted==1?t.first_delay:t.later_delay);*out=t.submitted;t.hash=raster_hash(&t.image);return true;
}
static bool t_status(void *ctx,uint64_t token,risc_display_present_status_v1 *out){
    (void)ctx;CHECK(token&&token<=t.submitted);CHECK(!t.borrowed);if(t.status_calls<50||t.status_calls%500==0)CHECK(t.hash==raster_hash(&t.image));t.status_calls++;
    if(t.display>=DISPLAY_STUCK_ACTIVE){if(t.display==DISPLAY_STUCK_ERROR)return false;out->state=t.display==DISPLAY_STUCK_ACTIVE?RISC_DISPLAY_PRESENT_ACTIVE:t.display==DISPLAY_STUCK_QUEUED?RISC_DISPLAY_PRESENT_QUEUED:99;return true;}
    if(t.display==DISPLAY_STATUS_ERROR&&!t.error_delivered++){return false;}
    if(t.display==DISPLAY_UNKNOWN&&!t.error_delivered++){out->state=99;return true;}
    if(t.now>=t.ready){
        if(t.pending){t.pending=false;t.terminal_frames++;if(t.display!=DISPLAY_FAIL&&t.display!=DISPLAY_SUPERSEDE){t.completed++;t.last_complete_tick=t.ticks;}}
        out->state=t.display==DISPLAY_FAIL?RISC_DISPLAY_PRESENT_FAILED:t.display==DISPLAY_SUPERSEDE?RISC_DISPLAY_PRESENT_SUPERSEDED:RISC_DISPLAY_PRESENT_COMPLETE;
    }else if(t.now-t.submitted_at<3){t.queued_seen++;out->state=RISC_DISPLAY_PRESENT_QUEUED;}
    else {t.active_seen++;out->state=RISC_DISPLAY_PRESENT_ACTIVE;}return true;
}
static uint64_t t_subscribe(void *ctx){(void)ctx;CHECK(!t.subscribed);t.subscribed=1;return 17;}
static bool t_unsubscribe(void *ctx,uint64_t sub){(void)ctx;CHECK(t.subscribed&&sub==17);CHECK(!t.pending);t.unsubscribe_attempts++;if(t.unsubscribe_fail)return false;t.subscribed=0;return true;}
static bool t_poll(void *ctx,size_t reports){(void)ctx;CHECK(reports==1);t.ticks++;CHECK(t.ticks<10000);return t.ticks!=t.poll_fail_at;}
static int32_t t_next(void *ctx,uint64_t sub,risc_touch_event_v1 *out){
    (void)ctx;CHECK(sub==17&&t.subscribed);if(t.event_index==t.event_count||t.events[t.event_index].tick>t.ticks)return 0;
    test_event e=t.events[t.event_index++];t.protocol_events++;
    if(e.result<0){if(e.kind){t.contact_count=e.kind==1;t.contacts[0]=(risc_touch_contact_v1){.id=e.id,.x=e.x,.y=e.y};}return e.result;}
    *out=(risc_touch_event_v1){.sequence=e.sequence?e.sequence:t.event_index,.timestamp_ms=t.now,.kind=e.kind,.id=e.id,.x=e.x,.y=e.y};
    unsigned contact_index=0;while(contact_index<t.contact_count&&t.contacts[contact_index].id!=e.id)contact_index++;
    if(e.kind==RISC_TOUCH_EVENT_DOWN){if(contact_index==t.contact_count){CHECK(t.contact_count<RISC_TOUCH_MAX_CONTACTS);t.contact_count++;}t.contacts[contact_index]=(risc_touch_contact_v1){.id=e.id,.x=e.x,.y=e.y};}
    if(e.kind==RISC_TOUCH_EVENT_MOVE&&contact_index<t.contact_count){t.contacts[contact_index].x=e.x;t.contacts[contact_index].y=e.y;}
    if(e.kind==RISC_TOUCH_EVENT_UP&&contact_index<t.contact_count){for(unsigned j=contact_index+1;j<t.contact_count;j++)t.contacts[j-1]=t.contacts[j];t.contact_count--;}
    return 1;
}
static bool t_snapshot(void *ctx,risc_touch_snapshot_v1 *out){
    (void)ctx;t.snapshots++;if(t.snapshots==t.fail_snapshot_at)return false;
    *out=(risc_touch_snapshot_v1){.sequence=t.event_index,.width=t.width,.height=t.height,.contact_count=t.contact_count};
    if(t.snapshots==2&&t.snapshot_sequence)out->sequence=t.snapshot_sequence;
    memcpy(out->contacts,t.contacts,t.contact_count*sizeof(*t.contacts));
    if(t.snapshots==t.mismatch_snapshot_at)out->width--;
    if(t.snapshots==t.invalid_count_snapshot_at)out->contact_count=RISC_TOUCH_MAX_CONTACTS+1;
    if(t.snapshots==t.invalid_point_snapshot_at){out->contact_count=1;out->contacts[0].x=t.width;}
    return true;
}
static bool t_battery(void *ctx,risc_battery_sample_v1 *out){(void)ctx;t.battery_reads++;*out=(risc_battery_sample_v1){3950,73,0};return t.optional!=OPTIONAL_READ_FAIL;}
static bool t_rtc(void *ctx,twatch_rtc_time_v1 *out){(void)ctx;t.rtc_reads++;*out=(twatch_rtc_time_v1){2026,10,19,1,14,30,59};return t.optional!=OPTIONAL_READ_FAIL;}
static bool t_motion(void *ctx,twatch_accel_sample_v1 *out){(void)ctx;CHECK(t.completed&&!t.pending);t.motion_reads++;*out=(twatch_accel_sample_v1){100,-100,1000};return t.optional!=OPTIONAL_READ_FAIL;}
static bool t_effect(void *ctx,uint8_t effect){(void)ctx;CHECK(effect==14&&t.completed&&!t.pending);t.pulses++;return t.optional!=OPTIONAL_READ_FAIL;}
static bool t_stop(void *ctx){(void)ctx;CHECK(!t.pending);t.stops++;return !t.stop_fail;}
static bool t_get(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *grant){
    const char *names[]={"display.output","input.touch.raw","board.battery","rtc.clock","motion.accel","haptic.effect"};
    const void *apis[]={&t.display_api,&t.touch_api,&t.battery_api,&t.rtc_api,&t.motion_api,&t.haptic_api};CHECK(instance==0);
    for(unsigned i=0;i<6;i++)if(!strcmp(names[i],name)){
        CHECK(version==(i==3?2u:1u));t.acquires[i]++;
        if(i>=4){CHECK(t.completed&&!t.pending);if(!t.first_optional_tick)t.first_optional_tick=t.ticks;}
        if(i>=2&&(t.optional==OPTIONAL_ABSENT||!(t.optional_mask&(1u<<(i-2)))))return false;
        CHECK(!(t.live&(1u<<i)));t.live|=1u<<i;*grant=(risc_runtime_capability_v1){sizeof(*grant),i+1,1,apis[i]};return true;
    }CHECK(false);return false;
}
static bool t_release(risc_runtime_capability_v1 *grant){
    CHECK(grant->slot&&grant->slot<=6);unsigned slot=grant->slot-1;CHECK(t.live&(1u<<slot));
    CHECK(grant->slot<t.last_release);t.release_attempts[slot]++;if(grant->slot==t.release_fail_slot)return false;t.last_release=grant->slot;t.releases[slot]++;t.release_count++;
    if(slot==5&&t.haptic_api.api_version==1&&t.haptic_api.effect&&t.haptic_api.stop)CHECK(t.stops==1);
    if(slot==1)CHECK(!t.subscribed);
    if(slot==0)CHECK(!t.pending&&!t.borrowed);
    t.live&=~(1u<<slot);return true;
}
static unsigned fixture_health_calls,fixture_monotonic_calls;
static bool t_monotonic(uint32_t*out){++fixture_monotonic_calls;*out=t.now;if(t.ticks>=t.health_limit){t.healthy_stop=true;return false;}return true;}
static bool t_health(risc_runtime_health_v1 *out){++fixture_health_calls;out->uptime_ms=t.now;if(t.ticks>=t.health_limit){t.healthy_stop=true;return false;}return true;}
static void t_yield(uint32_t ms){if(expect_retained&&t.retained)longjmp(retained_return,1);CHECK(ms&&ms<=100);t.now+=ms;CHECK(t.now<20000);}
static bool t_log(const char *line){CHECK(line);if(strstr(line,"WATCH_TEST retained="))t.retained=true;if(!strcmp(line,"WATCH_TEST return=home"))t.returned=true;return true;}
static risc_runtime_api_v1 test_runtime={.api_version=1,.struct_size=sizeof(test_runtime),.health=t_health,.yield_ms=t_yield,.diagnostic=t_log,.acquire=t_get,.release=t_release};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){return version==1?&test_runtime:NULL;}
static void reset_case(unsigned w,unsigned h,const char *name){
    memset(&t,0,sizeof(t));fixture_cases++;test_case=name;t.width=w;t.height=h;t.health_limit=400;t.first_delay=10;t.later_delay=8;t.last_release=7;t.optional_mask=15;
    CHECK(bringup_layout_open(&t.layout,w,h));t.image=gs_new(w,h);
    t.display_api=(risc_display_output_api_v1){.api_version=1,.struct_size=sizeof(t.display_api),.get_info=t_info,.acquire=t_acquire,.release=t_frame_release,.submit=t_submit,.present_status=t_status};
    t.touch_api=(risc_touch_api_v1){1,sizeof(t.touch_api),NULL,t_subscribe,t_unsubscribe,t_poll,t_next,t_snapshot};
    t.battery_api=(risc_battery_gauge_api_v1){1,sizeof(t.battery_api),NULL,t_battery};
    t.rtc_api=(twatch_rtc_api_v1){.api_version=2,.struct_size=sizeof(t.rtc_api),.read=t_rtc};
    t.motion_api=(twatch_motion_api_v1){.api_version=1,.struct_size=sizeof(t.motion_api),.read=t_motion};
    t.haptic_api=(twatch_haptic_api_v1){1,sizeof(t.haptic_api),NULL,t_effect,t_stop};
}
static void add_event(unsigned tick,int result,unsigned kind,unsigned id,unsigned x,unsigned y){CHECK(t.event_count<TEST_EVENTS);CHECK(!t.event_count||tick>=t.events[t.event_count-1].tick);t.events[t.event_count++]=(test_event){tick,result,(uint8_t)kind,(uint8_t)id,(uint16_t)x,(uint16_t)y,0};}
static void point_event(unsigned tick,unsigned kind,unsigned id,const watch_layout_rect *r){add_event(tick,1,kind,id,r->x+r->width/2u,r->y+r->height/2u);}
static void tap(unsigned tick,const watch_layout_rect *r){point_event(tick,RISC_TOUCH_EVENT_DOWN,1,r);point_event(tick+2,RISC_TOUCH_EVENT_UP,1,r);}
static void back(unsigned tick){tap(tick,&t.layout.back);}
static void finish_case(bool all_events){
    app_main();CHECK(!t.live&&!t.borrowed&&!t.pending&&!t.subscribed);CHECK(t.returned);gs_check(&t.image);
    if(all_events)CHECK(t.event_index==t.event_count);
    for(unsigned i=0;i<6;i++)CHECK(t.releases[i]<=1);
    free(t.image.allocation);
}
static void optional_mode_set(enum optional_mode mode){
    t.optional=mode;
    if(mode==OPTIONAL_INVALID){t.battery_api.api_version=9;t.rtc_api.api_version=9;t.motion_api.api_version=9;t.haptic_api.api_version=9;}
    if(mode==OPTIONAL_METHOD_MISSING){t.battery_api.read=NULL;t.rtc_api.read=NULL;t.motion_api.read=NULL;t.haptic_api.effect=NULL;}
}
static void lifecycle_cases(unsigned w,unsigned h){
    for(unsigned mask=0;mask<16;mask++){
        reset_case(w,h,"independent optional provider presence");t.optional_mask=mask;
        tap(40,&t.layout.motion);tap(80,&t.layout.haptic);back(120);finish_case(true);
        CHECK(t.completed>=1&&t.submitted>=1&&!t.healthy_stop);
        CHECK((t.battery_reads>0)==!!(mask&1));CHECK((t.rtc_reads>0)==!!(mask&2));
        CHECK((t.motion_reads>0)==!!(mask&4));CHECK(t.pulses==!!(mask&8));CHECK(t.stops==!!(mask&8));
        for(unsigned i=2;i<6;i++){CHECK(t.acquires[i]==1);CHECK(t.releases[i]==!!(mask&(1u<<(i-2))));}
        CHECK(t.releases[0]==1&&t.releases[1]==1);
    }

    for(unsigned mode=OPTIONAL_OK;mode<=OPTIONAL_READ_FAIL;mode++){
        reset_case(w,h,"normal and optional compatibility");optional_mode_set((enum optional_mode)mode);
        tap(40,&t.layout.haptic);tap(70,&t.layout.motion);tap(100,&t.layout.haptic);back(140);finish_case(true);
        CHECK(t.completed>=3&&t.queued_seen&&t.active_seen&&!t.healthy_stop);CHECK(t.acquires[4]==1);
        CHECK(t.pulses==(mode==OPTIONAL_OK||mode==OPTIONAL_READ_FAIL?2u:0u));
        CHECK((t.motion_reads>0)==(mode==OPTIONAL_OK||mode==OPTIONAL_READ_FAIL));
        CHECK(t.acquires[5]==1);
    }
    reset_case(w,h,"idle opens no optional motion or haptic");back(50);finish_case(true);CHECK(!t.acquires[4]&&!t.acquires[5]);
    reset_case(w,h,"pre-visible gestures are ignored");t.first_delay=100;tap(10,&t.layout.haptic);tap(20,&t.layout.motion);back(30);back(160);finish_case(true);CHECK(!t.acquires[4]&&!t.acquires[5]);CHECK(t.completed&&t.event_index==8);
    reset_case(w,h,"startup-held contact needs fresh down");t.contact_count=1;t.contacts[0]=(risc_touch_contact_v1){.id=1,.x=t.layout.haptic.x,.y=t.layout.haptic.y};
    point_event(40,RISC_TOUCH_EVENT_DOWN,1,&t.layout.haptic);point_event(42,RISC_TOUCH_EVENT_UP,1,&t.layout.haptic);tap(70,&t.layout.haptic);back(110);finish_case(true);CHECK(t.pulses==1);
    reset_case(w,h,"down before visibility and up afterward");t.first_delay=80;point_event(10,RISC_TOUCH_EVENT_DOWN,1,&t.layout.haptic);point_event(100,RISC_TOUCH_EVENT_UP,1,&t.layout.haptic);back(140);finish_case(true);CHECK(!t.pulses);
    reset_case(w,h,"up without matching down");point_event(40,RISC_TOUCH_EVENT_UP,1,&t.layout.haptic);back(80);finish_case(true);CHECK(!t.pulses);
    reset_case(w,h,"contact IDs must match");point_event(40,RISC_TOUCH_EVENT_DOWN,1,&t.layout.haptic);point_event(42,RISC_TOUCH_EVENT_UP,2,&t.layout.haptic);add_event(43,1,RISC_TOUCH_EVENT_MOVE,1,0,0);point_event(44,RISC_TOUCH_EVENT_UP,1,&t.layout.haptic);back(80);finish_case(true);CHECK(!t.pulses);
    reset_case(w,h,"down and up in different controls");point_event(40,RISC_TOUCH_EVENT_DOWN,1,&t.layout.motion);point_event(42,RISC_TOUCH_EVENT_UP,1,&t.layout.haptic);back(80);finish_case(true);CHECK(!t.acquires[4]&&!t.pulses);
    reset_case(w,h,"foreign move and up leave the original contact intact");
    point_event(40,RISC_TOUCH_EVENT_DOWN,1,&t.layout.haptic);add_event(41,1,RISC_TOUCH_EVENT_MOVE,2,0,0);point_event(42,RISC_TOUCH_EVENT_UP,2,&t.layout.haptic);point_event(43,RISC_TOUCH_EVENT_UP,1,&t.layout.haptic);back(90);finish_case(true);CHECK(t.pulses==1);
    reset_case(w,h,"missing up cannot trigger and a gap recovers");point_event(40,RISC_TOUCH_EVENT_DOWN,1,&t.layout.haptic);add_event(60,-1,2,0,0,0);tap(80,&t.layout.haptic);back(120);finish_case(true);CHECK(t.pulses==1&&t.snapshots>=3);
    for(unsigned first_up=1;first_up<=2;first_up++){
        unsigned remaining=3-first_up;reset_case(w,h,"second contact cancels until every held contact clears");
        point_event(40,RISC_TOUCH_EVENT_DOWN,1,&t.layout.haptic);point_event(41,RISC_TOUCH_EVENT_DOWN,2,&t.layout.haptic);point_event(42,RISC_TOUCH_EVENT_UP,first_up,&t.layout.haptic);
        point_event(43,RISC_TOUCH_EVENT_DOWN,first_up,&t.layout.haptic);point_event(44,RISC_TOUCH_EVENT_UP,first_up,&t.layout.haptic);point_event(45,RISC_TOUCH_EVENT_UP,remaining,&t.layout.haptic);
        tap(80,&t.layout.haptic);back(120);finish_case(true);CHECK(t.pulses==1&&t.snapshots>=4);
    }
    reset_case(w,h,"cancelled moved contact cannot be stolen by second finger");
    point_event(40,RISC_TOUCH_EVENT_DOWN,1,&t.layout.haptic);add_event(41,1,RISC_TOUCH_EVENT_MOVE,1,0,0);point_event(42,RISC_TOUCH_EVENT_DOWN,2,&t.layout.haptic);point_event(43,RISC_TOUCH_EVENT_UP,2,&t.layout.haptic);
    point_event(44,RISC_TOUCH_EVENT_UP,1,&t.layout.haptic);tap(80,&t.layout.haptic);back(120);finish_case(true);CHECK(t.pulses==1);
    reset_case(w,h,"button and unknown events do not become contacts");point_event(40,RISC_TOUCH_EVENT_BUTTON_DOWN,1,&t.layout.haptic);point_event(41,RISC_TOUCH_EVENT_BUTTON_UP,1,&t.layout.haptic);point_event(42,99,1,&t.layout.haptic);point_event(43,RISC_TOUCH_EVENT_UP,1,&t.layout.haptic);back(90);finish_case(true);CHECK(!t.pulses);
    reset_case(w,h,"cross-control boundary within slop cancels");
    add_event(40,1,RISC_TOUCH_EVENT_DOWN,1,t.layout.motion.x+t.layout.motion.width-1,t.layout.motion.y+2);
    add_event(42,1,RISC_TOUCH_EVENT_UP,1,t.layout.haptic.x,t.layout.haptic.y+2);back(80);finish_case(true);CHECK(!t.acquires[4]&&!t.pulses);
    reset_case(w,h,"movement at native slop cancels inside control");
    add_event(40,1,RISC_TOUCH_EVENT_DOWN,1,t.layout.haptic.x+2,t.layout.haptic.y+2);
    add_event(41,1,RISC_TOUCH_EVENT_MOVE,1,t.layout.haptic.x+2+t.layout.slop,t.layout.haptic.y+2);
    add_event(42,1,RISC_TOUCH_EVENT_UP,1,t.layout.haptic.x+2,t.layout.haptic.y+2);back(80);finish_case(true);CHECK(!t.pulses);
    reset_case(w,h,"movement below native slop remains a tap");
    add_event(40,1,RISC_TOUCH_EVENT_DOWN,1,t.layout.haptic.x+2,t.layout.haptic.y+2);
    add_event(42,1,RISC_TOUCH_EVENT_UP,1,t.layout.haptic.x+1+t.layout.slop,t.layout.haptic.y+2);back(80);finish_case(true);CHECK(t.pulses==1);
    reset_case(w,h,"painted control top-left and bottom-right pixels act");
    add_event(40,1,RISC_TOUCH_EVENT_DOWN,1,t.layout.haptic.x,t.layout.haptic.y);add_event(42,1,RISC_TOUCH_EVENT_UP,1,t.layout.haptic.x,t.layout.haptic.y);
    add_event(70,1,RISC_TOUCH_EVENT_DOWN,1,t.layout.haptic.x+t.layout.haptic.width-1,t.layout.haptic.y+t.layout.haptic.height-1);
    add_event(72,1,RISC_TOUCH_EVENT_UP,1,t.layout.haptic.x+t.layout.haptic.width-1,t.layout.haptic.y+t.layout.haptic.height-1);back(110);finish_case(true);CHECK(t.pulses==2);
    reset_case(w,h,"first-visible snapshot discards older queued events");t.snapshot_sequence=100;tap(40,&t.layout.haptic);back(80);t.events[2].sequence=101;t.events[3].sequence=102;finish_case(true);CHECK(!t.pulses&&!t.healthy_stop);
    reset_case(w,h,"gutter and outer margins are inert");
    unsigned middle=(unsigned)(t.layout.motion.x+t.layout.motion.width+(t.layout.haptic.x-t.layout.motion.x-t.layout.motion.width)/2);
    for(unsigned j=0;j<3;j++){unsigned x=j==0?middle:j==1?0:w-1,y=t.layout.motion.y+t.layout.motion.height/2;add_event(40+j*10,1,RISC_TOUCH_EVENT_DOWN,1,x,y);add_event(42+j*10,1,RISC_TOUCH_EVENT_UP,1,x,y);}
    add_event(75,1,RISC_TOUCH_EVENT_DOWN,1,0,h-1);add_event(77,1,RISC_TOUCH_EVENT_UP,1,0,h-1);back(110);finish_case(true);CHECK(!t.pulses&&!t.acquires[4]);
    reset_case(w,h,"move out and return cancels gesture");point_event(40,RISC_TOUCH_EVENT_DOWN,1,&t.layout.haptic);add_event(41,1,RISC_TOUCH_EVENT_MOVE,1,0,0);point_event(42,RISC_TOUCH_EVENT_MOVE,1,&t.layout.haptic);point_event(43,RISC_TOUCH_EVENT_UP,1,&t.layout.haptic);back(90);finish_case(true);CHECK(!t.pulses);
    reset_case(w,h,"outside native surface cancels gesture");point_event(40,RISC_TOUCH_EVENT_DOWN,1,&t.layout.haptic);add_event(41,1,RISC_TOUCH_EVENT_MOVE,1,w,h);point_event(43,RISC_TOUCH_EVENT_UP,1,&t.layout.haptic);back(90);finish_case(false);CHECK(!t.pulses&&!t.healthy_stop);
    reset_case(w,h,"gap-held contact needs fresh down");point_event(40,RISC_TOUCH_EVENT_DOWN,1,&t.layout.haptic);add_event(41,-1,1,1,t.layout.haptic.x,t.layout.haptic.y);point_event(42,RISC_TOUCH_EVENT_DOWN,1,&t.layout.haptic);point_event(43,RISC_TOUCH_EVENT_UP,1,&t.layout.haptic);tap(70,&t.layout.haptic);back(110);finish_case(true);CHECK(t.snapshots>=2&&t.pulses==1);
    reset_case(w,h,"gap clears earlier tap candidate");point_event(40,RISC_TOUCH_EVENT_DOWN,1,&t.layout.haptic);add_event(41,-1,2,1,0,0);point_event(43,RISC_TOUCH_EVENT_UP,1,&t.layout.haptic);tap(70,&t.layout.haptic);back(110);finish_case(true);CHECK(t.snapshots>=2&&t.pulses==1);
    reset_case(w,h,"poll failure drains and cancels partial gesture");t.poll_fail_at=42;point_event(40,RISC_TOUCH_EVENT_DOWN,1,&t.layout.haptic);point_event(42,RISC_TOUCH_EVENT_UP,1,&t.layout.haptic);tap(70,&t.layout.haptic);back(110);finish_case(true);CHECK(t.pulses==1);
    reset_case(w,h,"poll failure does not accept same-poll tap");t.poll_fail_at=40;point_event(40,RISC_TOUCH_EVENT_DOWN,1,&t.layout.haptic);point_event(40,RISC_TOUCH_EVENT_UP,1,&t.layout.haptic);tap(70,&t.layout.haptic);back(110);finish_case(true);CHECK(t.pulses==1);
    for(unsigned mode=0;mode<4;mode++){
        reset_case(w,h,"invalid initial snapshot returns without raster");if(mode==0)t.fail_snapshot_at=1;if(mode==1)t.mismatch_snapshot_at=1;if(mode==2)t.invalid_count_snapshot_at=1;if(mode==3)t.invalid_point_snapshot_at=1;finish_case(true);CHECK(!t.submitted);
        reset_case(w,h,"invalid first-visible snapshot exits safely");if(mode==0)t.fail_snapshot_at=2;if(mode==1)t.mismatch_snapshot_at=2;if(mode==2)t.invalid_count_snapshot_at=2;if(mode==3)t.invalid_point_snapshot_at=2;finish_case(true);CHECK(t.completed==1&&!t.healthy_stop);
        reset_case(w,h,"invalid gap snapshot exits safely");if(mode==0)t.fail_snapshot_at=3;if(mode==1)t.mismatch_snapshot_at=3;if(mode==2)t.invalid_count_snapshot_at=3;if(mode==3)t.invalid_point_snapshot_at=3;add_event(40,-1,0,0,0,0);finish_case(true);CHECK(t.completed&&!t.healthy_stop);
    }
    reset_case(w,h,"deferred actions wait for active or queued frame");t.later_delay=80;tap(40,&t.layout.motion);back(180);finish_case(true);CHECK(!t.pulses&&t.acquires[4]==1&&t.first_optional_tick>42);
    reset_case(w,h,"deferred haptic waits for active or queued frame");t.later_delay=80;tap(40,&t.layout.haptic);back(180);finish_case(true);CHECK(t.pulses==1&&t.first_optional_tick>42);
    reset_case(w,h,"GAP cancels action already queued behind frame");t.later_delay=80;tap(40,&t.layout.haptic);add_event(45,-1,2,0,0,0);back(180);finish_case(true);CHECK(!t.pulses&&!t.acquires[5]);
    reset_case(w,h,"GAP cancels Back already queued behind frame");t.later_delay=80;back(40);add_event(45,-1,2,0,0,0);back(180);finish_case(true);CHECK(t.ticks>=182&&!t.healthy_stop);
    reset_case(w,h,"poll error cancels Back already queued behind frame");t.later_delay=80;t.poll_fail_at=45;back(40);back(180);finish_case(true);CHECK(t.ticks>=182&&!t.healthy_stop);
    reset_case(w,h,"back waits for accepted frame to finish");t.later_delay=80;tap(40,&t.layout.motion);back(48);finish_case(true);CHECK(t.completed==t.submitted&&t.now>=t.ready);
    for(unsigned mode=DISPLAY_FAIL;mode<=DISPLAY_BAD_LEASE;mode++){
        reset_case(w,h,"presentation and lease rejection");t.display=(enum display_mode)mode;back(100);finish_case(false);
        if(mode==DISPLAY_ACQUIRE_FAIL)CHECK(!t.submitted&&!t.frame_releases);
        else if(mode==DISPLAY_SUBMIT_FAIL||mode==DISPLAY_BAD_LEASE)CHECK(!t.submitted&&t.frame_releases==1);
        else CHECK(t.submitted==1&&t.terminal_frames==1);
        CHECK(!t.acquires[4]&&!t.acquires[5]);
    }
    printf("Bringup lifecycle %ux%u: optional APIs, gestures, snapshots, gaps, deferred frames, rejection and reverse cleanup PASS\n",w,h);
}
static void retained_cases(void) {
    for(volatile unsigned mode=DISPLAY_STUCK_ACTIVE;mode<=DISPLAY_STUCK_UNKNOWN;mode++){
        reset_case(128,128,"unresolved accepted frame retains custody");t.display=(enum display_mode)mode;expect_retained=true;
        if(!setjmp(retained_return)){app_main();CHECK(false);}
        expect_retained=false;CHECK(t.retained&&!t.returned&&t.pending&&!t.borrowed&&t.subscribed);
        CHECK(t.live==15&&!t.release_count&&!t.frame_releases&&t.status_calls>=10000);
        CHECK(t.hash==raster_hash(&t.image));gs_check(&t.image);free(t.image.allocation);
    }
    puts("Bringup retained custody: unresolved ACTIVE, QUEUED, status error and unknown state never release grants or return Home PASS");
    for(volatile unsigned failure=0;failure<8;failure++){
        reset_case(128,128,"cleanup failure retains remaining custody");t.stop_fail=failure==0;t.unsubscribe_fail=failure==1;if(failure>=2)t.release_fail_slot=8-failure;
        tap(40,&t.layout.motion);tap(80,&t.layout.haptic);back(120);expect_retained=true;
        if(!setjmp(retained_return)){app_main();CHECK(false);}
        expect_retained=false;CHECK(t.retained&&!t.returned&&!t.pending&&!t.borrowed&&t.stops==1);
        if(failure<2){CHECK(t.live==63&&!t.release_count&&t.subscribed);CHECK(t.unsubscribe_attempts==failure);}
        else {CHECK(!t.subscribed&&t.unsubscribe_attempts==1);CHECK(t.live==(1u<<t.release_fail_slot)-1);CHECK(t.release_count==6-t.release_fail_slot);
            for(unsigned i=0;i<6;i++){CHECK(t.release_attempts[i]==(i+1>=t.release_fail_slot));CHECK(t.releases[i]==(i+1>t.release_fail_slot));}}
        gs_check(&t.image);free(t.image.allocation);
    }
    puts("Bringup retained cleanup: failed haptic stop, unsubscribe and every reverse-order grant release stop further cleanup and Home return PASS");
}
static void safe_bounds_cases(void) {
    for(unsigned w=240;w<=700;w+=23) for(unsigned h=240;h<=800;h+=29) {
        watch_layout v;bringup_layout l;const watch_layout_insets in={13,17,19,11};
        CHECK(watch_layout_native_open(&v,w,h,&in));CHECK(bringup_layout_open_safe(&l,&v));
        const watch_layout_rect *r[]={&l.title,&l.bars,&l.motion,&l.haptic,&l.back};
        for(unsigned n=0;n<5;n++){CHECK(r[n]->x>=v.safe.x&&r[n]->y>=v.safe.y);CHECK(r[n]->x+(int)r[n]->width<=v.safe.x+(int)v.safe.width);CHECK(r[n]->y+(int)r[n]->height<=v.safe.y+(int)v.safe.height);}
        CHECK(!bringup_hit(&l,0,0));CHECK(bringup_hit(&l,l.back.x,l.back.y)==3);
    }
    watch_layout v;bringup_layout l;const watch_layout_insets round={69,69,69,69};
    CHECK(watch_layout_native_open(&v,466,466,&round));CHECK(bringup_layout_open_safe(&l,&v));
    guarded_surface g=gs_new(466,466);CHECK(render(&g.surface,&l,1,"BATTERY UNKNOWN","NO RTC","MOTION UNAVAILABLE","DEVICE PENDING",233,233,1));gs_check(&g);write_raster(&g);free(g.allocation);
    puts("Bringup safe bounds: generated geometry, full 466 raster and 328 content controls PASS");
}
int main(void){
    bool cheap=getenv("WATCH_TEST_MONOTONIC")!=NULL;
    if(cheap)test_runtime.monotonic_ms=t_monotonic;
    safe_bounds_cases();
    const unsigned dimensions[][2]={{466,466},{240,240},{410,502},{128,128},{502,410},{320,172}};
    for(unsigned i=0;i<sizeof(dimensions)/sizeof(*dimensions);i++){renderer_case(dimensions[i][0],dimensions[i][1]);lifecycle_cases(dimensions[i][0],dimensions[i][1]);}
    /* Each scenario re-enters the actual app; repeat the first size after all others. */
    lifecycle_cases(240,240);retained_cases();
    CHECK(cheap ? (fixture_monotonic_calls>0&&!fixture_health_calls) : (fixture_health_calls>0&&!fixture_monotonic_calls));
    printf("Bringup production renderer and app_main: %u lifecycle cases plus five native rasters PASS; no physical hardware tested\n",fixture_cases);return 0;
}
