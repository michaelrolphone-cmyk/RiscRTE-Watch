#include "PortableResidentSettingsEffects.h"
static portable_resident_settings_effects clock_effects;
static bool clock_effect_alive(void*c){(void)c;return clock_resident_alive();}
static void clock_effect_retain(void*c){(void)c;(void)clock_resident_retain();}
static void clock_effect_clear(void){
 prse_clear(&clock_effects);
 clock_effects.runtime=rt;clock_effects.display=display;
 clock_effects.alive=clock_effect_alive;clock_effects.retain=clock_effect_retain;
 clock_effects.supported=1u<<RISC_SETTINGS_BRIGHTNESS;
#ifdef WATCH_QUICK_RADIOS
 clock_effects.supported|=(1u<<RISC_SETTINGS_WIFI)|(1u<<RISC_SETTINGS_BLUETOOTH);
#endif
#ifdef WATCH_CLOCK_ALARMS
 clock_effects.alarm=clock_alarm.api;clock_effects.supported|=1u<<RISC_SETTINGS_ALARM;
#endif
}
static bool clock_effect_reason(uint32_t reason){return prse_reason(reason);}
static int32_t clock_effect_dispatch(const risc_resident_request_v1*q){
 int32_t rc=prse_dispatch(&clock_effects,q);
 if(rc==RISC_RESIDENT_OK&&q->reason==RISC_SETTINGS_APPLY(RISC_SETTINGS_BRIGHTNESS)&&clock_effects.result==RISC_SETTINGS_APPLIED)clock_quick.brightness=clock_effects.brightness;
 return rc;
}
