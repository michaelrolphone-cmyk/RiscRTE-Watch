/* Exercise production cold-boot and normal-return snapshot paths against a
 * native single-buffer provider. No wall-clock or hardware timing claim. */
#define WATCH_CLOCK_RETURN
#define main inherited_quick_main
#include "quick_clock_test.c"
#undef main
static uint16_t outgoing[TEST_WIDTH*TEST_HEIGHT],mixed[TEST_WIDTH*TEST_HEIGHT],blur[TEST_WIDTH*TEST_HEIGHT];
static bool checking_return;
static bool lifecycle_submit(void *c,risc_display_frame_v1 frame,const risc_display_rect_v1 *rects,
        size_t count,const risc_display_present_options_v1 *options,risc_display_present_token_v1 *token) {
    fixture_expected_face(true);
    memcpy(mixed,expected_pixels,sizeof(mixed));
    unsigned alpha=checking_return?(clock_ms>=60u?256u:clock_ms*256u/60u):portable_transition_alpha(clock_ms);
    assert(portable_transition_rgb565(mixed,TEST_WIDTH*2u,outgoing,TEST_WIDTH*2u,blur,sizeof(blur),TEST_WIDTH,TEST_HEIGHT,alpha));
    assert(!memcmp(mixed,pixels,sizeof(mixed)));
    return submit_frame(c,frame,rects,count,options,token);
}
static void prepare_case(bool returning) {
    assert(!allocations&&!held);checking_return=returning;
    clock_ms=submitted=released=last_complete=face_writes=0;
    owned=pending=launched=ready=false;scenario=0;hardware_brightness=67;
    rt=&runtime;rtc=&ra;touch=(watch_launcher_touch){0};
    reset_telemetry();pqa_session_init(&clock_quick);picker=(watch_face_picker){0};
    assert(watch_face_set_size(&picker,TEST_WIDTH,TEST_HEIGHT));
    assert(pqa_set_dimensions(&clock_quick.ui,TEST_WIDTH,TEST_HEIGHT));
    sleep_status[0]=0;
    static risc_display_output_api_v1 output;
    output=da.base;output.submit=lifecycle_submit;
    assert(watch_viewport_open(&clock_viewport,&output));display=&clock_viewport.display;
    for(unsigned y=0;y<TEST_HEIGHT;y++)for(unsigned x=0;x<TEST_WIDTH;x++)outgoing[y*TEST_WIDTH+x]=(uint16_t)(x*109u+y*29u);
    if(!returning) {
        risc_display_surface_v1 s={0,outgoing,TEST_WIDTH,TEST_HEIGHT,TEST_WIDTH*2,sizeof(outgoing),5};
        assert(watch_boot_render(&s,1800));
    }
    memcpy(pixels,outgoing,sizeof(pixels));
}
static void finish_case(void) {
    assert(!held&&!owned&&!pending&&!allocations&&submitted>1&&last_complete==submitted&&hardware_brightness==67);
    fixture_expected_face(true);assert(!memcmp(expected_pixels,pixels,sizeof(pixels)));
    watch_viewport_close(&clock_viewport);
}
static unsigned legacy_touch_calls,legacy_touch_retains;
static bool legacy_touch_poll(void *c,size_t n){(void)c;assert(n==1&&!legacy_touch_retains);legacy_touch_calls++;return true;}
static int32_t legacy_touch_next(void*c,uint64_t id,risc_touch_event_v1*out){(void)c;(void)out;assert(id==1&&!legacy_touch_retains);legacy_touch_calls++;return -2;}
static bool legacy_touch_snapshot(void*c,risc_touch_snapshot_v1*out){(void)c;(void)out;assert(false);return false;}
static bool legacy_touch_retain(void){assert(!legacy_touch_retains);legacy_touch_retains++;return true;}
static void legacy_terminal_custody(void) {
 risc_runtime_api_v1 terminal_runtime=runtime;
 terminal_runtime.retain_invocation=legacy_touch_retain;
 terminal_runtime.struct_size=RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE;rt=&terminal_runtime;
 risc_touch_api_v1 terminal_touch=ta;
 terminal_touch.poll=legacy_touch_poll;terminal_touch.next=legacy_touch_next;terminal_touch.snapshot=legacy_touch_snapshot;
 touch=(watch_launcher_touch){.api=&terminal_touch,.subscription=1};clock_touch_retained=false;
 assert(!launcher_touch_capture(&touch,100).valid&&touch.terminal&&clock_touch_retained);
 assert(legacy_touch_calls==2&&legacy_touch_retains==1);
 uint32_t now=0;risc_display_surface_v1 blocked={0};
 assert(!alive(&now)&&!frame(&blocked)&&!present()&&!pace_frame(0));
 sample_launcher_touch(100,true);assert(!launcher_touch_close(&touch)&&!launcher_touch_open(&touch));
 clock_exit_checkpoint(__LINE__);assert(legacy_touch_calls==2&&legacy_touch_retains==1);
}
int main(void) {
    prepare_case(true);assert(return_to_clock());finish_case();
    prepare_case(false);assert(logo_to_clock());finish_case();
    legacy_terminal_custody();
    printf("Crown native lifecycle %ux%u: exact physical retained snapshots, boot blend, final sharp frame, no provider lease/allocation leak PASS\n",TEST_WIDTH,TEST_HEIGHT);
    #ifdef WATCH_PERFORMANCE_TRACE
 assert(fixture_perf_calls&&fixture_perf_complete);
#endif
 return 0;
}
