static risc_resident_callbacks_v1 full_callbacks;
static bool full_controls,full_triggered,full_short;
static unsigned full_key_event,full_deadlines;
static unsigned full_runs,full_alarms_before,full_acknowledged_at;
static int32_t full_register(uint64_t token,const risc_resident_callbacks_v1*c){assert(token==9);full_callbacks=*c;return RISC_RESIDENT_OK;}
static int32_t full_exit(uint64_t token){assert(token==9);return RISC_RESIDENT_OK;}
static bool full_health(risc_runtime_health_v1*h){check_io();h->uptime_ms=clock_ms;return !ended&&clock_ms<150000;}
static bool full_diagnostic(const char*s){(void)s;return true;}
static int32_t full_ack(void*c,const alarm_token_v1*t){clock_ms+=70000;full_acknowledged_at=clock_ms;return alarm_ack(c,t);}
static alarm_service_sleep_v1 full_alarm;
static int32_t full_prepare_alarm(void*c,alarm_sleep_v1*out){full_deadlines++;return alarm_prepare(c,out);}
static int32_t full_prepare_sleep(uint64_t*token){assert(!touch.subscription&&!held&&!pending&&!owned);*token=99;return RISC_RUNTIME_SLEEP_OK;}
static int32_t full_resume_sleep(uint64_t token){assert(token==99&&!held&&!pending&&!owned);return RISC_RUNTIME_SLEEP_OK;}
static bool full_key(void*c,uint32_t*out){if(full_key_event){*out=full_key_event;full_key_event=0;return true;}return fast_key(c,out);}
static int32_t full_status(void*c,alarm_status_v1*out){
#ifdef WATCH_ATOMIC_PREFERENCES
 if(full_controls&&!full_triggered&&clock_quick_scope){full_triggered=true;alarm_view.state=ALARM_STATE_ALERT;alarm_view.occurrence.generation=1;}
#else
 if(full_controls&&!full_triggered&&clock_quick.ui.position_q8>0){full_triggered=true;alarm_view.state=ALARM_STATE_ALERT;alarm_view.occurrence.generation=1;}
#endif
 return alarm_status(c,out);
}
static bool full_acquire(const char*n,uint32_t v,uint64_t i,risc_runtime_capability_v1*g){bool ok=fast_acquire(n,v,i,g);if(ok&&g->api==&alarm_api)g->api=&full_alarm;return ok;}
static int32_t full_run(uint64_t token,const char*path,risc_resident_result_v1*out){
 assert(token==9&&!path&&!owned&&!pending&&!touch.subscription);full_runs++;
 /* This is the continuation before any normal Home frame/input turn. */
 assert(!first_seen&&!clock_alarm_attempted&&!clock_alarm.api&&!alarm_acquires);
 full_alarms_before=alarm_acquires;
 clock_ms+=1001;
 if(full_short) {
  full_key_event=2;
  risc_resident_request_v1 request={sizeof(request),RISC_RESIDENT_CHECKPOINT_POLL,0,0};risc_resident_reply_v1 reply={sizeof(reply),0};
  assert(full_callbacks.dispatch(NULL,&request,&reply)==RISC_RESIDENT_OK&&(reply.flags&RISC_RESIDENT_REPLY_POLICY_REQUEST));
  request.reason=RISC_RESIDENT_CHECKPOINT_POLICY;
  assert(full_callbacks.dispatch(NULL,&request,&reply)==RISC_RESIDENT_OK);
  assert(alarm_acquires==1&&full_deadlines&&alarm_resumes==1&&sleeps==1&&motion_sleeps==1);
  assert(!touch.subscription&&!held&&!pending&&!owned&&!broadcast_live&&!contexts_live&&!clock_resident_keys);
  ended=true;out->status=RISC_RESIDENT_OK;return RISC_RESIDENT_OK;
 }
 risc_resident_request_v1 request={sizeof(request),full_controls?RISC_RESIDENT_CHECKPOINT_CONTROLS:RISC_RESIDENT_CHECKPOINT_POLICY,0,0};
 risc_resident_reply_v1 reply={sizeof(reply),0};
 assert(full_callbacks.dispatch(NULL,&request,&reply)==RISC_RESIDENT_OK);
 assert(alarm_acquires==full_alarms_before+1&&clock_alarm.api==&full_alarm.base&&clock_alarm_attempted);
 assert(alarm_frames&&full_acknowledged_at&&!alarm_view.occurrence.generation);
 assert(clock_resident_activity>=full_acknowledged_at);
 if(!full_controls)assert(clock_resident_policy_at>=full_acknowledged_at);
#ifdef WATCH_ATOMIC_PREFERENCES
 assert(!clock_quick_scope&&!quick_atomic_running&&!atomic_token);
#endif
 assert(!sleeps&&!owned&&!pending&&!touch.subscription&&!broadcast_live&&!contexts_live);
 /* Dismissal establishes a fresh idle interval, not the pre-modal timestamp. */
 clock_ms+=1001;request.reason=RISC_RESIDENT_CHECKPOINT_POLICY;
 assert(full_callbacks.dispatch(NULL,&request,&reply)==RISC_RESIDENT_OK&&!sleeps);
 ended=true;out->status=RISC_RESIDENT_OK;return RISC_RESIDENT_OK;
}
static bool full_get(struct risc_resident_client_v1*c){*c=(risc_resident_client_v1){.api_version=1,.struct_size=sizeof(*c),.invocation=9,.role=RISC_RESIDENT_ROLE_HOST,.register_shell=full_register,.run_foreground=full_run,.request_foreground_exit=full_exit};return true;}
int main(int argc,char**argv){
 full_controls=argc>1&&!strcmp(argv[1],"controls");full_short=argc>1&&!strcmp(argv[1],"short-key");
 reset_case();full_alarm=alarm_api;full_alarm.base.acknowledge=full_ack;full_alarm.base.status=full_status;full_alarm.base.prepare_sleep=full_prepare_alarm;pa.key_events=full_key;
 if(!full_controls&&!full_short){alarm_view.state=ALARM_STATE_ALERT;alarm_view.occurrence.generation=1;}
 fast_runtime.struct_size=sizeof(fast_runtime);fast_runtime.resident_shell=full_get;fast_runtime.health=full_health;fast_runtime.diagnostic=full_diagnostic;fast_runtime.acquire=full_acquire;fast_runtime.sleep_prepare=full_prepare_sleep;fast_runtime.sleep_resume=full_resume_sleep;
 app_main();
 assert(full_runs==1&&!retains&&!retained_fence&&!owned&&!pending&&!held&&!touch.subscription);
 assert(grants==ungrants&&subscribed==unsubscribed&&!allocations);
 if(full_short)puts("FAST_WAKE/ALARMS resident short-key: fresh alarm deadline, motion Light sleep/resume, paused background and clean teardown PASS");
 else printf("FAST_WAKE/ALARMS resident %s: alarm acquisition, due modal, >idle dismissal, fresh activity, paused background, clean teardown PASS\n",full_controls?"active-controls":"policy-continuation");
}
