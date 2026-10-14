/* Invoked after the loading interstitial and provider promotion. No capture
 * or storage is added to panic handling, log callbacks, or timer-only wakes. */
#ifdef WATCH_CRASH_REPORT
#include "CrashReportSession.h"
#ifndef WATCH_CRASH_REPORT_NAMESPACE
#error Select a dedicated Home AppData namespace
#endif
static crash_report_session clock_crash;
static bool clock_crash_safe(void *context) {
 (void)context;uint32_t now;
 return !native_clock.halted && !clock_contexts_uncertain && !clock_broadcast_uncertain && alive(&now);
}
static bool clock_crash_prepare(void *context) {
 (void)context;return clock_crash_safe(NULL) && clock_contexts_pause() && clock_broadcast_pause();
}
static void clock_crash_retain(void *context) {(void)context;watch_runtime_halt(&native_clock);}
static uint64_t clock_crash_now(void *context) {(void)context;uint32_t now=0;(void)alive(&now);return now;}
static void clock_crash_init(void) {
 clock_crash_modal=false;
 crash_report_session_hooks hooks={.runtime=rt,.safe=clock_crash_safe,.prepare_storage=clock_crash_prepare,
  .now=clock_crash_now,.retain=clock_crash_retain,.namespace_id=WATCH_CRASH_REPORT_NAMESPACE};
#ifdef WATCH_CRASH_REPORT_VOLUME
 hooks.sd_enabled=true;hooks.volume_instance=WATCH_CRASH_REPORT_VOLUME;
#endif
 crash_session_init(&clock_crash,&hooks);
}
static bool clock_crash_step(void) {
 if(clock_handoff || clock_contexts_handoff || launcher_swipe_pending || clock_deferred_phase<4 || held || !clock_display_settled || clock_alarm_modal || picker.open || pqa_visible(&clock_quick.ui))return true;
 if(!crash_session_read(&clock_crash))return false;
 if(clock_crash.pending && !clock_crash.presented) {
  clock_crash_modal=true;
  CLOCK_WAKE_MARK("crash-report-begin");
  if(!clock_crash_prepare(NULL))return false;
  uint32_t offset=0,now=0,events=0;
  do {
   uint32_t next=offset;risc_display_surface_v1 s={0};
   if(!frame(&s) || !nova_watch_crash_report(&s,clock_crash.text,clock_crash.text_size,offset,&next) || !present())return false;
   if(!clock_key_read(&events,"WATCH_KEY stage=crash-discard"))return false;
   bool neutral=false,down=false;uint8_t id=0;
   for(;;) {
    if(!clock_crash_safe(NULL) || !alive(&now) || !clock_key_read(&events,"WATCH_KEY stage=crash-report"))return false;
    watch_launcher_observation o=launcher_touch_capture(&touch,now);
    if(!o.valid || o.count>1){neutral=false;down=false;}
    else if(!o.count){if(neutral && down)break;neutral=true;}
    else if(neutral){if(down && id!=o.id){neutral=false;down=false;}else {down=true;id=o.id;}}
    if(events&1u)break;
    rt->yield_ms(20);
   }
   offset=next;
  }while(offset<clock_crash.text_size);
  clock_crash.presented=true;
  if(!crash_session_ack(&clock_crash))return false;
  watch_face_reset_contact(&picker);launcher_swipe_pending=launcher_activity_pending=false;
  (void)pqa_input(&clock_quick.ui,now,false,0,0,0,0,false);
  clock_crash_modal=false;
  CLOCK_WAKE_MARK("crash-report-end");
 }
 return crash_session_checkpoint(&clock_crash);
}
#endif
