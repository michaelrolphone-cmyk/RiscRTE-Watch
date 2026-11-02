#pragma once
/* Watch-owned policy over public Runtime mechanisms. No hardware identities,
 * pointers, grant tokens, or boot-local counters enter the checkpoint. */
#include "RiscRuntimeV1.h"
#ifdef WATCH_SHARED_HOME_BOOT
#include "RiscRetainedWakeV1.h"
#include "RiscRealtimeV1.h"
#include "RiscProviderPromotionV1.h"
#include "PortableHomeBoot.h"
#else
#include "sdk/RiscRetainedWakeV1.h"
#include "sdk/RiscRealtimeV1.h"
#include "sdk/RiscProviderPromotionV1.h"
#endif
#include "../clock/display_time.h"
#include <string.h>
#ifdef WATCH_SHARED_HOME_CIVIL
#ifdef WATCH_SHARED_HOME_BOOT
#include "RiscCivilClockV1.h"
#else
#include "sdk/RiscCivilClockV1.h"
#endif
#endif
#ifdef WATCH_SHARED_HOME_RETAINED
#include "PortableDeskClock.h"
#if PORTABLE_DESK_CLOCK_RECORD_BYTES != 408u
#error "Shared Home retained clock requires the existing 408-byte schema3"
#endif
#endif
#define WATCH_CLOCK_CHECKPOINT_TYPE 0x57434c4bu
#define WATCH_CLOCK_CHECKPOINT_SCHEMA 1u

typedef struct {
 const risc_runtime_api_v1 *runtime;
 risc_runtime_capability_v1 time_grant,wake_grant;
 const risc_realtime_control_api_v1 *time;
 const risc_retained_wake_api_v1 *wake;
 const twatch_rtc_api_v1 *fallback;
 bool time_live,wake_live,halted,resume;
#ifdef WATCH_SHARED_HOME_CIVIL
 risc_runtime_capability_v1 civil_grant;
 const risc_civil_clock_policy_v1 *civil;
 bool civil_live;
#endif
 bool rtc_reconcile_pending; /* RTC absent/unreadable/unset or seed not accepted. */
 uint32_t cause;
#ifdef WATCH_SHARED_HOME_RETAINED
 bool desk_supported,desk_resume;
 portable_desk_record desk_record;
#endif
} watch_clock_runtime;

/* Canonical append-only Runtime 0.1.54 invocation fence; frozen Watch SDK
 * retains its public prefix, so read only after proving this suffix exists. */
static inline void watch_runtime_retain(const risc_runtime_api_v1 *rt) {
 bool (*retain)(void)=NULL;
 size_t offset=RISC_RUNTIME_CAPABILITIES_V1_SIZE+sizeof(bool (*)(void));
 if(rt && rt->struct_size>=offset+sizeof(retain)) {
  memcpy(&retain,(const unsigned char*)rt+offset,sizeof(retain));
  if(retain)(void)retain();
 }
}
static inline void watch_runtime_halt(watch_clock_runtime *c) {
 c->halted=true;watch_runtime_retain(c->runtime);
}

static bool watch_runtime_close(watch_clock_runtime *c) {
 if(c->halted)return false;
#ifdef WATCH_SHARED_HOME_CIVIL
 if(c->civil_live) {
  if(!c->runtime->release(&c->civil_grant)){watch_runtime_halt(c);return false;}
  c->civil_live=false;c->civil=NULL;
 }
#endif
 if(c->wake_live) {
  if(!c->runtime->release(&c->wake_grant)){watch_runtime_halt(c);return false;}
  c->wake_live=false;c->wake=NULL;
 }
 if(c->time_live) {
  if(!c->runtime->release(&c->time_grant)){watch_runtime_halt(c);return false;}
  c->time_live=false;c->time=NULL;
 }
 return true;
}
static bool watch_runtime_snapshot(watch_clock_runtime *c,risc_realtime_snapshot_v1 *out) {
 if(c->halted || !c->time)return false;
 risc_realtime_snapshot_v1 sample={.struct_size=sizeof(sample)};
 int rc=c->time->read(c->time->context,&sample);
 if(rc==RISC_REALTIME_CONTEXT){watch_runtime_halt(c);return false;}
 if(rc!=RISC_REALTIME_OK || sample.struct_size!=sizeof(sample) || sample.reserved ||
    sample.nanoseconds>=1000000000u || sample.nanoseconds%1000u ||
    sample.monotonic_before_us>sample.monotonic_after_us ||
    sample.validity>RISC_REALTIME_VALID || sample.epoch_seconds<0 || sample.epoch_seconds>INT32_MAX ||
    (!sample.validity && (sample.epoch_seconds || sample.nanoseconds)))return false;
 *out=sample;return true;
}
#ifdef WATCH_SHARED_HOME_CIVIL
/* Explicit foreground-only admission. Caller must classify/replay sparse TIMER
 * before this call; neither open() nor native snapshot() acquires a provider. */
static inline bool watch_runtime_bind_civil(watch_clock_runtime *c) {
 if(c->halted)return false;
 if(c->civil_live)return c->civil!=NULL;
 c->civil_grant=(risc_runtime_capability_v1){.struct_size=sizeof(c->civil_grant)};
 if(!c->runtime->acquire(RISC_CIVIL_CLOCK_CAPABILITY,1,0,&c->civil_grant))return false;
 c->civil_live=true;
 const risc_civil_clock_api_v1 *base=c->civil_grant.api;
 c->civil=base?risc_civil_clock_get_policy(base,base->struct_size):NULL;
 return c->civil!=NULL;
}
static inline bool watch_runtime_civil_result(watch_clock_runtime*c,int rc) {
 if(rc!=0 && rc!=-1)watch_runtime_halt(c);
 return rc==0 && !c->halted;
}
static inline bool watch_runtime_interpret(watch_clock_runtime*c,const twatch_rtc_time_v1*date,int64_t*out) {
 if(c->halted||!c->civil||!tw_valid_time(date))return false;
 risc_civil_calendar_v1 raw={.struct_size=sizeof(raw),.year=date->year,.month=date->month,
  .day=date->day,.hour=date->hour,.minute=date->minute,.second=date->second,.weekday=date->weekday};
 int64_t epoch=-1;int rc=c->civil->interpret_rtc(c->civil->context,&raw,&epoch);
 if(!watch_runtime_civil_result(c,rc)||epoch<0||epoch>INT64_C(4102444799))return false;
 *out=epoch;return true;
}
/* Returns civil calendar already projected by the installed policy, plus the
 * original snapshot fraction. Caller must NOT apply legacy display_time again. */
static inline bool watch_runtime_read_policy(watch_clock_runtime*c,int64_t*utc_epoch,risc_civil_calendar_v1*out) {
 if(c->halted||!c->civil)return false;
 int64_t epoch;uint32_t ns=0;
 if(c->fallback) {
  twatch_rtc_time_v1 raw={0};
  if(!c->fallback->read(c->fallback->context,&raw)||!watch_runtime_interpret(c,&raw,&epoch))return false;
 } else {
  risc_realtime_snapshot_v1 sample;
  if(!watch_runtime_snapshot(c,&sample)||sample.validity!=RISC_REALTIME_VALID)return false;
  epoch=sample.epoch_seconds;ns=sample.nanoseconds;
 }
 risc_civil_calendar_v1 value={.struct_size=sizeof(value)};
 int rc=c->civil->project(c->civil->context,epoch,ns,&value);
 if(!watch_runtime_civil_result(c,rc)||value.struct_size!=sizeof(value)||value.reserved||
    value.nanoseconds!=ns||value.year<2000||value.year>2099||!value.zone[0]||
    !memchr(value.zone,0,sizeof(value.zone)))return false;
 twatch_rtc_time_v1 result={.year=(uint16_t)value.year,.month=value.month,.day=value.day,
  .hour=value.hour,.minute=value.minute,.second=value.second,.weekday=value.weekday};
 if(!tw_valid_time(&result))return false;
 *utc_epoch=epoch;*out=value;return true;
}
static inline bool watch_runtime_read_projected(watch_clock_runtime*c,int64_t*utc_epoch,twatch_rtc_time_v1*out,uint16_t*fraction_ms) {
 risc_civil_calendar_v1 value;
 if(!watch_runtime_read_policy(c,utc_epoch,&value))return false;
 *out=(twatch_rtc_time_v1){.year=(uint16_t)value.year,.month=value.month,.day=value.day,
  .hour=value.hour,.minute=value.minute,.second=value.second,.weekday=value.weekday};
 *fraction_ms=(uint16_t)(value.nanoseconds/1000000u);return true;
}
#endif
/* Existing Watch RTC ownership is explicitly fixed UTC+08. The UTC native
 * clock never receives Denver local wall time, compiler time, or an estimate. */
static bool watch_runtime_epoch(const twatch_rtc_time_v1 *date,int64_t *out) {
 if(!tw_valid_time(date))return false;
 int64_t days=10957; /* 2000-01-01 minus Unix epoch. */
 for(unsigned y=2000;y<date->year;y++)days+=365u+(y%4==0);
 for(unsigned m=1;m<date->month;m++)days+=watch_month_days(date->year,m);
 int64_t epoch=(days+date->day-1)*86400+(int64_t)date->hour*3600+
               (int64_t)date->minute*60+date->second-8*3600;
 if(epoch<0 || epoch>INT32_MAX)return false;
 *out=epoch;return true;
}
static bool watch_runtime_calendar(int64_t epoch,twatch_rtc_time_v1 *out) {
 if(epoch<946656000 || epoch>INT32_MAX)return false;
 uint32_t local=(uint32_t)(epoch+8*3600),days=local/86400;
 uint32_t remaining=days-10957,seconds=local%86400;
 unsigned year=2000,month=1;
 while(remaining>=365u+(year%4==0)){remaining-=365u+(year%4==0);++year;}
 while(remaining>=watch_month_days(year,month)){remaining-=watch_month_days(year,month);++month;}
 *out=(twatch_rtc_time_v1){.year=(uint16_t)year,.month=(uint8_t)month,.day=(uint8_t)(remaining+1),
  .weekday=(uint8_t)((days+4)%7),.hour=(uint8_t)(seconds/3600),
  .minute=(uint8_t)(seconds/60%60),.second=(uint8_t)(seconds%60)};
 return tw_valid_time(out);
}
static bool watch_runtime_open(watch_clock_runtime *c,const risc_runtime_api_v1 *rt,bool is_default) {
 *c=(watch_clock_runtime){.runtime=rt,.cause=RISC_BOOT_RESET};
 c->time_grant=(risc_runtime_capability_v1){.struct_size=sizeof(c->time_grant)};
 if(!rt->acquire(RISC_REALTIME_CONTROL_CAPABILITY,1,0,&c->time_grant))return false;
 c->time_live=true;c->time=c->time_grant.api;
 if(!c->time || c->time->api_version!=1 || c->time->struct_size<sizeof(*c->time) ||
    !c->time->read || !c->time->seed)return false;
 if(!is_default)return true;
 c->wake_grant=(risc_runtime_capability_v1){.struct_size=sizeof(c->wake_grant)};
 if(!rt->acquire(RISC_RETAINED_WAKE_CAPABILITY,1,0,&c->wake_grant)) {
  /* A standalone boot-test or other non-retained deployment can omit this
   * service. Start cold; never claim a restored checkpoint without a grant. */
  rt->diagnostic("WATCH_CLOCK retained-wake=unavailable resume=cold");
  return true;
 }
 c->wake_live=true;c->wake=c->wake_grant.api;
 if(!c->wake || c->wake->api_version!=1 || c->wake->struct_size<sizeof(*c->wake) ||
    !c->wake->read || !c->wake->stage || !c->wake->clear)return false;
 risc_retained_wake_record_v1 record={.struct_size=sizeof(record)};
 int rc=
#ifdef WATCH_SHARED_HOME_BOOT
 portable_home_boot_read(c->wake,WATCH_CLOCK_CHECKPOINT_TYPE,WATCH_CLOCK_CHECKPOINT_SCHEMA,&record,&c->cause);
#else
 c->wake->read(c->wake->context,WATCH_CLOCK_CHECKPOINT_TYPE,WATCH_CLOCK_CHECKPOINT_SCHEMA,&record,&c->cause);
#endif
 if(rc==RISC_RETAINED_WAKE_CONTEXT){watch_runtime_halt(c);return false;}
 if(rc!=RISC_RETAINED_WAKE_OK && rc!=RISC_RETAINED_WAKE_ABSENT && rc!=RISC_RETAINED_WAKE_MISMATCH) {
#ifdef WATCH_SHARED_HOME_RETAINED
  if(rc!=RISC_RETAINED_WAKE_INVALID)watch_runtime_halt(c);
#endif
  return false;
 }
 if(c->cause>RISC_BOOT_DEEP_OTHER)return false;
 c->resume=rc==RISC_RETAINED_WAKE_OK && c->cause>=RISC_BOOT_DEEP_TIMER &&
  record.struct_size==sizeof(record) && record.type==WATCH_CLOCK_CHECKPOINT_TYPE &&
  record.schema_version==WATCH_CLOCK_CHECKPOINT_SCHEMA && record.size==1 && record.payload[0]==1;
#ifdef WATCH_SHARED_HOME_RETAINED
 /* A mismatched legacy read never consumes the native boot snapshot. Decode
  * the shared app-owned schema only through the verified extended capability.
  * No display, input, storage, RTC, or presentation provider has opened yet. */
 const risc_retained_wake_api_v1_extended *extended=risc_retained_wake_extended(c->wake);
 c->desk_supported=extended && extended->max_payload_bytes>=PORTABLE_DESK_CLOCK_RECORD_BYTES;
 if(rc==RISC_RETAINED_WAKE_MISMATCH && c->desk_supported) {
  uint8_t bytes[PORTABLE_DESK_CLOCK_RECORD_BYTES];uint32_t used=0,cause=UINT32_MAX;
  int result=
#ifdef WATCH_SHARED_HOME_BOOT
   portable_home_boot_read_bytes(extended,PORTABLE_DESK_CLOCK_RECORD_TYPE,
#else
   extended->read_bytes(c->wake->context,PORTABLE_DESK_CLOCK_RECORD_TYPE,
#endif
      PORTABLE_DESK_CLOCK_RECORD_SCHEMA,bytes,sizeof(bytes),&used,&cause);
  if(result!=RISC_RETAINED_WAKE_OK && result!=RISC_RETAINED_WAKE_ABSENT &&
     result!=RISC_RETAINED_WAKE_MISMATCH && result!=RISC_RETAINED_WAKE_INVALID) {
   watch_runtime_halt(c);return false;
  }
  if(result!=RISC_RETAINED_WAKE_INVALID) {
   if(cause>RISC_BOOT_DEEP_OTHER)return false;
   c->cause=cause;
   c->desk_resume=result==RISC_RETAINED_WAKE_OK && cause==RISC_BOOT_DEEP_TIMER &&
       portable_desk_decode(bytes,used,&c->desk_record);
  }
 }
#endif
 return true;
}
#ifdef WATCH_SHARED_HOME_RETAINED
/* 1 is staged; 0 is explicit absent/clean refusal; -2 is retained custody.
 * The caller stages only before preparation. This does not alter or migrate
 * the existing one-byte Watch checkpoint used by ordinary Hybrid sleep. */
static inline int watch_runtime_stage_desk(watch_clock_runtime *c,const portable_desk_record *record) {
 if(c->halted)return -2;
 if(!c->wake || !c->desk_supported)return 0;
 const risc_retained_wake_api_v1_extended *extended=risc_retained_wake_extended(c->wake);
 uint8_t bytes[PORTABLE_DESK_CLOCK_RECORD_BYTES];
 if(!extended || extended->max_payload_bytes<sizeof(bytes) ||
    !portable_desk_encode(record,bytes,sizeof(bytes)))return 0;
 int rc=extended->stage_bytes(c->wake->context,PORTABLE_DESK_CLOCK_RECORD_TYPE,
     PORTABLE_DESK_CLOCK_RECORD_SCHEMA,bytes,sizeof(bytes));
 if(rc==RISC_RETAINED_WAKE_OK)return 1;
 if(rc==RISC_RETAINED_WAKE_INVALID)return 0;
 watch_runtime_halt(c);return -2;
}
#endif
static bool watch_runtime_seed(watch_clock_runtime *c,const twatch_rtc_api_v1 *rtc) {
 c->rtc_reconcile_pending=true;
 risc_realtime_snapshot_v1 sample;
 if(!watch_runtime_snapshot(c,&sample))return false;
 /* A valid retained native sample is an interpolation source, not an
  * accuracy check against the external RTC. Reconcile whenever the caller
  * supplies that RTC, including deep/light wake. Never write the RTC. */
 twatch_rtc_time_v1 date={0};int64_t epoch=0;
 if(!rtc || !rtc->read)return true; /* Explicit TIME UNSET remains visible. */
 if(!rtc->read(rtc->context,&date)) {
  /* Fresh battery-backed RTCs can have an oscillator-stop flag or invalid
   * calendar. Keep TIME UNSET explicit; never invent compile time or gate the
   * first display frame on a clock the user has not set yet. */
  if(sample.validity!=RISC_REALTIME_UNSET)return false;
  c->runtime->diagnostic("WATCH_CLOCK rtc=unreadable-or-unset time=unset");
  return true;
 }
#ifdef WATCH_SHARED_HOME_CIVIL
 if(!watch_runtime_interpret(c,&date,&epoch))return false;
 if(epoch>INT32_MAX){c->fallback=rtc;c->rtc_reconcile_pending=false;return true;}
#else
 if(!watch_runtime_epoch(&date,&epoch)) {
  /* Runtime v1 has a signed-32-bit epoch. Preserve the accepted Watch's
   * wider 2000..2099 civil domain through its original RTC outside that range. */
  if(tw_valid_time(&date)){c->fallback=rtc;c->rtc_reconcile_pending=false;return true;}
  return sample.validity==RISC_REALTIME_UNSET;
 }
#endif
 int rc=c->time->seed(c->time->context,epoch,0);
 if(rc==RISC_REALTIME_CONTEXT)watch_runtime_halt(c);
 if(rc==RISC_REALTIME_OK){c->fallback=NULL;c->rtc_reconcile_pending=false;}
 return rc==RISC_REALTIME_OK;
}
static bool watch_runtime_read(watch_clock_runtime *c,twatch_rtc_time_v1 *out,uint16_t *fraction_ms) {
 if(c->halted)return false;
 if(c->fallback) {
  twatch_rtc_time_v1 date={0};
  if(!c->fallback->read(c->fallback->context,&date) || !tw_valid_time(&date))return false;
  *out=date;*fraction_ms=0;return true;
 }
 risc_realtime_snapshot_v1 sample;
 if(!watch_runtime_snapshot(c,&sample) || sample.validity!=RISC_REALTIME_VALID ||
    !watch_runtime_calendar(sample.epoch_seconds,out))return false;
 *fraction_ms=(uint16_t)(sample.nanoseconds/1000000u);return true;
}
static bool watch_runtime_stage(watch_clock_runtime *c) {
 if(c->halted)return false;
 if(!c->wake)return true;
 const risc_retained_wake_record_v1 record={.struct_size=sizeof(record),
  .type=WATCH_CLOCK_CHECKPOINT_TYPE,.schema_version=WATCH_CLOCK_CHECKPOINT_SCHEMA,.size=1,.payload={1}};
 int rc=c->wake->stage(c->wake->context,&record);
 if(rc==RISC_RETAINED_WAKE_CONTEXT)watch_runtime_halt(c);
 return rc==RISC_RETAINED_WAKE_OK;
}
static bool watch_runtime_abandon(watch_clock_runtime *c) {
 if(c->halted)return false;
 if(!c->wake)return true;
 int rc=c->wake->clear(c->wake->context);
 if(rc==RISC_RETAINED_WAKE_CONTEXT)watch_runtime_halt(c);
 return rc==RISC_RETAINED_WAKE_OK;
}
static int watch_runtime_promote(watch_clock_runtime *c) {
 if(c->halted)return RISC_PROVIDER_PROMOTION_RETAINED;
 risc_runtime_capability_v1 grant={.struct_size=sizeof(grant)};
 if(!c->runtime->acquire(RISC_PROVIDER_PROMOTION_CAPABILITY,1,0,&grant)){watch_runtime_halt(c);return RISC_PROVIDER_PROMOTION_RETAINED;}
 const risc_provider_promotion_api_v1 *api=grant.api;
 int rc=api && api->api_version==1 && api->struct_size>=sizeof(*api) && api->promote?
  api->promote(api->context):RISC_PROVIDER_PROMOTION_FAILED;
 if(rc==RISC_PROVIDER_PROMOTION_RETAINED){watch_runtime_halt(c);return rc;}
 if(!c->runtime->release(&grant)){watch_runtime_halt(c);return RISC_PROVIDER_PROMOTION_RETAINED;}
 return rc;
}
