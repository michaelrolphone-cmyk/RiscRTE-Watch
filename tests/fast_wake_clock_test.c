/* Production Clock and renderer; fake device/API boundaries and explicit costs.
 * No host wall-clock measurement is reported as ESP32 hardware timing. */
#define WATCH_CONTEXTS_CLIENT
#define WATCH_RUNTIME_FEATURES
#define WATCH_BLE_BROADCAST
#define WATCH_PAIRED_BOOT_CONFIRM
#define WATCH_CLOCK_ALARMS
#define WATCH_CLOCK_POINTS
#define WATCH_MOTION_WAKE
#define WATCH_ALARM_SLEEP_RESUME
#define WATCH_QUICK_RADIOS
#ifndef PORTABLE_LOW_BATTERY
#define PORTABLE_LOW_BATTERY
#endif
#define main inherited_quick_fixture_main
#define WATCH_TEST_RUNTIME_EXTERN
#include "quick_clock_test.c"

#undef main

#ifdef WATCH_AUTOMATIC_PROVISION
static unsigned fixture_provision_mode,fixture_provision_scans,fixture_volume_acquires;
static const risc_storage_volume_api_v1 fixture_provision_volume={.api_version=1,.struct_size=sizeof(fixture_provision_volume)};
static int fp_refresh(void*p){(void)p;assert(clock_provision_busy);return 0;}
static int fp_step(void*p,const risc_storage_volume_api_v1*v){(void)p;assert(v==(fixture_provision_mode==2?&fixture_provision_volume:NULL)&&clock_provision_busy&&!clock_contexts_started&&!clock_broadcast_started);return ++fixture_provision_scans<5?INSTALLED_APPS_WORKING:INSTALLED_APPS_OK;}
static int fp_cancel(void*p){(void)p;return 0;}
static int fp_begin(void*p,const char*s,uint64_t t){(void)p;(void)s;(void)t;assert(false);return 0;}
static int fp_feed(void*p,uint64_t t){(void)p;(void)t;return 0;}
static int fp_install(void*p,uint32_t i,uint32_t g){(void)p;(void)i;(void)g;return 0;}
static int fp_descriptor(void*p,const char*s){(void)p;(void)s;return 0;}
static uint32_t fp_count(void*p){(void)p;return 0;}
static bool fp_get(void*p,uint32_t i,installed_app_v1*out){(void)p;(void)i;(void)out;return false;}
static const installed_apps_automatic_v1 fixture_provision_api={
 .base={.base={.base={.api_version=1,.struct_size=sizeof(fixture_provision_api),.refresh=fp_refresh,.step=fp_step,.cancel=fp_cancel},.provision_tag=INSTALLED_APPS_PROVISION_TAG,.provision_version=1,.install_descriptor=fp_descriptor},.feed_tag=INSTALLED_APPS_FEED_TAG,.feed_version=1,.refresh=fp_feed,.step=fp_cancel,.cancel=fp_cancel,.generation=fp_count,.count=fp_count,.get=fp_get,.install=fp_install},.automatic_tag=INSTALLED_APPS_AUTOMATIC_TAG,.automatic_version=1,.begin=fp_begin,.step=fp_step,.cancel=fp_cancel};
#else
#define fixture_provision_mode 0
#endif

static bool initial_default_entry;
static bool fast_initial_default(void){return initial_default_entry;}
static uint32_t boot_cause,checkpoint_result,retains,confirmations,promotions;
static uint32_t service_acquires,first_at,service_input_polls,last_service_polls;
static uint32_t first_frames,soft_frames,wake_began,soft_latency,input_at,input_stages;
static uint32_t fake_service_cost,transfer_cost,fail_service,stage_calls;
static uint32_t wake_count,requested_wakes,alarm_frames,seed_calls;
static bool ended,early_launch,first_seen,fail_display,fail_release;
static bool unreadable_rtc;
static bool fail_context_pause,confirmed_early,unset_time;
static unsigned touch_fault_kind,touch_fault_seen;
static bool touch_poll_failed;
static bool retained_fence,swipe_during_frame,swipe_latched,low_on_wake;
static bool broadcast_live,contexts_live,contexts_enabled,alarm_on_wake;
static bool fail_broadcast_restart,fail_ble_enable,early_sleep;
static bool expect_context_handoff,queued_context_handoff;
static uint32_t broadcast_starts,contexts_starts,broadcast_at,contexts_at;
static uint32_t resume_input_at,radio_at,frame_at,radio_apply_count;
static uint32_t motion_prepares,motion_resumes,motion_sleeps,alarm_resumes,points_loads;
static uint32_t service_start_cost,broadcast_frame;
static char wake_stages[128][32];
static uint32_t wake_stage_times[128],wake_stage_count;
static uint32_t display_load_cost,sleep_setting_cost,sleep_setting_reads;
static uint32_t alarm_load_cost,alarm_acquires,rtc_acquires;
static bool crown_before_alarm,fail_late_rtc_read,late_rtc_retention,native_context_failed;
static int32_t native_read_result;
static unsigned native_reads;
static int32_t light_wake_drift;
static unsigned rtc_refusal_wake,rtc_recovery_wake,rtc_reconcile_errors,rtc_read_calls;
static unsigned transient_rtc_failures,rtc_read_at[16];
static bool wait_rtc_recovery,first_time_valid,first_time_warning;
static bool live_rtc_update,live_rtc_applied,live_rtc_presented;
static twatch_rtc_time_v1 live_rtc_value,live_rtc_expected;
static uint32_t live_rtc_applied_at;
static int64_t live_rtc_epoch;
static uint32_t prealarm_crown_events;
static const char *fail_capability;
static unsigned key_trace_count,key_acquire_cost,startup_events;
static bool repeated_hold,trace_manual,trace_discard,trace_long;
static unsigned sleep_fault,failure_reports;
static char failure_stage[80];
static bool fail_frame_acquire,fail_unblank,fail_marker_sample,fail_next_health;
static unsigned fail_sleep_acquire,refused_cycles,refused_restores,clean_refusal_frames,sleep_acquire_errors;
static bool sleep_failure_armed;
static bool alarm_touch_test;
static unsigned alarm_touch_phase,alarm_touch_wrong_y;
static void check_refusal_pixels(void){
 fixture_expected_face(false);
 if(sleep_status[0]){
  unsigned side=watch_home_side(TEST_WIDTH,TEST_HEIGHT),ui_height=TEST_HEIGHT*240u/side;
  unsigned x0=watch_home_left(TEST_WIDTH,TEST_HEIGHT)+10u*side/240u;
  unsigned x1=watch_home_left(TEST_WIDTH,TEST_HEIGHT)+230u*side/240u;
  unsigned y0=(ui_height-37u)*side/240u,y1=(ui_height-9u)*side/240u;
  for(unsigned y=0;y<TEST_HEIGHT;y++)for(unsigned x=0;x<TEST_WIDTH;x++)
   if(y<y0||y>=y1||x<x0||x>=x1)
    assert(completed_pixels[y*TEST_WIDTH+x]==expected_pixels[y*TEST_WIDTH+x]);
  assert(memcmp(completed_pixels,expected_pixels,sizeof(expected_pixels)));
  fixture_expected_face(true);
 }
 assert(!memcmp(completed_pixels,expected_pixels,sizeof(expected_pixels)));
}
static unsigned stage_count(const char *name) {
 unsigned found=0;for(unsigned i=0;i<wake_stage_count;i++)found+=!strcmp(name,wake_stages[i]);return found;
}
static unsigned stage_index(const char *name) {
 for(unsigned i=0;i<wake_stage_count;i++)if(!strcmp(name,wake_stages[i]))return i;
 assert(false);return 0;
}
static void check_io(void){assert(!retained_fence);}
static risc_realtime_snapshot_v1 native_time;
static alarm_status_v1 alarm_view;
static contexts_status_v1 contexts_view;
static int32_t native_read(void*c,risc_realtime_snapshot_v1*out){check_io();(void)c;++native_reads;if(native_context_failed)return RISC_REALTIME_CONTEXT;if(native_read_result)return native_read_result;*out=native_time;return 0;}
static int32_t native_seed(void*c,int64_t seconds,uint32_t ns){check_io();(void)c;assert(!ns);seed_calls++;native_time.epoch_seconds=seconds;native_time.validity=1;return 0;}
static int32_t wake_read(void*c,uint32_t type,uint32_t schema,risc_retained_wake_record_v1*out,uint32_t*cause){
 check_io();(void)c;*cause=boot_cause;*out=(risc_retained_wake_record_v1){.struct_size=sizeof(*out),.type=type,.schema_version=schema,.size=1,.payload={1}};return checkpoint_result;}
static int32_t wake_stage(void*c,const risc_retained_wake_record_v1*r){check_io();(void)c;assert(r->size==1);return 0;}
static int32_t wake_clear(void*c){check_io();(void)c;return 0;}
static int32_t native_promote(void*c){check_io();(void)c;promotions++;return 0;}
static const risc_realtime_control_api_v1 native_api={1,sizeof(native_api),NULL,native_read,native_seed};
static const risc_retained_wake_api_v1 wake_api={1,sizeof(wake_api),NULL,wake_read,wake_stage,wake_clear};
static const risc_provider_promotion_api_v1 promote_api={1,sizeof(promote_api),NULL,native_promote};
static bool fast_retain(void){retains++;retained_fence=true;return true;}
static bool fast_confirm(void){check_io();assert(!queued_context_handoff);confirmations++;if(service_acquires<4)confirmed_early=true;return true;}
static bool fast_health(risc_runtime_health_v1*h){check_io();
 if(touch_fault_kind==2&&clock_rendering){clock_ms+=8;} /* modeled raster work */
 h->uptime_ms=clock_ms;if(fail_next_health){fail_next_health=false;return false;}return !ended&&clock_ms<30000u;}
static bool fast_diagnostic(const char*s){
 if(!strncmp(s,"WATCH_KEY ",10)){
  assert(++key_trace_count<=24);
  if(!strcmp(s,"WATCH_KEY decision=manual-sleep"))trace_manual=true;
  if(!strcmp(s,"WATCH_KEY stage=startup-discard"))trace_discard=true;
  if(!strcmp(s,"WATCH_KEY decision=long-no-sleep"))trace_long=true;
 }
 if(!strncmp(s,"WATCH_CLOCK sleep=failed stage=",30)){
  assert(strlen(s)<sizeof(failure_stage));strcpy(failure_stage,s);failure_reports++;
 }
 if(!strcmp(s,"WATCH_CLOCK rtc-reconcile=unavailable retry-ms=5000"))rtc_reconcile_errors++;
 if(!strncmp(s,"WATCH_SLEEP error=",18)){
  const char *expected=fail_sleep_acquire==1?
   "WATCH_SLEEP error=settings-acquire capability=storage.key-value instance=1":
   "WATCH_SLEEP error=motion-acquire capability=motion.accel instance=0";
  assert(fail_sleep_acquire&&!strcmp(s,expected));sleep_acquire_errors++;
 }
 if(!strncmp(s,"WATCH_CLOCK sleep=refused",25)){
  const char *expected=fail_sleep_acquire==1?"WATCH_CLOCK sleep=refused stage=SETTINGS detail=unavailable":
   "WATCH_CLOCK sleep=refused stage=MOTION detail=unavailable";
  assert(fail_sleep_acquire&&!strcmp(s,expected));
  assert(!strcmp(sleep_status,fail_sleep_acquire==1?"SLEEP ERR SETTINGS":"SLEEP ERR MOTION"));
  assert(!hardware_brightness&&!owned&&!pending&&!touch.subscription);
  refused_cycles++;
 }
 if(!strncmp(s,"WATCH_WAKE stage=",17)) {
  assert(wake_stage_count<128);unsigned now,elapsed;
  assert(sscanf(s,"WATCH_WAKE stage=%31s t_ms=%u elapsed_ms=%u",wake_stages[wake_stage_count],&now,&elapsed)==3);
  assert(now==clock_ms&&elapsed==clock_ms-clock_wake_started);
  wake_stage_times[wake_stage_count++]=now;
  if(fail_marker_sample&&!strcmp(wake_stages[wake_stage_count-1],"display-begin"))fail_next_health=true;
 }
 if(!strcmp(s,"WATCH_CLOCK ready crown=enabled")){ready=true;ready_time=clock_ms;}
 if(strstr(s,"stage=first-frame")){assert(!first_seen);first_seen=true;first_at=clock_ms;first_frames=submitted;
  first_time_valid=face.time_valid;first_time_warning=!strcmp(sleep_status,"TIME CHECK ERR");
  assert(hardware_brightness==clock_quick.brightness&&!pending&&!owned&&touch.subscription&&last_complete==submitted);assert(service_acquires==0);
  assert(points_loads==1&&face.points==&clock_points_view&&!sleep_setting_reads&&!alarm_acquires&&!alarm_frames);
  if(face.time_valid){twatch_rtc_time_v1 expected={0};
#ifdef WATCH_NATIVE_TIME_AUTHORITY
   uint16_t fraction;assert(watch_runtime_read(&native_clock,&expected,&fraction));
#else
   twatch_rtc_time_v1 raw={0};assert(watch_runtime_calendar(native_time.epoch_seconds,&raw));assert(watch_display_time(&raw,&expected));
#endif
   assert(!memcmp(&face.time,&expected,sizeof(expected)));}
  fixture_expected_face(true);assert(!memcmp(completed_pixels,expected_pixels,sizeof(expected_pixels)));
  if(early_launch)launcher_swipe_pending=true;
 }
 if(strstr(s,"stage=first-input-poll")){input_at=clock_ms;input_stages++;assert(touch.subscription&&polls);if(wake_count)resume_input_at=clock_ms;}
 if(strstr(s,"stage=light-resume")){wake_began=clock_ms;assert(!clock_quick.ui.radios_valid&&!clock_radios.valid&&!clock_radios.available);assert(!clock_broadcast_started&&!clock_contexts_started);}
 if(strstr(s,"stage=light-frame")){soft_frames++;soft_latency=clock_ms-wake_began;assert(!pending&&!owned&&hardware_brightness==clock_quick.brightness);
  assert(!sleep_status[0]);if(refused_restores)check_refusal_pixels();
 }
 if(strstr(s,"stage=radios-begin")){radio_at=clock_ms;assert(input_stages>0);if(wake_count)assert(resume_input_at>=wake_began);assert(frame_at>=(wake_count?wake_began:first_at));}
 if(strstr(s,"stage=broadcast-begin"))broadcast_at=clock_ms;
 if(strstr(s,"stage=contexts-begin"))contexts_at=clock_ms;
 if(strstr(s,"stage=services-ready")){
  stage_calls++;
  if(live_rtc_update){
   /* Model an already accepted RTC update and its Runtime seed. The RTC
    * setting application's write transaction is a separate qualification. */
   assert(!live_rtc_applied&&!owned&&!pending);
   int64_t epoch;assert(watch_runtime_epoch(&live_rtc_value,&epoch)&&epoch==live_rtc_epoch);
   assert(native_api.seed(native_api.context,epoch,0)==RISC_REALTIME_OK);
   live_rtc_applied=true;live_rtc_applied_at=clock_ms;
  }else if(wake_count+refused_cycles==requested_wakes&&!early_launch&&!wait_rtc_recovery&&(!fixture_provision_mode||stage_calls>=2))ended=true;
 }
 return true;
}
static bool fast_status(void*c,uint64_t token,risc_display_present_status_v1*out){
 check_io();(void)c;assert(token==submitted&&pending);
 if(swipe_during_frame&&first_seen&&submitted==2&&clock_ms-frame_started>=10&&!swipe_latched){launcher_swipe_pending=true;swipe_latched=true;swipe_time=clock_ms;}
 if(clock_ms-frame_started<transfer_cost){out->state=RISC_DISPLAY_PRESENT_ACTIVE;return true;}
 pending=false;out->state=fail_display?RISC_DISPLAY_PRESENT_FAILED:RISC_DISPLAY_PRESENT_COMPLETE;
 if(!fail_display){memcpy(completed_pixels,pixels,sizeof(pixels));last_complete=submitted;frame_at=clock_ms;if(clock_alarm_modal)alarm_frames++;
  if(live_rtc_applied&&face.time_valid&&!memcmp(&face.time,&live_rtc_expected,sizeof(face.time))){
   fixture_expected_face(true);assert(!memcmp(completed_pixels,expected_pixels,sizeof(expected_pixels)));
   assert(hardware_brightness==clock_quick.brightness&&hardware_brightness>0);
   assert(native_time.epoch_seconds==live_rtc_epoch);
   assert(clock_ms-live_rtc_applied_at<=500);live_rtc_presented=true;ended=true;
  }
  if(wait_rtc_recovery&&first_seen&&!clock_rtc_reconcile_pending&&face.time_valid&&!sleep_status[0])ended=true;
  if(refused_restores&&hardware_brightness){
   check_refusal_pixels();clean_refusal_frames++;
  }
 }return true;
}
static bool fast_frame_acquire(void*c,uint32_t format,risc_display_surface_v1*out) {
 check_io();if(sleep_fault==2 && ready && !hardware_brightness)return false;return !fail_frame_acquire&&acquire_frame(c,format,out);
}
static bool fast_brightness(void*c,uint16_t level,uint16_t maximum) {
 if(sleep_fault==1 && ready && !level)return false;
 if(level&&refused_cycles>refused_restores){
  assert(!owned&&!pending&&last_complete==submitted&&!hardware_brightness);
  assert(sleep_status[0]);check_refusal_pixels();refused_restores++;
 }
 check_io();return !(level&&fail_unblank)&&brightness(c,level,maximum);
}
static bool fast_key(void*c,uint32_t*out){check_io();(void)c;*out=0;
 if(startup_events){*out=startup_events;startup_events=0;return true;}
 if(repeated_hold&&ready&&clock_ms-ready_time>300){*out=1;return true;}
 if(crown_before_alarm&&ready&&!alarm_acquires){*out=2;crown_before_alarm=false;prealarm_crown_events++;}
 else if(clock_alarm_modal&&alarm_view.occurrence.generation&&alarm_frames&&!alarm_touch_test)*out=1;
 else if(first_seen&&(clock_deferred_phase==4||(early_sleep&&clock_deferred_phase==1))&&wake_count+refused_cycles<requested_wakes&&clock_ms-ready_time>300){*out=2;sleep_failure_armed=fail_sleep_acquire!=0&&refused_cycles<2;}
 return true;}
static int32_t fast_sleep(void*c,risc_light_sleep_result_v1*out){check_io();(void)c;(void)out;assert(!touch.subscription&&!ble_state&&!pending&&!owned);assert(!broadcast_live&&!contexts_live);sleeps++;wake_count++;clock_ms+=500;native_time.epoch_seconds+=light_wake_drift;if(wake_count==rtc_refusal_wake)fail_late_rtc_read=true;if(wake_count==rtc_recovery_wake)fail_late_rtc_read=false;if(low_on_wake){low_percent=5;clock_low_battery_sampled=false;}if(alarm_on_wake){alarm_view.state=ALARM_STATE_ALERT;alarm_view.occurrence.generation=1;}return RISC_LIGHT_SLEEP_OK;}
static int32_t fast_sleep_timed(void*c,uint32_t ms,risc_light_sleep_result_v1*out){(void)ms;return fast_sleep(c,out);}
static bool fast_rtc(void*c,twatch_rtc_time_v1*out){check_io();assert(rtc_read_calls<16);rtc_read_at[rtc_read_calls++]=clock_ms;if(transient_rtc_failures){transient_rtc_failures--;return false;}if(fail_late_rtc_read){native_context_failed=late_rtc_retention;return false;}if(unreadable_rtc)return false;if(unset_time){memset(out,0,sizeof(*out));return true;}if(live_rtc_applied){*out=live_rtc_value;return true;}return rtc_read(c,out);}
static int32_t alarm_status(void*c,alarm_status_v1*out){check_io();(void)c;*out=alarm_view;return 0;}
static int32_t alarm_step(void*c){check_io();(void)c;return 0;}
static int32_t alarm_refresh(void*c){check_io();(void)c;return 0;}
static int32_t alarm_ack(void*c,const alarm_token_v1*token){check_io();(void)c;assert(token->generation);if(alarm_touch_test)assert(alarm_touch_phase==4);alarm_view.state=ALARM_STATE_READY;alarm_view.occurrence.generation=0;return 0;}
static int32_t alarm_prepare(void*c,alarm_sleep_v1*out){check_io();(void)c;*out=(alarm_sleep_v1){.struct_size=sizeof(*out)};return 0;}
static int32_t alarm_resume(void*c,const alarm_sleep_v1*plan){check_io();(void)c;assert(plan->struct_size==sizeof(*plan));alarm_resumes++;return ALARM_OK;}
static const alarm_service_sleep_v1 alarm_api={{1,sizeof(alarm_api),NULL,alarm_status,alarm_step,alarm_refresh,alarm_ack,alarm_prepare,alarm_refresh},alarm_resume};
static bool motion_prepare(void*c){check_io();(void)c;motion_prepares++;return true;}
static bool motion_resume(void*c){check_io();(void)c;motion_resumes++;return true;}
static bool motion_pending(void*c,bool*out){check_io();(void)c;*out=false;return true;}
static bool motion_info(void*c,twatch_tap_info_v1*out){check_io();(void)c;*out=(twatch_tap_info_v1){sizeof(*out),TWATCH_TAP_BMA423,0,7,3};return true;}
static bool motion_configure(void*c,uint16_t value){check_io();(void)c;assert(value==3);return true;}
static bool motion_observe(void*c,twatch_tap_observation_v1*out){check_io();(void)c;(void)out;assert(false);return false;}
static const twatch_motion_api_v1 motion_api={.api_version=1,.struct_size=sizeof(motion_api),.prepare_wake=motion_prepare,.wake_pending=motion_pending,.resume_wake=motion_resume,.tap_info=motion_info,.tap_configure=motion_configure,.tap_observe_begin=motion_configure,.tap_observe=motion_observe};
static int32_t motion_sleep(void*c,uint32_t ms,risc_light_sleep_result_v1*out){check_io();motion_sleeps++;assert(motion_prepares==motion_sleeps);return fast_sleep_timed(c,ms,out);}
static int32_t motion_deep(void*c,uint32_t ms){check_io();(void)c;(void)ms;assert(false);return RISC_DEEP_SLEEP_UNSUPPORTED;}
static bool context_pause(void*c){check_io();(void)c;assert(!held&&!pending);contexts_live=false;return !fail_context_pause;}
static bool context_step(void*c,const contexts_policy_v1*p){check_io();(void)c;assert(!held&&!pending&&!clock_radios_pending&&clock_contexts_started);
 assert(!queued_context_handoff);
 if(!contexts_live){
#ifdef WATCH_CRASH_REPORT
  if(clock_deferred_phase==4){assert(clock_crash.attempted&&clock_crash.hooks.files_enabled);contexts_live=true;}
  else
#endif
  {assert(clock_deferred_phase==2&&clock_ms>=contexts_at&&frame_at<=contexts_at&&submitted>broadcast_frame);contexts_live=true;contexts_starts++;clock_ms+=service_start_cost;}
 }
 assert(!p->enabled||contexts_enabled);if(low_percent<10)assert(!p->audio_allowed&&!p->radio_allowed);
 return true;}
static bool context_status(void*c,contexts_status_v1*out){check_io();(void)c;*out=contexts_view;return true;}
static bool context_request(void*c,uint32_t sources){check_io();(void)c;(void)sources;return true;}
static bool context_begin(void*c,uint32_t source){check_io();(void)c;contexts_view.export_active=source;return true;}
static bool context_export(void*c,uint32_t s,uint32_t k,uint32_t i,const void*b,uint32_t n){check_io();(void)c;(void)s;(void)k;(void)i;(void)b;(void)n;return true;}
static bool context_finish(void*c,uint32_t source,uint32_t result){check_io();(void)c;(void)source;(void)result;return true;}
static int32_t context_label(void*c,uint32_t s,uint32_t i,contexts_label_v1*out){check_io();(void)c;(void)s;(void)i;(void)out;return 0;}
static bool context_claim(void*c,uint32_t s,uint32_t i,const char*n,uint32_t g){check_io();(void)c;(void)s;(void)i;(void)n;(void)g;return false;}
static bool context_result(void*c,uint32_t s,uint32_t g,uint32_t r){check_io();(void)c;(void)s;(void)g;(void)r;return true;}
static bool context_capture(void*c){check_io();(void)c;return true;}
static const contexts_service_v1 context_api={.api_version=1,.struct_size=sizeof(context_api),.step=context_step,.pause=context_pause,.status=context_status,.request_export=context_request,.begin_export=context_begin,.export_record=context_export,.finish_export=context_finish,.label=context_label,.claim_preset=context_claim,.preset_result=context_result,.capture_audio=context_capture};
static bool broadcast_step(void*c,bool allowed,const telemetry_broadcast_policy_v1*p){check_io();(void)c;assert(allowed&&!clock_radios_pending&&clock_broadcast_started);
 assert(!queued_context_handoff);
 if(!broadcast_live){
#ifdef WATCH_CRASH_REPORT
  if(clock_deferred_phase==4){assert(clock_crash.attempted&&clock_crash.hooks.files_enabled);broadcast_live=true;}
  else
#endif
  {assert(clock_deferred_phase==1&&clock_ms>=broadcast_at&&frame_at<=broadcast_at);broadcast_frame=submitted;broadcast_live=true;broadcast_starts++;clock_ms+=service_start_cost;}
 }
 assert(!p->radios_allowed||(!clock_quick.ui.airplane&&clock_quick.ui.bluetooth_enabled));
 return !(fail_broadcast_restart&&wake_count);}
static bool broadcast_pause(void*c){check_io();(void)c;broadcast_live=false;return true;}
static bool broadcast_status(void*c,telemetry_broadcast_status_v1*out){(void)c;memset(out,0,sizeof(*out));return true;}
static int32_t broadcast_enumerate(void*c,uint32_t index,risc_telemetry_field_v1*out){(void)c;(void)index;(void)out;return false;}
static int32_t broadcast_read(void*c,uint32_t id,int32_t*out){(void)c;(void)id;(void)out;return 0;}
static const telemetry_broadcast_v1 broadcast_api={.api_version=1,.struct_size=sizeof(broadcast_api),.step=broadcast_step,.pause=broadcast_pause,.status=broadcast_status,.enumerate=broadcast_enumerate,.read=broadcast_read};
static const portable_bluetooth_control_v1 fast_ble;
static const risc_key_value_v1 fast_kv;
static bool fast_acquire(const char*n,uint32_t version,uint64_t instance,risc_runtime_capability_v1*g){
 check_io();
#ifdef WATCH_AUTOMATIC_PROVISION
 if(!strcmp(n,INSTALLED_APPS_CAPABILITY)){assert(first_seen&&clock_deferred_phase==4);if(!fixture_provision_mode)return false;g->api=&fixture_provision_api;++grants;return true;}
 if(fixture_provision_mode&&!strcmp(n,"storage.volume")){assert(instance==0);++fixture_volume_acquires;if(fixture_provision_mode!=2)return false;g->api=&fixture_provision_volume;++grants;return true;}
#endif
#ifdef WATCH_CRASH_REPORT
 // This lifecycle fixture has no persistent crash spool. Model optional
 // AppData absence instead of handing its nonzero namespace to device mocks.
 if(!strcmp(n,RISC_APP_DATA_CAPABILITY)){assert(version==1&&instance==WATCH_CRASH_REPORT_NAMESPACE);return false;}
#endif
 if(sleep_failure_armed&&((fail_sleep_acquire==1&&!strcmp(n,RISC_KEY_VALUE_CAPABILITY)&&instance==1)||
    (fail_sleep_acquire==2&&!strcmp(n,TWATCH_MOTION_CAPABILITY)&&instance==0))){
  assert(!hardware_brightness&&!touch.subscription&&!pending&&!owned&&!broadcast_live&&!contexts_live);
  for(unsigned pixel=0;pixel<TEST_WIDTH*TEST_HEIGHT;pixel++)assert(!completed_pixels[pixel]);
  sleep_failure_armed=false;return false;
 }
 if(!strcmp(n,"input.power-key"))clock_ms+=key_acquire_cost;
 if(fail_capability&&!strcmp(n,fail_capability))return false;
 if(!strcmp(n,"display.output"))clock_ms+=display_load_cost;
 if(!strcmp(n,"rtc.clock"))rtc_acquires++;
 if(!strcmp(n,ALARM_SERVICE_CAPABILITY)){
  assert(first_seen&&input_stages&&!pending&&!owned&&!service_acquires);
  alarm_acquires++;clock_ms+=alarm_load_cost;
 }
 if(!strcmp(n,"net.wifi")||!strcmp(n,"bluetooth.hci")||!strcmp(n,TELEMETRY_BROADCAST_CAPABILITY)||!strcmp(n,CONTEXTS_SERVICE_CAPABILITY)){
  assert(first_seen&&!pending&&!owned);service_acquires++;service_input_polls=polls;
  if(clock_radios_pending)assert(radio_at&&input_stages);
  /* Two radio grants are one synchronous policy stage; later stages require
   * another foreground input opportunity and another completed clock frame. */
  if(service_acquires==3||service_acquires==4)assert(polls>last_service_polls);
  last_service_polls=polls;clock_ms+=fake_service_cost;if(fail_service==service_acquires)return false;
 }
 const void*p=NULL;
 if(!strcmp(n,RISC_REALTIME_CONTROL_CAPABILITY))p=&native_api;
 else if(!strcmp(n,RISC_RETAINED_WAKE_CAPABILITY))p=&wake_api;
 else if(!strcmp(n,RISC_PROVIDER_PROMOTION_CAPABILITY))p=&promote_api;
 else if(!strcmp(n,ALARM_SERVICE_CAPABILITY))p=&alarm_api;
 else if(!strcmp(n,TELEMETRY_BROADCAST_CAPABILITY))p=&broadcast_api;
 else if(!strcmp(n,CONTEXTS_SERVICE_CAPABILITY))p=&context_api;
 else if(!strcmp(n,TWATCH_MOTION_CAPABILITY)){assert(instance==0);p=&motion_api;}
 else if(!strcmp(n,RISC_KEY_VALUE_CAPABILITY)&&instance==5){points_loads++;g->api=&fast_kv;grants++;return true;}
 if(p){assert(version==1&&(!instance||instance==0));g->api=p;grants++;return true;}
 bool acquired=acquire_cap(n,version,instance,g);
 if(acquired&&g->api==&qa_ble)g->api=&fast_ble;
 if(acquired&&g->api==&face_kv)g->api=&fast_kv;
 return acquired;
}
static bool fast_release(risc_runtime_capability_v1*g){check_io();if(fail_release&&g->api==&context_api){fail_release=false;return false;}return release_cap(g);}
static bool fast_launch(const char*path){check_io();assert(!held&&!pending);
 if(expect_context_handoff){assert(!strcmp(path,"waterfall.elf")&&!queued_context_handoff);queued_context_handoff=true;}
 else{assert(!strcmp(path,"springboard.elf")&&!touch.subscription);ended=true;}
 launches++;return true;}
static unsigned alarm_touch_y(void) {
 return alarm_touch_phase<=2?alarm_touch_wrong_y:(unsigned)clock_viewport.layout.safe.y+watch_home_alarm_dismiss_at(clock_viewport.layout.safe.width,clock_viewport.layout.safe.height);
}
static bool fast_touch_poll(void*c,size_t n) {
 check_io();touch_poll_failed=false;
 if(first_seen && !touch_fault_seen && (touch_fault_kind==6 || touch_fault_kind==7)) {
  touch_poll_failed=true;return false;
 }
 return poll(c,n);
}
static uint64_t fast_touch_subscribe(void*c){check_io();return subscribe(c);}
static bool fast_touch_unsubscribe(void*c,uint64_t id) {
 check_io();if(touch_fault_kind==4&&!touch_fault_seen){touch_fault_seen++;return false;}
 return unsubscribe(c,id);
}
static bool fast_snapshot(void*c,risc_touch_snapshot_v1*out){check_io();(void)c;
 assert(!touch_poll_failed);
 if(!touch_fault_seen && (((touch_fault_kind==3||touch_fault_kind>=8)&&first_seen) || (touch_fault_kind==5&&wake_count))) {
  touch_fault_seen++;return false;
 }
 *out=(risc_touch_snapshot_v1){.width=TEST_WIDTH,.height=TEST_HEIGHT};
 if(alarm_touch_test){
  out->sequence=alarm_touch_phase;
  if(alarm_touch_phase&1u){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){1,0,TEST_WIDTH/2u,alarm_touch_y()};}
 }
 return true;
}
static int32_t fast_touch_next(void*c,uint64_t id,risc_touch_event_v1*out){
 check_io();
 if(touch_fault_seen && (touch_fault_kind==3 || touch_fault_kind==5))return -2;
 if(touch_fault_seen==1 && touch_fault_kind>=8){
  touch_fault_seen++;return touch_fault_kind==8?0:touch_fault_kind==9?-1:1;
 }
 if(!touch_fault_seen && ((touch_fault_kind==1&&first_seen&&!clock_rendering) ||
       (touch_fault_kind==2&&clock_rendering) || (touch_fault_kind==7&&touch_poll_failed))) {
  touch_fault_seen++;return -2;
 }
 if(touch_fault_kind==6&&touch_poll_failed){touch_fault_seen++;return -1;}
 if(!alarm_touch_test)return next(c,id,out);
 if(!clock_alarm_modal||!alarm_frames||alarm_touch_phase>=4)return 0;
 ++alarm_touch_phase;
 *out=(risc_touch_event_v1){.sequence=alarm_touch_phase,.kind=(alarm_touch_phase&1u)?RISC_TOUCH_EVENT_DOWN:RISC_TOUCH_EVENT_UP,.id=1,.x=TEST_WIDTH/2u,.y=alarm_touch_y()};
 return 1;
}
static bool fast_ble_enable(void*c,bool enabled){check_io();radio_apply_count++;if(fail_ble_enable)return false;return ble_enable(c,enabled);}
static bool fast_ble_status(void*c,uint8_t*out){check_io();return ble_status(c,out);}
static const portable_bluetooth_control_v1 fast_ble={.api_version=1,.struct_size=sizeof(fast_ble),.set_enabled=fast_ble_enable,.status=fast_ble_status};
static int32_t fast_get(void*c,const char*k,void*b,uint32_t n,uint32_t*z){check_io();
#ifdef WATCH_TEST_TIMEZONE_GET
 if(!strcmp(k,"time_zone"))return WATCH_TEST_TIMEZONE_GET(c,k,b,n,z);
#endif
 if(!strcmp(k,PORTABLE_SLEEP_KEY)){sleep_setting_reads++;clock_ms+=sleep_setting_cost;}
 if(!strcmp(k,PORTABLE_CONTEXT_ENABLED_KEY)&&contexts_enabled){assert(n>=4);memcpy(b,(uint8_t[]){0x43,1,1,1^0xa5},4);*z=4;return RISC_KEY_VALUE_OK;}
 return face_get(c,k,b,n,z);}
static const risc_key_value_v1 fast_kv={1,sizeof(fast_kv),NULL,fast_get,face_put};
static unsigned launch_failure_kind,launch_failure_reads;
static bool fast_launch_failure(risc_runtime_launch_failure_v1 *out){
 assert(out && out->struct_size==sizeof(*out));++launch_failure_reads;
 if(!launch_failure_kind)return false;
 out->flags=launch_failure_kind==1?RISC_LAUNCH_FAILURE_EXTERNAL_IMAGE:0;
 strcpy(out->application,"optional-app.elf");strcpy(out->detail,"media absent or invalid");
 launch_failure_kind=0;return true;
}
static const risc_runtime_api_v1 fast_runtime={.api_version=1,.struct_size=RISC_RUNTIME_SERVICE_CHECKPOINT_V1_SIZE,.health=fast_health,.yield_ms=yield,.diagnostic=fast_diagnostic,.request_launch=fast_launch,.acquire=fast_acquire,.release=fast_release,.confirm_boot=fast_confirm,.retain_invocation=fast_retain,.take_launch_failure=fast_launch_failure,.initial_default=fast_initial_default};
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t version){assert(version==1);return &fast_runtime;}
static void reset_case(void){
 touch_fault_kind=touch_fault_seen=0;touch_poll_failed=false;
 fixture_safe_insets=(risc_display_insets_v1){0};
#ifdef WATCH_AUTOMATIC_PROVISION
 fixture_provision_mode=fixture_provision_scans=fixture_volume_acquires=0;
#endif
#ifdef WATCH_ATOMIC_PREFERENCES
 clock_atomic_retained=false;clock_quick_scope=false;quick_atomic_running=false;
 quick_atomic_grant=(risc_runtime_capability_v1){0};quick_atomic_edit=(portable_preference_atomic){0};quick_atomic_discard();
 home_automatic_grant=(risc_runtime_capability_v1){0};home_automatic=(portable_preference_automatic){0};
 clock_face_grant=(risc_runtime_capability_v1){0};clock_face_edit=(portable_preference_atomic){0};atomic_token=0;
#endif
 initial_default_entry=false;alarm_touch_test=false;alarm_touch_phase=alarm_touch_wrong_y=0;
 live_rtc_update=live_rtc_applied=live_rtc_presented=false;live_rtc_applied_at=0;
 launch_failure_kind=launch_failure_reads=0;
 key_trace_count=key_acquire_cost=startup_events=0;repeated_hold=trace_manual=trace_discard=trace_long=false;
 (void)runtime;
 assert(!allocations);scenario=30;clock_ms=submitted=released=grants=ungrants=subscribed=unsubscribed=polls=launches=sleeps=touch_step=0;
 ready=owned=pending=launched=false;last_complete=swipe_submitted=swipe_time=0;hardware_brightness=40;
 boot_cause=RISC_BOOT_RESET;checkpoint_result=RISC_RETAINED_WAKE_ABSENT;retains=confirmations=promotions=0;
 service_acquires=first_at=service_input_polls=last_service_polls=first_frames=soft_frames=wake_began=soft_latency=input_at=input_stages=0;
 fake_service_cost=700;transfer_cost=95;fail_service=stage_calls=0;wake_count=requested_wakes=alarm_frames=seed_calls=0;
 ended=early_launch=first_seen=fail_display=fail_release=fail_context_pause=confirmed_early=unset_time=false;unreadable_rtc=false;
 retained_fence=swipe_during_frame=swipe_latched=low_on_wake=false;
 broadcast_live=contexts_live=contexts_enabled=alarm_on_wake=false;
 fail_broadcast_restart=fail_ble_enable=early_sleep=false;
 expect_context_handoff=queued_context_handoff=false;
 broadcast_starts=contexts_starts=broadcast_at=contexts_at=0;
 resume_input_at=radio_at=frame_at=radio_apply_count=0;
 motion_prepares=motion_resumes=motion_sleeps=alarm_resumes=points_loads=broadcast_frame=0;service_start_cost=400;
 wake_stage_count=display_load_cost=sleep_setting_cost=sleep_setting_reads=0;
 alarm_load_cost=alarm_acquires=rtc_acquires=0;
 native_read_result=RISC_REALTIME_OK;native_reads=0;
 crown_before_alarm=fail_late_rtc_read=late_rtc_retention=native_context_failed=false;prealarm_crown_events=0;
 light_wake_drift=3600;rtc_refusal_wake=rtc_recovery_wake=rtc_reconcile_errors=rtc_read_calls=0;
 transient_rtc_failures=0;memset(rtc_read_at,0,sizeof(rtc_read_at));wait_rtc_recovery=first_time_valid=first_time_warning=false;
 fail_capability=NULL;fail_frame_acquire=fail_unblank=fail_marker_sample=fail_next_health=false;
 fail_sleep_acquire=refused_cycles=refused_restores=clean_refusal_frames=sleep_acquire_errors=0;sleep_failure_armed=false;
 low_percent=55;low_flags=0;low_read_fail=false;memset(low_lengths,0,sizeof(low_lengths));memset(low_writes,0,sizeof(low_writes));
 saved_face=radio_saved=false;ble_state=0;face_writes=face_reads=0;memset(quick_saved,0,sizeof(quick_saved));
 da.base.present_status=fast_status;pa.key_events=fast_key;pa.light_sleep=fast_sleep;pa.light_sleep_for=fast_sleep_timed;ra.read=fast_rtc;
 da.base.acquire=fast_frame_acquire;da.base.set_brightness=fast_brightness;
 pa.light_sleep_set=motion_sleep;pa.deep_sleep_set=motion_deep;ta.snapshot=fast_snapshot;ta.next=fast_touch_next;ta.poll=fast_touch_poll;ta.subscribe=fast_touch_subscribe;ta.unsubscribe=fast_touch_unsubscribe;
 native_time=(risc_realtime_snapshot_v1){.struct_size=sizeof(native_time),.validity=RISC_REALTIME_VALID,.epoch_seconds=1791074400};
 alarm_view=(alarm_status_v1){.api_version=1,.struct_size=sizeof(alarm_view),.state=ALARM_STATE_READY};
 contexts_view=(contexts_status_v1){.struct_size=sizeof(contexts_view)};
 contexts_view.audio.model_state=contexts_view.radio.model_state=CONTEXTS_MODEL_READY;
}
#ifdef WATCH_AUTOMATIC_PROVISION
static void check_provision_utc(bool accepted,uint64_t expected,bool context_failed) {
 uint64_t utc=UINT64_MAX;
 watch_clock_runtime runtime_before;memcpy(&runtime_before,&native_clock,sizeof(runtime_before));
 unsigned char face_before[sizeof(face)],time_before[sizeof(native_time)];
 memcpy(face_before,&face,sizeof(face));memcpy(time_before,&native_time,sizeof(native_time));
 bool pending_before=clock_rtc_reconcile_pending,delayed_before=clock_rtc_retry_delayed;
 uint32_t retry_before=clock_rtc_retry_sampled_at;
 unsigned rtc_before=rtc_read_calls,seeds_before=seed_calls,writes_before=face_writes;
 unsigned retains_before=retains,reads_before=native_reads;
 bool reads=!native_clock.halted&&native_clock.time;
 assert(clock_provision_utc(NULL,&utc)==accepted);
 assert(utc==(accepted?expected:UINT64_MAX));
 assert(native_reads==reads_before+(unsigned)reads);
 assert(!memcmp(face_before,&face,sizeof(face))&&!memcmp(time_before,&native_time,sizeof(native_time)));
 assert(rtc_read_calls==rtc_before&&seed_calls==seeds_before&&face_writes==writes_before);
 assert(clock_rtc_reconcile_pending==pending_before&&clock_rtc_retry_delayed==delayed_before&&clock_rtc_retry_sampled_at==retry_before);
 if(context_failed)runtime_before.halted=true; /* Preserve the existing custody fence. */
 assert(!memcmp(&runtime_before,&native_clock,sizeof(native_clock)));
 assert(retains==retains_before+(unsigned)context_failed);
}
static void check_provision_utc_snapshots(void) {
 reset_case();
 native_clock=(watch_clock_runtime){.runtime=&fast_runtime,.time=&native_api,.time_live=true,.fallback=&ra};
 clock_rtc_reconcile_pending=clock_rtc_retry_delayed=true;clock_rtc_retry_sampled_at=1234;
 risc_realtime_snapshot_v1 valid=native_time;
 risc_realtime_snapshot_v1 unset={.struct_size=sizeof(unset),.validity=RISC_REALTIME_UNSET,
  .monotonic_before_us=123,.monotonic_after_us=456};
 for(unsigned pending=0;pending<2;pending++){
  native_clock.rtc_reconcile_pending=(bool)pending;
  native_time=unset;check_provision_utc(true,0,false);
  native_time=valid;check_provision_utc(!pending,(uint64_t)valid.epoch_seconds,false);
 }
 native_clock.rtc_reconcile_pending=false;
 native_time=valid;native_time.nanoseconds=999999000;
 check_provision_utc(true,(uint64_t)valid.epoch_seconds,false);
 native_time.epoch_seconds=INT32_MAX;check_provision_utc(true,INT32_MAX,false);
 /* A valid Unix epoch must never be mistaken for the private UNSET token. */
 native_time=valid;native_time.epoch_seconds=0;check_provision_utc(false,0,false);
 /* Old invalid/unreadable RTC calendars are not native UNSET evidence. */
 for(unsigned pending=0;pending<2;pending++){
  native_clock.rtc_reconcile_pending=(bool)pending;
  unset_time=unreadable_rtc=true;native_time=valid;
  check_provision_utc(!pending,(uint64_t)valid.epoch_seconds,false);
 }
 native_clock.rtc_reconcile_pending=true;native_time=unset;
 static const int32_t failures[]={RISC_REALTIME_INVALID,RISC_REALTIME_IO,1,-99};
 for(unsigned i=0;i<sizeof(failures)/sizeof(*failures);i++){
  native_read_result=failures[i];check_provision_utc(false,0,false);
 }
 native_read_result=RISC_REALTIME_OK;
 /* Both sample kinds must pass the complete native contract before admission. */
 for(unsigned known=0;known<2;known++)for(unsigned malformed=0;malformed<11;malformed++){
  native_clock.rtc_reconcile_pending=false;native_time=known?valid:unset;
  switch(malformed){
   case 0:native_time.struct_size=0;break;
   case 1:native_time.struct_size--;break;
   case 2:native_time.struct_size++;break;
   case 3:native_time.reserved=1;break;
   case 4:native_time.nanoseconds=1000000000u;break;
   case 5:native_time.nanoseconds=1;break;
   case 6:native_time.monotonic_before_us=native_time.monotonic_after_us+1;break;
   case 7:native_time.validity=RISC_REALTIME_VALID+1;break;
   case 8:native_time.validity=UINT32_MAX;break;
   case 9:native_time.epoch_seconds=-1;break;
   case 10:native_time.epoch_seconds=(int64_t)INT32_MAX+1;break;
  }
  check_provision_utc(false,0,false);
 }
 native_time=unset;native_time.epoch_seconds=1;check_provision_utc(false,0,false);
 native_time=unset;native_time.nanoseconds=1000;check_provision_utc(false,0,false);
 native_time=unset;native_clock.time=NULL;check_provision_utc(false,0,false);
 native_clock.time=&native_api;native_context_failed=true;
 check_provision_utc(false,0,true);
 check_provision_utc(false,0,false); /* No further read after CONTEXT retention. */
 assert(retained_fence);
 puts("Home provisioning UTC: strict UNSET with pending reconciliation, valid-clock fencing, zero-token separation, malformed/error/context refusal, and no RTC/native/face/fallback/reconciliation writes PASS");
}
#endif
static void check_clean(void){assert(!retained_fence&&!held&&!pending&&!owned&&!allocations);assert(grants==ungrants);assert(subscribed==unsubscribed);assert(!confirmed_early);assert(motion_prepares==wake_count&&motion_resumes==wake_count&&motion_sleeps==wake_count&&alarm_resumes==wake_count);}
static void retained_reset(void){assert(retained_fence&&!allocations);grants=ungrants;subscribed=unsubscribed;owned=pending=false;held=0;}
static void saved_radio(uint8_t flags){radio_saved=true;memcpy(radio_bytes,(uint8_t[]){0x51,1,flags,(uint8_t)(flags^0xa5)},4);}
static void check_boot_stages(void) {
 static const char *const expected[]={"entry","runtime-grants-begin","runtime-grants-end",
  "display-begin","display-end","power-services-begin","power-services-end","rtc-begin","rtc-end",
  "settings-begin","settings-end","points-begin","points-end",
  "touch-begin","touch-end","render-begin","render-end",
  "transfer-begin","transfer-end","unblank-begin","unblank-end","first-frame",
  "sleep-settings-begin","sleep-settings-end","first-input-poll",
  "alarm-begin","alarm-end","reconcile-begin","reconcile-end"};
 assert(wake_stage_count>=sizeof(expected)/sizeof(*expected));
 for(unsigned i=0,j=0;i<sizeof(expected)/sizeof(*expected);i++,j++) {
  if(!strcmp(wake_stages[j],"rtc-deferred"))j++;
  assert(j<wake_stage_count&&!strcmp(wake_stages[j],expected[i]));
 }
 assert(stage_count("first-frame")==1&&stage_count("entry")==1);
}
static char diagnostic_line[80];
static bool capture_diagnostic(const char *line){assert(strlen(line)<sizeof(diagnostic_line));strcpy(diagnostic_line,line);return false;}
static void check_refusal_diagnostics(void){
 static const risc_runtime_api_v1 capture={.diagnostic=capture_diagnostic};
 static const char *const stages[]={"UNKNOWN","SENSOR","PANEL","PMU","ALARM","LIGHT","DEEP","ACQUIRE","API","SETTINGS","MOTION","RUNTIME","SERVICE","UNKNOWN"};
 static const int32_t details[]={0,-1,17,INT32_MIN,INT32_MAX};
 const risc_runtime_api_v1 *saved=rt;rt=&capture;
 memcpy(expected_pixels,pixels,sizeof(pixels));
 for(unsigned stage=0;stage<14;stage++)for(unsigned detail=0;detail<5;detail++){
  watch_sleep_stage=stage;watch_sleep_detail=details[detail];clock_sleep_refused();
  bool has_detail=stage<9||stage>11;
  char expected[80];
  if(has_detail)snprintf(expected,sizeof(expected),"WATCH_CLOCK sleep=refused stage=%s detail=%ld",stages[stage],(long)details[detail]);
  else snprintf(expected,sizeof(expected),"WATCH_CLOCK sleep=refused stage=%s detail=unavailable",stages[stage]);
  assert(!strcmp(diagnostic_line,expected)&&!memcmp(pixels,expected_pixels,sizeof(pixels)));
  if(has_detail)snprintf(expected,sizeof(expected),"SLEEP ERR %s %ld",stages[stage],(long)details[detail]);
  else snprintf(expected,sizeof(expected),"SLEEP ERR %s",stages[stage]);
  assert(!strcmp(sleep_status,expected));
 }
 watch_sleep_stage=0;watch_sleep_detail=0;sleep_status[0]=0;rt=saved;
}
#ifdef WATCH_ATOMIC_PREFERENCES
/* Five copied low-battery mutations each yield once before terminal collect. */
#define LOW_POLICY_WAIT_MS 5u
#else
#define LOW_POLICY_WAIT_MS 0u
#endif
static int regression_main(void){
 /* A transient external RTC read failure must recover in the same Home
  * invocation, whether native time began UNSET or valid/provisional. */
 for(unsigned native_valid=0;native_valid<2;native_valid++) {
  reset_case();transient_rtc_failures=2;wait_rtc_recovery=true;
  native_time.validity=native_valid?RISC_REALTIME_VALID:RISC_REALTIME_UNSET;
  if(!native_valid)native_time.epoch_seconds=0;
  app_main();check_clean();assert(first_at==95&&input_at==95&&first_time_warning);
  assert(first_time_valid==(bool)native_valid&&rtc_read_calls==3&&rtc_reconcile_errors==1);
  assert(rtc_read_at[1]>=first_at&&rtc_read_at[2]-rtc_read_at[1]>=CLOCK_RTC_RETRY_MS);
  assert(rtc_read_at[2]-rtc_read_at[1]<CLOCK_RTC_RETRY_MS+100);
  assert(seed_calls==1&&face.time_valid&&!sleep_status[0]&&!clock_rtc_reconcile_pending);
  assert(confirmations==1&&polls>100&&submitted>8);
 }

 /* Cold RTC failures also require fresh native custody before any settings,
  * touch, display or optional service admission continues. */
 for(unsigned native_valid=0;native_valid<2;native_valid++) {
  reset_case();fail_late_rtc_read=late_rtc_retention=true;
  native_time.validity=native_valid?RISC_REALTIME_VALID:RISC_REALTIME_UNSET;
  if(!native_valid)native_time.epoch_seconds=0;
  app_main();assert(retained_fence&&retains==1&&!first_seen&&!seed_calls&&!confirmations&&!service_acquires&&!points_loads);retained_reset();
 }
 /* Inject five-minute errors in both directions at every deep-wake cause.
  * The first frame/input stays fast; settled display must follow read-only RTC. */
 for(unsigned cause=RISC_BOOT_DEEP_TIMER;cause<=RISC_BOOT_DEEP_OTHER;cause++)for(int sign=-1;sign<=1;sign+=2) {
  reset_case();boot_cause=cause;twatch_rtc_time_v1 raw,expected;int64_t hardware_epoch;
  assert(rtc_read(NULL,&raw)&&watch_runtime_epoch(&raw,&hardware_epoch));
  native_time.epoch_seconds=hardware_epoch+sign*300;
  app_main();check_clean();assert(first_at==95&&input_at==95&&seed_calls==1&&rtc_acquires==1);
  assert(native_time.epoch_seconds==hardware_epoch&&watch_display_time(&raw,&expected));
  assert(face.time_valid&&!memcmp(&face.time,&expected,sizeof(expected)));
 }
 for(unsigned failure=0;failure<2;failure++) {
  reset_case();boot_cause=RISC_BOOT_DEEP_GPIO;
  if(failure)fail_late_rtc_read=true;else fail_capability="rtc.clock";
  int64_t retained_epoch=native_time.epoch_seconds;app_main();check_clean();
  assert(first_at==95&&input_at==95&&confirmations==1&&service_acquires==4&&!seed_calls);
  assert(face.time_valid&&native_time.epoch_seconds==retained_epoch&&!strcmp(sleep_status,"TIME CHECK ERR"));
  assert(rtc_reconcile_errors==1&&hardware_brightness==clock_quick.brightness&&clock_rtc_reconcile_pending);
 }
 /* Repeated light wakes correct drift in either direction after the fast
  * resumed frame/input. An ordinary RTC refusal preserves valid native time;
  * a later successful wake must clear the visible warning and reconcile. */
 for(int sign=-1;sign<=1;sign+=2) {
  reset_case();requested_wakes=2;light_wake_drift=sign*300;
  twatch_rtc_time_v1 raw,expected;int64_t hardware_epoch;
  assert(rtc_read(NULL,&raw)&&watch_runtime_epoch(&raw,&hardware_epoch));
  app_main();check_clean();
  assert(wake_count==2&&soft_frames==2&&soft_latency==95&&input_stages==3);
  assert(seed_calls==3&&rtc_read_calls==3&&rtc_acquires==1&&!rtc_reconcile_errors);
  assert(native_time.epoch_seconds==hardware_epoch&&watch_display_time(&raw,&expected));
  assert(face.time_valid&&!memcmp(&face.time,&expected,sizeof(expected)));
 }
 for(unsigned recover=0;recover<2;recover++) {
  reset_case();rtc_refusal_wake=1;rtc_recovery_wake=recover?2:0;requested_wakes=recover?2:1;
  twatch_rtc_time_v1 raw;int64_t hardware_epoch;
  assert(rtc_read(NULL,&raw)&&watch_runtime_epoch(&raw,&hardware_epoch));
  app_main();check_clean();assert(wake_count==requested_wakes&&rtc_reconcile_errors==1);
  assert(soft_latency==95&&face.time_valid&&hardware_brightness==clock_quick.brightness);
  assert(rtc_read_calls==requested_wakes+1&&seed_calls==1+recover);
  assert(native_time.epoch_seconds==hardware_epoch+(recover?0:light_wake_drift));
  assert(recover?!sleep_status[0]:!strcmp(sleep_status,"TIME CHECK ERR"));
 }
 reset_case();boot_cause=RISC_BOOT_DEEP_GPIO;fail_late_rtc_read=late_rtc_retention=true;
 app_main();assert(retained_fence&&retains==1&&!seed_calls&&!confirmations&&!service_acquires);retained_reset();
 for(unsigned cause=0;cause<=RISC_BOOT_DEEP_OTHER;cause++)for(unsigned present=0;present<2;present++){
  reset_case();boot_cause=cause;checkpoint_result=present?RISC_RETAINED_WAKE_OK:RISC_RETAINED_WAKE_ABSENT;
  app_main();check_clean();assert(first_seen&&first_at==95&&first_frames==1&&input_at==95&&input_stages==1);assert(service_acquires==4&&confirmations==1&&promotions==1&&stage_calls==1);
  assert(seed_calls==1);assert(face.time_valid);
  assert(rtc_acquires==1&&alarm_acquires==1);
  check_boot_stages();
 }
 reset_case();display_load_cost=275;sleep_setting_cost=900;app_main();check_clean();check_boot_stages();
 assert(first_at==370&&input_at==1270&&wake_stage_times[stage_index("display-end")]-wake_stage_times[stage_index("display-begin")]==275);
 assert(wake_stage_times[stage_index("first-frame")]==370&&wake_stage_times[stage_index("sleep-settings-end")]==1270);
 assert(wake_stage_times[stage_index("first-frame")]==wake_stage_times[stage_index("unblank-end")]);
 reset_case();boot_cause=RISC_BOOT_DEEP_GPIO;alarm_load_cost=700;app_main();check_clean();check_boot_stages();
 assert(first_at==95&&input_at==95&&rtc_acquires==1&&alarm_acquires==1&&radio_at==890);
 assert(wake_stage_times[stage_index("alarm-end")]-wake_stage_times[stage_index("alarm-begin")]==700);
 reset_case();fail_marker_sample=true;app_main();check_clean();
 assert(first_seen&&first_at==95&&confirmations==1&&stage_count("display-begin")==1&&!stage_count("display-end"));
 static const char *const failed_caps[]={RISC_REALTIME_CONTROL_CAPABILITY,"display.output","input.touch.raw"};
 static const char *const failed_stages[]={"runtime-grants","display","touch"};
 for(unsigned i=0;i<sizeof(failed_caps)/sizeof(*failed_caps);i++) {
  reset_case();fail_capability=failed_caps[i];app_main();check_clean();
  char begin[32],end[32];snprintf(begin,sizeof(begin),"%s-begin",failed_stages[i]);snprintf(end,sizeof(end),"%s-end",failed_stages[i]);
  assert(stage_count(begin)==1&&!stage_count(end)&&!service_acquires&&!confirmations);
  assert(first_seen==!strcmp(failed_caps[i],ALARM_SERVICE_CAPABILITY));
 }
 reset_case();fail_capability=RISC_RETAINED_WAKE_CAPABILITY;app_main();check_clean();assert(first_seen&&confirmations==1&&!native_clock.resume);
 reset_case();fail_capability=ALARM_SERVICE_CAPABILITY;app_main();check_clean();
 assert(first_seen&&confirmations==1&&stage_count("alarm-begin")==1&&stage_count("alarm-end")==1&&!alarm_frames);
 reset_case();unreadable_rtc=true;native_time.validity=RISC_REALTIME_UNSET;native_time.epoch_seconds=0;
 app_main();check_clean();assert(first_seen&&!face.time_valid&&!seed_calls&&confirmations==1);
 reset_case();unreadable_rtc=true;app_main();check_clean();assert(first_seen&&face.time_valid&&confirmations==1&&rtc_reconcile_errors==1&&first_time_warning);
 reset_case();fail_capability="board.battery";app_main();check_clean();assert(first_seen&&!face.battery_valid&&confirmations==1);
 reset_case();fail_frame_acquire=true;app_main();check_clean();
 assert(stage_count("render-begin")==1&&!stage_count("render-end")&&!stage_count("transfer-begin")&&!first_seen);
 reset_case();fail_unblank=true;app_main();check_clean();
 assert(stage_count("transfer-end")==1&&stage_count("unblank-begin")==1&&!stage_count("unblank-end")&&!first_seen&&!hardware_brightness);
 reset_case();early_launch=true;app_main();check_clean();assert(first_seen&&launches==1&&!service_acquires&&!confirmations);
 reset_case();swipe_during_frame=true;app_main();check_clean();assert(swipe_latched&&launches==1&&!service_acquires&&!confirmations&&clock_ms==190);
 reset_case();scenario=17;saved_face=true;memcpy(saved_blob,(uint8_t[]){0x46,1,1,1^0xa5},4);quick_saved[0]=true;quick_values[0]=60;app_main();check_clean();assert(first_seen&&picker.selected==1&&face.hour_24&&hardware_brightness==60);
 reset_case();unset_time=true;native_time.validity=RISC_REALTIME_UNSET;native_time.epoch_seconds=0;app_main();check_clean();assert(first_seen&&!face.time_valid&&!seed_calls);
 reset_case();alarm_view.state=ALARM_STATE_ALERT;alarm_view.occurrence.generation=1;app_main();check_clean();assert(alarm_frames&&first_seen&&first_at==95&&service_acquires==4);
 reset_case();boot_cause=RISC_BOOT_DEEP_GPIO;alarm_load_cost=700;alarm_view.state=ALARM_STATE_ALERT;alarm_view.occurrence.generation=1;
 app_main();check_clean();assert(first_at==95&&input_at==95&&alarm_frames&&rtc_acquires==1&&alarm_acquires==1&&!alarm_view.occurrence.generation);
 assert(stage_index("first-input-poll")<stage_index("alarm-begin")&&stage_index("reconcile-end")<stage_index("radios-begin"));
 reset_case();boot_cause=RISC_BOOT_DEEP_GPIO;alarm_load_cost=700;crown_before_alarm=true;
 alarm_view.state=ALARM_STATE_ALERT;alarm_view.occurrence.generation=1;
 app_main();check_clean();assert(first_at==95&&input_at==95&&prealarm_crown_events==1&&alarm_frames&&!sleeps&&confirmations==1);
 reset_case();boot_cause=RISC_BOOT_DEEP_GPIO;unset_time=true;native_time.validity=RISC_REALTIME_UNSET;native_time.epoch_seconds=0;
 app_main();check_clean();assert(first_seen&&!face.time_valid&&rtc_acquires==1&&!stage_count("rtc-deferred"));
 reset_case();fail_display=true;app_main();check_clean();assert(!first_seen&&!service_acquires&&!confirmations);
 assert(stage_count("transfer-begin")==1&&!stage_count("transfer-end")&&!stage_count("unblank-begin"));
 for(unsigned fail=3;fail<=4;fail++){reset_case();fail_service=fail;app_main();check_clean();assert(first_seen&&service_acquires==4&&confirmations==1);}
 reset_case();contexts_enabled=true;requested_wakes=2;app_main();check_clean();assert(seed_calls==3&&wake_count==2&&soft_frames==2&&soft_latency==95&&stage_calls==3&&input_stages==3&&broadcast_starts==3&&contexts_starts==3);
 assert(stage_count("first-frame")==1&&stage_count("render-begin")==3&&stage_count("transfer-end")==3&&stage_count("unblank-end")==3);
 /* Settings acquisition refusal must preserve the selected face outside its
  * explicit error strip, finish the redraw before unblank, survive retries,
  * and clear the error completely after the next successful sleep/wake. */
 for(unsigned failure=1;failure<=1;failure++)for(unsigned selected=0;selected<WATCH_FACE_COUNT;selected++){
  reset_case();fail_sleep_acquire=failure;requested_wakes=3;contexts_enabled=true;
  saved_face=true;memcpy(saved_blob,(uint8_t[]){0x46,1,(uint8_t)selected,(uint8_t)(selected^0xa5)},4);
  app_main();check_clean();assert(picker.selected==selected&&first_at==95);
  assert(refused_cycles==2&&refused_restores==2&&sleep_acquire_errors==2&&clean_refusal_frames>=2);
  assert(sleeps==1&&wake_count==1&&soft_frames==1&&soft_latency==95&&!sleep_status[0]);
  assert(stage_calls==4&&input_stages==4&&broadcast_starts==4&&contexts_starts==4);
 }
 check_refusal_diagnostics();
 reset_case();contexts_enabled=true;early_sleep=true;requested_wakes=1;app_main();check_clean();assert(wake_count==1&&soft_frames==1&&stage_calls==1&&broadcast_starts==1&&contexts_starts==1&&confirmations==1);
 reset_case();low_percent=5;contexts_enabled=true;app_main();check_clean();assert(first_at==95&&input_at==95&&radio_at==190+LOW_POLICY_WAIT_MS&&clock_quick.brightness==15&&hardware_brightness==15&&!clock_radios.flags&&low_writes[0]==1);
 reset_case();low_on_wake=true;contexts_enabled=true;requested_wakes=1;app_main();check_clean();assert(wake_count==1&&soft_latency==95&&resume_input_at==wake_began+95&&radio_at==resume_input_at+95+LOW_POLICY_WAIT_MS&&clock_quick.brightness==15&&!clock_radios.flags&&low_writes[0]==1);
 reset_case();alarm_on_wake=true;requested_wakes=1;app_main();check_clean();assert(wake_count==1&&alarm_frames&&soft_latency>95&&soft_frames==1);
 for(unsigned variant=0;variant<4;variant++){
  reset_case();contexts_enabled=true;
  const uint8_t modes[]={PORTABLE_RADIO_WIFI,PORTABLE_RADIO_BLUETOOTH,PORTABLE_RADIO_AIRPLANE|PORTABLE_RADIO_PREVIOUS_BLUETOOTH,0};
  saved_radio(modes[variant]);if(variant==3){radio_bytes[3]^=1;ble_state=1;}
  app_main();check_clean();assert(confirmations==1&&first_at==95&&input_at==95);
  assert(ble_state==(variant==1));assert(clock_quick.ui.radios_valid==(variant!=3));assert(!clock_quick.ui.radios_valid||clock_radios.flags==modes[variant]);
 }
 for(unsigned fail=1;fail<=2;fail++){reset_case();fail_service=fail;app_main();check_clean();assert(first_seen&&!clock_quick.ui.radios_valid&&confirmations==1);}
 reset_case();saved_radio(PORTABLE_RADIO_BLUETOOTH);fail_ble_enable=true;app_main();assert(first_seen&&retained_fence&&retains&&!confirmations&&native_clock.halted&&radio_apply_count==1);retained_reset();
 reset_case();requested_wakes=1;fail_ble_enable=true;app_main();assert(first_seen&&retained_fence&&retains&&!wake_count&&confirmations==1&&native_clock.halted&&radio_apply_count==1);retained_reset();
 reset_case();contexts_enabled=true;requested_wakes=1;fail_broadcast_restart=true;app_main();assert(first_seen&&retained_fence&&retains&&wake_count==1&&confirmations==1&&clock_contexts.api);retained_reset();
 reset_case();contexts_enabled=expect_context_handoff=true;contexts_view.export_pending=CONTEXTS_RADIO;app_main();check_clean();assert(queued_context_handoff&&launches==1&&!confirmations&&clock_contexts_handoff);
 reset_case();fail_context_pause=true;app_main();assert(first_seen&&retains&&!confirmations&&clock_contexts_uncertain);
 /* Terminal retained fixture reset is outside that invocation. */
 retained_reset();
 reset_case();fail_release=true;app_main();assert(first_seen&&retains&&confirmations==1);retained_reset();
 puts("RTC reconciliation: deep/light wake +/-5-minute drift, late acquire/read refusal continuity, bounded active/next-wake retry, cold UNSET and normal-return VALID recovery, and fresh native custody fencing PASS");
 puts("Fast Watch Clock: production Points/motion/alarm-resume feature paths; 10 cold/deep variants; exact first frame/input; mid-frame navigation; cold/light low-battery; phased service starts; radio policy/failure variants; early/repeated light wake; due alarms; terminal retain fences PASS");
 puts("Modeled cost only: 95 ms first Clock frame and input, four 700 ms service acquisitions plus 400 ms service starts deferred; 95 ms light-resume frame without an alarm; no physical timing claim.");
 puts("Boot/wake stage timing: ordered essential grants, settings, alarm/touch/reconciliation, render/transfer/unblank; first-frame precedes sleep settings; failure markers stop at failed boundary; light logs bounded per wake PASS");
 puts("Minimal first-frame closure: valid deep native time defers RTC then reconciles after first input; alarm/output admission follows first frame/input and precedes handoff/radios; 700 ms modeled load deferred; deep due alarms and stale crown cancellation PASS");
 puts("Sleep refusal: settings acquisition, every face unchanged outside explicit error strip, completed redraw before unblank, repeated retry/cleanup, success clears error, and exact stage/signed-detail diagnostics PASS");
 return 0;
}

static void live_rtc_update_cases(void){
 /* Fixed external-RTC UTC+08 input and independently specified Denver output:
  * forward/backward adjustment, both DST boundaries, leap-day/year rollover.
  * This checks the real Home render/present loop after the shared native clock
  * changes. The 500ms bound is modeled fixture time, never device latency. */
 static const struct {twatch_rtc_time_v1 rtc,expected;int64_t epoch;} cases[]={
  {{.year=2026,.month=10,.day=4,.hour=0,.minute=5,.second=40,.weekday=0},
   {.year=2026,.month=10,.day=3,.hour=10,.minute=5,.second=40,.weekday=6},INT64_C(1791043540)},
  {{.year=2026,.month=10,.day=3,.hour=23,.minute=55,.second=40,.weekday=6},
   {.year=2026,.month=10,.day=3,.hour=9,.minute=55,.second=40,.weekday=6},INT64_C(1791042940)},
  {{.year=2026,.month=3,.day=8,.hour=16,.minute=59,.second=59,.weekday=0},
   {.year=2026,.month=3,.day=8,.hour=1,.minute=59,.second=59,.weekday=0},INT64_C(1772960399)},
  {{.year=2026,.month=3,.day=8,.hour=17,.weekday=0},
   {.year=2026,.month=3,.day=8,.hour=3,.weekday=0},INT64_C(1772960400)},
  {{.year=2026,.month=11,.day=1,.hour=15,.minute=59,.second=59,.weekday=0},
   {.year=2026,.month=11,.day=1,.hour=1,.minute=59,.second=59,.weekday=0},INT64_C(1793519999)},
  {{.year=2026,.month=11,.day=1,.hour=16,.weekday=0},
   {.year=2026,.month=11,.day=1,.hour=1,.weekday=0},INT64_C(1793520000)},
  {{.year=2028,.month=2,.day=29,.hour=8,.weekday=2},
   {.year=2028,.month=2,.day=28,.hour=17,.weekday=1},INT64_C(1835395200)},
  {{.year=2027,.month=1,.day=1,.hour=8,.weekday=5},
   {.year=2026,.month=12,.day=31,.hour=17,.weekday=4},INT64_C(1798761600)},
 };
 unsigned count=0;
 for(unsigned selected=0;selected<WATCH_FACE_COUNT;selected++)
  for(unsigned i=0;i<sizeof(cases)/sizeof(*cases);i++){
   reset_case();live_rtc_update=true;live_rtc_value=cases[i].rtc;live_rtc_expected=cases[i].expected;live_rtc_epoch=cases[i].epoch;
   saved_face=true;memcpy(saved_blob,(uint8_t[]){0x46,1,(uint8_t)selected,(uint8_t)(selected^0xa5)},4);
   app_main();check_clean();
   assert(picker.selected==selected&&live_rtc_applied&&live_rtc_presented&&seed_calls==2&&!sleeps);
   assert(face.time_valid&&!memcmp(&face.time,&live_rtc_expected,sizeof(face.time)));count++;
  }
 printf("Live RTC/native update: %u completed-face cases, all faces, forward/backward, DST/leap/year boundaries, modeled <=500ms, clean custody PASS\n",count);
}

#ifndef WATCH_FAST_WAKE_TEST_ENTRY
#define WATCH_FAST_WAKE_TEST_ENTRY main
#endif
int WATCH_FAST_WAKE_TEST_ENTRY(void){
 live_rtc_update_cases();
 if(getenv("WATCH_TEST_RTC_UPDATES_ONLY"))return 0;
 for(unsigned cause=0;cause<=RISC_BOOT_DEEP_OTHER;cause++) {
  reset_case();initial_default_entry=true;boot_cause=cause;app_main();check_clean();
  if(cause<RISC_BOOT_DEEP_TIMER){assert(first_seen&&first_frames>10&&first_at>2000);}
  else {assert(first_seen&&first_frames==1&&first_at==95);}
 }
 puts("Cold first-default intro; deep wake and subsequent Home remain fast PASS");

 regression_main();
 for(unsigned boundary=0;boundary<2;boundary++) {
  reset_case();alarm_touch_test=true;
  alarm_touch_wrong_y=boundary?watch_home_alarm_retry_at(TEST_WIDTH,TEST_HEIGHT):watch_home_alarm_dismiss_at(TEST_WIDTH,TEST_HEIGHT)-1u;
  alarm_view.state=ALARM_STATE_ALERT;alarm_view.occurrence.generation=1;
  app_main();check_clean();assert(alarm_touch_phase==4&&!alarm_view.occurrence.generation&&alarm_frames);
 }
 puts("Alarm native touch: just-above-dismiss and retry-row taps stay inert; exact dismiss boundary acknowledges through raw event queue PASS");
 for(unsigned boundary=0;boundary<3;boundary++) {
  reset_case();alarm_touch_test=true;
  unsigned inset=TEST_WIDTH*15u/100u;
  fixture_safe_insets=(risc_display_insets_v1){inset,inset,inset,inset};
  unsigned w=TEST_WIDTH-2u*inset,h=TEST_HEIGHT-2u*inset;
  alarm_touch_wrong_y=boundary==0?inset+watch_home_alarm_dismiss_at(w,h)-1u:
      boundary==1?inset+watch_home_alarm_retry_at(w,h):TEST_HEIGHT-1u;
  alarm_view.state=ALARM_STATE_ALERT;alarm_view.occurrence.generation=1;
  app_main();check_clean();assert(alarm_touch_phase==4&&!alarm_view.occurrence.generation&&alarm_frames);
 }
 puts("Alarm provider insets: actual Home modal rejects above-dismiss/retry/gutter taps then accepts translated dismiss boundary PASS");

 for(unsigned kind=1;kind<=2;kind++){
  reset_case();launch_failure_kind=kind;app_main();check_clean();
  assert(first_seen&&launch_failure_reads==1&&!launch_failure_kind);
  assert(!strcmp(sleep_status,kind==1?"APP MEDIA UNAVAILABLE":"APP FAILED TO START"));
 }
 puts("Launch feedback: actual Home first-frame pixels include missing-media/general failure status; no extra grants; fresh invocation reads once PASS");
 for(unsigned failure=1;failure<=2;failure++){
  reset_case();sleep_fault=failure;failure_reports=0;requested_wakes=1;
  app_main();check_clean();assert(!sleeps&&failure_reports==1);
  assert(!strcmp(failure_stage,failure==1?"WATCH_CLOCK sleep=failed stage=BLANK-BRIGHTNESS":"WATCH_CLOCK sleep=failed stage=BLANK-ACQUIRE"));
 }
 sleep_fault=0;
 const char *caps[]={"input.power-key","power.sleep","display.power"};
 const char *stages[]={"KEY-CAPABILITY","POWER-CAPABILITY","PANEL-CAPABILITY"};
 for(unsigned i=0;i<3;i++){
  reset_case();fail_capability=caps[i];failure_reports=0;
  app_main();check_clean();assert(first_seen&&!sleeps&&failure_reports==1);
  assert(!strcmp(failure_stage+strlen("WATCH_CLOCK sleep=failed stage="),stages[i]));
 }
 reset_case();key_acquire_cost=700;requested_wakes=1;app_main();check_clean();assert(sleeps==1&&trace_manual&&first_at==795);
 /* A fresh default invocation after a child receives no child gesture twice.
  * Startup consumes stale events, while the later press still sleeps normally. */
 reset_case();startup_events=2;requested_wakes=1;app_main();check_clean();assert(trace_discard&&trace_manual&&sleeps==1);
 reset_case();startup_events=1;requested_wakes=1;app_main();check_clean();assert(trace_discard&&trace_manual&&sleeps==1);
 reset_case();repeated_hold=true;app_main();check_clean();assert(!sleeps&&trace_long&&key_trace_count<=24);
 unsigned unchanged_frames=submitted;for(unsigned i=0;i<100;i++)clock_key_trace("WATCH_KEY test=budget");assert(key_trace_count==24&&submitted==unchanged_frames);
 puts("Bounded key trace: delayed key acquisition, fresh Home after stale short/long gesture, held-input suppression, manual sleep,24-message cap and unchanged cleanup PASS");
 puts("Sleep diagnostics: real Home blank brightness/acquire failure injection, absent key/power/panel capability, exact stage and unchanged cleanup PASS");
#ifdef WATCH_AUTOMATIC_PROVISION
 check_provision_utc_snapshots();
 for(unsigned volume=1;volume<=2;++volume){
 reset_case();fixture_provision_mode=volume;saved_radio(0);app_main();check_clean();
 assert(fixture_provision_scans==5&&fixture_volume_acquires==1&&stage_calls==2&&broadcast_starts==2&&contexts_starts==2&&!clock_provision_busy);
 }
 puts("Actual Home provisioning: first-frame admission, absent/authorized optional SD scans via instance0, saved-radio fence, paused Contexts/broadcast, resumed service phases and clean teardown PASS");
#endif
 for(unsigned kind=1;kind<=10;kind++) {
  reset_case();touch_fault_kind=kind;
  if(kind==4||kind==5)requested_wakes=1;
  app_main();fprintf(stderr,"touch fault kind=%u seen=%u retained=%u\n",kind,touch_fault_seen,retains);assert(touch_fault_seen==(kind>=8?2u:1u));
  if(kind==6||kind>=8){check_clean();continue;}
  assert(retained_fence&&retains==1&&clock_touch_retained&&touch.terminal&&native_clock.halted);
  uint32_t ignored=0;risc_display_surface_v1 blocked={0};
  assert(!alive(&ignored)&&!frame(&blocked)&&!present()&&!pace_frame(0));
  assert(!clock_render_checkpoint(NULL));sample_launcher_touch(1000,true);
  assert(!launcher_touch_capture(&touch,1000).valid&&!launcher_touch_close(&touch));
  assert(!launcher_touch_open(&touch)&&!clock_sleep_failure("terminal-test"));
  clock_exit_checkpoint(__LINE__);assert(retains==1);
  retained_reset();
 }
 puts("Home touch custody: foreground/render next fault, snapshot/open/close faults, failed-poll fault, benign snapshot status 0/-1/event and recoverable GAP; zero callbacks after retention PASS");
 return 0;
}
