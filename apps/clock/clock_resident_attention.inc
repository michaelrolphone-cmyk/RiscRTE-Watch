/* Narrow observation while a foreground capture owns the native exit barrier.
 * Only existing key/RTC reads and copied alarm status are callable here. */
#include "RiscResidentAttentionV1.h"
static bool clock_attention_enabled,clock_attention_observing,clock_attention_alarm;
static bool clock_attention_sampled;
static uint32_t clock_attention_at,clock_attention_seconds,clock_attention_deadline,clock_attention_snapshot;
static risc_runtime_capability_v1 clock_attention_rtc_grant;
static const twatch_rtc_api_v1 *clock_attention_rtc;
static bool clock_attention_close(void) {
 if(!clock_resident_alive())return false;
 if(!clock_attention_rtc_grant.struct_size&&!clock_attention_rtc_grant.api&&!clock_attention_rtc_grant.slot&&!clock_attention_rtc_grant.generation)return true;
 if(clock_attention_rtc_grant.struct_size!=sizeof(clock_attention_rtc_grant))return false;
 if(clock_attention_rtc_grant.api||clock_attention_rtc_grant.slot||clock_attention_rtc_grant.generation) {
  if(!clock_attention_rtc_grant.api||!clock_attention_rtc_grant.slot||!clock_attention_rtc_grant.generation)return false;
  if(!rt->release(&clock_attention_rtc_grant)||!clock_resident_alive()||clock_attention_rtc_grant.struct_size!=sizeof(clock_attention_rtc_grant)||clock_attention_rtc_grant.api||clock_attention_rtc_grant.slot||clock_attention_rtc_grant.generation)return false;
 }
 clock_attention_rtc=NULL;return true;
}
#ifdef WATCH_CLOCK_ALARMS
static int32_t clock_attention_copy_deadline(alarm_sleep_v1 *decision) {
 for(unsigned n=0;n<64;n++) {
  *decision=(alarm_sleep_v1){.struct_size=sizeof(*decision)};
  int32_t rc=clock_alarm.api->prepare_sleep(clock_alarm.api->context,decision);
  if(!clock_resident_alive())return ALARM_OUTPUT;
  if(rc!=ALARM_PENDING)return rc;
  rc=clock_alarm.api->step(clock_alarm.api->context);
  if(!clock_resident_alive()||rc<ALARM_FOREGROUND||rc>ALARM_PENDING||rc==ALARM_OUTPUT)return ALARM_OUTPUT;
  alarm_status_v1 status={.struct_size=sizeof(status)};
  rc=clock_alarm.api->status(clock_alarm.api->context,&status);
  if(!clock_resident_alive()||rc!=ALARM_OK||status.api_version!=1||status.struct_size<sizeof(status)||status.state>ALARM_STATE_CUE)return ALARM_OUTPUT;
  if(status.output_uncertain||status.error<ALARM_FOREGROUND||status.error>ALARM_PENDING||status.error==ALARM_OUTPUT)return ALARM_OUTPUT;
  if(status.state==ALARM_STATE_BLOCKED)return status.error<0?status.error:ALARM_INVALID;
  if(status.occurrence.generation||status.state==ALARM_STATE_CUE)return ALARM_BUSY;
 }
 return ALARM_BUSY;
}
#endif
/* Called only while the host is settled, before child entry or after a settled
 * modal. prepare_sleep is used solely to copy the nearest reconciled deadline
 * (including Points), never as a reusable sleep ticket or permission to ACK. */
static bool clock_attention_prime(void) {
 if(!clock_attention_enabled)return true;
 if(!clock_resident_alive()||clock_attention_observing)return false;
 clock_attention_alarm=false;clock_attention_sampled=false;clock_attention_deadline=0;
#ifdef WATCH_CLOCK_ALARMS
 if(!clock_resident_alarm_ready())return false;
 if(clock_alarm.api) {
  clock_attention_rtc=rtc;
  if(!clock_attention_rtc) {
   if(clock_attention_rtc_grant.struct_size!=sizeof(clock_attention_rtc_grant)||((clock_attention_rtc_grant.api||clock_attention_rtc_grant.slot||clock_attention_rtc_grant.generation)&&(!clock_attention_rtc_grant.api||!clock_attention_rtc_grant.slot||!clock_attention_rtc_grant.generation)))return false;
   if(clock_attention_rtc_grant.api)clock_attention_rtc=clock_attention_rtc_grant.api;
   else {
    clock_attention_rtc_grant=(risc_runtime_capability_v1){.struct_size=sizeof(clock_attention_rtc_grant)};
    bool ok=rt->acquire("rtc.clock",2,0,&clock_attention_rtc_grant);
    if(!clock_resident_alive())return false;
    if(!ok){if(clock_attention_rtc_grant.struct_size!=sizeof(clock_attention_rtc_grant)||clock_attention_rtc_grant.api||clock_attention_rtc_grant.slot||clock_attention_rtc_grant.generation)return false;}
    else {
     if(clock_attention_rtc_grant.struct_size!=sizeof(clock_attention_rtc_grant)||!clock_attention_rtc_grant.api||!clock_attention_rtc_grant.slot||!clock_attention_rtc_grant.generation)return false;
     clock_attention_rtc=clock_attention_rtc_grant.api;
    }
   }
  }
  if(!clock_attention_rtc)return false;
  if(clock_attention_rtc&&(clock_attention_rtc->api_version!=2||clock_attention_rtc->struct_size<sizeof(*clock_attention_rtc)||!clock_attention_rtc->read))return false;
  if(!touch.subscription&&(touch.grant.api||!launcher_touch_open(&touch)))return false;
  if(!clock_resident_alive()||!clock_resident_alarm_pump()||!clock_resident_alive())return false;
  alarm_sleep_v1 decision={.struct_size=sizeof(decision)};
  int32_t rc=clock_attention_copy_deadline(&decision);
  if(!clock_resident_alive())return false;
  /* Reconcile an intervening occurrence through the existing modal before
   * attempting a child capture. Never start a child over uncertain output. */
  if(rc==ALARM_OUTPUT||rc<ALARM_FOREGROUND||rc>ALARM_PENDING)return false;
  if(rc!=ALARM_OK) {
   if(!clock_resident_alarm_pump()||!clock_resident_alive())return false;
   rc=clock_attention_copy_deadline(&decision);
   if(!clock_resident_alive())return false;
  }
  if(!portable_alarm_status(&clock_alarm)||!clock_resident_alive()||clock_alarm.status.output_uncertain)return false;
  clock_attention_snapshot=clock_alarm.status.snapshot;
  if(rc==ALARM_OK) {
   if(decision.struct_size!=sizeof(decision)||decision.snapshot!=clock_alarm.status.snapshot)return false;
   clock_attention_deadline=decision.deadline;clock_attention_seconds=decision.rtc_seconds;
  } else if(clock_alarm.status.state==ALARM_STATE_BLOCKED&&clock_alarm_error_seen&&clock_alarm_seen_snapshot==clock_alarm.status.snapshot&&clock_alarm_seen_error==clock_alarm.status.error) {
   /* Original dismissed error is not a recurring automatic modal. A refresh
    * changes the copied snapshot/state and will request reconciliation. */
   clock_attention_seconds=clock_alarm.status.rtc_seconds;
  } else return false;
  uint32_t anchor;if(!alive(&anchor)||!clock_resident_alive())return false;
  clock_attention_at=anchor;clock_attention_sampled=true;
  if(!clock_resident_background_pause()||!launcher_touch_close(&touch)||!clock_resident_alive())return false;
 }
#endif
 return true;
}
static int32_t clock_attention_observe(void *context,uint32_t now,risc_resident_attention_v1 *out) {
 (void)context;
 if(!clock_resident_alive())return RISC_RESIDENT_RETAINED;
 if(!out||out->struct_size!=sizeof(*out))return RISC_RESIDENT_INVALID;
 if(!clock_resident_child||clock_resident_dispatching||clock_attention_observing)return RISC_RESIDENT_BUSY;
 clock_attention_observing=true;
#ifdef WATCH_CLOCK_ALARMS
 if(clock_alarm.api) {
  alarm_status_v1 s={.struct_size=sizeof(s)};
  int32_t rc=clock_alarm.api->status(clock_alarm.api->context,&s);
  if(!clock_resident_alive())goto retained;
  if(rc==ALARM_BUSY){clock_attention_observing=false;return RISC_RESIDENT_BUSY;}
  if(rc!=ALARM_OK||s.output_uncertain||s.error<ALARM_FOREGROUND||s.error>ALARM_PENDING||s.error==ALARM_OUTPUT||s.api_version!=1||s.struct_size<sizeof(s)||s.state>ALARM_STATE_CUE)goto retained;
  bool seen=s.state==ALARM_STATE_BLOCKED&&clock_alarm_error_seen&&clock_alarm_seen_snapshot==s.snapshot&&clock_alarm_seen_error==s.error;
  if(s.output_uncertain||s.occurrence.generation||s.snapshot!=clock_attention_snapshot||(!seen&&(s.state!=ALARM_STATE_READY||s.error!=ALARM_OK)))clock_attention_alarm=true;
  if(!seen&&(!clock_attention_sampled||(uint32_t)(now-clock_attention_at)>=1000u)) {
   twatch_rtc_time_v1 t={0};uint32_t seconds=0;
   bool ok=clock_attention_rtc&&clock_attention_rtc->read(clock_attention_rtc->context,&t);
   if(!clock_resident_alive())goto retained;
   ok=ok&&t.weekday<=6&&alarm_calendar_seconds(t.year,t.month,t.day,t.hour,t.minute,t.second,&seconds);
   if(!ok)clock_attention_alarm=true;
   else {
    if(clock_attention_sampled) {
     uint32_t elapsed=(uint32_t)(now-clock_attention_at)/1000u;
     uint64_t delta=seconds>=clock_attention_seconds?(uint64_t)seconds-clock_attention_seconds:UINT64_MAX;
     if(delta==UINT64_MAX||(delta>elapsed?delta-elapsed:elapsed-delta)>2u)clock_attention_alarm=true;
    }
    if(clock_attention_deadline&&seconds>=clock_attention_deadline)clock_attention_alarm=true;
    clock_attention_seconds=seconds;
   }
   clock_attention_at=now;clock_attention_sampled=true;
  }
 }
#endif
 uint32_t keys=0;
 if(!watch_capabilities_key_events(&device_clients,&keys)||!clock_resident_alive()||(keys&~3u))goto retained;
 if(keys&&(uint32_t)(now-clock_resident_armed_at)>=250u)clock_resident_keys|=keys;
 out->pending_flags=(clock_attention_alarm?RISC_RESIDENT_ATTENTION_ALARM:0u)|
  ((clock_resident_keys&2u)?RISC_RESIDENT_ATTENTION_CONTROLS:0u)|((clock_resident_keys&1u)?RISC_RESIDENT_ATTENTION_EXIT:0u);
 clock_attention_observing=false;return RISC_RESIDENT_OK;
retained:
 clock_attention_observing=false;clock_resident_terminal=true;return RISC_RESIDENT_RETAINED;
}
static int32_t clock_attention_reject(void *context,uint32_t bits) {
 (void)context;
 if(!clock_resident_alive())return RISC_RESIDENT_RETAINED;
 if(!bits||(bits&~(RISC_RESIDENT_ATTENTION_CONTROLS|RISC_RESIDENT_ATTENTION_EXIT)))return RISC_RESIDENT_INVALID;
 if(clock_attention_observing||clock_resident_dispatching||!clock_resident_child)return RISC_RESIDENT_BUSY;
 uint32_t keys=((bits&RISC_RESIDENT_ATTENTION_CONTROLS)?2u:0u)|((bits&RISC_RESIDENT_ATTENTION_EXIT)?1u:0u);
 if((clock_resident_keys&keys)!=keys)return RISC_RESIDENT_INVALID;
 clock_resident_keys&=~keys;return RISC_RESIDENT_OK;
}
static bool clock_attention_bind(void) {
 if(clock_attention_rtc_grant.api||clock_attention_rtc_grant.slot||clock_attention_rtc_grant.generation)return false;
 clock_attention_enabled=clock_attention_observing=clock_attention_alarm=clock_attention_sampled=false;
 clock_attention_rtc=NULL;clock_attention_rtc_grant=(risc_runtime_capability_v1){.struct_size=sizeof(clock_attention_rtc_grant)};
 if(!risc_resident_attention_available_v1(&clock_resident_extended))return true;
 risc_resident_attention_callbacks_v1 callbacks={.api_version=1,.struct_size=sizeof(callbacks),.tag=RISC_RESIDENT_ATTENTION_TAG,.supported_flags=RISC_RESIDENT_ATTENTION_ALL,.observe=clock_attention_observe,.reject_manual=clock_attention_reject};
 int32_t rc=clock_resident_extended.register_attention(clock_resident.invocation,&callbacks);
 if(!clock_resident_alive()||rc!=RISC_RESIDENT_OK)return false;
 clock_attention_enabled=true;return true;
}
