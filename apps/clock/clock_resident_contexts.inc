/* Side-effect-free applicability observations after an explicit, settled
 * readiness request. All choices describe semantic policy, never a device. */
#ifdef WATCH_SHARED_HOME_BOOT
#include "RiscResidentContextsV1.h"
#include "PortableHomePower.h"
static unsigned clock_contexts_power_modes(void) {
    portable_home_power state={.display=panel,.power=pmu};
    bool checkpoint=native_clock.wake&&native_clock.time&&!native_clock.halted;
    unsigned plain=portable_home_power_modes(&state,checkpoint);
    /* Optional motion is acquired by the existing sleep path, not by queries.
     * Until that choice is cached, promise only the intersection of both paths. */
    state.enrolled_wake_sources=true;
    return plain&portable_home_power_modes(&state,checkpoint);
}
static unsigned clock_contexts_output_modes(void) {
#ifdef WATCH_CLOCK_ALARMS
    return alarm_service_output_modes(clock_alarm.api);
#else
    return 0;
#endif
}
static bool clock_contexts_notifications(void) {
#ifdef WATCH_CLOCK_ALARMS
    return clock_alarm.api&&clock_quick.ui.dnd_valid;
#else
    return false;
#endif
}
static bool clock_contexts_option(unsigned field,unsigned value) {
    if(field==RISC_CONTEXTS_POWER){
        static const unsigned modes[]={PORTABLE_HOME_MODE_HYBRID,PORTABLE_HOME_MODE_LIGHT,PORTABLE_HOME_MODE_DEEP};
        return value<3&&(clock_contexts_power_modes()&modes[value]);
    }
    if(field==RISC_CONTEXTS_NOTIFY){
        if(value>3||!clock_contexts_notifications())return false;
        return !value||(clock_contexts_output_modes()&value)==value;
    }
    return false;
}
static bool clock_contexts_field(unsigned field) {
    switch(field){
    case RISC_CONTEXTS_FACE:return true; /* This is the native-faces controller. */
    case RISC_CONTEXTS_POWER:case RISC_CONTEXTS_SLEEP:return clock_contexts_power_modes()!=0;
    case RISC_CONTEXTS_DEEP:return (clock_contexts_power_modes()&PORTABLE_HOME_MODE_HYBRID)!=0;
#ifdef WATCH_QUICK_RADIOS
    case RISC_CONTEXTS_WIFI:return clock_quick.ui.wifi_available&&clock_quick.ui.wifi_valid;
    case RISC_CONTEXTS_BLUETOOTH:return clock_quick.ui.bluetooth_available&&clock_quick.ui.bluetooth_valid;
#endif
    case RISC_CONTEXTS_BRIGHTNESS:return display&&display->set_brightness&&clock_quick.ui.brightness_valid;
    case RISC_CONTEXTS_VOLUME:return clock_quick.ui.volume_valid&&(clock_contexts_output_modes()&ALARM_MODE_SOUND);
    case RISC_CONTEXTS_NOTIFY:case RISC_CONTEXTS_DND:return clock_contexts_notifications();
    default:return false;
    }
}
static bool clock_contexts_query_reason(uint32_t reason) {
    return reason>=UINT32_C(0x43530000)&&reason<UINT32_C(0x43530300);
}
static int32_t clock_contexts_query(const risc_resident_request_v1*q) {
    if(q->flags||q->reserved)return RISC_RESIDENT_INVALID;
    if(q->reason==RISC_CONTEXTS_SETTINGS_PREPARE){
        if(!clock_resident_preferences()||!clock_resident_alarm_ready()||!clock_resident_alive())
            return clock_resident_retain();
        return RISC_RESIDENT_OK;
    }
    uint32_t first=RISC_CONTEXTS_SETTING_QUERY(0);
    if(q->reason>=first&&q->reason<first+RISC_CONTEXTS_SETTING_COUNT)
        return clock_contexts_field(q->reason-first)?RISC_RESIDENT_OK:RISC_RESIDENT_BUSY;
    first=RISC_CONTEXTS_SETTING_OPTION_QUERY(0,0);
    if(q->reason>=first&&q->reason<first+RISC_CONTEXTS_SETTING_COUNT*16u){
        unsigned encoded=q->reason-first,field=encoded/16u,value=encoded%16u;
        if((field!=RISC_CONTEXTS_POWER&&field!=RISC_CONTEXTS_NOTIFY)||
           (field==RISC_CONTEXTS_POWER&&value>2)||(field==RISC_CONTEXTS_NOTIFY&&value>3))
            return RISC_RESIDENT_BUSY;
        return clock_contexts_option(field,value)?RISC_RESIDENT_OK:RISC_RESIDENT_BUSY;
    }
    return RISC_RESIDENT_BUSY;
}
#endif
