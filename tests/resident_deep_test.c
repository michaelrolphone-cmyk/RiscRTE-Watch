#define WATCH_RESIDENT_HOST
#include "apps/clock/watch_alarm_sleep.h"
#include <assert.h>
#include <stdio.h>
static unsigned panel_prepared,power_prepared,panel_restored,power_restored,pad_holds,deep_calls,light_calls,deadlines;
static bool pending,restore_failure;
static bool prepare_panel(void*c){(void)c;panel_prepared++;return true;}
static bool prepare_power(void*c){(void)c;power_prepared++;return true;}
static bool restore_panel(void*c){(void)c;panel_restored++;return !restore_failure;}
static bool restore_power(void*c){(void)c;power_restored++;return true;}
static int32_t restore_status(void*c){return restore_panel(c)?0:RISC_LIGHT_SLEEP_PLATFORM;}
static int32_t hold_panel(void*c){(void)c;pad_holds++;return 0;}
static bool wake_pending(void*c,bool*out){(void)c;*out=pending;return true;}
static int32_t deep(void*c){(void)c;deep_calls++;return RISC_DEEP_SLEEP_UNSUPPORTED;}
static int32_t deep_timed(void*c,uint32_t ms){assert(ms);return deep(c);}
static int32_t light(void*c,risc_light_sleep_result_v1*out){(void)c;light_calls++;out->wake_cause=RISC_LIGHT_SLEEP_WAKE_TIMER;return RISC_LIGHT_SLEEP_OK;}
static int32_t timed(void*c,uint32_t ms,risc_light_sleep_result_v1*out){assert(ms);return light(c,out);}
static bool diagnostic(const char*s){assert(s);return true;}
static int32_t deadline(void*c,alarm_sleep_v1*out){(void)c;deadlines++;out->rtc_seconds=100;out->deadline=10000;return ALARM_OK;}
static int32_t refresh(void*c){(void)c;return ALARM_OK;}
static const risc_display_power_api_v1 panel={.api_version=1,.struct_size=sizeof(panel),.prepare_sleep=prepare_panel,.resume=restore_panel,.prepare_deep_sleep=hold_panel,.resume_status=restore_status};
static const risc_power_sleep_api_v1 power={.api_version=1,.struct_size=sizeof(power),.prepare_sleep=prepare_power,.resume=restore_power,.light_sleep=light,.light_sleep_for=timed,.sleep_wake_pending=wake_pending,.deep_sleep=deep,.deep_sleep_for=deep_timed};
static const alarm_service_v1 alarm={.api_version=1,.struct_size=sizeof(alarm),.prepare_sleep=deadline,.refresh=refresh};
static void reset(void){panel_prepared=power_prepared=panel_restored=power_restored=pad_holds=deep_calls=light_calls=deadlines=0;watch_sleep_deep_deferred=false;pending=restore_failure=false;}
int main(void){
 for(unsigned with_alarm=0;with_alarm<2;with_alarm++)for(unsigned mode=PORTABLE_SLEEP_DEEP;mode<=PORTABLE_SLEEP_HYBRID;mode++){
  reset();watch_sleep_defer_deep=true;
  int rc=with_alarm?watch_alarm_sleep_prepared(&panel,&power,mode,&alarm,diagnostic):watch_sleep_prepared(&panel,&power,mode,diagnostic);
  assert(rc==WATCH_SLEEP_DEFERRED&&watch_sleep_deep_deferred&&!pad_holds&&!deep_calls&&panel_restored==1&&power_restored==1);
  if(mode==PORTABLE_SLEEP_HYBRID)assert(light_calls==1);
  unsigned before=deadlines;watch_sleep_defer_deep=false;
  rc=with_alarm?watch_alarm_sleep_prepared(&panel,&power,PORTABLE_SLEEP_DEEP,&alarm,diagnostic):watch_sleep_prepared(&panel,&power,PORTABLE_SLEEP_DEEP,diagnostic);
  assert(rc==WATCH_SLEEP_REFUSED&&pad_holds==1&&deep_calls==1&&panel_restored==2&&power_restored==2);
  if(with_alarm)assert(deadlines>before);
  watch_sleep_deep_deferred=true; /* simulate prior invocation state */
  rc=with_alarm?watch_alarm_sleep_prepared(&panel,&power,PORTABLE_SLEEP_LIGHT,&alarm,diagnostic):watch_sleep_prepared(&panel,&power,PORTABLE_SLEEP_LIGHT,diagnostic);
  assert(rc==WATCH_SLEEP_WOKE&&!watch_sleep_deep_deferred);
 }
 reset();watch_sleep_defer_deep=true;pending=true;
 assert(watch_alarm_sleep_prepared(&panel,&power,PORTABLE_SLEEP_HYBRID,&alarm,diagnostic)==WATCH_SLEEP_WOKE&&!watch_sleep_deep_deferred&&!pad_holds);
 reset();watch_sleep_defer_deep=true;restore_failure=true;
 assert(watch_alarm_sleep_prepared(&panel,&power,PORTABLE_SLEEP_DEEP,&alarm,diagnostic)==WATCH_SLEEP_FAILED&&watch_sleep_deep_deferred&&!pad_holds);
 puts("Resident Deep boundary: direct/Hybrid, alarm/no-alarm, fresh deadline, pending wake and restore failure PASS");
}
