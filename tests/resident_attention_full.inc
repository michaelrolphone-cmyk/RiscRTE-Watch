static risc_resident_callbacks_v1 attention_host;
static risc_resident_attention_callbacks_v1 attention_callback;
static unsigned attention_keys,attention_runs,attention_exits,attention_steps,attention_acknowledges;
static bool attention_bind_failure;
static bool attention_capture,attention_short,attention_long,attention_alarm_case;
static uint32_t attention_base_seconds;
static int32_t att_register(uint64_t t,const risc_resident_callbacks_v1*c){assert(t==9);attention_host=*c;return 0;}
static int32_t att_register_observer(uint64_t t,const risc_resident_attention_callbacks_v1*c){assert(t==9&&c->tag==RISC_RESIDENT_ATTENTION_TAG);if(attention_bind_failure)return RISC_RESIDENT_INVALID;attention_callback=*c;return 0;}
static int32_t att_query_stub(uint64_t t,risc_resident_attention_v1*q){(void)t;(void)q;return RISC_RESIDENT_INCOMPATIBLE;}
static bool att_get(risc_resident_client_v1*c);
static int32_t att_reject_stub(uint64_t t,uint32_t b){(void)t;(void)b;return RISC_RESIDENT_INCOMPATIBLE;}
static int32_t att_exit(uint64_t t){assert(t==9&&!attention_capture);attention_exits++;return 0;}
static bool att_key(void*c,uint32_t*out){(void)c;*out=attention_keys;attention_keys=0;if(!attention_capture&&clock_alarm_modal&&alarm_view.occurrence.generation&&alarm_frames)*out=1;return true;}
static bool att_rtc(void*c,twatch_rtc_time_v1*out){(void)c;*out=(twatch_rtc_time_v1){.year=2026,.month=10,.day=4,.weekday=0,.hour=0,.minute=0,.second=40};out->second+=(uint8_t)(clock_ms/1000u);return true;}
static int32_t att_status(void*c,alarm_status_v1*out){(void)c;*out=alarm_view;return 0;}
static int32_t att_step(void*c){(void)c;assert(!attention_capture);attention_steps++;return 0;}
static int32_t att_ack(void*c,const alarm_token_v1*t){assert(!attention_capture&&t->generation==12&&t->revision==9&&t->deadline==attention_base_seconds+3);attention_acknowledges++;return alarm_ack(c,t);}
static int32_t att_deadline(void*c,alarm_sleep_v1*d){(void)c;assert(!attention_capture);*d=(alarm_sleep_v1){sizeof(*d),alarm_view.snapshot,attention_base_seconds+clock_ms/1000u,attention_alarm_case&&!attention_acknowledges?attention_base_seconds+3:0};return 0;}
static alarm_service_sleep_v1 att_alarm;
static bool att_acquire(const char*n,uint32_t v,uint64_t i,risc_runtime_capability_v1*g){assert(!attention_capture);bool ok=fast_acquire(n,v,i,g);if(ok&&g->api==&alarm_api)g->api=&att_alarm;return ok;}
static int32_t att_sleep_begin(uint64_t*t){assert(!attention_capture&&!touch.subscription&&!held&&!pending&&!owned);*t=99;return RISC_RUNTIME_SLEEP_OK;}
static int32_t att_sleep_end(uint64_t t){assert(t==99&&!attention_capture&&!held&&!pending&&!owned);return RISC_RUNTIME_SLEEP_OK;}
static bool att_diagnostic(const char*s){(void)s;return true;}
static bool att_health(risc_runtime_health_v1*h){check_io();h->uptime_ms=clock_ms;return !ended&&clock_ms<150000;}
static int32_t att_run(uint64_t t,const char*p,risc_resident_result_v1*out){
 assert(t==9&&!p&&!owned&&!pending&&!touch.subscription&&attention_callback.observe);attention_runs++;
 unsigned before_steps=attention_steps,before_grants=grants,before_frames=submitted;
 attention_capture=true;
 for(unsigned i=0;i<200;i++){
  clock_ms+=10;
  risc_resident_attention_v1 q={sizeof(q),attention_callback.supported_flags,0,clock_ms};
  assert(attention_callback.observe(NULL,clock_ms,&q)==RISC_RESIDENT_OK&&!q.pending_flags);
 }
 assert(attention_steps==before_steps&&grants==before_grants&&submitted==before_frames&&!sleeps&&!attention_exits);
 if(attention_alarm_case){clock_ms=3500;}
 else if(attention_short||attention_long)attention_keys=attention_short?2:1;
 else {attention_capture=false;ended=true;out->status=0;return 0;}
 risc_resident_attention_v1 q={sizeof(q),attention_callback.supported_flags,0,clock_ms};
 assert(attention_callback.observe(NULL,clock_ms,&q)==0);
 assert(q.pending_flags==(attention_alarm_case?1u:attention_short?2u:4u));
 assert(attention_steps==before_steps&&grants==before_grants&&submitted==before_frames&&!sleeps&&!attention_exits);
 attention_capture=false;
 if(attention_alarm_case){alarm_view.state=ALARM_STATE_ALERT;alarm_view.occurrence=(alarm_token_v1){1,9,attention_base_seconds+3,12};}
 risc_resident_request_v1 request={sizeof(request),RISC_RESIDENT_CHECKPOINT_POLL,RISC_RESIDENT_POLL_INHIBIT_IDLE,0};risc_resident_reply_v1 reply={sizeof(reply),0};
 assert(attention_host.dispatch(NULL,&request,&reply)==0&&(reply.flags&RISC_RESIDENT_REPLY_POLICY_REQUEST));
 request.reason=RISC_RESIDENT_CHECKPOINT_POLICY;request.flags=0;
 int rc=attention_host.dispatch(NULL,&request,&reply);
 fprintf(stderr,"policy rc=%d terminal=%d alarmstate=%u snapshot=%u deadline=%u\n",rc,clock_resident_terminal,alarm_view.state,alarm_view.snapshot,clock_attention_deadline);assert(rc==(attention_long?RISC_RESIDENT_EXIT:RISC_RESIDENT_OK));
 assert(!clock_resident_keys&&!owned&&!pending&&!touch.subscription);
 if(attention_alarm_case)assert(attention_acknowledges==1&&alarm_frames&&!sleeps);
 if(attention_long)assert(attention_exits==1&&!sleeps);
 if(attention_short)assert(sleeps==1);
 ended=true;out->status=0;return 0;
}
static bool att_get(risc_resident_client_v1*c){assert(c->struct_size>=sizeof(risc_resident_attention_client_v1));risc_resident_attention_client_v1*e=(void*)c;*e=(risc_resident_attention_client_v1){.base={.api_version=1,.struct_size=sizeof(*e),.invocation=9,.role=RISC_RESIDENT_ROLE_HOST,.register_shell=att_register,.run_foreground=att_run,.request_foreground_exit=att_exit},.attention_tag=RISC_RESIDENT_ATTENTION_TAG,.attention_version=1,.register_attention=att_register_observer,.attention=att_query_stub,.reject_manual=att_reject_stub};return true;}
int main(int argc,char**argv){
 attention_bind_failure=argc>1&&!strcmp(argv[1],"bind-failure");
 attention_short=argc>1&&!strcmp(argv[1],"short-key");attention_long=argc>1&&!strcmp(argv[1],"long-key");attention_alarm_case=argc>1&&!strcmp(argv[1],"alarm");reset_case();
 assert(alarm_calendar_seconds(2026,10,4,0,0,40,&attention_base_seconds));alarm_view.snapshot=7;
 att_alarm=alarm_api;att_alarm.base.status=att_status;att_alarm.base.step=att_step;att_alarm.base.acknowledge=att_ack;att_alarm.base.prepare_sleep=att_deadline;
 pa.key_events=att_key;ra.read=att_rtc;
 fast_runtime.struct_size=sizeof(fast_runtime);fast_runtime.resident_shell=att_get;fast_runtime.health=att_health;fast_runtime.diagnostic=att_diagnostic;fast_runtime.acquire=att_acquire;fast_runtime.sleep_prepare=att_sleep_begin;fast_runtime.sleep_resume=att_sleep_end;
 app_main();if(attention_bind_failure){assert(clock_resident_terminal&&retains&&!attention_runs);puts("Full production Home rejected attention registration: retained with no child dispatch PASS");return 0;}assert(attention_runs==1&&!retains&&!retained_fence&&!owned&&!pending&&!held&&!touch.subscription);fprintf(stderr,"cleanup grants=%u/%u subscriptions=%u/%u allocations=%u terminal=%u\n",grants,ungrants,subscribed,unsubscribed,allocations,clock_resident_terminal);assert(grants==ungrants&&subscribed==unsubscribed&&!allocations);
 printf("Full production Home attention %s: quiet capture, checked handoff, exact alarm/key behavior PASS\n",argc>1?argv[1]:"quiet");return 0;
}
