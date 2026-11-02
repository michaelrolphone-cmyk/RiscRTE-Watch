#pragma once
/* Physical-only adapter over existing provider-owned capabilities.
 * No app checkpoint, alarm, time zone, display geometry, device identity,
 * delay, acquisition, storage policy, or schedule lives in this layer.
 * The app reconciles deadlines after prepare() and before hold(). */
#include "RiscDisplayPowerV1.h"
#include "RiscPowerSleepV1.h"
#include "RiscTimedSleepV1.h"
typedef struct {
 const risc_display_power_api_v1 *display;
 const risc_power_sleep_api_v1 *power;
 bool enrolled_wake_sources;
 bool display_touched,power_touched,held,retained;
} portable_home_power;
enum { PORTABLE_HOME_POWER_REFUSED=0,PORTABLE_HOME_POWER_READY=1,PORTABLE_HOME_POWER_RETAINED=-2 };
static inline int portable_home_power_fence(portable_home_power *s) {
 s->retained=true;return PORTABLE_HOME_POWER_RETAINED;
}
/* Pure availability observation. A short/malformed/absent suffix is not a
 * timed sleep implementation and never falls back to unbounded entry. */
static inline bool portable_home_power_supported(const portable_home_power *s) {
 if(!s||!s->display||!s->power)return false;
 const risc_display_power_api_v1 *d=s->display;
 const risc_power_sleep_api_v1 *p=s->power;
 return d->api_version==1&&d->struct_size>=RISC_DISPLAY_POWER_RESUME_STATUS_SIZE&&
  d->prepare_sleep&&d->resume_status&&d->prepare_deep_sleep&&
  p->api_version==1&&p->struct_size>=RISC_POWER_TIMED_DEEP_SLEEP_SIZE&&
  p->prepare_sleep&&p->resume&&p->sleep_wake_pending&&
  (s->enrolled_wake_sources?(p->struct_size>=RISC_POWER_WAKE_SET_SIZE&&p->deep_sleep_set):p->deep_sleep_for!=NULL);
}
enum { PORTABLE_HOME_MODE_LIGHT=1u,PORTABLE_HOME_MODE_DEEP=2u,PORTABLE_HOME_MODE_HYBRID=4u };
/* Cached applicability only: no provider call, acquisition, preference read or
 * mutation. Checkpoint support is app/native admission, not hardware identity. */
static inline unsigned portable_home_power_modes(const portable_home_power*s,bool checkpoint_supported) {
 if(!s||s->retained||!s->display||!s->power)return 0;
 const risc_display_power_api_v1*d=s->display;const risc_power_sleep_api_v1*p=s->power;
 if(d->api_version!=1||d->struct_size<RISC_DISPLAY_POWER_RESUME_STATUS_SIZE||
    !d->prepare_sleep||!d->resume_status||p->api_version!=1||
    p->struct_size<RISC_POWER_TIMED_SLEEP_SIZE||!p->prepare_sleep||!p->resume||!p->sleep_wake_pending)return 0;
 unsigned modes=0;
 if(s->enrolled_wake_sources?(p->struct_size>=RISC_POWER_WAKE_SET_SIZE&&p->light_sleep_set!=NULL):p->light_sleep_for!=NULL)
  modes|=PORTABLE_HOME_MODE_LIGHT;
 if(checkpoint_supported&&portable_home_power_supported(s))modes|=PORTABLE_HOME_MODE_DEEP;
 if((modes&(PORTABLE_HOME_MODE_LIGHT|PORTABLE_HOME_MODE_DEEP))==(PORTABLE_HOME_MODE_LIGHT|PORTABLE_HOME_MODE_DEEP))
  modes|=PORTABLE_HOME_MODE_HYBRID;
 return modes;
}
static inline int portable_home_power_restore(portable_home_power *s) {
 if(!s||s->retained)return PORTABLE_HOME_POWER_RETAINED;
 if(s->power_touched) {
  if(!s->power->resume(s->power->context))return portable_home_power_fence(s);
  s->power_touched=false;
 }
 if(s->display_touched) {
  if(s->display->resume_status(s->display->context)!=0)return portable_home_power_fence(s);
  s->display_touched=false;s->held=false;
 }
 return PORTABLE_HOME_POWER_READY;
}
static inline int portable_home_power_refuse(portable_home_power *s) {
 return portable_home_power_restore(s)==PORTABLE_HOME_POWER_READY?PORTABLE_HOME_POWER_REFUSED:PORTABLE_HOME_POWER_RETAINED;
}
/* Provider owns settling (including any panel delay); no deep pad hold is
 * requested here. Every attempted preparation, including false, is paired
 * with checked restoration. Ordinary refusal is already rolled back. */
static inline int portable_home_power_prepare(portable_home_power *s) {
 if(!s||s->retained)return PORTABLE_HOME_POWER_RETAINED;
 if(s->display_touched||s->power_touched||s->held)return portable_home_power_fence(s);
 if(!portable_home_power_supported(s))return PORTABLE_HOME_POWER_REFUSED;
 s->display_touched=true;
 if(!s->display->prepare_sleep(s->display->context))return portable_home_power_refuse(s);
 s->power_touched=true;
 if(!s->power->prepare_sleep(s->power->context))return portable_home_power_refuse(s);
 return PORTABLE_HOME_POWER_READY;
}
/* Call only after app-owned alarm reconciliation and record staging. After
 * this boundary do not call services, storage, yield, acquire, or release. */
static inline int portable_home_power_hold(portable_home_power *s) {
 if(!s||s->retained)return PORTABLE_HOME_POWER_RETAINED;
 if(!s->display_touched||!s->power_touched||s->held)return portable_home_power_fence(s);
 int32_t rc=s->display->prepare_deep_sleep(s->display->context);
 if(rc==0){s->held=true;return PORTABLE_HOME_POWER_READY;}
 if(rc==RISC_DEEP_SLEEP_RETAINED||rc>0||rc<RISC_DEEP_SLEEP_UNSUPPORTED)return portable_home_power_fence(s);
 return portable_home_power_refuse(s);
}
/* Purely supplied duration: the application subtracts elapsed processing and
 * takes min(minute,alarm) before calling. Success cannot return. A clean
 * refusal still has holds: caller must restore before clearing any record. */
static inline int portable_home_power_enter(portable_home_power *s,uint32_t duration_ms) {
 if(!s||s->retained)return PORTABLE_HOME_POWER_RETAINED;
 if(!s->held||!s->power_touched)return portable_home_power_fence(s);
 if(!duration_ms||duration_ms>RISC_TIMED_SLEEP_MAX_MS)return PORTABLE_HOME_POWER_REFUSED;
 bool pending=true;
 if(!s->power->sleep_wake_pending(s->power->context,&pending))return portable_home_power_fence(s);
 if(pending)return PORTABLE_HOME_POWER_REFUSED;
 int32_t rc=s->enrolled_wake_sources?
  s->power->deep_sleep_set(s->power->context,duration_ms):
  s->power->deep_sleep_for(s->power->context,duration_ms);
 if(rc>=0||rc==RISC_DEEP_SLEEP_RETAINED||rc<RISC_DEEP_SLEEP_UNSUPPORTED)return portable_home_power_fence(s);
 return PORTABLE_HOME_POWER_REFUSED;
}
