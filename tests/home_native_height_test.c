/* Native-height Home render/input integration and preserved 240px goldens. */
#include "WatchHomeLayout.h"
#include "WatchViewport.h"
#include "apps/clock/nova/nova.h"
#include "apps/clock/faces/picker.h"
#include "apps/clock/effects/effects.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t image_hash(const risc_display_surface_v1 *s) {
    uint64_t hash=UINT64_C(1469598103934665603);
    for(unsigned y=0;y<s->height;y++)for(unsigned x=0;x<s->width*2;x++) {
        hash^=((uint8_t*)s->pixels)[y*s->stride_bytes+x];hash*=UINT64_C(1099511628211);
    }
    return hash;
}
static nova_watch_state state_for(unsigned n) {
    nova_watch_state state={.time={.year=2026,.month=10,.day=13,.hour=21,.minute=37,.second=49},
        .time_valid=true,.battery_valid=true,.battery_percent=73,.subsecond_ms=725,.animation_ms=13579};
    if(n==1){state.hour_24=true;state.time.hour=0;state.time.second=0;state.subsecond_ms=0;}
    if(n==2){state.time_valid=false;state.battery_valid=false;}
    static const nova_points_state ready={.status=NOVA_POINTS_READY,.phase=NOVA_PHASE_OFF_WORK,
        .now_rtc=1000,.next_count=3,.today_count=2,.previous_valid=true,
        .previous={.at_rtc=900,.hour=20,.minute=0,.kind=NOVA_POINT_WORK_END},
        .next={{.at_rtc=1100,.hour=22,.minute=30,.kind=NOVA_POINT_BEDTIME},
               {.at_rtc=1200,.hour=23,.minute=0,.kind=NOVA_POINT_CUSTOM_1,.label="WIND DOWN",.color_index=3},
               {.at_rtc=30000,.hour=8,.minute=0,.kind=NOVA_POINT_WORK_START,.day_offset=1}},
        .today={{.at_rtc=900,.hour=20,.minute=0,.kind=NOVA_POINT_WORK_END},
                {.at_rtc=1100,.hour=22,.minute=30,.kind=NOVA_POINT_BEDTIME}}};
    static const nova_points_state empty={.status=NOVA_POINTS_EMPTY},error={.status=NOVA_POINTS_ERROR};
    if(n==3)state.points=&ready;
    if(n==4)state.points=&empty;
    if(n==5)state.points=&error;
    return state;
}
#ifndef HOME_WRITE_GOLDENS
#include "home_240_goldens.inc"
#endif
static unsigned golden_index;
static void golden(const risc_display_surface_v1 *s) {
    uint64_t actual=image_hash(s);
#ifdef HOME_WRITE_GOLDENS
    printf("UINT64_C(0x%016llx),\n",(unsigned long long)actual);
#else
    assert(golden_index<sizeof(home_240_goldens)/sizeof(*home_240_goldens));
    if(actual!=home_240_goldens[golden_index]){
        fprintf(stderr,"240px golden %u: %016llx != %016llx\n",golden_index,
            (unsigned long long)actual,(unsigned long long)home_240_goldens[golden_index]);abort();
    }
#endif
    golden_index++;
}
static void test_goldens(void) {
    uint16_t *pixels=calloc(240*240,2);assert(pixels);
    risc_display_surface_v1 s={0,pixels,240,240,480,240*240*2,5};
    nova_watch_picker_cache *cache=nova_watch_picker_cache_create(240,294);assert(cache);
    for(unsigned n=0;n<6;n++) {
        nova_watch_state state=state_for(n);
        for(unsigned id=0;id<WATCH_FACE_COUNT;id++) {assert(nova_watch_face_render(&s,&state,id));golden(&s);}
        assert(nova_watch_render(&s,&state));golden(&s);
        int positions[WATCH_FACE_CATEGORY_COUNT]={0};
        for(unsigned c=0;c<WATCH_FACE_CATEGORY_COUNT;c++) {
            assert(nova_watch_picker_collections_render(&s,&state,3,-(int)c*240*256,positions,NULL,0,256,cache));golden(&s);
            assert(nova_watch_picker_collections_render(&s,&state,3,-((int)c*240+97)*256,positions,"SAVE FAILED",3,267,cache));golden(&s);
        }
    }
    for(unsigned flags=0;flags<64;flags++) {
        assert(nova_watch_alarm_label_render(&s,"Evening reminder",flags&1,flags&2,flags&4,flags&8,flags&16,flags&32));golden(&s);
    }
    assert(nova_watch_context_message(&s,"KITCHEN EVENING ACTIVITY"));golden(&s);
    assert(nova_watch_sleep_status(&s,"SLEEP ERR MOTION"));golden(&s);
    for(unsigned age=0;age<=1800;age+=300){assert(watch_boot_render(&s,age));golden(&s);}
    for(unsigned scan=0;scan<19;scan++){assert(watch_ripple_render(&s,scan));golden(&s);}
    nova_watch_picker_cache_destroy(cache);free(pixels);
#ifndef HOME_WRITE_GOLDENS
    assert(golden_index==sizeof(home_240_goldens)/sizeof(*home_240_goldens));
#endif
}
#ifndef HOME_WRITE_GOLDENS
static unsigned checkpoints;
static bool count_capture(void *context) {(void)context;checkpoints++;return true;}
static void check_guards(const uint16_t *buffer,unsigned height,unsigned stride) {
    for(unsigned i=0;i<16;i++)assert(buffer[i]==0xa5a5);
    for(unsigned y=0;y<height;y++)for(unsigned x=240;x<stride;x++)assert(buffer[16+y*stride+x]==0xa5a5);
    for(unsigned i=0;i<16;i++)assert(buffer[16+height*stride+i]==0xa5a5);
}
static unsigned ink_in(const risc_display_surface_v1 *s,unsigned first,unsigned end) {
    unsigned count=0;
    for(unsigned y=first;y<end;y++)for(unsigned x=0;x<240;x++)
        count+=((uint16_t*)((uint8_t*)s->pixels+y*s->stride_bytes))[x]!=0;
    return count;
}
static void test_native_render(void) {
    const unsigned height=294,stride=248;
    uint16_t *storage=malloc((height*stride+32)*2),*square_pixels=calloc(240*240,2);assert(storage&&square_pixels);
    memset(storage,0xa5,(height*stride+32)*2);
    risc_display_surface_v1 tall={0,storage+16,240,height,stride*2,height*stride*2,5};
    risc_display_surface_v1 square={0,square_pixels,240,240,480,240*240*2,5};
    nova_watch_picker_cache *cache=nova_watch_picker_cache_create(240,294);assert(cache);
    for(unsigned n=0;n<6;n++) {
        nova_watch_state state=state_for(n);state.capture_audio=count_capture;
        for(unsigned id=0;id<WATCH_FACE_COUNT;id++) {
            checkpoints=0;assert(nova_watch_face_render(&tall,&state,id));assert(checkpoints>0);
            assert(nova_watch_face_render(&square,&state,id));
            for(unsigned y=0;y<240;y++)assert(!memcmp((uint8_t*)tall.pixels+(y+27)*tall.stride_bytes,square_pixels+y*240,480));
            assert(ink_in(&tall,0,27)>0&&ink_in(&tall,267,294)>0);
            check_guards(storage,height,stride);
        }
        assert(nova_watch_render(&tall,&state));assert(nova_watch_render(&square,&state));
        for(unsigned y=0;y<240;y++)assert(!memcmp((uint8_t*)tall.pixels+(y+27)*tall.stride_bytes,square_pixels+y*240,480));
        check_guards(storage,height,stride);
        int positions[WATCH_FACE_CATEGORY_COUNT]={0};
        for(unsigned c=0;c<WATCH_FACE_CATEGORY_COUNT;c++)for(unsigned shift=0;shift<294;shift+=21) {
            assert(nova_watch_picker_collections_render(&tall,&state,3,-((int)c*294+(int)shift)*256,positions,NULL,0,256,cache));
            check_guards(storage,height,stride);
        }
    }
    nova_watch_state state=state_for(0);
    nova_points_state points={.status=NOVA_POINTS_READY,.now_rtc=100,.next_count=1};
    points.next[0]=(nova_point_event){.at_rtc=200,.hour=22,.minute=30,.kind=NOVA_POINT_BEDTIME};state.points=&points;
    assert(nova_watch_face_render(&tall,&state,0));uint64_t before=image_hash(&tall);
    points.next[0].hour=23;assert(nova_watch_face_render(&tall,&state,0));assert(image_hash(&tall)!=before);
    /* The extra summary is live without changing the proportional core face. */
    assert(nova_watch_face_render(&square,&state,0));
    for(unsigned y=0;y<240;y++)assert(!memcmp((uint8_t*)tall.pixels+(y+27)*tall.stride_bytes,square_pixels+y*240,480));
    for(unsigned flags=0;flags<64;flags++) {
        assert(nova_watch_alarm_label_render(&tall,"Evening reminder",flags&1,flags&2,flags&4,flags&8,flags&16,flags&32));
        assert(nova_watch_alarm_label_render(&square,"Evening reminder",flags&1,flags&2,flags&4,flags&8,flags&16,flags&32));
        for(unsigned y=150;y<240;y++)assert(!memcmp((uint8_t*)tall.pixels+(y+54)*tall.stride_bytes,square_pixels+y*240,480));
        check_guards(storage,height,stride);
    }
    assert(nova_watch_context_message(&tall,"KITCHEN EVENING ACTIVITY"));assert(ink_in(&tall,235,250));
    assert(nova_watch_sleep_status(&tall,"SLEEP ERR MOTION"));assert(ink_in(&tall,260,285));check_guards(storage,height,stride);
    for(unsigned age=0;age<=1800;age+=300) {
        assert(watch_boot_render(&tall,age));assert(watch_boot_render(&square,age));
        for(unsigned y=0;y<240;y++)assert(!memcmp((uint8_t*)tall.pixels+(y+27)*tall.stride_bytes,square_pixels+y*240,480));
        check_guards(storage,height,stride);
    }
    for(unsigned scan=0;scan<19;scan++){assert(watch_ripple_render(&tall,scan));check_guards(storage,height,stride);}
    assert(!ink_in(&tall,0,height));
    risc_display_surface_v1 invalid=tall;invalid.height=0;assert(!nova_watch_face_render(&invalid,&state,0));
    invalid=tall;invalid.height=2049;assert(!nova_watch_face_render(&invalid,&state,0));
    invalid=tall;invalid.size_bytes--;assert(!nova_watch_face_render(&invalid,&state,0));
    invalid=tall;invalid.stride_bytes=UINT32_MAX-1;assert(!nova_watch_face_render(&invalid,&state,0));
    nova_watch_picker_cache_destroy(cache);free(square_pixels);free(storage);
    puts("Home native render: every face/core parity, live added Point summary, picker collections, bottom-anchored alerts/status, proportional effects, padded guards PASS");
}
static void test_native_input(void) {
    watch_layout layout;assert(watch_layout_open(&layout,410,502,NULL));
    assert(layout.height==294&&watch_home_picker_center(layout.height)==119);
    unsigned hit_count=0;
    for(unsigned y=0;y<502;y++)for(unsigned x=0;x<410;x++) {
        uint16_t lx,ly;assert(watch_layout_touch(&layout,x,y,&lx,&ly));
        watch_face_picker p={0};assert(watch_face_set_height(&p,294));p.open=true;
        assert(watch_face_input(&p,0,true,0,0,0,0)==WATCH_FACE_NONE);
        assert(watch_face_input(&p,10,true,1,1,lx,ly)==WATCH_FACE_NONE);
        bool selected=watch_face_input(&p,20,true,0,0,0,0)==WATCH_FACE_SELECT;
        bool expected=lx>54&&lx<186&&ly>53&&ly<185;
        assert(selected==expected);hit_count+=selected;
        assert((ly>=watch_home_alarm_retry(layout.height))==(y>=431));
        assert((ly>=watch_home_alarm_dismiss(layout.height)&&ly<watch_home_alarm_retry(layout.height))==(y>=349&&y<431));
    }
    watch_face_picker p={0};assert(watch_face_set_height(&p,294));
    watch_face_input(&p,0,true,0,0,0,0);watch_face_input(&p,10,true,1,1,120,293);
    assert(watch_face_input(&p,620,true,1,1,120,293)==WATCH_FACE_OPEN);
    assert(!watch_face_set_height(&p,240));
    watch_face_input(&p,630,true,0,0,0,0);watch_face_input(&p,640,true,1,1,120,100);
    watch_face_input(&p,660,true,1,1,120,160);assert(p.axis==2&&p.category_position);
    watch_face_input(&p,670,true,1,1,120,294);assert(!p.down&&!p.neutral);
    watch_face_input(&p,680,true,0,0,0,0);watch_face_input(&p,690,true,1,1,120,119);
    watch_face_input(&p,700,true,2,1,120,119);assert(!p.down&&!p.neutral);
    puts("Home native input: all 205820 pixels, exact visible-card hit boundaries, final-row hold, category drag, invalid/multitouch cancellation PASS");
    assert(hit_count>0);
}
#endif
int main(void) {
#ifdef HOME_WRITE_GOLDENS
    puts("/* Captured from Home 16d15ab before native-height renderer edits. */\nstatic const uint64_t home_240_goldens[]={");
#endif
    test_goldens();
#ifndef HOME_WRITE_GOLDENS
    test_native_render();test_native_input();
#endif
#ifdef HOME_WRITE_GOLDENS
    puts("};");
#else
    printf("Home 240px golden parity: %u frames PASS\n",golden_index);
#endif
    return 0;
}
