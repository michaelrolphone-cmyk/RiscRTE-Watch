#ifdef WATCH_AUTOMATIC_PROVISION
#include "PortableAutomaticProvision.h"
static portable_automatic_provision clock_provision;
static bool clock_provision_safe(void*p){(void)p;uint32_t now;return !native_clock.halted&&!clock_contexts_uncertain&&!clock_broadcast_uncertain&&alive(&now);}
static bool clock_provision_network(void*p){(void)p;return clock_radios.valid&&(clock_radios.flags&PORTABLE_RADIO_WIFI)&&!(clock_radios.flags&PORTABLE_RADIO_AIRPLANE);}
static bool clock_provision_prepare(void*p){(void)p;
 if(!clock_contexts_pause()||!clock_broadcast_pause())return false;
 clock_provision_busy=true;clock_contexts_started=clock_broadcast_started=false;return true;
}
static void clock_provision_resume(void){
 if(!clock_provision_busy)return;
 clock_provision_busy=false;
 if(clock_deferred_phase==4)clock_deferred_phase=1;
}
static bool clock_provision_utc(void*p,uint64_t*out){(void)p;risc_realtime_snapshot_v1 time;
 if(!watch_runtime_snapshot(&native_clock,&time))return false;
 /* A strictly validated UNSET snapshot is the controller's private zero token,
  * not a clock seed. A fresh unset RTC deliberately leaves reconciliation due. */
 if(time.validity==RISC_REALTIME_UNSET){*out=0;return true;}
 if(native_clock.rtc_reconcile_pending||!time.epoch_seconds)return false;
 *out=(uint64_t)time.epoch_seconds;return true;
}
static void clock_provision_retain(void*p){(void)p;watch_runtime_halt(&native_clock);}
static void clock_provision_init(void){
 portable_automatic_hooks hooks={.runtime=rt,.network_allowed=clock_provision_network,.safe=clock_provision_safe,.prepare=clock_provision_prepare,
 .utc=clock_provision_utc,.retain=clock_provision_retain,.profile="core",.wifi_instance=15,.settings_namespace=6};
#ifdef WATCH_PROVISION_VOLUME
 hooks.sd_enabled=true;hooks.volume_instance=WATCH_PROVISION_VOLUME;
#endif
 portable_automatic_init(&clock_provision,&hooks);clock_provision_initialized=true;
}
static bool clock_provision_close(void){
 if(!clock_provision_initialized)return true;
 for(;;){int rc=portable_automatic_close(&clock_provision);if(rc==INSTALLED_APPS_OK){clock_provision_resume();return true;}
  if(rc!=INSTALLED_APPS_WORKING||!clock_provision_safe(NULL))return false;
  rt->yield_ms(5);
 }
}
static bool clock_provision_step(uint32_t now){
 if(clock_deferred_phase<4||held||!clock_display_settled||clock_alarm_modal||picker.open||pqa_visible(&clock_quick.ui)||launcher_swipe_pending||clock_handoff||clock_contexts_handoff)return true;
 int rc=portable_automatic_step(&clock_provision,now);
 if(!clock_provision.phase&&!clock_provision.closing)clock_provision_resume();
 return rc!=INSTALLED_APPS_RETAINED;
}
#endif
