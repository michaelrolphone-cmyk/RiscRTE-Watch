/* Production Home, touch recognizer and renderer, real Files lease/parser.
 * Launch scheduling and peripherals are explicit modeled boundaries. */
extern void serial_probe_init(int,int,int);
extern int serial_probe_checkpoint(uint32_t);
extern unsigned serial_probe_requests(void);
extern uint32_t serial_probe_expired_at(void),serial_probe_disconnected_at(void);
extern int serial_probe_exit_safe(void),serial_probe_pending(void);
extern void serial_probe_path(uint32_t,int);
extern void serial_probe_late(uint32_t);
static const char *nav_case;
static bool nav_baseline,nav_done,nav_path,nav_none,nav_late,nav_arrived;
static bool nav_wifi,nav_export,nav_workflow,nav_message;
static unsigned nav_claims,nav_reports,nav_exports,nav_failed,nav_workflow_failed,nav_evaluations,nav_captures,nav_busy_evaluations,nav_busy_captures,nav_feedback_frames;
static contexts_fingerprint_service_v1 nav_context_api;
static telemetry_broadcast_v1 nav_broadcast_api;
static bool nav_pending,nav_undrained,nav_disconnect;
static bool nav_injected_wifi,nav_export_requested,nav_retry_path;
static unsigned nav_workflow_failed;
static uint32_t nav_at,nav_safe_at,nav_fresh_at;
static unsigned nav_checkpoints,nav_busy,nav_frames,nav_polls,nav_feedback,nav_launches,nav_busy_launches,nav_closes;
static bool nav_safe=true;
static bool nav_health(risc_runtime_health_v1*h){check_io();h->uptime_ms=clock_ms;return !nav_done&&clock_ms<70000;}
static int32_t nav_checkpoint(void){
 check_io();assert(first_seen&&!held&&!pending&&!owned&&last_complete==submitted);++nav_checkpoints;
 nav_safe=serial_probe_checkpoint(clock_ms);
 if(nav_late&&nav_arrived&&!nav_retry_path&&serial_probe_requests()==3){
  serial_probe_path(clock_ms,0);nav_retry_path=true;nav_safe=false;
 }
 if(!nav_safe)++nav_busy;
 else if(nav_busy&&!nav_safe_at)nav_safe_at=clock_ms;
 return nav_safe?RISC_RUNTIME_CHECKPOINT_OK:RISC_RUNTIME_CHECKPOINT_BUSY;
}
static bool nav_key(void*c,uint32_t*out){check_io();(void)c;*out=0;return true;}
static bool nav_snapshot(void*c,risc_touch_snapshot_v1*out){
 check_io();(void)c;*out=(risc_touch_snapshot_v1){.width=TEST_WIDTH,.height=TEST_HEIGHT};
 if(!ready||clock_ms-ready_time<6000)return true;
 if(!nav_at){nav_at=clock_ms;nav_polls=polls;serial_probe_path(clock_ms,nav_pending?1:nav_undrained?2:0);nav_path=true;}
 if(nav_export){
  if(!nav_export_requested||(nav_late&&nav_safe_at&&!nav_fresh_at&&clock_ms-nav_safe_at>=1200)){
   if(nav_export_requested){assert(contexts_view.radio.model_state==CONTEXTS_MODEL_FAILED);nav_fresh_at=clock_ms;}
   nav_export_requested=true;contexts_view.export_pending=CONTEXTS_RADIO;contexts_view.radio.model_state=CONTEXTS_MODEL_REQUESTED;
  }
  return true;
 }
 if(nav_workflow){
  clock_contexts.rules_available=true;
  if(nav_late&&nav_safe_at&&!nav_fresh_at&&clock_ms-nav_safe_at>=1200)nav_fresh_at=clock_ms;
  return true;
 }
 if(nav_wifi){
  if(!nav_injected_wifi||(!nav_fresh_at&&nav_safe_at&&clock_ms-nav_safe_at>=1200)){
   if(nav_injected_wifi)nav_fresh_at=clock_ms;
   nav_injected_wifi=true;clock_quick.ui.radio_controls=false;clock_radios.available=false;clock_quick.ui.pending|=PQA_WIFI;
  }
  return true;
 }
 uint32_t age=clock_ms-nav_at;
 bool down=true;unsigned x=age<120?120:60;
 if(nav_safe_at&&clock_ms-nav_safe_at>=600){down=false;if(clock_ms-nav_safe_at>=1200){if(!nav_fresh_at)nav_fresh_at=clock_ms;down=true;x=clock_ms-nav_fresh_at<120?120:60;}}
 if(down){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){1,0,x*TEST_WIDTH/240,TEST_HEIGHT/2};}
 return true;
}
static bool nav_unsubscribe(void*c,uint64_t h){if(nav_at&&!serial_probe_exit_safe())++nav_closes;return unsubscribe(c,h);}
static bool nav_launch(const char*path){
 check_io();assert(!held&&!owned&&!pending);++nav_launches;
 assert(!strcmp(path,nav_export?"waterfall.elf":nav_workflow?"timer.elf":nav_wifi?"wifi_settings.elf":"springboard.elf"));
 assert((nav_export||nav_workflow)||!touch.subscription);
 if(nav_late&&!nav_arrived){serial_probe_late(clock_ms);nav_arrived=true;}
 if(!serial_probe_exit_safe()){++nav_busy_launches;if(!nav_baseline)return false;}
 ++launches;nav_done=true;return true;
}
static bool nav_status(void*c,uint64_t token,risc_display_present_status_v1*out){
 bool ok=fast_status(c,token,out);
 if(out->state==RISC_DISPLAY_PRESENT_COMPLETE&&nav_at&&!serial_probe_exit_safe()){assert(hardware_brightness);++nav_frames;
  if(!strcmp(sleep_status,"SERVICE BUSY - RETRY")||!strcmp(sleep_status,"NAVIGATION UNAVAILABLE")){
   fixture_expected_face(false);assert(memcmp(completed_pixels,expected_pixels,sizeof(expected_pixels)));++nav_feedback_frames;
  }
 }
 if(nav_message&&nav_reports&&(nav_safe_at||nav_baseline)&&out->state==RISC_DISPLAY_PRESENT_COMPLETE)nav_done=true;
 return ok;
}
static bool nav_diagnostic(const char*s){
 if(strstr(s,"navigation=busy"))++nav_feedback;
 return fast_diagnostic(s);
}
static bool nav_broadcast_step(void*c,bool allowed,const telemetry_broadcast_policy_v1*p){
 if(nav_reports||nav_failed){assert(allowed&&!held&&!pending);broadcast_live=true;return true;}
 return broadcast_step(c,allowed,p);
}
static bool nav_context_step(void*c,const contexts_policy_v1*p){
 if(nav_reports||nav_failed){assert(!held&&!pending);contexts_live=true;return true;}
 return context_step(c,p);
}
static bool nav_context_capture(void*c){++nav_captures;if(nav_at&&!serial_probe_exit_safe())++nav_busy_captures;return context_capture(c);}
static bool nav_context_begin(void*c,uint32_t source){++nav_exports;return context_begin(c,source);}
static bool nav_context_finish(void*c,uint32_t source,uint32_t result){
 ++nav_failed;assert(result==CONTEXTS_EXPORT_UNSUPPORTED);
 contexts_view.export_pending=contexts_view.export_active=0;contexts_view.radio.model_state=CONTEXTS_MODEL_FAILED;
 return context_finish(c,source,result);
}
static bool nav_fingerprint(void*c,uint32_t operation,void*request){
 (void)c;
 if(operation==CONTEXTS_FP_STATUS){contexts_fingerprint_status_v1*q=request;q->room_slot=q->event_slot=-1;return true;}
 if(operation==CONTEXTS_FP_CONFIG)return true;
 if(operation==CONTEXTS_FP_RULES){contexts_rules_v1*q=request;q->available=true;return true;}
 if(operation==CONTEXTS_FP_EVALUATE){++nav_evaluations;if(nav_at&&!serial_probe_exit_safe())++nav_busy_evaluations;return true;}
 if(operation==CONTEXTS_FP_WORKFLOW){
  contexts_workflow_v1*q=request;
  if(q->claim){if(nav_at&&(!nav_claims||(nav_late&&nav_fresh_at&&nav_claims==1))){++nav_claims;q->available=true;q->step.kind=nav_message?CR_MESSAGE:CR_LAUNCH;strcpy(q->step.text,nav_message?"Context ready":"timer.elf");}}
  else {++nav_reports;if(q->result==CR_FAILED){assert(nav_late&&!nav_baseline&&nav_reports==1);++nav_workflow_failed;}else assert(q->result==CR_RAN);}
  return true;
 }
 assert(false);return false;
}
static bool nav_acquire(const char*n,uint32_t version,uint64_t instance,risc_runtime_capability_v1*g){
 if(!strcmp(n,RISC_SHARED_DATA_CAPABILITY))return false; /* Optional persisted model absent in this boundary fixture. */
 bool ok=fast_acquire(n,version,instance,g);
 if(ok&&!strcmp(n,CONTEXTS_SERVICE_CAPABILITY))g->api=&nav_context_api.base;
 if(ok&&!strcmp(n,TELEMETRY_BROADCAST_CAPABILITY))g->api=&nav_broadcast_api;
 return ok;
}
int main(int argc,char**argv){
 assert(argc==3);nav_case=argv[1];nav_baseline=!strcmp(argv[2],"baseline");
 nav_pending=!strcmp(nav_case,"swipe-pending");nav_undrained=!strcmp(nav_case,"swipe-undrained");nav_disconnect=!strcmp(nav_case,"swipe-disconnect");nav_none=strstr(nav_case,"no-session")!=NULL;
 nav_late=strstr(nav_case,"-late")!=NULL;nav_wifi=strstr(nav_case,"wifi-")==nav_case;
 nav_export=strstr(nav_case,"contexts-export")==nav_case;nav_workflow=strstr(nav_case,"contexts-workflow")==nav_case;nav_message=strstr(nav_case,"message")!=NULL;
 assert(nav_pending||nav_undrained||nav_disconnect||nav_none||nav_late||nav_wifi||nav_export||nav_workflow||!strcmp(nav_case,"swipe-active"));
 reset_case();probe_long=true;fixture_provision_mode=2;saved_radio(0);
 const uint8_t idle_record[]={0x54,1,0,0,0xa5};memcpy(low_bytes[1],idle_record,5);low_lengths[1]=5;
 serial_probe_init(!nav_none&&!nav_late,0,nav_disconnect);
 nav_broadcast_api=broadcast_api;nav_broadcast_api.step=nav_broadcast_step;
 nav_context_api.base=context_api;nav_context_api.base.step=nav_context_step;nav_context_api.base.capture_audio=nav_context_capture;
 nav_context_api.base.begin_export=nav_context_begin;nav_context_api.base.finish_export=nav_context_finish;
 if(nav_workflow){nav_context_api.base.struct_size=sizeof(nav_context_api);nav_context_api.fingerprint_abi=0x31504643u;nav_context_api.fingerprint=nav_fingerprint;}
 fast_runtime.acquire=nav_acquire;
 fast_runtime.struct_size=sizeof(fast_runtime);fast_runtime.health=nav_health;fast_runtime.diagnostic=nav_diagnostic;
 fast_runtime.service_checkpoint=nav_checkpoint;fast_runtime.request_launch=nav_launch;
 pa.key_events=nav_key;ta.snapshot=nav_snapshot;ta.unsubscribe=nav_unsubscribe;da.base.present_status=nav_status;
 app_main();check_clean();assert(first_seen&&ready&&!retains&&hardware_brightness>0&&grants==ungrants&&subscribed==unsubscribed&&!allocations);
 assert(nav_at&&nav_path&&nav_done&&launches==(nav_message?0u:1u)&&!sleeps);
 if(nav_message){assert(nav_claims==1&&nav_reports==1);if(!nav_baseline)assert(nav_evaluations>10&&nav_busy_evaluations>5&&nav_busy_captures>100&&nav_safe_at);}
 else if(nav_export||nav_workflow){
  if(nav_late&&!nav_baseline){
   assert(nav_exports==(nav_export?2u:0u)&&nav_claims==(nav_workflow?2u:0u)&&nav_reports==(nav_workflow?2u:0u));
   assert(nav_failed==(nav_export?1u:0u)&&nav_workflow_failed==(nav_workflow?1u:0u));
   assert(nav_launches==2&&nav_busy_launches==1&&nav_safe_at&&nav_fresh_at&&nav_retry_path&&serial_probe_requests()==4&&nav_frames>20&&nav_busy_captures>100);
  }else {
  assert(nav_exports==(nav_export?1u:0u)&&nav_claims==(nav_workflow?1u:0u)&&nav_reports==(nav_workflow?1u:0u)&&!nav_failed);
  if(!nav_none){if(nav_baseline)assert(nav_busy_launches==1&&!nav_safe_at);else assert(!nav_busy_launches&&nav_safe_at&&nav_frames>20&&nav_busy_captures>100);}
  }
 }
 else
 if(nav_none){assert(!nav_busy_launches&&!nav_busy&&!nav_closes&&!nav_feedback);}
 else if(nav_baseline){assert(nav_busy_launches==1&&nav_closes==(nav_late?0u:1u)&&!nav_feedback&&!nav_fresh_at);}
 else {assert(nav_busy_launches==(nav_late?1u:0u)&&!nav_closes&&nav_launches==(nav_late?2u:1u)&&nav_feedback==(nav_late?0u:1u)&&nav_feedback_frames>0&&nav_fresh_at&&nav_safe_at);assert(nav_frames>20&&polls>nav_polls+100&&nav_checkpoints>20&&!launcher_swipe_pending);}
 printf("HOME_SERIAL_NAV %ux%u variant=%s case=%s requests=%u checkpoints=%u busy=%u visible_busy_frames=%u input_polls=%u busy_closes=%u launch_calls=%u busy_launches=%u launches=%u feedback=%u gesture_ms=%u safe_ms=%u fresh_gesture_ms=%u expiry_ms=%u disconnect_ms=%u workflow_claims=%u workflow_reports=%u exports=%u context_failures=%u workflow_failures=%u evaluations=%u busy_evaluations=%u busy_capture=%u feedback_frames=%u PASS\n",TEST_WIDTH,TEST_HEIGHT,argv[2],nav_case,serial_probe_requests(),nav_checkpoints,nav_busy,nav_frames,polls,nav_closes,nav_launches,nav_busy_launches,launches,nav_feedback,nav_at,nav_safe_at,nav_fresh_at,serial_probe_expired_at(),serial_probe_disconnected_at(),nav_claims,nav_reports,nav_exports,nav_failed,nav_workflow_failed,nav_evaluations,nav_busy_evaluations,nav_busy_captures,nav_feedback_frames);
 return 0;
}
