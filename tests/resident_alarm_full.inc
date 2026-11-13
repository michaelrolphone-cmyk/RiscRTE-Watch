#include "RiscResidentAlarmV1.h"
#define main inherited_alarm_idle_main
#include "resident_idle_full.inc"
#undef main
static const char *alarm_only_case;
static unsigned alarm_only_key_reads,alarm_only_health_reads;
static bool alarm_only_fail_health;
static bool alarm_only_is(const char *s){return !strcmp(alarm_only_case,s);}
static bool alarm_only_key(void *c,uint32_t *out){
 ++alarm_only_key_reads;return att_key(c,out);
}
static bool alarm_only_health(risc_runtime_health_v1 *h){
 ++alarm_only_health_reads;
 if(alarm_only_fail_health){alarm_only_fail_health=false;return false;}
 return att_health(h);
}

static bool alarm_only_prime_armed;
static unsigned alarm_only_prime_injections,alarm_only_cue_steps;
static int32_t alarm_only_deadline(void *c,alarm_sleep_v1 *d){
 if(alarm_only_prime_armed){
  alarm_only_prime_armed=false;++alarm_only_prime_injections;
  assert(!attention_capture&&!clock_alarm_modal&&!alarm_view.occurrence.generation);
  if(alarm_only_is("outcome-prime-failure"))return ALARM_OUTPUT;
  assert(alarm_only_is("outcome-late-prime"));
  alarm_view.state=ALARM_STATE_ALERT;
  alarm_view.occurrence=(alarm_token_v1){1,9,attention_base_seconds+3,12};
  return ALARM_BUSY;
 }
 return att_deadline(c,d);
}
static int32_t alarm_only_step(void *c){
 int32_t rc=att_step(c);
 if(alarm_only_is("outcome-cue")&&alarm_view.state==ALARM_STATE_CUE){
  if(++alarm_only_cue_steps==2){alarm_view.state=ALARM_STATE_READY;alarm_view.occurrence=(alarm_token_v1){0};}
 }
 return rc;
}

static int32_t idle_outcome_case(risc_resident_result_v1 *result) {
 risc_resident_request_v1 q={sizeof(q),RISC_RESIDENT_QUERY_LAST_IDLE_ALARM,0,0};
 risc_resident_reply_v1 r={sizeof(r),0};
 assert(attention_host.dispatch(NULL,&q,&r)==RISC_RESIDENT_BUSY&&!r.flags);
 clock_resident_activity=clock_ms;clock_resident_keys=3;attention_keys=3;
 bool modal=alarm_only_is("outcome-modal")||alarm_only_is("outcome-reset");
 bool late=alarm_only_is("outcome-late-prime"),cue=alarm_only_is("outcome-cue");
 bool prime_failure=alarm_only_is("outcome-prime-failure");
 uint32_t revision=clock_alarm_input_revision;
 if(late||prime_failure)alarm_only_prime_armed=true;
 if(cue){alarm_view.state=ALARM_STATE_CUE;alarm_view.occurrence=(alarm_token_v1){0};}
 if(modal) {
  alarm_view.state=ALARM_STATE_ALERT;
  alarm_view.occurrence=(alarm_token_v1){1,9,attention_base_seconds+3,12};
 }
 q.reason=RISC_RESIDENT_APPLY_IDLE_ONLY;
 int32_t apply_rc=attention_host.dispatch(NULL,&q,&r);
 if(prime_failure){
  assert(apply_rc==RISC_RESIDENT_RETAINED&&clock_resident_terminal&&retains);
  assert(alarm_only_prime_injections==1&&!clock_resident_idle_result_valid);
  assert(clock_alarm_input_revision==revision&&!attention_acknowledges&&!sleeps&&!attention_exits);
  unsigned steps=attention_steps,keys=alarm_only_key_reads,healths=alarm_only_health_reads;
  q.reason=RISC_RESIDENT_QUERY_LAST_IDLE_ALARM;
  assert(attention_host.dispatch(NULL,&q,&r)==RISC_RESIDENT_RETAINED);
  assert(!clock_resident_idle_result_valid&&steps==attention_steps&&keys==alarm_only_key_reads&&healths==alarm_only_health_reads);
  result->status=RISC_RESIDENT_RETAINED;return RISC_RESIDENT_RETAINED;
 }
 assert(apply_rc==RISC_RESIDENT_OK);
 if(late)assert(alarm_only_prime_injections==1&&attention_acknowledges==1&&alarm_frames);
 if(cue)assert(alarm_only_cue_steps==2&&!attention_acknowledges&&!alarm_frames&&alarm_view.state==ALARM_STATE_READY);
 if(late||cue)assert(clock_alarm_input_revision!=revision);
 assert(r.flags&RISC_RESIDENT_REPLY_REDRAW);
 assert(!sleeps&&!attention_exits&&clock_resident_keys==3);
 q.reason=RISC_RESIDENT_QUERY_LAST_IDLE_ALARM;
 unsigned keys=alarm_only_key_reads,steps=attention_steps,frames=submitted,healths=alarm_only_health_reads;
 for(unsigned i=0;i<2;i++) {
  assert(attention_host.dispatch(NULL,&q,&r)==RISC_RESIDENT_OK);
  assert(r.flags==((modal||late||cue)?RISC_RESIDENT_REPLY_REDRAW:0));
 }
 assert(keys==alarm_only_key_reads&&steps==attention_steps&&frames==submitted&&healths==alarm_only_health_reads);
 if(alarm_only_is("outcome-reset")) {
  q.reason=RISC_RESIDENT_APPLY_IDLE_ONLY;clock_resident_activity=clock_ms;
  assert(attention_host.dispatch(NULL,&q,&r)==RISC_RESIDENT_OK);
  q.reason=RISC_RESIDENT_QUERY_LAST_IDLE_ALARM;
  assert(attention_host.dispatch(NULL,&q,&r)==RISC_RESIDENT_OK&&!r.flags);
 }
 if(alarm_only_is("outcome-busy")) {
  held=true;assert(attention_host.dispatch(NULL,&q,&r)==RISC_RESIDENT_BUSY);held=false;
  assert(attention_host.dispatch(NULL,&q,&r)==RISC_RESIDENT_OK&&!r.flags);
  q.reason=RISC_RESIDENT_APPLY_IDLE_ONLY;held=true;
  assert(attention_host.dispatch(NULL,&q,&r)==RISC_RESIDENT_BUSY);held=false;
  q.reason=RISC_RESIDENT_QUERY_LAST_IDLE_ALARM;
  assert(attention_host.dispatch(NULL,&q,&r)==RISC_RESIDENT_BUSY);
 }
 ended=true;result->status=RISC_RESIDENT_OK;return RISC_RESIDENT_OK;
}

static int32_t alarm_only_run(uint64_t token,const char *path,risc_resident_result_v1 *result){
 assert(token==9&&!path&&!owned&&!pending&&!touch.subscription);
 ++attention_runs;
 if(!strncmp(alarm_only_case,"outcome-",8))return idle_outcome_case(result);
 clock_resident_keys=3;attention_keys=3;
 if(alarm_only_is("due-idle")){clock_resident_activity=clock_ms-60001u;sleep_mode=PORTABLE_SLEEP_DEEP;}
 if(alarm_only_is("modal")){
  alarm_view.state=ALARM_STATE_ALERT;
  alarm_view.occurrence=(alarm_token_v1){1,9,attention_base_seconds+3,12};
  clock_resident_activity=clock_ms-60001u;
 }
 unsigned keys=alarm_only_key_reads,steps=attention_steps,frames=submitted,healths=alarm_only_health_reads;
 unsigned acq=grants,rel=ungrants,sub=subscribed,unsub=unsubscribed;
 risc_resident_request_v1 q={sizeof(q),RISC_RESIDENT_QUERY_ALARM_ONLY,0,0};
 risc_resident_reply_v1 reply={sizeof(reply),0};
 bool blocked=false;
 if(alarm_only_is("old-host")){
  assert(attention_host.dispatch(NULL,&q,&reply)==RISC_RESIDENT_BUSY&&!reply.flags);
  q.reason=RISC_RESIDENT_CHECKPOINT_ALARM;
  assert(attention_host.dispatch(NULL,&q,&reply)==RISC_RESIDENT_BUSY&&!reply.flags);
  assert(alarm_only_key_reads==keys&&attention_steps==steps&&submitted==frames&&clock_resident_keys==3&&attention_keys==3&&!sleeps&&!attention_exits);
  ended=true;result->status=RISC_RESIDENT_OK;return RISC_RESIDENT_OK;
 }
 if(alarm_only_is("unavailable")){
  const void *saved=clock_alarm.api;clock_alarm.api=NULL;
  assert(attention_host.dispatch(NULL,&q,&reply)==RISC_RESIDENT_BUSY&&!reply.flags);
  q.reason=RISC_RESIDENT_CHECKPOINT_ALARM;
  assert(attention_host.dispatch(NULL,&q,&reply)==RISC_RESIDENT_BUSY&&!reply.flags);
  clock_alarm.api=saved;blocked=true;
 }else if(alarm_only_is("held")){
  held=true;assert(attention_host.dispatch(NULL,&q,&reply)==RISC_RESIDENT_BUSY&&!reply.flags);held=false;blocked=true;
 }else if(alarm_only_is("reentrant")){
  clock_resident_dispatching=true;
  assert(attention_host.dispatch(NULL,&q,&reply)==RISC_RESIDENT_INVALID);
  clock_resident_dispatching=false;blocked=true;
 }else{
  if(alarm_only_is("query-flags"))q.flags=1;
  if(alarm_only_is("query-reserved"))q.reserved=1;
  if(q.flags||q.reserved){
   assert(attention_host.dispatch(NULL,&q,&reply)==RISC_RESIDENT_INVALID&&!reply.flags);blocked=true;
  }else assert(attention_host.dispatch(NULL,&q,&reply)==RISC_RESIDENT_OK&&!reply.flags);
 }
 assert(alarm_only_key_reads==keys&&attention_steps==steps&&submitted==frames&&alarm_only_health_reads==healths);
 assert(grants==acq&&ungrants==rel&&subscribed==sub&&unsubscribed==unsub);
 assert(clock_resident_keys==3&&attention_keys==3&&!sleeps&&!attention_exits);
 if(!blocked){
  q=(risc_resident_request_v1){sizeof(q),RISC_RESIDENT_CHECKPOINT_ALARM,0,0};
  if(alarm_only_is("apply-flags"))q.flags=1;
  if(alarm_only_is("apply-reserved"))q.reserved=1;
  if(alarm_only_is("health-failure"))alarm_only_fail_health=true;
  int32_t rc=attention_host.dispatch(NULL,&q,&reply);
  if(alarm_only_is("health-failure")){
   assert(rc==RISC_RESIDENT_RETAINED&&clock_resident_terminal&&retains&&retained_fence);
   unsigned after=alarm_only_health_reads;
   assert(attention_host.dispatch(NULL,&q,&reply)==RISC_RESIDENT_RETAINED);
   assert(alarm_only_health_reads==after&&alarm_only_key_reads==keys&&attention_steps==steps);
   result->status=RISC_RESIDENT_RETAINED;return RISC_RESIDENT_RETAINED;
  }
  if(q.flags||q.reserved){
   assert(rc==RISC_RESIDENT_INVALID&&!reply.flags&&attention_steps==steps&&alarm_only_key_reads==keys);
  }else{
   assert(rc==RISC_RESIDENT_OK&&reply.flags==RISC_RESIDENT_REPLY_REDRAW);
   assert(!owned&&!pending&&!touch.subscription&&clock_resident_keys==3);
   if(alarm_only_is("modal")){
    assert(attention_acknowledges==1&&alarm_frames&&alarm_only_key_reads>keys);
    assert(!clock_alarm_input_cancelled);
   }else assert(alarm_only_key_reads==keys&&attention_keys==3&&!attention_acknowledges);
  }
 }
 assert(!sleeps&&!attention_exits&&!clock_resident_sleep_pending);
 ended=true;result->status=RISC_RESIDENT_OK;return RISC_RESIDENT_OK;
}
static bool alarm_only_get(risc_resident_client_v1 *c){
 if(!att_get(c))return false;
 c->run_foreground=alarm_only_run;return true;
}
int main(int argc,char **argv){
 assert(argc==2);alarm_only_case=argv[1];reset_case();
 assert(alarm_calendar_seconds(2026,10,4,0,0,40,&attention_base_seconds));
 alarm_view.snapshot=7;
 att_alarm=alarm_api;att_alarm.base.status=att_status;att_alarm.base.step=alarm_only_step;
 att_alarm.base.acknowledge=att_ack;att_alarm.base.prepare_sleep=alarm_only_deadline;
 pa.key_events=alarm_only_key;ra.read=att_rtc;
 fast_runtime.struct_size=sizeof(fast_runtime);fast_runtime.resident_shell=alarm_only_get;
 fast_runtime.health=alarm_only_health;fast_runtime.diagnostic=att_diagnostic;
 fast_runtime.acquire=att_acquire;fast_runtime.sleep_prepare=att_sleep_begin;fast_runtime.sleep_resume=att_sleep_end;
 app_main();
#ifdef WATCH_HOME_CANVAS
 assert(attention_canvas_absent==1&&!clock_canvas_selected&&!clock_canvas.session);
#endif

 if(alarm_only_is("health-failure")||alarm_only_is("outcome-prime-failure")){
  assert(attention_runs==1&&clock_resident_terminal&&retains&&retained_fence&&!sleeps&&!attention_exits);
 }else assert(attention_runs==1&&!retains&&!retained_fence&&!owned&&!pending&&!held&&!touch.subscription&&grants==ungrants&&subscribed==unsubscribed&&!allocations);
 printf("Home alarm-only %s: production controller with Runtime/hardware doubles PASS\n",alarm_only_case);
 return 0;
}
