/* Application-local sleep preserves the complete physical RGB565 image and
 * original app stack. The runtime selects generic power services, instance 0;
 * neither battery gauges nor display.output carry private sleep suffixes. */
#include "PortableAppSleep.h"
#include "WatchPortableBridgeGuard.h"
#include "watch_sleep.h"
#ifdef PORTABLE_ALARM_CLIENT
#include "watch_alarm_sleep.h"
#ifdef PORTABLE_MOTION_WAKE
#include "watch_motion_client.h"
#endif
#endif
#include <stdlib.h>
#include <string.h>
#ifdef PORTABLE_QUICK_ACTIONS
extern unsigned portable_quick_brightness(void);
#ifdef PORTABLE_LOW_BATTERY
extern uint32_t portable_quick_sleep_light_ms(void);
#endif
#endif

static watch_portable_bridge_guard guard;
static risc_runtime_capability_v1 power_grant,panel_grant;
static const risc_power_sleep_api_v1 *power;
static const risc_display_power_api_v1 *panel;
/* Reachable on terminal return: retained invocations must not free this image. */
static uint16_t *saved;
static bool active;
static bool diagnostic(const char *line) {
    return !guard.retained && watch_bridge_bool(&guard,guard.runtime->diagnostic(line));
}
static int32_t power_result(int32_t result) {
    if(result==RISC_LIGHT_SLEEP_RETAINED || result==RISC_LIGHT_SLEEP_CONTEXT)watch_bridge_retain(&guard);
    return guard.retained?RISC_LIGHT_SLEEP_RETAINED:result;
}
static bool power_prepare(void *c) {
    (void)c;return !guard.retained && watch_bridge_bool(&guard,power->prepare_sleep(power->context));
}
static bool power_resume(void *c) {
    (void)c;return !guard.retained && watch_bridge_bool(&guard,power->resume(power->context));
}
static bool power_pending(void *c,bool *out) {
    (void)c;return !guard.retained && watch_bridge_bool(&guard,power->sleep_wake_pending(power->context,out));
}
static int32_t power_light(void *c,risc_light_sleep_result_v1 *out) {
    (void)c;return guard.retained?RISC_LIGHT_SLEEP_RETAINED:power_result(power->light_sleep(power->context,out));
}
static int32_t power_light_for(void *c,uint32_t ms,risc_light_sleep_result_v1 *out) {
    (void)c;return guard.retained?RISC_LIGHT_SLEEP_RETAINED:power_result(power->light_sleep_for(power->context,ms,out));
}
static int32_t power_light_set(void *c,uint32_t ms,risc_light_sleep_result_v1 *out) {
    (void)c;return guard.retained?RISC_LIGHT_SLEEP_RETAINED:power_result(power->light_sleep_set(power->context,ms,out));
}
static int32_t deep_result(int32_t rc) {return power_result(rc>=0?RISC_DEEP_SLEEP_RETAINED:rc);}
static int32_t power_deep(void *c) {
    (void)c;return guard.retained?RISC_DEEP_SLEEP_RETAINED:deep_result(power->deep_sleep(power->context));
}
static int32_t power_deep_for(void *c,uint32_t ms) {
    (void)c;return guard.retained?RISC_DEEP_SLEEP_RETAINED:deep_result(power->deep_sleep_for(power->context,ms));
}
static int32_t power_deep_set(void *c,uint32_t ms) {
    (void)c;return guard.retained?RISC_DEEP_SLEEP_RETAINED:deep_result(power->deep_sleep_set(power->context,ms));
}
static bool panel_prepare(void *c) {
    (void)c;return !guard.retained && watch_bridge_bool(&guard,panel->prepare_sleep(panel->context));
}
static bool panel_resume(void *c) {
    (void)c;return !guard.retained && watch_bridge_bool(&guard,panel->resume(panel->context));
}
static int32_t panel_deep(void *c) {
    (void)c;return guard.retained?RISC_DEEP_SLEEP_RETAINED:power_result(panel->prepare_deep_sleep(panel->context));
}
static int32_t panel_status(void *c) {
    (void)c;return guard.retained?RISC_DEEP_SLEEP_RETAINED:power_result(panel->resume_status(panel->context));
}

#ifdef PORTABLE_ALARM_CLIENT
static const alarm_service_v1 *alarm;
static int32_t alarm_result(int32_t rc) {
    /* Alarm codes are a distinct domain: ALARM_INVALID is not native CONTEXT.
     * Only failed owner health establishes a terminal invocation here. */
    if(rc<0)(void)watch_bridge_bool(&guard,false);
    return rc;
}
static int32_t alarm_prepare(void *c,alarm_sleep_v1 *out) {
    (void)c;return guard.retained?ALARM_INVALID:alarm_result(alarm->prepare_sleep(alarm->context,out));
}
static int32_t alarm_step(void *c) {
    (void)c;return guard.retained?ALARM_INVALID:alarm_result(alarm->step(alarm->context));
}
static int32_t alarm_status(void *c,alarm_status_v1 *out) {
    (void)c;return guard.retained?ALARM_INVALID:alarm_result(alarm->status(alarm->context,out));
}
static int32_t alarm_refresh(void *c) {
    (void)c;return guard.retained?ALARM_INVALID:alarm_result(alarm->refresh(alarm->context));
}
#ifdef WATCH_ALARM_SLEEP_RESUME
static int32_t alarm_resume(void *c,const alarm_sleep_v1 *decision) {
    (void)c;const alarm_service_sleep_v1 *a=(const alarm_service_sleep_v1 *)alarm;
    return guard.retained?ALARM_INVALID:alarm_result(a->resume_sleep(alarm->context,decision));
}
#endif
#ifdef PORTABLE_MOTION_WAKE
static const risc_key_value_v1 *settings;
static const twatch_motion_api_v1 *motion;
static risc_key_value_v1 guarded_settings;
static twatch_motion_api_v1 guarded_motion;
static int32_t settings_get(void *c,const char *key,void *buf,uint32_t cap,uint32_t *length) {
    (void)c;if(guard.retained)return RISC_KEY_VALUE_CONTEXT;
    int32_t rc=settings->get(settings->context,key,buf,cap,length);
    if(rc==RISC_KEY_VALUE_CONTEXT)watch_bridge_retain(&guard);
    else if(rc<0 && rc!=RISC_KEY_VALUE_NOT_FOUND)(void)watch_bridge_bool(&guard,false);
    return rc;
}
static bool motion_prepare(void *c) {
    (void)c;return !guard.retained && watch_bridge_bool(&guard,motion->prepare_wake(motion->context));
}
static bool motion_pending(void *c,bool *pending) {
    (void)c;return !guard.retained && watch_bridge_bool(&guard,motion->wake_pending(motion->context,pending));
}
static bool motion_resume(void *c) {
    (void)c;if(guard.retained)return false;
    bool ok=motion->resume_wake(motion->context);
    if(!ok)watch_bridge_retain(&guard); /* The policy defines failed unarm as retained. */
    return ok;
}
static bool motion_info(void *c,twatch_tap_info_v1 *info) {
    (void)c;return !guard.retained && watch_bridge_bool(&guard,motion->tap_info(motion->context,info));
}
static bool motion_configure(void *c,uint16_t value) {
    (void)c;return !guard.retained && watch_bridge_bool(&guard,motion->tap_configure(motion->context,value));
}
static uint32_t motion_error(void *c) {
    (void)c;return guard.retained?0:motion->wake_error(motion->context);
}
static bool service_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *out) {
    if(!watch_bridge_acquire(&guard,name,version,instance,out))return false;
    if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY) && portable_tap_store_valid(out->api)) {
        settings=out->api;guarded_settings=*settings;guarded_settings.get=settings_get;
        out->api=&guarded_settings;
    } else if(!strcmp(name,TWATCH_MOTION_CAPABILITY) && watch_motion_ready(out->api) &&
              portable_motion_tap_valid(out->api)) {
        motion=out->api;guarded_motion=*motion;
        guarded_motion.prepare_wake=motion_prepare;guarded_motion.wake_pending=motion_pending;
        guarded_motion.resume_wake=motion_resume;guarded_motion.tap_info=motion_info;
        guarded_motion.tap_configure=motion_configure;
        if(motion->wake_error)guarded_motion.wake_error=motion_error;
        out->api=&guarded_motion;
    }
    return true;
}
static bool service_release(risc_runtime_capability_v1 *grant) {
    if(guard.retained)return false;
    risc_runtime_capability_v1 original=*grant;
    if(original.api==&guarded_settings)original.api=settings;
    if(original.api==&guarded_motion)original.api=motion;
    if(!watch_bridge_release(&guard,&original))return false;
    *grant=(risc_runtime_capability_v1){0};return true;
}
#endif
#endif

static bool display_valid(const risc_display_output_api_v1 *d) {
    return d && d->api_version==1 && d->struct_size>=sizeof(*d) &&
        d->get_info && d->acquire && d->release && d->submit && d->present_status && d->set_brightness;
}
static bool surface_valid(const risc_display_surface_v1 *s,uint32_t width,uint32_t height) {
    return s->frame && s->pixels && s->width==width && s->height==height &&
        s->pixel_format==RISC_DISPLAY_FORMAT_RGB565 && width && height &&
        width<=UINT32_MAX/2u && s->stride_bytes>=width*2u &&
        (uint64_t)(height-1u)*s->stride_bytes+(uint64_t)width*2u<=s->size_bytes;
}
static bool acquire_surface(const risc_display_output_api_v1 *d,risc_display_surface_v1 *s) {
    if(guard.retained)return false;
    bool ok=d->acquire(d->context,RISC_DISPLAY_FORMAT_RGB565,s);
    if(!ok && (s->frame || s->pixels)){watch_bridge_retain(&guard);return false;}
    return watch_bridge_bool(&guard,ok);
}
static void release_surface(const risc_display_output_api_v1 *d,risc_display_surface_v1 *s) {
    if(guard.retained || !s->frame)return;
    d->release(d->context,s->frame);s->frame=0;
    /* release has no status channel; ensure the invocation still exists. */
    (void)watch_bridge_bool(&guard,false);
}
static bool transfer(const risc_display_output_api_v1 *d,uint32_t width,uint32_t height,const uint16_t *pixels) {
    risc_display_surface_v1 s={0};
    if(!acquire_surface(d,&s))return false;
    if(!surface_valid(&s,width,height)){release_surface(d,&s);return false;}
    size_t row_bytes=(size_t)width*2u;
    for(uint32_t y=0;y<height;y++) {
        void *row=(uint8_t *)s.pixels+(size_t)y*s.stride_bytes;
        if(pixels)memcpy(row,(const uint8_t *)pixels+(size_t)y*row_bytes,row_bytes);
        else memset(row,0,row_bytes);
    }
    const risc_display_present_options_v1 options={0,0,0};
    risc_display_present_token_v1 token=0;
    bool submitted=d->submit(d->context,s.frame,NULL,0,&options,&token);
    if(!submitted) {
        if(token)watch_bridge_retain(&guard);
        else if(watch_bridge_bool(&guard,false),!guard.retained)release_surface(d,&s);
        return false;
    }
    if(!token){watch_bridge_retain(&guard);return false;}
    for(unsigned n=0;n<10000;n++) {
        risc_display_present_status_v1 status={0};
        /* An unobservable submission is still provider-owned. Do not free or
         * unblank when completion/retirement cannot be established. */
        if(!d->present_status(d->context,token,&status)){watch_bridge_retain(&guard);return false;}
        if(status.state==RISC_DISPLAY_PRESENT_COMPLETE)return true;
        if(status.state==RISC_DISPLAY_PRESENT_FAILED || status.state==RISC_DISPLAY_PRESENT_SUPERSEDED)return false;
        if(status.state!=RISC_DISPLAY_PRESENT_QUEUED && status.state!=RISC_DISPLAY_PRESENT_ACTIVE){watch_bridge_retain(&guard);return false;}
        guard.runtime->yield_ms(1);
        /* Cooperative polling may itself invalidate the owner invocation. */
        (void)watch_bridge_bool(&guard,false);
        if(guard.retained)return false;
    }
    watch_bridge_retain(&guard);return false;
}

static bool restore(const risc_display_output_api_v1 *d,uint32_t width,uint32_t height) {
    return watch_bridge_bool(&guard,d->set_brightness(d->context,0,100)) &&
        transfer(d,width,height,saved) && watch_bridge_bool(&guard,d->set_brightness(d->context,
#ifdef PORTABLE_QUICK_ACTIONS
            (uint16_t)portable_quick_brightness(),
#else
            40,
#endif
            100));
}

#ifdef PORTABLE_ALARM_CLIENT
int portable_app_alarm_sleep(const risc_runtime_api_v1 *rt,const risc_display_output_api_v1 *d,
        const risc_battery_gauge_api_v1 *gauge,const alarm_service_v1 *alarms) {
#else
int portable_app_sleep(const risc_runtime_api_v1 *rt,const risc_display_output_api_v1 *d,
        const risc_battery_gauge_api_v1 *gauge) {
#endif
    (void)gauge;
    if(guard.retained)return WATCH_SLEEP_RETAINED;
    if(active || !watch_bridge_runtime_valid(rt))return WATCH_SLEEP_FAILED;
    guard.runtime=rt;active=true;
    int rc=WATCH_SLEEP_REFUSED;
    bool darkened=false,policy_started=false;
    uint32_t width=0,height=0;
    if(!display_valid(d))goto done;
    if(!watch_bridge_acquire(&guard,RISC_POWER_SLEEP_CAPABILITY,1,0,&power_grant))goto done;
    power=power_grant.api;
    if(!power || power->api_version!=1 || power->struct_size<RISC_POWER_TIMED_SLEEP_SIZE ||
       !power->prepare_sleep || !power->resume || !power->light_sleep_for || !power->sleep_wake_pending)goto done;
    if(!watch_bridge_acquire(&guard,RISC_DISPLAY_POWER_CAPABILITY,1,0,&panel_grant))goto done;
    panel=panel_grant.api;
    if(!panel || panel->api_version!=1 || panel->struct_size<RISC_DISPLAY_POWER_LIGHT_SLEEP_SIZE ||
       !panel->prepare_sleep || !panel->resume)goto done;
    risc_power_sleep_api_v1 guarded_power={.api_version=1,.struct_size=power->struct_size,
        .prepare_sleep=power_prepare,.resume=power_resume,.light_sleep=power->light_sleep?power_light:NULL,
        .deep_sleep=power->deep_sleep?power_deep:NULL,.light_sleep_for=power_light_for,.sleep_wake_pending=power_pending};
    if(power->struct_size>=RISC_POWER_TIMED_DEEP_SLEEP_SIZE && power->deep_sleep_for)guarded_power.deep_sleep_for=power_deep_for;
    if(power->struct_size>=RISC_POWER_WAKE_SET_SIZE) {
        if(power->light_sleep_set)guarded_power.light_sleep_set=power_light_set;
        if(power->deep_sleep_set)guarded_power.deep_sleep_set=power_deep_set;
    }
    risc_display_power_api_v1 guarded_panel={.api_version=1,.struct_size=panel->struct_size,
        .prepare_sleep=panel_prepare,.resume=panel_resume};
    if(panel->struct_size>=RISC_DISPLAY_POWER_DEEP_SLEEP_SIZE && panel->prepare_deep_sleep)guarded_panel.prepare_deep_sleep=panel_deep;
    if(panel->struct_size>=RISC_DISPLAY_POWER_RESUME_STATUS_SIZE && panel->resume_status)guarded_panel.resume_status=panel_status;
#ifdef PORTABLE_ALARM_CLIENT
    alarm=alarms;
    if(alarms && (alarms->api_version!=1 || alarms->struct_size<sizeof(*alarms) ||
       !alarms->prepare_sleep || !alarms->step || !alarms->status || !alarms->refresh || !watch_alarm_resume_ready(alarms)))goto done;
    alarm_service_sleep_v1 guarded_alarm={0};
    if(alarms) {
        guarded_alarm.base=*alarms;guarded_alarm.base.prepare_sleep=alarm_prepare;
        guarded_alarm.base.step=alarm_step;guarded_alarm.base.status=alarm_status;guarded_alarm.base.refresh=alarm_refresh;
#ifdef WATCH_ALARM_SLEEP_RESUME
        guarded_alarm.resume_sleep=alarm_resume;
#endif
    }
#endif
    risc_display_info_v1 info={.struct_size=sizeof(info)};
    if(!watch_bridge_bool(&guard,d->get_info(d->context,&info)))goto done;
    if(info.api_version!=1 || info.struct_size<sizeof(info) || !info.width || !info.height ||
       info.width>UINT32_MAX/2u || info.width*2u>UINT32_MAX/info.height ||
       (size_t)info.width>SIZE_MAX/2u/info.height)goto done;
#ifdef PORTABLE_LOW_BATTERY
    watch_sleep_light_ms=portable_quick_sleep_light_ms();
#endif
    width=info.width;height=info.height;
    saved=malloc((size_t)info.width*info.height*2u);
    if(!saved)goto done;
    risc_display_surface_v1 s={0};rc=WATCH_SLEEP_FAILED;
    if(!acquire_surface(d,&s))goto done;
    bool valid=surface_valid(&s,info.width,info.height);
    if(valid)for(uint32_t y=0;y<info.height;y++)
        memcpy(saved+(size_t)y*info.width,(uint8_t *)s.pixels+(size_t)y*s.stride_bytes,(size_t)info.width*2u);
    release_surface(d,&s);
    if(!valid || guard.retained)goto done;
    /* A false brightness result need not mean hardware was unchanged. */
    darkened=true;
    if(!watch_bridge_bool(&guard,d->set_brightness(d->context,0,100)))goto done;
    if(!transfer(d,info.width,info.height,NULL))goto done;
    policy_started=true;
#ifdef PORTABLE_ALARM_CLIENT
#ifdef PORTABLE_MOTION_WAKE
    risc_runtime_api_v1 guarded_runtime={.api_version=1,.struct_size=sizeof(guarded_runtime),
        .acquire=service_acquire,.release=service_release,.diagnostic=diagnostic};
    rc=watch_motion_sleep(&guarded_runtime,&guarded_panel,&guarded_power,PORTABLE_SLEEP_HYBRID,
                         alarms?&guarded_alarm.base:NULL);
#else
    rc=alarms?watch_alarm_sleep_prepared(&guarded_panel,&guarded_power,PORTABLE_SLEEP_HYBRID,&guarded_alarm.base,diagnostic):
              watch_sleep_prepared(&guarded_panel,&guarded_power,PORTABLE_SLEEP_HYBRID,diagnostic);
#endif
#else
    rc=watch_sleep_prepared(&guarded_panel,&guarded_power,PORTABLE_SLEEP_HYBRID,diagnostic);
#endif
    if(rc==WATCH_SLEEP_RETAINED)watch_bridge_retain(&guard);
    if(guard.retained)goto done;
    if(rc>=0 && !restore(d,width,height))rc=WATCH_SLEEP_FAILED;
done:
    if(guard.retained)return WATCH_SLEEP_RETAINED;
    /* Before power preparation, an ordinary display refusal leaves no pending
     * ownership. Restore the snapshot once before returning the failure. An
     * unobservable submission already fenced above and cannot enter here. */
    if(darkened && !policy_started && saved && !restore(d,width,height))rc=WATCH_SLEEP_FAILED;
    if(guard.retained)return WATCH_SLEEP_RETAINED;
    /* A release refusal fences before even the next release or free. */
    if(!watch_bridge_release(&guard,&panel_grant) || !watch_bridge_release(&guard,&power_grant))return WATCH_SLEEP_RETAINED;
    free(saved);saved=NULL;power=NULL;panel=NULL;active=false;
    return rc;
}
