#if defined(WATCH_NATIVE_ALARM_V2) && (!defined(WATCH_NATIVE_TIME_AUTHORITY) || !defined(WATCH_RUNTIME_FEATURES) || !defined(WATCH_CLOCK_ALARMS) || !defined(ALARM_SERVICE_TAGGED_V2))
#error Native tagged alarms require native Home time, Runtime custody and API2
#endif
#if defined(WATCH_NATIVE_TIME_AUTHORITY) && !defined(WATCH_RUNTIME_FEATURES)
#error Native time authority requires Runtime time services
#endif
#ifdef WATCH_ATOMIC_PREFERENCES
#define PORTABLE_HOME_AUTOMATIC_PREFERENCES
#endif
#if defined(WATCH_PAIRED_BOOT_CONFIRM) && !defined(WATCH_CLOCK_RETURN)
#include "runtime_boot_confirm.h"
#endif
#include "RiscRuntimeV1.h"
#include "RiscRuntimeClockV1.h"
#include "RiscRuntimeCheckpointV1.h"
#ifdef WATCH_SERIAL_TRANSFER_MODE
#include "RiscRuntimeHostControlV1.h"
#endif
#include "WatchRuntimeSleep.h"
#include "WatchViewport.h"
static watch_viewport clock_viewport;
#include "nova/nova.h"
#include "display_time.h"
#include "effects/effects.h"
#include "WatchCapabilityClients.h"
#include "twatch_caps.h"
#include "watch_sleep.h"
#include "WatchSleepDefaults.h"
#include "transitions/PortableTransition.h"
#include <stdlib.h>
#include <string.h>
static char sleep_status[32];
#ifdef WATCH_CRASH_REPORT
static bool clock_crash_modal;
#endif
#ifdef WATCH_AUTOMATIC_PROVISION
static bool clock_provision_initialized,clock_provision_busy;
static bool clock_provision_close(void);
#endif
#ifdef WATCH_FAST_WAKE
/* Selected only by the separately qualified clock-first product build. */
static unsigned clock_deferred_phase;
static bool clock_confirm_waiting,clock_confirm_deadline_reported;
static uint32_t clock_confirm_wait_started;
static bool clock_radios_pending,clock_input_reported;
static bool clock_broadcast_started,clock_contexts_started;
static bool clock_boot_frame_pending;
static uint32_t clock_wake_started;
static void clock_wake_mark(const char *stage);
static void clock_wake_visible(void);
#define CLOCK_WAKE_MARK(stage) (clock_wake_mark(stage),!clock_runtime_unavailable)
#else
#define CLOCK_WAKE_MARK(stage) (true)
#endif

static const risc_runtime_api_v1 *rt;
/* Invocation-local custody fence; terminal touch faults forbid all later I/O. */
static bool clock_touch_retained;
/* Current clock callback refusal means active Runtime authority is unavailable.
 * This is not a claim of touch/provider retention and must not request retain. */
static bool clock_runtime_unavailable;
#ifdef WATCH_PERFORMANCE_TRACE
#include "performance_trace.h"
static bool clock_interaction_safe(void);
static watch_home_performance clock_perf;
#define CLOCK_PERF_EVENT(phase,value) whp_emit(rt,clock_interaction_safe,phase,value)
#define CLOCK_PERF_SAMPLE(valid,down) whp_sample(&clock_perf,rt,clock_interaction_safe,valid,down)
#define CLOCK_PERF_CHECKPOINT() whp_count(&clock_perf.checkpoints)
#define CLOCK_PERF_BEGIN() whp_frame_begin(&clock_perf,rt,clock_interaction_safe)
#define CLOCK_PERF_COMPLETE() whp_complete(&clock_perf,rt,clock_interaction_safe)
#else
#define CLOCK_PERF_EVENT(phase,value) ((void)0)
#define CLOCK_PERF_SAMPLE(valid,down) ((void)0)
#define CLOCK_PERF_CHECKPOINT() ((void)0)
#define CLOCK_PERF_BEGIN() ((void)0)
#define CLOCK_PERF_COMPLETE() ((void)0)
#endif
/* Settled checkpoint admission, shared by all Home handoff paths. Runtime's
 * request_launch remains authoritative if a service arrives after this sample. */
static bool clock_navigation_ready;
#ifdef WATCH_CLOCK_LAUNCHER
static bool clock_navigation_allowed(void) {
    if(clock_navigation_ready)return true;
    if(strcmp(sleep_status,"SERVICE BUSY - RETRY")) {
        memcpy(sleep_status,"SERVICE BUSY - RETRY",sizeof("SERVICE BUSY - RETRY"));
        rt->diagnostic("WATCH_CLOCK navigation=busy action=retry");
    }
    return false;
}
#endif
#ifdef WATCH_RUNTIME_FEATURES
#include "../runtime_features/clock_runtime.h"
static watch_clock_runtime native_clock;
#ifdef WATCH_FAST_WAKE
static bool clock_rtc_reconcile_pending,clock_rtc_retry_delayed;
static uint32_t clock_rtc_retry_sampled_at;
#define CLOCK_RTC_RETRY_MS 5000u
#endif
#endif
#ifdef WATCH_CONTEXTS_CLIENT
#include "PortableContextsClient.h"
static portable_contexts_client clock_contexts;
static bool clock_contexts_uncertain,clock_contexts_handoff;
static char clock_context_message[33];
static bool clock_contexts_capture_checkpoint(void *context) {
    if(clock_touch_retained||clock_runtime_unavailable)return false;
    (void)context;
#ifdef WATCH_AUTOMATIC_PROVISION
    if(clock_provision_busy)return true;
#endif
    if(clock_contexts_uncertain)return false;
#ifdef WATCH_RUNTIME_FEATURES
    if(native_clock.halted)return false;
#endif
    if(portable_contexts_capture(&clock_contexts))return true;
    clock_contexts_uncertain=true;
    if(!portable_contexts_retain(&clock_contexts))for(;;)rt->yield_ms(50);
    return false;
}
static bool clock_contexts_pause(void) {
    if(clock_contexts_uncertain)return false;
    if(portable_contexts_pause(&clock_contexts))return true;
    clock_contexts_uncertain=true;
#ifdef WATCH_RUNTIME_FEATURES
    watch_runtime_retain(rt);
#endif
    return false;
}
#endif
#ifdef WATCH_BLE_BROADCAST
#include "PortableBroadcastClient.h"
static portable_broadcast_client clock_broadcast;
static bool clock_broadcast_uncertain,clock_handoff;
static bool clock_broadcast_step(void) {
    if(clock_broadcast_uncertain)return false;
#ifdef WATCH_FAST_WAKE
    /* Neither a retained client nor alarm/presentation pacing may restart BLE
     * before the saved radio policy has been reapplied after wake. */
    if(clock_radios_pending || !clock_broadcast_started)return true;
#endif
    if(!clock_broadcast.api)return true;
    if(!portable_broadcast_step(&clock_broadcast,true)) {
        clock_broadcast_uncertain=true;
#ifdef WATCH_RUNTIME_FEATURES
        watch_runtime_retain(rt);
#endif
        return false;
    }
    return true;
}
static bool clock_broadcast_pause(void) {
    if(clock_broadcast_uncertain)return false;
    if(!portable_broadcast_pause(&clock_broadcast)) {
        clock_broadcast_uncertain=true;
#ifdef WATCH_RUNTIME_FEATURES
        watch_runtime_retain(rt);
#endif
        return false;
    }
    return true;
}
#endif
#if defined(WATCH_CONTEXTS_CLIENT) || defined(WATCH_BLE_BROADCAST)
static bool clock_background_pause(void) {
#ifdef WATCH_AUTOMATIC_PROVISION
    if(!clock_provision_close())return false;
#endif
#ifdef WATCH_CONTEXTS_CLIENT
    if(!clock_contexts_pause())return false;
#endif
#ifdef WATCH_BLE_BROADCAST
    if(!clock_broadcast_pause())return false;
#endif
    return true;
}
#endif
#ifdef WATCH_CLOCK_ALARMS
#ifndef WATCH_CLOCK_LAUNCHER
#error Alarm Clock requires its normal touch launcher deployment
#endif
static bool clock_alarm_modal,clock_display_settled;
static bool clock_alarm_attempted;
#ifdef WATCH_FAST_WAKE
static bool clock_alarm_input_cancelled;
#endif
static bool clock_alarm_foreground(void);
static bool clock_alarm_failure(void);
#endif
#ifdef WATCH_CLOCK_LAUNCHER
#include "faces/picker.h"
#ifdef WATCH_QUICK_ACTIONS
#include "PortableQuickSession.h"
#include "PortableQuickRender.h"
static pqa_session clock_quick;
#ifdef WATCH_QUICK_RADIOS
#include "PortableQuickRadios.h"
static pqa_radios clock_radios;
#ifdef PORTABLE_LOW_BATTERY
#include "PortableLowBattery.h"
static portable_low_battery clock_low_battery;
static uint32_t clock_low_battery_at;
static bool clock_low_battery_sampled;
#endif
#endif
#endif
static void clock_touch_retain(void) {
    if(clock_touch_retained||clock_runtime_unavailable)return;
    clock_touch_retained=true;
#ifdef WATCH_RUNTIME_FEATURES
    watch_runtime_halt(&native_clock);
#else
#ifdef RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE
    if(rt && rt->struct_size>=RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE && rt->retain_invocation)
        (void)rt->retain_invocation();
#endif
#endif
}
#include "launcher_touch.h"
#include "PortableSleepPolicy.h"
#include "PortableTimeFormat.h"
static unsigned sleep_mode;
static watch_face_picker picker;
static nova_watch_picker_cache *picker_scratch;
static bool picker_open_pending,picker_select_pending,picker_save_failed;
static unsigned picker_selection_pending;
static watch_launcher_touch touch;
static bool launcher_swipe_pending, launcher_activity_pending;
static uint32_t launcher_sampled_at;
#ifdef WATCH_CONTEXTS_CLIENT
/* Bounded raster-only journal. No recognizer/UI state changes while pixels are
 * being written. Overflow cancels the complete journal until fresh neutral. */
#define CLOCK_RASTER_OBSERVATIONS 128u
static watch_launcher_observation clock_raster_observations[CLOCK_RASTER_OBSERVATIONS];
static unsigned clock_raster_count;
static bool clock_rendering,clock_raster_overflow,clock_raster_activity;
static bool clock_render_checkpoint(void *context);
#endif
static void launcher_touch_latch(unsigned action,bool activity) {
    if(action)CLOCK_PERF_EVENT(RISC_PERF_DISPATCHED,0x400u+action);
    if(action==WATCH_FACE_LAUNCHER)launcher_swipe_pending=true;
    if(action==WATCH_FACE_OPEN)picker_open_pending=true;
    if(action==WATCH_FACE_SELECT){picker_selection_pending=watch_face_page_for(picker.category)->ids[picker.target];picker_select_pending=true;}
    if(activity)launcher_activity_pending=true;
}
/* Sampling must not wait for a whole presentation. Latch the first
 * qualified swipe, but let the accepted frame finish before handing it off. */
static bool sample_launcher_touch(uint32_t now, bool force) {
    if(clock_touch_retained||clock_runtime_unavailable)return false;
#ifdef WATCH_CONTEXTS_CLIENT
    if(!clock_contexts_capture_checkpoint(NULL))return false;
#endif
#ifdef WATCH_CLOCK_ALARMS
    if(clock_alarm_modal)return true;
#endif
    if (!touch.subscription || launcher_swipe_pending ||
        (!force && (uint32_t)(now-launcher_sampled_at)<8u)) return true;
    bool activity=false;
    launcher_sampled_at=now;
    unsigned action=launcher_touch_sample(&touch,&picker,now,&activity);
    if(clock_touch_retained||clock_runtime_unavailable)return false;
    launcher_touch_latch(action,activity);
    return true;
}
#endif
static const risc_display_output_api_v1 *display;
static const twatch_rtc_api_v1 *rtc;
static watch_capability_clients device_clients;
static bool clock_key_read(uint32_t *,const char *);
static const risc_battery_gauge_api_v1 *battery_api;
static const risc_power_sleep_api_v1 *pmu;
static const risc_display_power_api_v1 *panel;
static risc_display_frame_v1 held;
static nova_watch_state face;
#ifdef WATCH_CLOCK_POINTS
#include "points_projection.h"
static points_config clock_points_config;
static points_meta clock_points_meta;
static nova_points_state clock_points_view;
static bool clock_points_available;
static uint32_t clock_points_second;
static bool clock_points_sampled;
static bool clock_points_load(void) {
    risc_runtime_capability_v1 grant={.struct_size=sizeof(grant)};
    clock_points_config=(points_config){0};clock_points_meta=(points_meta){0};clock_points_sampled=false;
    clock_points_view=(nova_points_state){.status=NOVA_POINTS_UNAVAILABLE};clock_points_available=false;
    if(!rt->acquire(RISC_KEY_VALUE_CAPABILITY,1,5,&grant))return true;
    const risc_key_value_v1 *kv=grant.api;
    if(kv&&kv->api_version==1&&kv->struct_size>=sizeof(*kv)&&kv->get) {
        uint8_t bytes[POINTS_RECORD_SIZE];uint32_t n=0;
        int32_t result=kv->get(kv->context,POINTS_CONFIG_KEY,bytes,sizeof(bytes),&n);
        clock_points_available=result==RISC_KEY_VALUE_NOT_FOUND||
            (result==RISC_KEY_VALUE_OK&&points_config_decode(&clock_points_config,bytes,n));
#ifdef POINTS_DEFAULTS_AVAILABLE
        /* A missing catalog exposes the shared virtual defaults. Never replace
         * a present empty, malformed or unavailable saved catalog. */
        bool defaults=result==RISC_KEY_VALUE_NOT_FOUND;
        if(defaults)clock_points_config=points_default_config();
#endif
#if WATCH_POINTS_EXTENDED
        n=0;result=kv->get(kv->context,POINTS_META_KEY,bytes,sizeof(bytes),&n);
#ifdef POINTS_DEFAULTS_AVAILABLE
        if(defaults&&result==RISC_KEY_VALUE_NOT_FOUND)clock_points_meta=points_default_meta();
#endif
        if(result==RISC_KEY_VALUE_OK&&!points_meta_decode(&clock_points_meta,bytes,n)) {
            clock_points_meta=(points_meta){0};
            rt->diagnostic("WATCH_CLOCK points-meta=invalid fallback=generic");
        } else if(result!=RISC_KEY_VALUE_OK&&result!=RISC_KEY_VALUE_NOT_FOUND) {
            clock_points_meta=(points_meta){0};
            rt->diagnostic("WATCH_CLOCK points-meta=unavailable fallback=generic");
        }
#endif
        clock_points_view.status=clock_points_available?NOVA_POINTS_EMPTY:NOVA_POINTS_ERROR;
    }
    return rt->release(&grant);
}
#endif
static uint32_t rtc_sampled_at, rtc_second_at, battery_sampled_at;
static bool sampled_rtc, sampled_battery;
static void reset_telemetry(void) {
    face=(nova_watch_state){0}; sampled_rtc=false; sampled_battery=false;
#ifdef WATCH_CONTEXTS_CLIENT
#ifdef WATCH_CLOCK_LAUNCHER
    face.capture_audio=clock_render_checkpoint;
#else
    face.capture_audio=clock_contexts_capture_checkpoint;
#endif
#endif
#ifdef WATCH_CLOCK_POINTS
    clock_points_sampled=false;face.points=&clock_points_view;
#endif
}
#ifdef WATCH_CLOCK_LAUNCHER
#ifdef WATCH_NATIVE_TIME_AUTHORITY
static void clock_timezone_load(const risc_key_value_v1 *kv) {
    int loaded=watch_runtime_load_zone(&native_clock,kv);
    if(!native_clock.display_zone_valid) {
        memcpy(sleep_status,"TIME ZONE ERR",sizeof("TIME ZONE ERR"));
        rt->diagnostic("WATCH_CLOCK time-zone=unconfirmed");
    } else {
        if(!strcmp(sleep_status,"TIME ZONE ERR"))sleep_status[0]=0;
        rt->diagnostic(loaded==PORTABLE_TIMEZONE_MISSING?"WATCH_CLOCK time-zone=UTC-default":"WATCH_CLOCK time-zone=saved");
    }
}
#endif
static bool reload_time_format(void) {
    unsigned mode=PORTABLE_TIME_FORMAT_12;
    risc_runtime_capability_v1 grant={.struct_size=sizeof(grant)};
    if(rt->acquire(RISC_KEY_VALUE_CAPABILITY,1,PORTABLE_TIME_FORMAT_STORE_INSTANCE,&grant)) {
        (void)portable_time_format_load(grant.api,&mode);
#ifdef WATCH_NATIVE_TIME_AUTHORITY
        clock_timezone_load(grant.api);
#endif
        if(!rt->release(&grant))return false;
    }
#ifdef WATCH_NATIVE_TIME_AUTHORITY
    else clock_timezone_load(NULL);
#endif
    face.hour_24=mode==PORTABLE_TIME_FORMAT_24;
    return true;
}
#endif
static bool same_second(const twatch_rtc_time_v1 *a,const twatch_rtc_time_v1 *b) {
    return a->year==b->year && a->month==b->month && a->day==b->day &&
        a->hour==b->hour && a->minute==b->minute && a->second==b->second;
}
static bool alive(uint32_t *ms) {
    if(clock_touch_retained||clock_runtime_unavailable)return false;
    if(risc_runtime_monotonic_ms_v1(rt,ms))return true;
#ifdef RISC_RUNTIME_MONOTONIC_V1_SIZE
    /* Preserve this caller's existing unavailable-authority latch. The shared
     * reader never retries health after a present clock callback fails. */
    if(rt->struct_size>=RISC_RUNTIME_MONOTONIC_V1_SIZE && rt->monotonic_ms)
        clock_runtime_unavailable=true;
#endif
    return false;
}
#if defined(WATCH_CONTEXTS_CLIENT) && defined(WATCH_CLOCK_LAUNCHER)
/* Keep both endpoints of an unchanged input run. Native high-detail faces may
 * span many capture intervals; redundant neutral/held snapshots must not fill
 * the journal and discard a complete gesture already observed in that frame. */
static bool clock_same_observation(const watch_launcher_observation *a,const watch_launcher_observation *b) {
    return a->valid==b->valid && a->count==b->count &&
        (!a->count || (a->id==b->id && a->x==b->x && a->y==b->y));
}
static bool clock_render_checkpoint(void *context) {
    if(clock_runtime_unavailable)return false;
    CLOCK_PERF_CHECKPOINT();
    if(!clock_contexts_capture_checkpoint(context))return false;
    if(!clock_rendering||!touch.subscription||launcher_swipe_pending)return true;
#ifdef WATCH_CLOCK_ALARMS
    if(clock_alarm_modal)return true;
#endif
    uint32_t now;
    if(!alive(&now))return false;
    if((uint32_t)(now-launcher_sampled_at)<8u)return true;
    launcher_sampled_at=now;
    watch_launcher_observation o=launcher_touch_capture(&touch,now);
    if(clock_touch_retained||clock_runtime_unavailable)return false;
    clock_raster_activity|=o.activity;
    if(clock_raster_count>=2 &&
       clock_same_observation(&clock_raster_observations[clock_raster_count-1],&o) &&
       clock_same_observation(&clock_raster_observations[clock_raster_count-2],&o))
        clock_raster_observations[clock_raster_count-1]=o;
    else if(clock_raster_count<CLOCK_RASTER_OBSERVATIONS)
        clock_raster_observations[clock_raster_count++]=o;
    else clock_raster_overflow=true;
    return true;
}
static void clock_replay_raster_input(void) {
    if(clock_raster_overflow) {
        watch_launcher_observation cancelled={.now=launcher_sampled_at};
        (void)launcher_touch_observe(&picker,&cancelled);
    } else for(unsigned i=0;i<clock_raster_count&&!launcher_swipe_pending;i++) {
        const watch_launcher_observation *o=&clock_raster_observations[i];
        launcher_touch_latch(launcher_touch_observe(&picker,o),o->activity);
    }
    launcher_activity_pending|=clock_raster_activity;
    clock_raster_count=0;clock_raster_overflow=false;clock_raster_activity=false;
}
#endif
static bool present(void) {
    if(clock_runtime_unavailable)return false;
#ifdef WATCH_CONTEXTS_CLIENT
    if(!clock_contexts_capture_checkpoint(NULL))return false;
#endif
    risc_display_present_token_v1 token=0;
    uint32_t start,now;
    if (!alive(&start)) return false;
    bool healthy=true;
    const risc_display_present_options_v1 opts={0,0,0};
#ifdef WATCH_CLOCK_ALARMS
    clock_display_settled=false;
#endif
    CLOCK_PERF_EVENT(RISC_PERF_PRESENT_SUBMIT,0x400);
    if (!display->submit(display->context,held,NULL,0,&opts,&token)) return false;
    held=0;
    for(unsigned n=0;n<=10000;n++) {
#ifdef WATCH_CONTEXTS_CLIENT
        if(!clock_contexts_capture_checkpoint(NULL))return false;
#endif
        risc_display_present_status_v1 s={0};
        if (!display->present_status(display->context,token,&s)) return false;
        if (s.state==RISC_DISPLAY_PRESENT_COMPLETE) {
#ifdef WATCH_CLOCK_ALARMS
            clock_display_settled=true;
#endif
            CLOCK_PERF_COMPLETE();
            return healthy;
        }
        if (s.state==RISC_DISPLAY_PRESENT_FAILED || s.state==RISC_DISPLAY_PRESENT_SUPERSEDED) return false;
        if (alive(&now)) {
            if ((uint32_t)(now-start)>=10000) return false;
#ifdef WATCH_CLOCK_LAUNCHER
#ifdef WATCH_CRASH_REPORT
            if(!clock_crash_modal)
#endif
            if(!sample_launcher_touch(now,false))return false;
            if(clock_touch_retained||clock_runtime_unavailable)return false;
#ifdef WATCH_CONTEXTS_CLIENT
            if(clock_contexts_uncertain)return false;
#endif
#endif
        } else {if(clock_runtime_unavailable)return false;healthy=false;}
        /* A lost health sample must not abandon an already accepted transfer.
         * Drain it within the same bounded poll count, then exit the app. */
        rt->yield_ms(1);
    }
    return false;
}
static bool frame(risc_display_surface_v1 *surface) {
    if(clock_touch_retained||clock_runtime_unavailable)return false;
#ifdef WATCH_CONTEXTS_CLIENT
    if(!clock_contexts_capture_checkpoint(NULL))return false;
#endif
    if (!display->acquire(display->context,RISC_DISPLAY_FORMAT_RGB565,surface)) return false;
    held=surface->frame;return held!=0;
}
static bool draw_clock_pixels(uint32_t now,risc_display_surface_v1 *surface) {
    /* Clock sampling and fractional phase are independent of frame cadence.
     * Only a successful authoritative sample may advance civil time. The first observed
     * edge anchors the fractional hand within one 100ms sampling interval. */
    if (!sampled_rtc || (uint32_t)(now-rtc_sampled_at)>=100u) {
        twatch_rtc_time_v1 date={0};
        #ifdef WATCH_RUNTIME_FEATURES
        uint16_t native_fraction=0;
        bool valid=watch_runtime_read(&native_clock,&date,&native_fraction);
        if(native_clock.halted)return false;
#else
        bool valid=rtc && rtc->read(rtc->context,&date);
#endif
#ifdef WATCH_CLOCK_POINTS
        uint32_t raw_seconds=0;
        bool raw_valid=valid&&alarm_calendar_seconds(date.year,date.month,date.day,date.hour,date.minute,date.second,&raw_seconds);
        if(!raw_valid)clock_points_view.status=NOVA_POINTS_ERROR;
        else if(clock_points_available&&(!clock_points_sampled||clock_points_second!=raw_seconds||clock_points_view.status==NOVA_POINTS_ERROR)) {
            (void)watch_points_projection(&clock_points_config,&clock_points_meta,raw_seconds,&clock_points_view);
            clock_points_second=raw_seconds;clock_points_sampled=true;
        }
        face.points=&clock_points_view;
#endif
#ifndef WATCH_NATIVE_TIME_AUTHORITY
        valid=valid&&watch_display_time(&date,&date);
#endif
        #ifdef WATCH_RUNTIME_FEATURES
        if(valid && !native_clock.fallback)rtc_second_at=now-native_fraction;
        else if(valid && (!face.time_valid || !same_second(&date,&face.time)))rtc_second_at=now;
#else
        if (valid && (!face.time_valid || !same_second(&date,&face.time))) rtc_second_at=now;
#endif
        face.time=date; face.time_valid=valid;
        rtc_sampled_at=now; sampled_rtc=true;
    }
    if (!sampled_battery || (uint32_t)(now-battery_sampled_at)>=5000u) {
        risc_battery_sample_v1 sample={0};
        face.battery_valid=battery_api && battery_api->read && battery_api->read(battery_api->context,&sample) &&
            sample.percent<=100 && !(sample.flags&RISC_BATTERY_PROFILE_MISSING);
        face.battery_percent=sample.percent;
        battery_sampled_at=now; sampled_battery=true;
    }
    uint32_t phase=now-rtc_second_at;
    face.subsecond_ms=face.time_valid ? (phase>999u?999u:(uint16_t)phase) : 0;
    face.animation_ms=now;
    #ifdef WATCH_CLOCK_LAUNCHER
    if(picker.open&&picker_scratch) {
        watch_face_animate(&picker,now);
        picker.category_positions[picker.category]=picker.position;
        return nova_watch_picker_collections_render(surface,&face,picker.selected,picker.category_position,picker.category_positions,picker_save_failed?"SAVE FAILED":NULL,picker.pulse_face,picker.pulse_active?nova_watch_picker_pulse(now-picker.pulse_started):256u,picker_scratch);
    }
#ifdef WATCH_CONTEXTS_CLIENT
    if(clock_context_message[0])return nova_watch_context_message(surface,clock_context_message);
#endif
    bool rendered=nova_watch_face_render(surface,&face,picker.selected);
    if(clock_touch_retained||clock_runtime_unavailable)return false;
#ifdef WATCH_QUICK_ACTIONS
#if defined(PORTABLE_QUICK_CAPABILITY_SAFE) && defined(WATCH_CONTEXTS_CLIENT)
    clock_quick.ui.contexts_available=clock_contexts.api&&!clock_contexts_uncertain;
#endif
    pqa_animate(&clock_quick.ui,now);
    if(rendered && pqa_visible(&clock_quick.ui)) {
        nova_watch_labels labels;nova_watch_format(&face,&labels);
        rendered=pqa_render(surface,&clock_quick.ui,labels.hour_minute,face.battery_valid,face.battery_percent);
    }
#endif
    if(rendered && sleep_status[0])rendered=nova_watch_sleep_status(surface,sleep_status);
    return rendered;
#else
    return nova_watch_render(surface,&face);
#endif
}
static bool draw_clock(uint32_t now,risc_display_surface_v1 *surface) {
    if(clock_touch_retained||clock_runtime_unavailable)return false;
#if defined(WATCH_CONTEXTS_CLIENT) && defined(WATCH_CLOCK_LAUNCHER)
    clock_raster_count=0;clock_raster_overflow=false;clock_raster_activity=false;
    clock_rendering=true;
#endif
    CLOCK_PERF_BEGIN();
    bool rendered=draw_clock_pixels(now,surface);
    CLOCK_PERF_EVENT(RISC_PERF_SPAN_END,0x1001);
#if defined(WATCH_CONTEXTS_CLIENT) && defined(WATCH_CLOCK_LAUNCHER)
    clock_rendering=false;
    if(rendered)clock_replay_raster_input();
    else {clock_raster_count=0;clock_raster_overflow=false;clock_raster_activity=false;}
#endif
    return rendered;
}
static bool pace_frame_policy(uint32_t began,bool interruptible) {
    (void)interruptible;
    if(clock_touch_retained||clock_runtime_unavailable)return false;
#ifdef WATCH_RUNTIME_FEATURES
    if(native_clock.halted)return false;
#endif
#ifdef WATCH_BLE_BROADCAST
    if(!clock_broadcast_step())return false;
#endif
#ifdef WATCH_CLOCK_LAUNCHER
    if(interruptible&&launcher_swipe_pending)return true;
#endif
    uint32_t now;
    if(!alive(&now))return false;
    uint32_t last=now;unsigned stalled=0;
    for(;;) {
        uint32_t spent=now-began;
        if(spent>=UINT32_C(0x80000000))return false;
        if(spent>=20u)return true;
        /* Pacing follows a completed frame. Sample into existing gesture
         * latches, never render or consume them here. Acquisition retains its
         * shared 8ms cadence, independently of presentation poll frequency. */
#ifdef WATCH_CLOCK_LAUNCHER
#ifdef WATCH_CRASH_REPORT
        if(clock_crash_modal) {
#ifdef WATCH_CONTEXTS_CLIENT
            if(!clock_contexts_capture_checkpoint(NULL))return false;
#endif
        } else
#endif
        if(!sample_launcher_touch(now,false))return false;
#else
#ifdef WATCH_CONTEXTS_CLIENT
        if(!clock_contexts_capture_checkpoint(NULL))return false;
#endif
#endif
        if(clock_touch_retained||clock_runtime_unavailable)return false;
#ifdef WATCH_CONTEXTS_CLIENT
        if(clock_contexts_uncertain)return false;
#endif
#ifdef WATCH_CLOCK_LAUNCHER
        if(interruptible&&launcher_swipe_pending)return true;
#endif
        /* Include provider cost in the existing 20ms interval rather than
         * adding a fresh wait after it. Present and animation physics stay
         * unchanged; animation pacing never consumes a pending action. */
        if(!alive(&now))return false;
        if((uint32_t)(now-last)>=UINT32_C(0x80000000))return false;
        spent=now-began;
        if(spent>=20u)return true;
        uint32_t remaining=20u-spent,slice=remaining<8u?remaining:8u;
#ifdef WATCH_CLOCK_LAUNCHER
        uint32_t sample_age=now-launcher_sampled_at;
        if(touch.subscription&&!launcher_swipe_pending&&sample_age<8u&&slice>8u-sample_age)
            slice=8u-sample_age;
#endif
        if(stalled&&slice>1u)slice=1u;
        last=now;rt->yield_ms(slice);
        if(!alive(&now))return false;
        uint32_t step=now-last;
        if(step>=UINT32_C(0x80000000))return false;
        if(step)stalled=0;else if(++stalled>=64u)return false;
        last=now;
    }
}
static bool pace_frame(uint32_t began) { return pace_frame_policy(began,true); }
#if !defined(WATCH_FAST_WAKE) || defined(WATCH_COLD_BOOT_ANIMATION) || defined(WATCH_CLOCK_RETURN)
static bool pace_animation_frame(uint32_t began) {
    return pace_frame_policy(began,false);
}
/* Duration is independent of renderer throughput and pending input. A valid
 * millisecond clock may repeat, but 64 cooperative 1ms waits without progress
 * fail closed. Wrap is valid; backwards/ambiguous half-range steps are not.
 * This guard never consumes pending input or bypasses alive() custody checks. */
typedef struct { uint32_t last; bool observed; } clock_animation_progress;
static bool clock_animation_now(clock_animation_progress *progress,uint32_t *out) {
    for(unsigned stalled=0;stalled<=64u;stalled++) {
        uint32_t now;
        if(!alive(&now))return false;
        uint32_t step=now-progress->last;
        if(step>=UINT32_C(0x80000000))return false;
        if(!progress->observed || step) {
            progress->last=now;progress->observed=true;*out=now;return true;
        }
        if(stalled==64u)return false;
        rt->yield_ms(1);
    }
    return false;
}
#endif
#if !defined(WATCH_FAST_WAKE) || defined(WATCH_COLD_BOOT_ANIMATION)
static bool hold_boot_frame(void) {
    uint32_t start,now;
    if (!alive(&start)) return false;
    /* Begin after the final presentation completed, not when it was submitted. */
    clock_animation_progress progress={start,false};
    for(;;) {
        if (!clock_animation_now(&progress,&now)) return false;
        uint32_t spent=now-start;
        if (spent>=250u) return true;
        uint32_t left=250u-spent;
        rt->yield_ms(left<20u?left:20u);
    }
    return false;
}
/* Transient app-owned images only: the provider's lease is never retained
 * across submit, sleep, or an application switch. Allocation failure preserves
 * the normal sharp clock path instead of making Clock unavailable. */
static bool logo_to_clock(void) {
    const unsigned width=clock_viewport.width,height=clock_viewport.height;
    const uint32_t stride=width*sizeof(uint16_t);
    const size_t bytes=(size_t)stride*height;
    uint16_t *old=malloc(bytes),*scratch=old?malloc(bytes):NULL;
    bool ok=false;
    if (!old || !scratch || width>PORTABLE_TRANSITION_MAX_SIDE || height>PORTABLE_TRANSITION_MAX_SIDE) {
        risc_display_surface_v1 s={0};uint32_t now;
        free(scratch);free(old);
        return alive(&now) && frame(&s) && draw_clock(now,&s) && present() && pace_animation_frame(now);
    }
    risc_display_surface_v1 logo={0,old,width,height,stride,bytes,RISC_DISPLAY_FORMAT_RGB565};
    uint32_t start,now;
    if (!watch_boot_render(&logo,1800) || !alive(&start)) goto done;
    clock_animation_progress progress={start,false};
    bool first=true;
    for(;;) {
        risc_display_surface_v1 s={0};
        if (!clock_animation_now(&progress,&now) || !frame(&s) || !draw_clock(now,&s)) goto done;
        if(first){start=now;first=false;}
        unsigned alpha=portable_transition_alpha(now-start);
        if (!portable_transition_rgb565(s.pixels,s.stride_bytes,old,stride,
                scratch,bytes,width,height,alpha) || !present() || !pace_animation_frame(now)) goto done;
        if (alpha==256u) {ok=true;break;}
    }
done:
    if(clock_touch_retained||clock_runtime_unavailable)return false;
#ifdef WATCH_RUNTIME_FEATURES
    if(native_clock.halted)return false;
#endif
#ifdef WATCH_BLE_BROADCAST
    if(clock_broadcast_uncertain)return false;
#endif
    free(scratch);free(old);return ok;
}
#endif /* !WATCH_FAST_WAKE: no intro allocations or timed holds. */
#ifdef WATCH_CLOCK_RETURN
/* Explicit normal-navigation entry, selected by the clock.elf build. This
 * deployment uses the same retained, single-buffer provider as Springboard.
 * Capture it before drawing, preserve its light level, and never play the boot
 * intro. Actual successful wake below still uses startup() in both variants. */
static bool return_to_clock(void) {
    const unsigned width=clock_viewport.width,height=clock_viewport.height;
    const uint32_t stride=width*sizeof(uint16_t);
    const size_t bytes=(size_t)stride*height;
    uint16_t *old=malloc(bytes),*scratch=old?malloc(bytes):NULL;
    bool ok=false;
    uint32_t start,now;
    risc_display_surface_v1 retained={0};
    if (!old || !scratch || width>PORTABLE_TRANSITION_MAX_SIDE || height>PORTABLE_TRANSITION_MAX_SIDE) {
        free(scratch);free(old);
        return alive(&now) && frame(&retained) && draw_clock(now,&retained) && present();
    }
    if (!frame(&retained)) goto done;
    size_t span;
    if (!retained.pixels || retained.pixel_format!=RISC_DISPLAY_FORMAT_RGB565 ||
        retained.width!=width || retained.height!=height || retained.stride_bytes<stride ||
        retained.stride_bytes%sizeof(uint16_t) ||
        !pt_image_span(retained.stride_bytes,stride,height,&span) || span>retained.size_bytes) goto done;
    for(unsigned y=0;y<height;y++)
        memcpy(old+(size_t)y*width,(const uint8_t *)retained.pixels+(size_t)y*retained.stride_bytes,stride);
    display->release(display->context,held);held=0;
    if (!alive(&start)) goto done;
    /* Alpha zero is already on the panel. Do not render or transfer a duplicate
     * full frame before the first changing image. Bound a stalled clock too. */
    clock_animation_progress progress={start,false};
    for(;;) {
        risc_display_surface_v1 s={0};
        if (!clock_animation_now(&progress,&now)) goto done;
        uint32_t age=now-start;
        if (!age) {rt->yield_ms(1);continue;}
        uint32_t painted;
        if (!frame(&s) || !draw_clock(now,&s) || !alive(&painted)) goto done;
        if((uint32_t)(painted-progress.last)>=UINT32_C(0x80000000))goto done;
        progress.last=painted;
        age=painted-start; /* Rendering time belongs to the same 60ms budget. */
        unsigned alpha=age>=60u?256u:age*256u/60u;
        if (!portable_transition_rgb565(s.pixels,s.stride_bytes,old,stride,
                scratch,bytes,width,height,alpha) || !present()) goto done;
        if (alpha==256u) {ok=true;break;}
        if (!pace_animation_frame(now)) goto done;
    }
done:
    if(clock_touch_retained||clock_runtime_unavailable)return false;
#ifdef WATCH_RUNTIME_FEATURES
    if(native_clock.halted)return false;
#endif
#ifdef WATCH_BLE_BROADCAST
    if(clock_broadcast_uncertain)return false;
#endif
    free(scratch);free(old);return ok;
}
#endif
static unsigned clock_brightness(void) {
#ifdef WATCH_QUICK_ACTIONS
    return clock_quick.brightness;
#else
    return 40;
#endif
}
#ifdef WATCH_ATOMIC_PREFERENCES
#include "clock_atomic.inc"
#endif
#ifdef PORTABLE_LOW_BATTERY
static bool clock_low_battery_poll(uint32_t now) {
    if(held)return true;
#ifdef WATCH_CLOCK_ALARMS
    if(!clock_display_settled||clock_alarm_modal)return true;
#endif
    if(clock_low_battery_sampled&&(uint32_t)(now-clock_low_battery_at)<5000u)return true;
    clock_low_battery_sampled=true;clock_low_battery_at=now;
    risc_battery_sample_v1 sample={0,255,RISC_BATTERY_PROFILE_MISSING};
    if(!battery_api || !battery_api->read||!battery_api->read(battery_api->context,&sample))return true;
#if defined(WATCH_BLE_BROADCAST) || defined(WATCH_CONTEXTS_CLIENT)
    if(portable_low_battery_sample_valid(&sample) && sample.percent<PORTABLE_LOW_BATTERY_THRESHOLD &&
       (!clock_low_battery.observed || !clock_low_battery.low) && !clock_background_pause())return false;
#endif
#ifdef WATCH_ATOMIC_PREFERENCES
    unsigned result=home_automatic_low_battery(&sample);
#else
    unsigned result=portable_low_battery_update(&clock_low_battery,rt,&sample);
#endif
    if(result&PORTABLE_LOW_BATTERY_RETAINED) {
#ifdef WATCH_RUNTIME_FEATURES
        watch_runtime_halt(&native_clock);
#endif
        return false;
    }
    if(result&PORTABLE_LOW_BATTERY_ERROR)rt->diagnostic("LOW_BATTERY settings=unconfirmed");
    if(!(result&PORTABLE_LOW_BATTERY_ENTERED))return true;
    pqa_cancel(&clock_quick.ui);(void)pqa_take_action(&clock_quick.ui);
#ifdef WATCH_FAST_WAKE
    if(!pqa_session_load(&clock_quick,rt)||!pqa_session_restore(&clock_quick,display))return false;
    clock_deferred_phase=0;clock_confirm_waiting=clock_confirm_deadline_reported=false;clock_confirm_wait_started=0;clock_radios_pending=true;
    clock_broadcast_started=clock_contexts_started=false;
    clock_radios.valid=clock_radios.available=clock_quick.ui.radios_valid=false;
#else
    if(!pqa_session_load(&clock_quick,rt)||!pqa_radios_load(&clock_radios,&clock_quick.ui,rt)||
       !pqa_session_restore(&clock_quick,display))return false;
#endif
    rt->diagnostic(result&PORTABLE_LOW_BATTERY_ERROR?"LOW_BATTERY crossing=partial-once":"LOW_BATTERY crossing=applied-once");return true;
}
#endif
#if !defined(WATCH_FAST_WAKE) || defined(WATCH_COLD_BOOT_ANIMATION)
static bool startup(void) {
    uint32_t start,now;
    if (!alive(&start)) return false;
    clock_animation_progress progress={start,false};
    bool first=true;
    for(;;) {
        risc_display_surface_v1 s={0};
        if (!clock_animation_now(&progress,&now) || !frame(&s)) return false;
        uint32_t age=now-start;
        if (!watch_boot_render(&s,age>1800?1800:age) || !present()) return false;
        /* The retained provider may already contain a previous app's frame.
         * Entry blanks it before acquiring other capabilities. Restore light
         * only after the first complete new intro frame, never before submit. */
        if(first) {
            if(!display->set_brightness(display->context,(uint16_t)clock_brightness(),100))return false;
            first=false;
        }
        if (age>=1800) return hold_boot_frame() && logo_to_clock();
        if (!pace_animation_frame(now)) return false;
    }
    return false;
}
#endif
#ifdef WATCH_CLOCK_ALARMS
#include "clock_alarm.inc"
#ifdef WATCH_MOTION_WAKE
#include "watch_motion_client.h"
#endif
#endif
#ifdef WATCH_INTERACTION_TRACE
#include "interaction_trace.h"
static watch_interaction_trace clock_interaction;
static bool clock_quick_first_frame;
#endif
#if defined(WATCH_INTERACTION_TRACE) || defined(WATCH_PERFORMANCE_TRACE)
static bool clock_interaction_safe(void) {
 if(clock_touch_retained||clock_runtime_unavailable)return false;
#ifdef WATCH_RUNTIME_FEATURES
 if(native_clock.halted)return false;
#endif
#ifdef WATCH_ATOMIC_PREFERENCES
 if(clock_atomic_retained)return false;
#endif
#if defined(WATCH_QUICK_ACTIONS) && (defined(PORTABLE_FRONTLIGHT_TONE) || defined(PORTABLE_QUICK_ATOMIC_PREFERENCES))
 if(clock_quick.retained)return false;
#endif
#ifdef WATCH_NATIVE_ALARM_V2
 if(clock_alarm.retained)return false;
#endif
#ifdef WATCH_CONTEXTS_CLIENT
 if(clock_contexts_uncertain)return false;
#endif
#ifdef WATCH_BLE_BROADCAST
 if(clock_broadcast_uncertain)return false;
#endif
 return true;
}
#endif
#ifdef WATCH_INTERACTION_TRACE
#define CLOCK_TRACE(phase,done,begin) wit_mark(&clock_interaction,phase,done,begin,held!=0)
#ifdef PORTABLE_LOW_BATTERY
#define CLOCK_IDLE_TRACE(reason,now,last) wit_sleep(&clock_interaction,reason,now,last,clock_quick.idle_ms)
#else
#define CLOCK_IDLE_TRACE(reason,now,last) wit_sleep(&clock_interaction,reason,now,last,WATCH_DEFAULT_IDLE_MS)
#endif
#else
#define CLOCK_TRACE(phase,done,begin) ((void)0)
#define CLOCK_IDLE_TRACE(reason,now,last) ((void)0)
#endif
#ifdef WATCH_CONTEXTS_CLIENT
#include "clock_contexts.inc"
#endif
#if defined(WATCH_FAST_WAKE) || (defined(WATCH_RUNTIME_FEATURES) && !defined(WATCH_CLOCK_RETURN))
static bool resume_from_deep(void) {
    risc_display_surface_v1 fresh={0};uint32_t now;
#ifdef WATCH_FAST_WAKE
    if(!display->set_brightness(display->context,0,100) || !alive(&now))return false;
    clock_wake_mark("render-begin");if(clock_runtime_unavailable)return false;
    if(!frame(&fresh) || !draw_clock(now,&fresh))return false;
    clock_wake_mark("render-end");if(clock_runtime_unavailable)return false;
    clock_wake_mark("transfer-begin");if(clock_runtime_unavailable)return false;
    if(!present())return false;
    clock_wake_mark("transfer-end");if(clock_runtime_unavailable)return false;
    clock_wake_mark("unblank-begin");if(clock_runtime_unavailable)return false;
    if(!display->set_brightness(display->context,(uint16_t)clock_brightness(),100))return false;
    clock_wake_visible();
    return !clock_runtime_unavailable;
#else
    return display->set_brightness(display->context,0,100) && alive(&now) &&
        frame(&fresh) && draw_clock(now,&fresh) && present() &&
        display->set_brightness(display->context,(uint16_t)clock_brightness(),100);
#endif
}
#endif
#ifdef WATCH_FAST_WAKE
/* Fixed stage names and decimal boot-local timestamps only. No storage,
 * allocation or provider activation solely for instrumentation. */
static void clock_wake_stage(const char *stage,uint32_t now) {
    char line[96];const char *prefix="WATCH_WAKE stage=";size_t used=0;
    while(*prefix && used+1<sizeof(line))line[used++]=*prefix++;
    while(*stage && used+1<sizeof(line))line[used++]=*stage++;
    prefix=" t_ms=";while(*prefix && used+1<sizeof(line))line[used++]=*prefix++;
    uint32_t values[]={now,now-clock_wake_started};
    for(unsigned item=0;item<2;item++) {
        if(item){prefix=" elapsed_ms=";while(*prefix&&used+1<sizeof(line))line[used++]=*prefix++;}
        char digits[10];unsigned n=0;uint32_t value=values[item];
        do{digits[n++]=(char)('0'+value%10u);value/=10u;}while(value&&n<sizeof(digits));
        while(n&&used+1<sizeof(line))line[used++]=digits[--n];
    }
    line[used]=0;(void)rt->diagnostic(line);
}
/* One best-effort health sample per boot/wake checkpoint. Failed diagnostic
 * samples do not change application control flow. Never called per frame. */
static void clock_wake_mark(const char *stage) {
    uint32_t now;
    if(alive(&now))clock_wake_stage(stage,now);
}
static void clock_wake_visible(void) {
    uint32_t shown;
    if(alive(&shown)) {
        clock_wake_stage("unblank-end",shown);
        if(clock_boot_frame_pending)clock_wake_stage("first-frame",shown);
    }
    clock_boot_frame_pending=false;
}
/* Called once per settled foreground turn, after its input and clock frame.
 * Provider calls remain synchronous; this is phased admission, not threads or
 * a promise that one provider's start can be preempted by the app. */
static bool clock_deferred_step(uint32_t now,int32_t serviced) {
    const char *completed=NULL;
    switch(clock_deferred_phase) {
    case 0:
        clock_wake_stage("radios-begin",now);
#ifdef WATCH_QUICK_RADIOS
        if(!pqa_radios_load(&clock_radios,&clock_quick.ui,rt)) {
#ifdef WATCH_RUNTIME_FEATURES
            watch_runtime_halt(&native_clock);
#endif
            return false;
        }
#endif
        clock_radios_pending=false;completed="radios-end";
        break;
    case 1:
        clock_wake_stage("broadcast-begin",now);
#ifdef WATCH_BLE_BROADCAST
        if(!clock_broadcast.api && !portable_broadcast_open(&clock_broadcast,rt) && clock_broadcast.grant.api)return false;
        clock_broadcast_started=true;
        if(!clock_broadcast_step())return false;
#endif
        completed="broadcast-end";break;
    case 2:
        clock_wake_stage("contexts-begin",now);
#ifdef WATCH_CONTEXTS_CLIENT
        if(!clock_contexts.api && !portable_contexts_open(&clock_contexts,rt) && clock_contexts.grant.api)return false;
        if(!clock_contexts_pause() || !clock_contexts_recover_export())return false;
        clock_contexts_started=true;
        if(!clock_contexts_tick())return false;
        if(clock_contexts_handoff)return true;
#endif
        completed="contexts-end";break;
    case 3:
        /* Healthy native ownership can refuse paired confirmation because it
         * may write internal flash. Use the settled turn's existing checkpoint
         * result; do not reenter it or weaken the native confirmation guard.
         * Each deferred turn still polls input, renders and yields normally.
         * The deadline is diagnostic only: healthy BUSY must not kill Home or
         * the serial service. Navigation/sleep cancellation remains available. */
        if(serviced==RISC_RUNTIME_CHECKPOINT_BUSY) {
            if(!clock_confirm_waiting) {
                clock_confirm_waiting=true;clock_confirm_wait_started=now;
                rt->diagnostic("WATCH_CLOCK confirm=deferred checkpoint=busy");
            } else if(!clock_confirm_deadline_reported &&
                      (uint32_t)(now-clock_confirm_wait_started)>=5000u) {
                clock_confirm_deadline_reported=true;
                rt->diagnostic("WATCH_CLOCK confirm=deferred deadline-ms=5000 checkpoint=busy");
            }
            return true;
        }
        clock_wake_stage("confirm-begin",now);
#if defined(WATCH_PAIRED_BOOT_CONFIRM) && !defined(WATCH_CLOCK_RETURN)
        /* Preserve admission of all original startup services before marking
         * the pair good. An early frame or navigation is never confirmation. */
#ifdef WATCH_RUNTIME_FEATURES
        {int promoted=watch_runtime_promote(&native_clock);
        if(promoted==RISC_PROVIDER_PROMOTION_RETAINED)return false;
        if(promoted!=RISC_PROVIDER_PROMOTION_OK && promoted!=RISC_PROVIDER_PROMOTION_ALREADY_READY) {
            rt->diagnostic("WATCH_CLOCK error=provider-promotion");return false;
        }}
#endif
        if(!watch_confirm_paired_boot(rt)) {
            rt->diagnostic("WATCH_CLOCK error=paired-boot-confirm");return false;
        }
#endif
        completed="services-ready";break;
    default:return true;
    }
    if(!alive(&now))return false;
    clock_wake_stage(completed,now);
    ++clock_deferred_phase;
    return true;
}
#endif
/* Only actual refusals get a visible error. Preserve the exact stage and
 * signed provider result in the runtime log as well, including INT32_MIN,
 * without an allocation, service acquisition or printf dependency. */
static void clock_sleep_refused(void) {
    static const char *const stages[]={"UNKNOWN","SENSOR","PANEL","PMU","ALARM","LIGHT","DEEP","ACQUIRE","API","SETTINGS","MOTION","RUNTIME","SERVICE"};
    const char *name=stages[watch_sleep_stage<13?watch_sleep_stage:0];
    /* Acquisition is boolean; these stages have no numeric provider error. */
    bool detail_available=watch_sleep_stage<9 || watch_sleep_stage>11;
    char line[80];size_t used=0;
    const char *parts[]={"WATCH_CLOCK sleep=refused stage=",name,
        detail_available?" detail=":" detail=unavailable"};
    for(unsigned part=0;part<3;part++) {
        const char *text=parts[part];
        while(*text && used+1<sizeof(line))line[used++]=*text++;
    }
    const char *label="SLEEP ERR ";size_t label_used=0;
    while(*label)sleep_status[label_used++]=*label++;
    while(*name)sleep_status[label_used++]=*name++;
    if(!detail_available){
        sleep_status[label_used]=0;line[used]=0;(void)rt->diagnostic(line);return;
    }
    sleep_status[label_used++]=' ';
    uint32_t value=(uint32_t)watch_sleep_detail;
    if(watch_sleep_detail<0){line[used++]='-';sleep_status[label_used++]='-';value=0u-value;}
    char digits[10];unsigned count=0;
    do{digits[count++]=(char)('0'+value%10u);value/=10u;}while(value&&count<sizeof(digits));
    while(count&&used+1<sizeof(line)&&label_used+1<sizeof(sleep_status)){
        char digit=digits[--count];line[used++]=digit;sleep_status[label_used++]=digit;
    }
    sleep_status[label_used]=0;
    line[used]=0;(void)rt->diagnostic(line);
}
/* Failure-only diagnostics: no polling, timing calls, allocations or new grants.
 * A fatal/retained path must never attempt a display redraw merely for status. */
static bool clock_sleep_failure(const char *stage) {
    if(clock_touch_retained||clock_runtime_unavailable)return false;
    char line[80];size_t used=0;
    const char *prefix="WATCH_CLOCK sleep=failed stage=";
    while(*prefix && used+1<sizeof(line))line[used++]=*prefix++;
    while(*stage && used+1<sizeof(line))line[used++]=*stage++;
    line[used]=0;
    if(rt && rt->diagnostic)(void)rt->diagnostic(line);
    return false;
}
/* At most24 transition messages per invocation. No per-poll output, clock
 * queries, allocation, waiting or capability acquisition. Events are the key
 * API's classified short-release/long-held gestures, not invented raw edges. */
static unsigned clock_key_trace_left;
static void clock_key_trace(const char *stage) {
    if(!clock_key_trace_left || !rt || !rt->diagnostic)return;
    --clock_key_trace_left;
    (void)rt->diagnostic(stage);
}
static bool clock_key_read(uint32_t *events,const char *stage) {
    if(!watch_capabilities_key_events(&device_clients,events)){
        clock_key_trace("WATCH_KEY read=failed");return false;
    }
    if(*events&3u){
        clock_key_trace(stage);
        clock_key_trace((*events&3u)==3u?"WATCH_KEY receipt=short-release+long-held":
            (*events&2u)?"WATCH_KEY receipt=short-release":"WATCH_KEY receipt=long-held");
    }
    return true;
}
static int sleep_cycle(void) {
#ifdef WATCH_ATOMIC_PREFERENCES
    CLOCK_TRACE("sleep-scopes-begin",false,false);
    if(!clock_atomic_finish())return WATCH_SLEEP_RETAINED;
    CLOCK_TRACE("sleep-scopes-done",true,false);
#endif
    if(!watch_capabilities_can_sleep(&device_clients))return clock_sleep_failure("CAPABILITY");
    sleep_status[0]=0;
#if defined(WATCH_BLE_BROADCAST) || defined(WATCH_CONTEXTS_CLIENT)
    if(!clock_background_pause())return WATCH_SLEEP_RETAINED;
#endif
#ifdef PORTABLE_LOW_BATTERY
    watch_sleep_light_ms=clock_quick.deep_ms;
#endif
#ifdef WATCH_QUICK_RADIOS
    if(!pqa_radios_suspend(rt)) {
        rt->diagnostic("QUICK Bluetooth cleanup-unconfirmed");
#if defined(WATCH_FAST_WAKE) && defined(WATCH_RUNTIME_FEATURES)
        watch_runtime_halt(&native_clock);return WATCH_SLEEP_RETAINED;
#else
        return clock_sleep_failure("RADIO-SUSPEND");
#endif
    }
#endif
    /* Clear retained panel RAM before sleep, not merely its PWM. If the
     * platform briefly restores the backlight before the panel resume hook,
     * the exposed completed image is black rather than the old Clock. */
    risc_display_surface_v1 blank={0};
    size_t span;
    CLOCK_TRACE("sleep-blank-begin",false,false);
    if (!display->set_brightness(display->context,0,100))return clock_sleep_failure("BLANK-BRIGHTNESS");
    if (!frame(&blank))return clock_sleep_failure("BLANK-ACQUIRE");
    if (
        !blank.pixels || blank.pixel_format!=RISC_DISPLAY_FORMAT_RGB565 ||
        blank.width!=clock_viewport.width || blank.height!=clock_viewport.height || blank.stride_bytes<blank.width*2u ||
        blank.stride_bytes%sizeof(uint16_t) ||
        !pt_image_span(blank.stride_bytes,blank.width*2u,blank.height,&span) || span>blank.size_bytes) return clock_sleep_failure("BLANK-SURFACE");
    for(unsigned y=0;y<blank.height;y++) memset((uint8_t *)blank.pixels+(size_t)y*blank.stride_bytes,0,blank.width*2u);
    if (!present()) return clock_sleep_failure("BLANK-PRESENT");
    CLOCK_TRACE("sleep-blank-done",true,false);
    /* Every preparation attempt is paired with resume, including partial
     * preparation and platform refusal. No framebuffer lease survives here. */
    unsigned mode=PORTABLE_SLEEP_LIGHT;
#ifdef WATCH_CLOCK_LAUNCHER
    mode=sleep_mode;
#endif
    uint64_t sleep_token=0;
    CLOCK_TRACE("sleep-native-prepare",false,false);
    int preparation=watch_runtime_sleep_begin(rt,&sleep_token);
    if(preparation==WATCH_SLEEP_RETAINED)return preparation;
    int rc=preparation;
    if(preparation==WATCH_SLEEP_WOKE) {
    CLOCK_TRACE("sleep-native-ready",true,false);
    CLOCK_TRACE("sleep-policy-enter",false,false);
#ifdef WATCH_CLOCK_ALARMS
    (void)watch_sleep_prepared;
#ifdef WATCH_MOTION_WAKE
    (void)watch_alarm_sleep_prepared;
    rc=watch_motion_sleep(rt,panel,pmu,mode,clock_alarm.api);
#else
    rc=watch_alarm_sleep_prepared(panel,pmu,mode,clock_alarm.api,rt->diagnostic);
#endif
#else
    rc=watch_sleep_prepared(panel,pmu,mode,rt->diagnostic);
#endif
    if(rc>=0)CLOCK_TRACE("sleep-policy-return",true,false);
    rc=watch_runtime_sleep_end(rt,sleep_token,rc);
    if(rc>=0)CLOCK_TRACE("sleep-native-restored",true,false);
    }
    if(rc==WATCH_SLEEP_RETAINED){
#ifdef WATCH_NATIVE_ALARM_V2
        clock_alarm.retained=true;(void)clock_alarm_custody();
#endif
        return WATCH_SLEEP_RETAINED;
    }
    if(rc<0)return clock_sleep_failure("SLEEP-RESTORE");
#ifdef WATCH_NATIVE_TIME_AUTHORITY
    /* Revalidate the Runtime-owned clock before any wake input or rendering.
     * CONTEXT retains custody; malformed/unreadable samples abort the wake. */
    if(!watch_runtime_seed(&native_clock,NULL))return false;
#endif
    uint32_t cleared=0;
    if(!clock_key_read(&cleared,"WATCH_KEY stage=wake-discard"))return clock_sleep_failure("WAKE-KEY-CLEAR");
#ifdef WATCH_FAST_WAKE
    clock_deferred_phase=0;clock_confirm_waiting=clock_confirm_deadline_reported=false;clock_confirm_wait_started=0;clock_radios_pending=true;clock_input_reported=false;
#if defined(WATCH_RUNTIME_FEATURES) && !defined(WATCH_NATIVE_TIME_AUTHORITY)
    clock_rtc_reconcile_pending=true;clock_rtc_retry_delayed=false;
#endif
    clock_broadcast_started=clock_contexts_started=false;
#ifdef WATCH_QUICK_RADIOS
    clock_radios.valid=clock_radios.available=clock_quick.ui.radios_valid=false;
#endif
    if(!alive(&clock_wake_started))return clock_sleep_failure("WAKE-HEALTH");
    clock_wake_stage("light-resume",clock_wake_started);
#elif defined(WATCH_QUICK_RADIOS)
    if(!pqa_radios_resume(&clock_radios,&clock_quick.ui,rt))return clock_sleep_failure("RADIO-RESUME");
#endif
    uint32_t discard;
    reset_telemetry();
#ifdef WATCH_CLOCK_LAUNCHER
    if(!reload_time_format())return clock_sleep_failure("WAKE-TIME-FORMAT");
#endif
#ifdef WATCH_CLOCK_ALARMS
    /* Due work is handled before wake intro; short/crown and touch are fresh. */
    if(!launcher_touch_open(&touch))return clock_sleep_failure("WAKE-TOUCH-OPEN");
    if(!clock_alarm_foreground()){
#ifdef WATCH_NATIVE_ALARM_V2
        if(!clock_alarm_custody())return WATCH_SLEEP_RETAINED;
#endif
        return clock_sleep_failure("WAKE-ALARM");
    }
    if(!launcher_touch_close(&touch))return clock_sleep_failure("WAKE-TOUCH-CLOSE");
#endif
    if (rc==WATCH_SLEEP_WOKE) {
        rt->diagnostic("WATCH_CLOCK woke");
#ifdef WATCH_FAST_WAKE
        if(!resume_from_deep())return false;
        uint32_t shown;if(!alive(&shown))return clock_sleep_failure("WAKE-FRAME-HEALTH");
        clock_wake_stage("light-frame",shown);
#else
        if (!display->set_brightness(display->context,0,100) || !startup()) return false;
#endif
    } else {
        clock_sleep_refused();
        risc_display_surface_v1 fresh={0};uint32_t now;
        /* The pre-sleep clear also runs before a refused attempt. Replace it
         * with a complete fresh Clock before restoring the previous level. */
        if (!display->set_brightness(display->context,0,100) || !alive(&now) ||
            !frame(&fresh) || !draw_clock(now,&fresh) || !present() ||
            !display->set_brightness(display->context,(uint16_t)clock_brightness(),100)) return clock_sleep_failure("REFUSAL-REDRAW");
    }
    return clock_key_read(&discard,"WATCH_KEY stage=wake-final-discard") || clock_sleep_failure("FINAL-KEY-CLEAR");
}
static void clock_launch_feedback(void) {
    if(rt->struct_size<RISC_RUNTIME_LAUNCH_FAILURE_V1_SIZE || !rt->take_launch_failure)return;
    risc_runtime_launch_failure_v1 failure={.struct_size=sizeof(failure)};
    if(!rt->take_launch_failure(&failure))return;
    failure.application[sizeof(failure.application)-1]=0;
    failure.detail[sizeof(failure.detail)-1]=0;
    const char *message=(failure.flags&RISC_LAUNCH_FAILURE_EXTERNAL_IMAGE)?
        "APP MEDIA UNAVAILABLE":"APP FAILED TO START";
    memcpy(sleep_status,message,strlen(message)+1);
    rt->diagnostic("WATCH_CLOCK launch=failed action=visible-status");
    rt->diagnostic(failure.application);rt->diagnostic(failure.detail);
}
#include "clock_crash.inc"
#include "clock_provision.inc"

/* One best-effort, RAM-only first-exit checkpoint per invocation. The Runtime
 * derives identity and denies revoked/retained authority. Never retry, acquire
 * a provider, allocate, or emit live serial output from this boundary. A source
 * line identifies the first outer return route, not a nested physical cause. */
#include "RiscDiagnosticCheckpointV1.h"
static unsigned clock_first_exit;
static void clock_exit_checkpoint(unsigned line) {
#ifdef WATCH_INTERACTION_TRACE
    if(!clock_interaction_safe())return;
#endif
    if(clock_touch_retained||clock_runtime_unavailable)return;
#ifdef WATCH_NATIVE_ALARM_V2
    if(clock_alarm.retained)return;
#endif
    if(clock_first_exit)return;
    clock_first_exit=line;
#ifdef WATCH_INTERACTION_TRACE
    if(clock_interaction.record[0]){wit_exit(&clock_interaction,line);return;}
#endif
    risc_diagnostic_checkpoint_client_v1 client={.struct_size=sizeof(client)};
    if(!risc_runtime_diagnostic_checkpoint_client(rt,&client) ||
       client.api_version!=RISC_DIAGNOSTIC_CHECKPOINT_API_V1 ||
       client.struct_size<sizeof(client) || !client.write)return;
    char text[]="WATCH_HOME_EXIT line=00000";
    unsigned n=line;
    for(unsigned i=sizeof(text)-1;i>sizeof(text)-6;){
        text[--i]=(char)('0'+n%10u);n/=10u;
    }
    (void)client.write(client.invocation,text,sizeof(text)-1);
}
#include "quick_pending.inc"
__attribute__((visibility("default"))) void app_main(void) {
    clock_touch_retained=clock_runtime_unavailable=false;
#ifdef WATCH_PERFORMANCE_TRACE
    clock_perf=(watch_home_performance){0};
#endif
    clock_first_exit=0;clock_navigation_ready=false;
#ifdef WATCH_AUTOMATIC_PROVISION
    clock_provision_initialized=clock_provision_busy=false;
#endif
    sleep_status[0]=0;clock_key_trace_left=24;rt=risc_runtime_get_api(1);
#ifdef WATCH_FAST_WAKE
    clock_deferred_phase=0;clock_confirm_waiting=clock_confirm_deadline_reported=false;clock_confirm_wait_started=0;clock_radios_pending=true;clock_input_reported=false;clock_wake_started=0;
#ifdef WATCH_RUNTIME_FEATURES
    clock_rtc_reconcile_pending=clock_rtc_retry_delayed=false;clock_rtc_retry_sampled_at=0;
#endif
    clock_broadcast_started=clock_contexts_started=false;
    clock_boot_frame_pending=true;
#endif
#if defined(WATCH_RUNTIME_FEATURES) && defined(WATCH_CLOCK_RETURN)
    (void)watch_runtime_promote;
#endif
#ifdef WATCH_CLOCK_ALARMS
    clock_alarm=(portable_alarm_client){0};clock_display_settled=true;clock_alarm_attempted=false;
    clock_alarm_modal=clock_alarm_failed_cleaned=clock_alarm_error_seen=false;
#ifdef WATCH_FAST_WAKE
    clock_alarm_input_cancelled=false;
#endif
#endif
#ifdef WATCH_CLOCK_LAUNCHER
    touch=(watch_launcher_touch){0};
#ifdef WATCH_QUICK_ACTIONS
    pqa_session_init(&clock_quick);
#ifdef WATCH_ATOMIC_PREFERENCES
    if(clock_atomic_retained){clock_exit_checkpoint(__LINE__);return;}
    clock_quick.save_user=quick_atomic_save;
#endif
#ifdef PORTABLE_LOW_BATTERY
    clock_low_battery=(portable_low_battery){0};clock_low_battery_sampled=false;clock_low_battery_at=0;
#endif
#endif
    launcher_swipe_pending=launcher_activity_pending=false;
    launcher_sampled_at=0;
    picker=(watch_face_picker){0};picker_scratch=NULL;
    picker_open_pending=picker_select_pending=picker_save_failed=false;
    picker_selection_pending=0;
#endif
    held=0;rtc=NULL;display=NULL;pmu=NULL;panel=NULL;battery_api=NULL;
    device_clients=(watch_capability_clients){0};
#ifdef WATCH_BLE_BROADCAST
    clock_broadcast=(portable_broadcast_client){0};clock_broadcast_uncertain=clock_handoff=false;
#endif
#ifdef WATCH_CONTEXTS_CLIENT
    clock_contexts=(portable_contexts_client){0};clock_contexts_uncertain=clock_contexts_handoff=false;clock_context_message[0]=0;
#endif
    reset_telemetry();
    if (!rt || rt->api_version!=1 || rt->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE ||
        !rt->health || !rt->yield_ms || !rt->diagnostic || !rt->acquire || !rt->release) {clock_exit_checkpoint(__LINE__);return;}
#ifdef WATCH_INTERACTION_TRACE
    wit_init(&clock_interaction,rt,clock_interaction_safe);clock_quick_first_frame=false;
#endif
#ifdef WATCH_FAST_WAKE
    if(!alive(&clock_wake_started)){clock_exit_checkpoint(__LINE__);return;}
    clock_wake_stage("entry",clock_wake_started);
#ifdef WATCH_QUICK_RADIOS
    clock_radios=(pqa_radios){0};clock_quick.ui.radio_controls=true;
    clock_quick.ui.radios_valid=false;
#endif
#endif
    risc_runtime_capability_v1 dg={.struct_size=sizeof(dg)},rg={.struct_size=sizeof(rg)};
    bool have_d=false,have_r=false;
#ifdef WATCH_RUNTIME_FEATURES
    if(!CLOCK_WAKE_MARK("runtime-grants-begin"))goto done;
    if(!watch_runtime_open(&native_clock,rt,
#ifdef WATCH_CLOCK_RETURN
        false
#else
        true
#endif
        )){clock_exit_checkpoint(__LINE__);goto done;}
    if(!CLOCK_WAKE_MARK("runtime-grants-end"))goto done;
#endif
#ifndef WATCH_FAST_WAKE
#ifdef WATCH_BLE_BROADCAST
    if(!portable_broadcast_open(&clock_broadcast,rt) && clock_broadcast.grant.api){clock_exit_checkpoint(__LINE__);goto done;}
#endif
#ifdef WATCH_CONTEXTS_CLIENT
    if(!portable_contexts_open(&clock_contexts,rt) && clock_contexts.grant.api){clock_exit_checkpoint(__LINE__);goto done;}
    if(!clock_contexts_pause()){clock_exit_checkpoint(__LINE__);goto done;}
#endif
#endif
    if(!CLOCK_WAKE_MARK("display-begin"))goto done;
    have_d=rt->acquire("display.output",1,0,&dg);
    if (!have_d) {clock_exit_checkpoint(__LINE__);goto done;}
    display=dg.api;
    if (!display || display->api_version!=1 || display->struct_size<sizeof(*display) ||
        !display->get_info || !display->acquire || !display->release || !display->submit ||
        !display->present_status || !display->set_brightness) {clock_exit_checkpoint(__LINE__);goto done;}
    if (!watch_viewport_open(&clock_viewport,display)) {
        rt->diagnostic("WATCH_CLOCK error=display-viewport");{clock_exit_checkpoint(__LINE__);goto done;}
    }
    display=&clock_viewport.display;
#ifdef WATCH_CLOCK_LAUNCHER
    if(!watch_face_set_size(&picker,clock_viewport.width,clock_viewport.height)){clock_exit_checkpoint(__LINE__);goto done;}
#endif
#ifdef WATCH_QUICK_ACTIONS
    if(!pqa_set_dimensions(&clock_quick.ui,clock_viewport.width,clock_viewport.height)){clock_exit_checkpoint(__LINE__);goto done;}
#ifdef PORTABLE_QUICK_SAFE_CONTENT
    if(!pqa_set_safe_content(&clock_quick.ui,clock_viewport.layout.safe.x,clock_viewport.layout.safe.y,
        clock_viewport.layout.safe.width,clock_viewport.layout.safe.height)){clock_exit_checkpoint(__LINE__);goto done;}
#endif
#endif
#ifndef WATCH_CLOCK_RETURN
    if (!display->set_brightness(display->context,0,100)) {clock_exit_checkpoint(__LINE__);goto done;}
#endif
    if(!CLOCK_WAKE_MARK("display-end"))goto done;
    if(!CLOCK_WAKE_MARK("power-services-begin"))goto done;
    if(!watch_capabilities_open(&device_clients,rt)){clock_exit_checkpoint(__LINE__);goto done;}
    battery_api=device_clients.battery;pmu=device_clients.sleep;panel=device_clients.display_power;
    if(!device_clients.key || !pmu || !panel) {
        const char *missing=!device_clients.key?"KEY-CAPABILITY":!pmu?"POWER-CAPABILITY":"PANEL-CAPABILITY";
        memcpy(sleep_status,"SLEEP ERR CAPABILITY",sizeof("SLEEP ERR CAPABILITY"));
        clock_sleep_failure(missing);
    }
    clock_key_trace(device_clients.key?"WATCH_KEY acquire=present instance=unique":"WATCH_KEY acquire=missing instance=unique");
    clock_key_trace(pmu&&panel?"WATCH_KEY sleep-capabilities=present":"WATCH_KEY sleep-capabilities=missing");
    if(!CLOCK_WAKE_MARK("power-services-end"))goto done;
    if(!CLOCK_WAKE_MARK("rtc-begin"))goto done;
#ifndef WATCH_NATIVE_TIME_AUTHORITY
#if defined(WATCH_FAST_WAKE) && defined(WATCH_RUNTIME_FEATURES)
    /* Deep wake already has a validated native civil-time snapshot. Avoid
     * loading the external RTC ELF solely to seed a clock that is valid.
     * Alarm admission later activates its own declared RTC dependency. */
    risc_realtime_snapshot_v1 retained_time;
    bool defer_rtc=native_clock.cause>=RISC_BOOT_DEEP_TIMER &&
        watch_runtime_snapshot(&native_clock,&retained_time) &&
        retained_time.validity==RISC_REALTIME_VALID;
    if(native_clock.halted){clock_exit_checkpoint(__LINE__);goto done;}
    clock_rtc_reconcile_pending=defer_rtc;
    if(!defer_rtc) {
#endif
    have_r=rt->acquire("rtc.clock",2,0,&rg);
    if (have_r) {
        rtc=rg.api;
        if (!rtc || rtc->api_version!=2 || rtc->struct_size<sizeof(*rtc) || !rtc->read) rtc=NULL;
    }
#if defined(WATCH_FAST_WAKE) && defined(WATCH_RUNTIME_FEATURES)
    } else if(!CLOCK_WAKE_MARK("rtc-deferred"))goto done;
#endif
#endif /* external RTC authority */
#ifdef WATCH_RUNTIME_FEATURES
#ifdef WATCH_NATIVE_TIME_AUTHORITY
    if(!watch_runtime_seed(&native_clock,NULL)){clock_exit_checkpoint(__LINE__);goto done;}
    rt->diagnostic("WATCH_CLOCK time-authority=native");
#else
    bool rtc_seeded=watch_runtime_seed(&native_clock,rtc);
    if(!rtc_seeded || native_clock.rtc_reconcile_pending) {
        /* An ordinary RTC refusal cannot strand Home. A fresh Runtime read
         * proves that even an UNSET clock still belongs to this invocation. */
        risc_realtime_snapshot_v1 remaining;
        if(!watch_runtime_snapshot(&native_clock,&remaining)){clock_exit_checkpoint(__LINE__);goto done;}
    }
#ifdef WATCH_FAST_WAKE
    clock_rtc_reconcile_pending=native_clock.rtc_reconcile_pending;
    if(clock_rtc_reconcile_pending&&!defer_rtc)
        memcpy(sleep_status,"TIME CHECK ERR",sizeof("TIME CHECK ERR"));
#endif
#endif /* external RTC authority */
#endif
    if(!CLOCK_WAKE_MARK("rtc-end"))goto done;
    if(!CLOCK_WAKE_MARK("settings-begin"))goto done;
    #ifdef WATCH_CLOCK_LAUNCHER
    /* Load before the first sharp Clock frame on fresh boot and normal return.
     * This is the existing shared app-settings namespace, never a driver write. */
    risc_runtime_capability_v1 face_grant={.struct_size=sizeof(face_grant)};
    if(rt->acquire(RISC_KEY_VALUE_CAPABILITY,1,WATCH_FACE_STORE_INSTANCE,&face_grant)) {
        unsigned selected=0;int rc=watch_face_load(face_grant.api,&selected);
        picker.selected=(uint8_t)selected;
        unsigned format=PORTABLE_TIME_FORMAT_12;(void)portable_time_format_load(face_grant.api,&format);
        face.hour_24=format==PORTABLE_TIME_FORMAT_24;
#ifdef WATCH_NATIVE_TIME_AUTHORITY
        clock_timezone_load(face_grant.api);
#endif
        if(!rt->release(&face_grant)){clock_exit_checkpoint(__LINE__);goto done;}
        if(rc!=RISC_KEY_VALUE_OK&&rc!=RISC_KEY_VALUE_NOT_FOUND)rt->diagnostic("WATCH_CLOCK face=unreadable default=nova");
    }
#ifdef WATCH_NATIVE_TIME_AUTHORITY
    else clock_timezone_load(NULL);
#endif
#endif
#ifdef WATCH_QUICK_ACTIONS
    if(!pqa_session_load(&clock_quick,rt)){clock_exit_checkpoint(__LINE__);goto done;}
#if defined(WATCH_QUICK_RADIOS) && !defined(WATCH_FAST_WAKE)
    if(!pqa_radios_load(&clock_radios,&clock_quick.ui,rt)){clock_exit_checkpoint(__LINE__);goto done;}
#endif
#endif
#ifdef WATCH_UNSET_TIME_LOCAL
    if(!watch_runtime_initialize_unset(&native_clock)){clock_exit_checkpoint(__LINE__);goto done;}
#endif
    if(!CLOCK_WAKE_MARK("settings-end"))goto done;
#ifdef WATCH_CLOCK_ALARMS
#ifdef WATCH_CLOCK_POINTS
    if(!CLOCK_WAKE_MARK("points-begin"))goto done;
    if(!clock_points_load()){clock_exit_checkpoint(__LINE__);goto done;}
    if(!CLOCK_WAKE_MARK("points-end"))goto done;
#endif
    /* Fast entry needs working input before acquiring the alarm/output closure.
     * The first settled input turn below reconciles it before any handoff,
     * sleep, radio or Contexts work. The original entry order is unchanged. */
#if defined(WATCH_FAST_WAKE) && !defined(WATCH_NATIVE_ALARM_V2)
    if(!CLOCK_WAKE_MARK("touch-begin"))goto done;
    if(!launcher_touch_open(&touch)){clock_exit_checkpoint(__LINE__);goto done;}
    if(!CLOCK_WAKE_MARK("touch-end"))goto done;
#else
    if(!clock_alarm_open_optional() || !launcher_touch_open(&touch) || !clock_alarm_foreground()){clock_exit_checkpoint(__LINE__);goto done;}
#endif
#endif
    clock_launch_feedback();
    uint32_t discarded,now,armed_at=0,last_activity=0;
    if (!clock_key_read(&discarded,"WATCH_KEY stage=startup-discard") ||
#ifdef WATCH_CLOCK_RETURN
        !return_to_clock() ||
#else
#ifdef WATCH_FAST_WAKE
#if defined(WATCH_COLD_BOOT_ANIMATION) && defined(WATCH_RUNTIME_FEATURES)
        /* The runtime identifies first Home entry; retained-wake identifies
         * cold vs deep boot. Never replay the intro on child return. */
        ((rt->struct_size>=RISC_RUNTIME_INITIAL_DEFAULT_V1_SIZE && rt->initial_default &&
          rt->initial_default() && native_clock.cause<RISC_BOOT_DEEP_TIMER) && !startup()) ||
#endif
        !resume_from_deep() ||
#elif defined(WATCH_RUNTIME_FEATURES)
        !(native_clock.resume?resume_from_deep():startup()) ||
#else
        !startup() ||
#endif
#endif
        !clock_key_read(&discarded,"WATCH_KEY stage=startup-discard") || !alive(&armed_at)) {clock_exit_checkpoint(__LINE__);goto done;}
#ifdef WATCH_CLOCK_LAUNCHER
#ifdef WATCH_CLOCK_ALARMS
    if(!rt->request_launch){clock_exit_checkpoint(__LINE__);goto done;}
#else
    if(!rt->request_launch || !launcher_touch_open(&touch)) {clock_exit_checkpoint(__LINE__);goto done;}
#endif
#endif
    clock_key_trace("WATCH_KEY arming=start interval-ms=250");
    last_activity=armed_at;
    CLOCK_IDLE_TRACE("entry-baseline",armed_at,last_activity);
#ifdef WATCH_CLOCK_LAUNCHER
    /* Read once per fresh Clock invocation, release the grant before sleep.
     * Settings returns through a fresh Clock, and deep wake boots default. */
    sleep_mode=WATCH_DEFAULT_SLEEP_MODE;
    if(!CLOCK_WAKE_MARK("sleep-settings-begin"))goto done;
    risc_runtime_capability_v1 sg={.struct_size=sizeof(sg)};
    if(rt->acquire(RISC_KEY_VALUE_CAPABILITY,RISC_KEY_VALUE_API_V1,PORTABLE_SLEEP_STORE_INSTANCE,&sg)) {
        int loaded=watch_sleep_load(sg.api,&sleep_mode);
        if(!rt->release(&sg)){clock_exit_checkpoint(__LINE__);goto done;}
        const char *fallback=watch_sleep_fallback_diagnostic(loaded);
        if(fallback)rt->diagnostic(fallback);
    } else rt->diagnostic(watch_sleep_fallback_diagnostic(PORTABLE_SLEEP_UNAVAILABLE));
    if(!CLOCK_WAKE_MARK("sleep-settings-end"))goto done;
    rt->diagnostic(sleep_mode==PORTABLE_SLEEP_DEEP?"WATCH_CLOCK mode=deep":sleep_mode==PORTABLE_SLEEP_LIGHT?"WATCH_CLOCK mode=light":"WATCH_CLOCK mode=hybrid");
#endif
#if defined(WATCH_PAIRED_BOOT_CONFIRM) && !defined(WATCH_CLOCK_RETURN) && !defined(WATCH_FAST_WAKE)
    /* Startup has presented a complete frame and admitted all startup services.
     * A pending native/store pair must remain unconfirmed on every earlier
     * failure, intentional exit, queued handoff or native-retained path. */
#ifdef WATCH_RUNTIME_FEATURES
    int promoted=watch_runtime_promote(&native_clock);
    if(promoted==RISC_PROVIDER_PROMOTION_RETAINED){clock_exit_checkpoint(__LINE__);return;}
    if(promoted!=RISC_PROVIDER_PROMOTION_OK && promoted!=RISC_PROVIDER_PROMOTION_ALREADY_READY) {
        rt->diagnostic("WATCH_CLOCK error=provider-promotion");{clock_exit_checkpoint(__LINE__);goto done;}
    }
#endif
    if(!watch_confirm_paired_boot(rt)) {
        rt->diagnostic("WATCH_CLOCK error=paired-boot-confirm");{clock_exit_checkpoint(__LINE__);goto done;}
    }
#endif
#ifdef WATCH_FAST_WAKE
    if(!alive(&now)){clock_exit_checkpoint(__LINE__);goto done;}
#endif
#ifdef WATCH_CRASH_REPORT
    clock_crash_init();
#endif
#ifdef WATCH_AUTOMATIC_PROVISION
    clock_provision_init();
#endif
    rt->diagnostic("WATCH_CLOCK ready crown=enabled");
#if defined(WATCH_CONTEXTS_CLIENT) && !defined(WATCH_FAST_WAKE)
    if(!clock_contexts_recover_export()){clock_exit_checkpoint(__LINE__);goto done;}
#endif
    while(alive(&now)) {
        /* Startup and the previous frame have fully completed presentation.
         * No frame is held here. Never dispatch this service from present(),
         * raster callbacks, pacing, or another ordinary yield/BUSY path. */
        int32_t serviced=risc_runtime_service_checkpoint_v1(rt);
        if(serviced!=RISC_RUNTIME_CHECKPOINT_OK && serviced!=RISC_RUNTIME_CHECKPOINT_BUSY){clock_exit_checkpoint(__LINE__);return;}
#ifdef WATCH_SERIAL_TRANSFER_MODE
        /* Native transfer intent is a cooperative exit, not revoked custody.
         * Stop new foreground work and release through the checked owner path. */
        if(risc_runtime_exit_requested_v1(rt)){clock_exit_checkpoint(__LINE__);goto done;}
#endif
        clock_navigation_ready=serviced==RISC_RUNTIME_CHECKPOINT_OK;
        if(clock_navigation_ready&&!strcmp(sleep_status,"SERVICE BUSY - RETRY"))sleep_status[0]=0;
#ifdef WATCH_BLE_BROADCAST
        if(!clock_broadcast_step()){clock_exit_checkpoint(__LINE__);return;}
#endif
#ifdef WATCH_CLOCK_ALARMS
#ifdef WATCH_FAST_WAKE
        if(clock_alarm.api && (!clock_alarm_foreground() || !alive(&now))){clock_exit_checkpoint(__LINE__);break;}
#else
        if(!clock_alarm_foreground() || !alive(&now)){clock_exit_checkpoint(__LINE__);break;}
#endif
#endif
#if defined(PORTABLE_LOW_BATTERY) && !defined(WATCH_ATOMIC_PREFERENCES)
        if(!clock_low_battery_poll(now)){clock_exit_checkpoint(__LINE__);break;}
#endif
#ifdef WATCH_CONTEXTS_CLIENT
#ifdef WATCH_FAST_WAKE
        if(clock_contexts_started && !clock_radios_pending && !clock_contexts_tick()){clock_exit_checkpoint(__LINE__);break;}
#else
        if(!clock_contexts_tick()){clock_exit_checkpoint(__LINE__);break;}
#endif
        if(clock_contexts_handoff){clock_exit_checkpoint(__LINE__);break;}
#endif
        uint32_t events=0;
        if (!clock_key_read(&events,"WATCH_KEY stage=foreground")) {clock_exit_checkpoint(__LINE__);break;}
        /* The clock closure currently exposes only PMU short/long key events.
         * Do not invent touch activity. Short press is reported on release. */
        if (events&3u) last_activity=now;
#ifdef WATCH_CLOCK_LAUNCHER
        if(!sample_launcher_touch(now,true))return;
        if(clock_touch_retained||clock_runtime_unavailable)return;
#ifdef WATCH_FAST_WAKE
        if(!clock_input_reported){clock_wake_stage("first-input-poll",now);clock_input_reported=true;}
#endif
#if defined(PORTABLE_LOW_BATTERY) && defined(WATCH_ATOMIC_PREFERENCES)
        /* Service foreground input before a copied policy transaction yields. */
        if(!clock_low_battery_poll(now)||!alive(&now)){clock_exit_checkpoint(__LINE__);break;}
#endif
#ifdef WATCH_CONTEXTS_CLIENT
        if(clock_contexts_uncertain){clock_exit_checkpoint(__LINE__);break;}
#endif
#if defined(WATCH_FAST_WAKE) && defined(WATCH_CLOCK_ALARMS)
        if(!clock_alarm_attempted) {
            /* First Clock and one real input poll precede synchronous alarm,
             * haptic and speaker activation. Due/expired occurrences retain
             * the service's normal reconciliation and failure handling. */
            if(!CLOCK_WAKE_MARK("alarm-begin"))goto done;
            if(!clock_alarm_open_optional()){clock_exit_checkpoint(__LINE__);break;}
            if(!CLOCK_WAKE_MARK("alarm-end"))goto done;
            clock_alarm_input_cancelled=false;
            if(!CLOCK_WAKE_MARK("reconcile-begin"))goto done;
            if(!clock_alarm_foreground() || !alive(&now)){clock_exit_checkpoint(__LINE__);break;}
            if(!CLOCK_WAKE_MARK("reconcile-end"))goto done;
            if(clock_alarm_input_cancelled){if(events&3u)clock_key_trace("WATCH_KEY decision=alarm-suppressed");events=0;}
            /* Loading or a visible alarm must not exhaust the idle interval. */
            last_activity=now;
            CLOCK_IDLE_TRACE("alarm-ready-baseline",now,last_activity);
        }
#endif
#if defined(WATCH_FAST_WAKE) && defined(WATCH_RUNTIME_FEATURES) && !defined(WATCH_NATIVE_TIME_AUTHORITY)
        /* Keep first-frame/input latency independent of RTC availability.
         * Reconcile before handoff/sleep; on ordinary refusal retry at a
         * bounded cadence while the visible native clock remains usable. */
        if(clock_rtc_reconcile_pending && (!clock_rtc_retry_delayed ||
           (uint32_t)(now-clock_rtc_retry_sampled_at)>=CLOCK_RTC_RETRY_MS)) {
            if(!have_r) {
                have_r=rt->acquire("rtc.clock",2,0,&rg);
                if(!have_r && (rg.api || rg.slot || rg.generation)) {
                    watch_runtime_halt(&native_clock);{clock_exit_checkpoint(__LINE__);break;}
                }
                rtc=have_r?rg.api:NULL;
                if(rtc && (rtc->api_version!=2 || rtc->struct_size<sizeof(*rtc) || !rtc->read))rtc=NULL;
            }
            bool reconciled=rtc && watch_runtime_seed(&native_clock,rtc) &&
                !native_clock.rtc_reconcile_pending;
            if(!reconciled) {
                /* VALID is provisional continuity; UNSET remains explicit.
                 * CONTEXT or malformed native custody still stops all I/O. */
                risc_realtime_snapshot_v1 remaining;
                if(!watch_runtime_snapshot(&native_clock,&remaining)){clock_exit_checkpoint(__LINE__);break;}
                memcpy(sleep_status,"TIME CHECK ERR",sizeof("TIME CHECK ERR"));
                rt->diagnostic("WATCH_CLOCK rtc-reconcile=unavailable retry-ms=5000");
                clock_rtc_retry_delayed=true;
            } else {
                clock_rtc_reconcile_pending=clock_rtc_retry_delayed=false;
                if(!strcmp(sleep_status,"TIME CHECK ERR"))sleep_status[0]=0;
            }
            if(!alive(&now)){clock_exit_checkpoint(__LINE__);break;}
            clock_rtc_retry_sampled_at=now;sampled_rtc=false;
        }
#endif
#ifdef WATCH_CONTEXTS_CLIENT
        if(clock_context_message[0]&&(launcher_activity_pending||(events&3u))){if(events&3u)clock_key_trace("WATCH_KEY decision=context-message-dismissed");clock_context_message[0]=0;clock_alarm_cancel_input();events=0;}
#endif
        if(launcher_activity_pending) last_activity=now;
        launcher_activity_pending=false;
#ifdef WATCH_QUICK_ACTIONS
#if defined(PORTABLE_QUICK_CAPABILITY_SAFE) && defined(WATCH_CONTEXTS_CLIENT)
        clock_quick.ui.contexts_available=clock_contexts.api&&!clock_contexts_uncertain;
#endif
        if(events&3u)pqa_close(&clock_quick.ui);
#ifdef WATCH_ATOMIC_PREFERENCES
        if(!clock_quick_scope&&(pqa_visible(&clock_quick.ui)||pqa_capture(&clock_quick.ui)||clock_quick.ui.pending)){
            CLOCK_TRACE("quick-modal-begin",false,true);
            if(!quick_atomic_modal_begin()){clock_exit_checkpoint(__LINE__);break;}
            CLOCK_TRACE("quick-modal-done",true,false);
#ifdef WATCH_INTERACTION_TRACE
            clock_quick_first_frame=true;
#endif
            clock_quick_scope=true;
        }
        ++quick_atomic_contact;
        uint32_t quick_actions=quick_atomic_take_action();bool volume_changed=false;
        if(!clock_quick.ui.radio_controls&&(quick_actions&PQA_WIFI)&&!clock_navigation_allowed())quick_actions&=~(uint32_t)PQA_WIFI;
#ifdef PORTABLE_QUICK_CAPABILITY_SAFE
        int pending_ready=clock_quick_pending_frame(quick_actions,&events);
        if(pending_ready<0){clock_exit_checkpoint(__LINE__);break;}
        if(!pending_ready)quick_actions=0;
#endif
        if(!quick_atomic_capture(quick_actions)){clock_exit_checkpoint(__LINE__);break;}
        quick_atomic_show_queue_error();
#else
        uint32_t quick_actions=pqa_take_action(&clock_quick.ui);bool volume_changed=false;
        if(!clock_quick.ui.radio_controls&&(quick_actions&PQA_WIFI)&&!clock_navigation_allowed())quick_actions&=~(uint32_t)PQA_WIFI;
#endif
#ifdef WATCH_CONTEXTS_CLIENT
#ifdef WATCH_AUTOMATIC_PROVISION
        if(quick_actions&&!clock_provision_close()){clock_exit_checkpoint(__LINE__);return;}
#endif
        if(quick_actions&&!clock_contexts_pause()){clock_exit_checkpoint(__LINE__);return;}
#endif
        if(quick_actions)CLOCK_PERF_EVENT(RISC_PERF_DISPATCHED,0x410);
        if(!pqa_session_apply(&clock_quick,rt,display,quick_actions,&volume_changed)){clock_exit_checkpoint(__LINE__);break;}
#ifdef WATCH_CONTEXTS_CLIENT
        if(quick_actions&PQA_CONTEXTS)portable_contexts_settings_changed(&clock_contexts,clock_quick.ui.contexts_valid);
#endif
#ifdef WATCH_QUICK_RADIOS
#ifdef WATCH_BLE_BROADCAST
        if((quick_actions&(PQA_WIFI|PQA_BLUETOOTH|PQA_AIRPLANE)) && !clock_broadcast_pause()){clock_exit_checkpoint(__LINE__);return;}
#endif
#ifdef WATCH_ATOMIC_PREFERENCES
        if(!pqa_radios_apply_user(&clock_radios,&clock_quick.ui,rt,quick_actions,quick_atomic_save,NULL,quick_atomic_guard)){clock_exit_checkpoint(__LINE__);break;}
#else
        if(!pqa_radios_apply(&clock_radios,&clock_quick.ui,rt,quick_actions)){clock_exit_checkpoint(__LINE__);break;}
#endif
#endif
#if defined(PORTABLE_QUICK_CAPABILITY_SAFE) && defined(WATCH_ATOMIC_PREFERENCES)
        clock_quick_pending_done();
#endif
#ifdef WATCH_CLOCK_ALARMS
        if(volume_changed && clock_alarm.api)(void)clock_alarm.api->refresh(clock_alarm.api->context);
#endif
        if(!clock_quick.ui.radio_controls && (quick_actions&PQA_WIFI)) {
            pqa_cancel(&clock_quick.ui);
            if(!pqa_session_restore(&clock_quick,display) || !launcher_touch_close(&touch)){clock_exit_checkpoint(__LINE__);break;}
#ifdef WATCH_AUTOMATIC_PROVISION
            if(!clock_provision_close()){clock_exit_checkpoint(__LINE__);break;}
#endif
            if(rt->request_launch("wifi_settings.elf")) {
#ifdef WATCH_BLE_BROADCAST
                clock_handoff=true;
#endif
                {clock_exit_checkpoint(__LINE__);break;}
            }
            clock_quick.ui.error_flags|=PQA_ERROR_WIFI;
            if(!launcher_touch_open(&touch)){clock_exit_checkpoint(__LINE__);break;}
        }
#endif
#ifdef WATCH_ATOMIC_PREFERENCES
        if(clock_quick_scope&&!pqa_visible(&clock_quick.ui)&&!pqa_capture(&clock_quick.ui)){
            if(!quick_atomic_close()){clock_exit_checkpoint(__LINE__);break;}
            clock_quick_scope=false;
        }
#endif
        if(picker_open_pending) {
            picker_open_pending=false;
#ifdef WATCH_ATOMIC_PREFERENCES
            picker_save_failed=!clock_face_begin();if(clock_atomic_retained){clock_exit_checkpoint(__LINE__);break;}
#endif
            if(!picker_scratch)picker_scratch=nova_watch_picker_cache_create(clock_viewport.width,clock_viewport.height);
            if(!picker_scratch){watch_face_close(&picker);rt->diagnostic("WATCH_CLOCK picker=out-of-memory");}
        }
        /* Crown close/sleep supersedes any contact action sampled while an
         * accepted display transfer was draining. */
        if(events&3u)picker_select_pending=false;
        if(picker_select_pending) {
            picker_select_pending=false;
#ifdef WATCH_ATOMIC_PREFERENCES
            bool saved=clock_face_save(picker_selection_pending);if(clock_atomic_retained){clock_exit_checkpoint(__LINE__);break;}
#else
            risc_runtime_capability_v1 setting={.struct_size=sizeof(setting)};bool saved=false;
            if(rt->acquire(RISC_KEY_VALUE_CAPABILITY,1,WATCH_FACE_STORE_INSTANCE,&setting)) {
                saved=watch_face_save(setting.api,picker_selection_pending);
                if(!rt->release(&setting)){clock_exit_checkpoint(__LINE__);break;}
            }
#endif
            if(saved) {
                picker.selected=(uint8_t)picker_selection_pending;
                watch_face_close(&picker);nova_watch_picker_cache_destroy(picker_scratch);picker_scratch=NULL;
                picker_open_pending=launcher_swipe_pending=false;
            }
            picker_save_failed=!saved;
            rt->diagnostic(saved?"WATCH_CLOCK face=saved":"WATCH_CLOCK face=save-failed");
        }
        /* A long-held crown gesture dismisses the picker without changing
         * selection; short-release manual sleep is evaluated below. */
        if(picker.open&&(events&1u)){watch_face_close(&picker);nova_watch_picker_cache_destroy(picker_scratch);picker_scratch=NULL;picker_save_failed=false;}
        if(launcher_swipe_pending) {
            /* Always consume the gesture. Leaving this latch set suppresses
             * input, rendering and pacing; it must never become a deferred
             * launch after the service becomes ready. The recognizer already
             * requires neutral and a fresh contact after a qualified swipe. */
            launcher_swipe_pending=false;
            if(clock_navigation_allowed()) {
                /* The completed sharp Clock remains in provider-owned storage.
                 * Incoming Springboard owns the single blur/crossfade and adopts
                 * the still-held contact as drag-only; never fade through black. */
                if(!launcher_touch_close(&touch)) {clock_exit_checkpoint(__LINE__);break;}
#ifdef WATCH_AUTOMATIC_PROVISION
                if(!clock_provision_close()){clock_exit_checkpoint(__LINE__);break;}
#endif
                if(rt->request_launch("springboard.elf")) {
#ifdef WATCH_BLE_BROADCAST
                    clock_handoff=true;
#endif
                    {clock_exit_checkpoint(__LINE__);break;}
                }
                memcpy(sleep_status,"NAVIGATION UNAVAILABLE",sizeof("NAVIGATION UNAVAILABLE"));
                rt->diagnostic("WATCH_CLOCK error=launcher-request");
                if(!launcher_touch_open(&touch) || !alive(&now)) {clock_exit_checkpoint(__LINE__);break;}
                last_activity=now;
            }
        }
#endif
        bool manual=(events&2u) && (uint32_t)(now-armed_at)>=250u;
        if(events&3u)clock_key_trace(!(events&2u)?"WATCH_KEY decision=long-no-sleep":
            !manual?"WATCH_KEY decision=arming-suppressed":
            !watch_capabilities_can_sleep(&device_clients)?"WATCH_KEY decision=capability-missing":
            "WATCH_KEY decision=manual-sleep");
        bool idle=(uint32_t)(now-last_activity)>=
#ifdef PORTABLE_LOW_BATTERY
            clock_quick.idle_ms;
#else
            WATCH_DEFAULT_IDLE_MS;
#endif
#ifdef PORTABLE_LOW_BATTERY
        idle=idle&&clock_quick.idle_ms!=0;
#endif
        /* Native service work may remain active between host requests.
         * Keep automatic idle from blanking Home before that work settles;
         * the next OK checkpoint re-enables the existing idle deadline.
         * An explicit crown sleep request retains its normal refusal path. */
        idle=idle&&serviced==RISC_RUNTIME_CHECKPOINT_OK;
        if(manual && !watch_capabilities_can_sleep(&device_clients)) {
            memcpy(sleep_status,"SLEEP ERR CAPABILITY",sizeof("SLEEP ERR CAPABILITY"));
            clock_sleep_failure(!device_clients.sleep?"POWER-CAPABILITY":"PANEL-CAPABILITY");
        }
        if ((manual || idle) && watch_capabilities_can_sleep(&device_clients)) {
            CLOCK_IDLE_TRACE(manual?"manual":"idle",now,last_activity);
#ifdef WATCH_CLOCK_LAUNCHER
#ifdef WATCH_QUICK_ACTIONS
            pqa_cancel(&clock_quick.ui);(void)pqa_take_action(&clock_quick.ui);
#endif
            CLOCK_TRACE("sleep-touch-close",false,true);
            /* Retained input subscriptions must not veto platform sleep. */
            if(!launcher_touch_close(&touch)) {clock_sleep_failure("TOUCH-CLOSE");{clock_exit_checkpoint(__LINE__);break;}}
            CLOCK_TRACE("sleep-touch-closed",true,false);
            watch_face_close(&picker);nova_watch_picker_cache_destroy(picker_scratch);picker_scratch=NULL;
            picker_open_pending=picker_select_pending=picker_save_failed=false;
#endif
#ifdef WATCH_RUNTIME_FEATURES
            if(!watch_runtime_stage(&native_clock)) {
                if(native_clock.halted){clock_exit_checkpoint(__LINE__);return;}
                clock_sleep_failure("CHECKPOINT-STAGE");{clock_exit_checkpoint(__LINE__);break;}
            }
#endif
            clock_key_trace(manual?"WATCH_KEY sleep-enter=manual":"WATCH_KEY sleep-enter=idle");
            int sleep_result=sleep_cycle();
            if(sleep_result==WATCH_SLEEP_RETAINED){clock_exit_checkpoint(__LINE__);return;} /* Runtime retains before fini. */
#ifdef WATCH_RUNTIME_FEATURES
            if(!watch_runtime_abandon(&native_clock)) {
                if(native_clock.halted){clock_exit_checkpoint(__LINE__);return;}
                clock_sleep_failure("CHECKPOINT-ABANDON");{clock_exit_checkpoint(__LINE__);break;}
            }
#endif
            if (!sleep_result || !alive(&armed_at)) {clock_exit_checkpoint(__LINE__);break;}
            /* A refused/held-key attempt also starts a new bounded interval;
             * it must not turn an expired timeout into a busy retry loop. */
            last_activity=armed_at;
            CLOCK_IDLE_TRACE("wake-baseline",armed_at,last_activity);
#ifdef WATCH_CLOCK_LAUNCHER
            if(!launcher_touch_open(&touch)) {clock_exit_checkpoint(__LINE__);break;}
#endif
            continue;
        }
        risc_display_surface_v1 s={0};
        #ifdef WATCH_INTERACTION_TRACE
        if(clock_quick_first_frame)CLOCK_TRACE("quick-frame-begin",false,false);
#endif
        if (!frame(&s) || !draw_clock(now,&s) || !present()) {clock_exit_checkpoint(__LINE__);break;}
#ifdef WATCH_INTERACTION_TRACE
        if(clock_quick_first_frame){CLOCK_TRACE("quick-frame-done",true,false);clock_quick_first_frame=false;}
#endif
        if(!pace_frame(now)){clock_exit_checkpoint(__LINE__);break;}
#ifdef WATCH_FAST_WAKE
#ifdef WATCH_SERIAL_TRANSFER_MODE
        /* HELLO may arrive during rendering, presentation or pacing. Poll only
         * after those operations settle and before admitting another optional
         * provider. Legacy/null cancellation suffixes keep their old cadence. */
        if(rt->struct_size>=RISC_RUNTIME_EXIT_REQUESTED_V1_SIZE && rt->exit_requested) {
            serviced=risc_runtime_service_checkpoint_v1(rt);
            if(serviced!=RISC_RUNTIME_CHECKPOINT_OK && serviced!=RISC_RUNTIME_CHECKPOINT_BUSY){clock_exit_checkpoint(__LINE__);return;}
            if(risc_runtime_exit_requested_v1(rt)){clock_exit_checkpoint(__LINE__);goto done;}
        }
#endif
        /* present() can latch navigation while a transfer drains. Honor it
         * before entering any optional synchronous provider work. */
#ifdef WATCH_CLOCK_LAUNCHER
        if(launcher_swipe_pending)continue;
#endif
        if(!alive(&now) || !clock_deferred_step(now,serviced)){clock_exit_checkpoint(__LINE__);break;}
#ifdef WATCH_CRASH_REPORT
        if(!clock_crash_step()){clock_exit_checkpoint(__LINE__);break;}
#endif
#ifdef WATCH_AUTOMATIC_PROVISION
        if(!clock_provision_step(now)){clock_exit_checkpoint(__LINE__);break;}
#endif
#ifdef WATCH_CONTEXTS_CLIENT
        if(clock_contexts_handoff){clock_exit_checkpoint(__LINE__);break;}
#endif
#endif
    }
    clock_exit_checkpoint(__LINE__); /* while(alive) ended without an earlier reason */
done:
    if(clock_touch_retained||clock_runtime_unavailable)return;
#ifdef WATCH_NATIVE_ALARM_V2
    if(!clock_alarm_custody())return;
#endif
#ifdef WATCH_AUTOMATIC_PROVISION
    if(!clock_provision_close()){clock_exit_checkpoint(__LINE__);return;}
#endif
#ifdef WATCH_ATOMIC_PREFERENCES
    if(!clock_atomic_finish()){clock_exit_checkpoint(__LINE__);return;}
#endif
#if defined(WATCH_FAST_WAKE) && defined(WATCH_BLE_BROADCAST)
    /* Failed background cleanup has already raised the native retention
     * fence. Do not perform Contexts I/O before the later broadcast check. */
    if(clock_broadcast_uncertain){clock_exit_checkpoint(__LINE__);return;}
#endif
#ifdef WATCH_RUNTIME_FEATURES
    if(native_clock.halted){clock_exit_checkpoint(__LINE__);return;}
#endif
#ifdef WATCH_CONTEXTS_CLIENT
    if(clock_contexts_uncertain||!clock_contexts_pause()){clock_exit_checkpoint(__LINE__);return;}
    if(!portable_contexts_close(&clock_contexts)) {
#ifdef WATCH_RUNTIME_FEATURES
        watch_runtime_retain(rt);
#endif
        {clock_exit_checkpoint(__LINE__);return;}
    }
#endif
#ifdef WATCH_BLE_BROADCAST
    if(clock_broadcast_uncertain){clock_exit_checkpoint(__LINE__);return;}
    if(!clock_handoff && !clock_broadcast_pause()){clock_exit_checkpoint(__LINE__);return;}
    if(!portable_broadcast_close(&clock_broadcast,rt)) {
#ifdef WATCH_RUNTIME_FEATURES
        watch_runtime_retain(rt);
#endif
        {clock_exit_checkpoint(__LINE__);return;}
    }
#endif
#ifdef WATCH_RUNTIME_FEATURES
    if(native_clock.halted){clock_exit_checkpoint(__LINE__);return;}
    if(!watch_runtime_close(&native_clock)){clock_exit_checkpoint(__LINE__);return;}
#endif
#ifdef WATCH_QUICK_ACTIONS
    if(clock_quick.ui.torch && display && !pqa_session_restore(&clock_quick,display))rt->diagnostic("QUICK brightness-restore-failed");
#endif
#ifdef WATCH_CLOCK_ALARMS
    clock_alarm_finish();
#ifdef WATCH_NATIVE_ALARM_V2
    if(!clock_alarm_custody())return;
#endif
#endif
#ifdef WATCH_CLOCK_LAUNCHER
    nova_watch_picker_cache_destroy(picker_scratch);picker_scratch=NULL;
    nova_watch_raster_cache_release();
    if(!launcher_touch_close(&touch))return;
#endif
    if (held && display && display->release) display->release(display->context,held);
    if (have_r) rt->release(&rg);
    if(!watch_capabilities_close(&device_clients)) {
#ifdef WATCH_RUNTIME_FEATURES
        watch_runtime_retain(rt);
#endif
        {clock_exit_checkpoint(__LINE__);return;}
    }
    watch_viewport_close(&clock_viewport);
    if (have_d) rt->release(&dg);
}
