/* Clock-only caller of the same shared service client. Included after the
 * settled frame helpers; contains no schedule/RTC/output policy. */
#include "PortableAlarmClient.h"
#include "PortableTouch.h"
#include "watch_alarm_sleep.h"
static portable_alarm_client clock_alarm;
static bool clock_alarm_custody_live(void) {
    if(!clock_canvas_live())return false;
#ifdef ALARM_SERVICE_TAGGED_V2
    if(clock_alarm.retained){
#ifdef WATCH_RUNTIME_FEATURES
        if(!native_clock.halted)watch_runtime_halt(&native_clock);
#endif
        return false;
    }
#endif
    return clock_canvas_live();
}
static bool clock_alarm_refresh_checked(void) {
    if(!clock_alarm_custody_live())return false;
#ifdef ALARM_SERVICE_TAGGED_V2
    bool ok=portable_alarm_refresh(&clock_alarm);
    return clock_alarm_custody_live()&&ok;
#else
    (void)clock_alarm.api->refresh(clock_alarm.api->context);return clock_alarm_custody_live();
#endif
}

/* An offline bring-up profile can omit alarm.service. Do not turn the missing
 * optional function into a default-app exit or retry it on every frame. An
 * acquired malformed interface is still an error and retains normal cleanup. */
static bool clock_alarm_open_optional(void) {
    clock_alarm_attempted=true;
#ifdef ALARM_SERVICE_TAGGED_V2
    memset(&clock_alarm,0,sizeof(clock_alarm));clock_alarm.grant.struct_size=sizeof(clock_alarm.grant);
    bool acquired=rt->acquire(ALARM_SERVICE_CAPABILITY,ALARM_SERVICE_API_V2,0,&clock_alarm.grant);
    if(!clock_canvas_live())return false;
    bool opened=acquired&&alarm_service_descriptor(clock_alarm.grant.api)!=NULL;
    if(opened)clock_alarm.api=clock_alarm.grant.api;
#else
    bool opened=portable_alarm_open(&clock_alarm,rt);
#endif
    if(!clock_alarm_custody_live())return false;
    if(opened)return true;
    if(clock_alarm.grant.api||clock_alarm.grant.slot||clock_alarm.grant.generation)return false;
    rt->diagnostic("WATCH_CLOCK alarm-service=unavailable alarms=disabled");
    return true;
}
static bool clock_alarm_failed_cleaned,clock_alarm_error_seen;
static int32_t clock_alarm_seen_error;
static uint32_t clock_alarm_seen_snapshot;
/* Monotonic modulo counter of actual alarm input cancellations, independent
 * of FAST_WAKE's consumed flag. Used only for copied resident outcomes. */
static uint32_t clock_alarm_input_revision;
static void clock_alarm_cancel_input(void) {
    ++clock_alarm_input_revision;
#ifdef WATCH_FAST_WAKE
    /* Entry can now sample input before the alarm closure has loaded. An
     * intervening due alarm must also cancel that sampled crown action. */
    clock_alarm_input_cancelled=true;
#endif
#ifdef WATCH_QUICK_ACTIONS
    pqa_cancel(&clock_quick.ui);clock_quick.ui.pending&=~(uint32_t)PQA_WIFI;
#endif
    launcher_swipe_pending=launcher_activity_pending=false;
    /* A long press sampled inside present() marks the picker open before the
     * settled loop allocates its cache. Cancelling that queued opening must
     * also close the unpresented picker, or invisible picker input would live
     * on after the alert. A displayed picker retains its state and allocation. */
    if(picker_open_pending && !picker_scratch)watch_face_close(&picker);
    picker_open_pending=picker_select_pending=false;
    watch_face_reset_contact(&picker);
}
static bool clock_alarm_failure(void) {
    if(!clock_alarm_custody_live())return false;
    if(clock_alarm_failed_cleaned)return false;
#ifdef WATCH_CONTEXTS_CLIENT
    if(!clock_contexts_pause())return false;
#endif
    clock_alarm_failed_cleaned=true;
    if(!portable_alarm_failure_stop(&clock_alarm)) {
#ifdef ALARM_SERVICE_TAGGED_V2
        clock_alarm.retained=true;(void)clock_alarm_custody_live();return false;
#else
        rt->diagnostic("ALARM clock-failed output-stop-unconfirmed; invocation retained");
        for(;;)rt->yield_ms(50);
#endif
    }
    return false;
}
#ifdef WATCH_CONTEXTS_CLIENT
static bool clock_alarm_pump_checked(void) {
#ifdef WATCH_CONTEXTS_CLIENT
    bool status_ok=portable_alarm_status(&clock_alarm);
    if(!clock_alarm_custody_live()||!status_ok)return false;
    if((portable_alarm_owned(&clock_alarm)||clock_alarm.status.state==ALARM_STATE_BLOCKED)&&
       !clock_contexts_pause())return false;
#endif
    bool pumped=portable_alarm_pump(&clock_alarm);
    if(!clock_alarm_custody_live()||!pumped)return false;
#ifdef WATCH_CONTEXTS_CLIENT
    if((portable_alarm_owned(&clock_alarm)||clock_alarm.status.state==ALARM_STATE_BLOCKED)&&
       !clock_contexts_pause())return false;
#endif
    return true;
}
#else
#define clock_alarm_pump_checked() portable_alarm_pump(&clock_alarm)
#endif
static bool clock_alarm_foreground(void) {
    if(!clock_canvas_live())return false;
    if(!clock_alarm.api)return true;
    (void)portable_touch_open;(void)portable_touch_close;
    if(!clock_display_settled || held)return clock_alarm_failure();
#ifdef WATCH_BLE_BROADCAST
    if(!clock_broadcast_step())return false;
#endif
    if(!clock_alarm_pump_checked())return clock_alarm_failure();
#ifdef ALARM_STATUS_CUE_SUPPORTED
    bool cue_seen=false;
    for(unsigned n=0;clock_alarm.status.state==ALARM_STATE_LOADING || portable_alarm_cue(&clock_alarm);n++) {
        if(n>=64)return clock_alarm_failure();
        if(portable_alarm_cue(&clock_alarm))cue_seen=true;
        rt->yield_ms(portable_alarm_cue(&clock_alarm)?20:1);
        if(!clock_alarm_pump_checked())return clock_alarm_failure();
        if(cue_seen && !portable_alarm_cue(&clock_alarm)) {
            clock_alarm_cancel_input();
            if(clock_alarm.status.state!=ALARM_STATE_BLOCKED)return true;
            break;
        }
    }
#else
    for(unsigned n=0;clock_alarm.status.state==ALARM_STATE_LOADING;n++) {
        if(n>=64)return clock_alarm_failure();
        rt->yield_ms(1);
        if(!clock_alarm_pump_checked())return clock_alarm_failure();
    }
#endif
    bool error=clock_alarm.status.state==ALARM_STATE_BLOCKED;
    if(!portable_alarm_owned(&clock_alarm) && (!error || (clock_alarm_error_seen &&
       clock_alarm_seen_error==clock_alarm.status.error && clock_alarm_seen_snapshot==clock_alarm.status.snapshot)))return true;
    clock_alarm_cancel_input();clock_alarm_modal=true;
#ifdef WATCH_HOME_CANVAS
    if(clock_canvas_selected)home_canvas_touch_discard_navigation(&clock_canvas_touch);
#endif
    portable_touch alert_touch={.api=touch.api,.subscription=touch.subscription};
    uint32_t discarded;
    if(!clock_key_read(&discarded,"WATCH_KEY stage=alarm-entry-discard"))return clock_alarm_failure();
    alarm_token_v1 shown=clock_alarm.status.occurrence;
    bool redraw=true;
    for(;;) {
        if(redraw) {
            risc_display_surface_v1 s={0};
            if(!frame(&s) || !nova_watch_alarm_label_render(&s,clock_alarm.status.label,clock_alarm.status.occurrence.kind==ALARM_KIND_COUNTDOWN,
               clock_alarm.status.state==ALARM_STATE_BLOCKED,clock_alarm.status.error==ALARM_RTC,
               clock_alarm.status.state==ALARM_STATE_DISMISSING,clock_alarm.status.output_uncertain,
               clock_alarm.status.occurrence.generation!=0) || !present() ||
               !display->set_brightness(display->context,(uint16_t)clock_brightness(),100))return clock_alarm_failure();
            redraw=false;
        }
        uint32_t now,events=0;
        if(!clock_display_settled || held || !alive(&now))return clock_alarm_failure();
#ifdef WATCH_BLE_BROADCAST
        if(!clock_broadcast_step())return false;
#endif
        rt->yield_ms(20);
        if(!clock_key_read(&events,"WATCH_KEY stage=alarm-modal"))return clock_alarm_failure();
        portable_touch_sample sample;portable_touch_read(&alert_touch,&sample);
        if(!clock_canvas_live())return false;
        bool tap=sample.valid && sample.released && sample.tap_eligible && !sample.cancelled && !sample.moved;
        bool retry=tap && sample.y>=watch_home_alarm_retry_at(clock_viewport.width,clock_viewport.height);
        bool dismiss=(events&3u) || (tap && sample.y>=watch_home_alarm_dismiss_at(clock_viewport.width,clock_viewport.height) && sample.y<watch_home_alarm_retry_at(clock_viewport.width,clock_viewport.height));
#ifdef WATCH_HOME_CANVAS
        if(clock_canvas_selected){
            bool controls=false;
            risc_input_navigation_frame_v1 nav=home_canvas_touch_navigation(&clock_canvas_touch,&controls);
            /* Modal actions are explicit logical button intents, never a
             * synthesized touch at a device-specific coordinate. */
            retry|=controls||!!(nav.pressed&(RISC_NAV_UP|RISC_NAV_PAGE_BACK));
            dismiss|=!!(nav.pressed&(RISC_NAV_CONFIRM|RISC_NAV_BACK|RISC_NAV_HOME));
        }
#endif
        if(clock_alarm.status.state==ALARM_STATE_BLOCKED && retry) {
            if(!clock_alarm_refresh_checked())return clock_alarm_failure();
            alert_touch.neutral=alert_touch.down=false;redraw=true;
        } else if(dismiss) {
            if(clock_alarm.status.occurrence.generation) {
                alarm_token_v1 token=clock_alarm.status.occurrence;
#ifdef ALARM_SERVICE_TAGGED_V2
                bool acknowledged=portable_alarm_acknowledge(&clock_alarm,&token);
                if(!clock_alarm_custody_live()||!acknowledged)return clock_alarm_failure();
#else
                (void)clock_alarm.api->acknowledge(clock_alarm.api->context,&token);
#endif
                alert_touch.neutral=alert_touch.down=false;redraw=true;
            } else if(clock_alarm.status.state==ALARM_STATE_BLOCKED && !clock_alarm.status.output_uncertain) {
                clock_alarm_error_seen=true;clock_alarm_seen_error=clock_alarm.status.error;
                clock_alarm_seen_snapshot=clock_alarm.status.snapshot;break;
            }
        }
        uint32_t old=clock_alarm.status.state;int32_t old_error=clock_alarm.status.error;
        if(!clock_alarm_pump_checked())return clock_alarm_failure();
        if(memcmp(&shown,&clock_alarm.status.occurrence,sizeof(shown))) {
            shown=clock_alarm.status.occurrence;alert_touch.neutral=alert_touch.down=false;redraw=true;
        }
        if(old!=clock_alarm.status.state || old_error!=clock_alarm.status.error)redraw=true;
        if(clock_alarm.status.state==ALARM_STATE_READY && !portable_alarm_owned(&clock_alarm))break;
    }
    clock_alarm_modal=false;clock_alarm_cancel_input();
    /* The alert can outlast Clock's idle interval. Its dismissal is activity:
     * the settled main loop must present the retained picker before starting
     * a fresh inactivity interval, rather than immediately sleeping it. */
    launcher_activity_pending=true;
    return true;
}
static void clock_alarm_finish(void) {
    if(!clock_alarm_custody_live())return;
    if(clock_alarm.api && !clock_alarm_failed_cleaned &&
       (!clock_display_settled || !portable_alarm_status(&clock_alarm) || portable_alarm_owned(&clock_alarm)))
        (void)clock_alarm_failure();
    if(!clock_alarm_custody_live())return;
#ifdef ALARM_SERVICE_TAGGED_V2
    /* A successful grant with a malformed null API still owns its token. */
    if(!clock_alarm.grant.api&&(clock_alarm.grant.slot||clock_alarm.grant.generation)){
        if(!rt->release(&clock_alarm.grant)||!clock_canvas_live()){
            clock_alarm.retained=true;(void)clock_alarm_custody_live();return;
        }
    }
    if(!portable_alarm_close(&clock_alarm,rt)){
        clock_alarm.retained=true;(void)clock_alarm_custody_live();return;
    }
#else
    if(!portable_alarm_close(&clock_alarm,rt))rt->diagnostic("ALARM clock-release-failed");
#endif
}
