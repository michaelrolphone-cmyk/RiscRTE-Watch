#pragma once
#include "watch_sleep.h"
#ifdef WATCH_NATIVE_ALARM_V2
#ifndef ALARM_SERVICE_TAGGED_V2
#error Native Home alarms require explicitly negotiated tagged API2
#endif
#include "PortableAlarmClient.h"
/* Reuse the shared client's result taxonomy. No separate adapter is linked into
 * Home: a terminal result is returned to its native invocation owner below. */
static int32_t watch_alarm_checked(const alarm_service_v1 *a,int32_t rc){
 portable_alarm_client client={.api=a};
 return portable_alarm_result(&client,rc)?rc:ALARM_RETAINED;
}
#else
#include "AlarmServiceV1.h"
#endif
#include "RiscTimedSleepV1.h"
/* Reuse owned crown sleep with an optional alarm deadline. No persistent
 * suppression: every entry, refusal and timed-Light boundary reconciles anew. */
static int32_t watch_alarm_deadline(const alarm_service_v1 *a,alarm_sleep_v1 *decision) {
    for(unsigned n=0;n<64;n++) {
#ifdef WATCH_NATIVE_ALARM_V2
        if(!alarm_service_descriptor(a))return ALARM_INVALID;
#endif
        *decision=(alarm_sleep_v1){.struct_size=sizeof(*decision)};
        int32_t rc=a->prepare_sleep(a->context,decision);
#ifdef WATCH_NATIVE_ALARM_V2
        rc=watch_alarm_checked(a,rc);
        if(rc==ALARM_OK && decision->struct_size!=sizeof(*decision))return ALARM_INVALID;
#endif
        if(rc!=ALARM_PENDING)return rc;
#ifdef WATCH_NATIVE_ALARM_V2
        rc=watch_alarm_checked(a,a->step(a->context));
        if(rc!=ALARM_OK && rc!=ALARM_PENDING)return rc;
        alarm_status_v1 status={.struct_size=sizeof(status)};
        rc=watch_alarm_checked(a,a->status(a->context,&status));
        if(rc!=ALARM_OK)return rc;
        rc=watch_alarm_checked(a,status.error);if(rc==ALARM_RETAINED)return rc;
        if(status.api_version!=1 || status.struct_size<sizeof(status) || status.state>ALARM_STATE_CUE)return ALARM_INVALID;
#else
        (void)a->step(a->context);
        alarm_status_v1 status={.struct_size=sizeof(status)};
        if(a->status(a->context,&status)!=ALARM_OK)return ALARM_INVALID;
#endif
        if(status.state==ALARM_STATE_BLOCKED)return status.error<0?status.error:ALARM_INVALID;
        if(status.occurrence.generation || status.output_uncertain
#ifdef ALARM_STATUS_CUE_SUPPORTED
           ||status.state==ALARM_STATE_CUE
#endif
           )return ALARM_BUSY;
    }
    return ALARM_BUSY;
}
static uint32_t watch_alarm_duration(const alarm_sleep_v1 *s) {
    if(!s->deadline)return 0;
    if(s->deadline<=s->rtc_seconds)return 1;
    uint32_t seconds=s->deadline-s->rtc_seconds;
#ifdef WATCH_NATIVE_ALARM_V2
    /* UTC samples contain whole seconds. Wake conservatively before the
       possible fractional deadline; this is not a physical latency bound. */
    if(seconds>RISC_TIMED_SLEEP_MAX_MS/1000u)return RISC_TIMED_SLEEP_MAX_MS;
    return seconds>1u?(seconds-1u)*1000u:1u;
#else
    return seconds>RISC_TIMED_SLEEP_MAX_MS/1000u?RISC_TIMED_SLEEP_MAX_MS:seconds*1000u;
#endif
}
/* Current cohorts explicitly require the append-only service sleep suffix.
 * Frozen historical builds keep the old prefix contract and source inputs. */
static bool watch_alarm_resume_ready(const alarm_service_v1 *a) {
#ifdef WATCH_NATIVE_ALARM_V2
    const alarm_service_descriptor_v2 *descriptor=alarm_service_descriptor(a);
    return descriptor && descriptor->resume_sleep;
#elif defined(WATCH_ALARM_SLEEP_RESUME)
    const alarm_service_sleep_v1 *extended=(const alarm_service_sleep_v1 *)a;
    return a && a->api_version==ALARM_SERVICE_API_V1 &&
        a->struct_size>=ALARM_SERVICE_SLEEP_V1_SIZE && extended->resume_sleep;
#else
    (void)a;return true;
#endif
}
static int32_t watch_alarm_resume_light(const alarm_service_v1 *a,const alarm_sleep_v1 *decision) {
#ifdef WATCH_NATIVE_ALARM_V2
    return watch_alarm_checked(a,alarm_service_resume(a,decision));
#elif defined(WATCH_ALARM_SLEEP_RESUME)
    const alarm_service_sleep_v1 *extended=(const alarm_service_sleep_v1 *)a;
    return watch_alarm_resume_ready(a)?extended->resume_sleep(a->context,decision):ALARM_INVALID;
#else
    (void)a;(void)decision;return ALARM_OK;
#endif
}
/* No service or storage calls belong after this pad-hold boundary. */
static int32_t watch_alarm_enter_deep(const risc_display_power_api_v1 *panel,
        const risc_power_sleep_api_v1 *pmu,const twatch_motion_api_v1 *motion,uint32_t duration) {
    if(panel->struct_size<RISC_DISPLAY_POWER_RESUME_STATUS_SIZE || !panel->prepare_deep_sleep || !panel->resume_status ||
       pmu->struct_size<RISC_POWER_TIMED_SLEEP_SIZE || !pmu->sleep_wake_pending ||
       (duration?(pmu->struct_size<RISC_POWER_TIMED_DEEP_SLEEP_SIZE || !pmu->deep_sleep_for):!pmu->deep_sleep))
        return RISC_DEEP_SLEEP_UNSUPPORTED;
    int32_t rc=panel->prepare_deep_sleep(panel->context);
    if(rc!=0)return rc;
    bool pending=true;
    if(!watch_wake_pending(pmu,motion,&pending))return RISC_DEEP_SLEEP_PLATFORM;
    if(pending)return RISC_DEEP_SLEEP_ACTIVE_WAKE;
    rc=watch_enter_deep(pmu,motion,duration);
    return rc>=0?RISC_DEEP_SLEEP_RETAINED:rc;
}
static int watch_alarm_sleep_motion_prepared(const risc_display_power_api_v1 *panel,
        const risc_power_sleep_api_v1 *pmu,const twatch_motion_api_v1 *motion,unsigned mode,const alarm_service_v1 *a,
        bool (*diagnostic)(const char *)) {
    bool deep=mode==PORTABLE_SLEEP_DEEP,hybrid=mode==PORTABLE_SLEEP_HYBRID;
    int32_t rc=RISC_LIGHT_SLEEP_INVALID;
    /* The ordinary panel sleep performs its120ms delay without a native pad
       hold. Reconcile AFTER that delay but BEFORE Deep creates an exit barrier.
       Runtime must never see service KV while appExitSafe is false. */
    watch_sleep_stage=1;watch_sleep_detail=0;
    if(!watch_alarm_resume_ready(a)){watch_sleep_stage=4;watch_sleep_detail=ALARM_INVALID;return WATCH_SLEEP_REFUSED;}
    if(!watch_motion_ready(motion))return WATCH_SLEEP_REFUSED;
    bool motion_ok=!motion || motion->prepare_wake(motion->context);
    if(motion_ok)watch_sleep_stage=2;
    bool panel_ok=motion_ok && panel->prepare_sleep(panel->context);
    if(panel_ok)watch_sleep_stage=3;
    bool pmu_ok=panel_ok && pmu->prepare_sleep(pmu->context);
    risc_light_sleep_result_v1 result={.struct_size=sizeof(result)};
    alarm_sleep_v1 decision={0};
    int32_t alarm_result=ALARM_INVALID;
    bool alarm_failed=false;
    if(pmu_ok){
        watch_sleep_stage=4;alarm_result=watch_alarm_deadline(a,&decision);watch_sleep_detail=alarm_result;
#ifdef WATCH_NATIVE_ALARM_V2
        if(alarm_result==ALARM_RETAINED)return WATCH_SLEEP_RETAINED;
#endif
        alarm_failed=alarm_result!=ALARM_OK && alarm_result!=ALARM_BUSY;
    }
    if(pmu_ok && alarm_result==ALARM_OK) {
        watch_sleep_stage=deep?6:5;
        uint32_t duration=watch_alarm_duration(&decision);
        if(deep)rc=watch_alarm_enter_deep(panel,pmu,motion,duration);
        else {
            /* No alarm deadline means no alarm-imposed limit, not an
             * instruction to discard the configured Hybrid transition.
             * The same Light-first policy applies to both service ABIs. */
            bool hybrid_timer=hybrid && (!duration || duration>WATCH_SLEEP_LIGHT_MS);
            uint32_t light=hybrid_timer?WATCH_SLEEP_LIGHT_MS:duration;
            rc=light?(pmu->struct_size>=RISC_POWER_TIMED_SLEEP_SIZE && pmu->light_sleep_for?
                watch_enter_light(pmu,motion,light,&result):RISC_LIGHT_SLEEP_UNSUPPORTED):
                watch_enter_light(pmu,motion,0,&result);
            /* Only successful native Light sleep may consume this exact plan.
             * Reanchor before any service step/refresh or Deep pad hold. A
             * refusal/retained native result never authorizes this boundary. */
            if(rc==RISC_LIGHT_SLEEP_OK) {
                alarm_result=watch_alarm_resume_light(a,&decision);
#ifdef WATCH_NATIVE_ALARM_V2
                if(alarm_result==ALARM_RETAINED)return WATCH_SLEEP_RETAINED;
#endif
                if(alarm_result!=ALARM_OK) {
                    watch_sleep_stage=4;watch_sleep_detail=alarm_result;alarm_failed=true;
                    rc=RISC_LIGHT_SLEEP_PLATFORM;
                }
            }
            if(rc==RISC_LIGHT_SLEEP_OK && result.wake_cause==RISC_LIGHT_SLEEP_WAKE_TIMER && hybrid_timer) {
                bool pending=true;
                if(!pmu->sleep_wake_pending || !watch_wake_pending(pmu,motion,&pending))rc=RISC_LIGHT_SLEEP_PLATFORM;
                else if(!pending) {
                    alarm_result=watch_alarm_deadline(a,&decision);
#ifdef WATCH_NATIVE_ALARM_V2
                    if(alarm_result==ALARM_RETAINED)return WATCH_SLEEP_RETAINED;
#endif
                    if(alarm_result==ALARM_OK) {
                        /* Fresh RTC-backed decision first; hold and recheck crown
                           immediately afterward, then owned entry. No post-hold KV. */
                        duration=watch_alarm_duration(&decision);watch_sleep_stage=6;
                        rc=watch_alarm_enter_deep(panel,pmu,motion,duration);
                    } else if(alarm_result!=ALARM_BUSY) {
                        /* A due occurrence wakes normally; a failed service
                         * reconciliation must not masquerade as a successful
                         * wake or lose its RTC/storage error to automatic retry. */
                        watch_sleep_stage=4;watch_sleep_detail=alarm_result;alarm_failed=true;
                        rc=RISC_LIGHT_SLEEP_PLATFORM;
                    }
                }
            }
        }
    }
    if(watch_sleep_stage==5 || watch_sleep_stage==6)watch_sleep_detail=rc;
    if(rc==RISC_LIGHT_SLEEP_RETAINED){
#ifndef WATCH_NATIVE_ALARM_V2
        diagnostic("WATCH_ALARM sleep=retained");
#endif
        return WATCH_SLEEP_RETAINED;
    }
    if(motion && !motion->resume_wake(motion->context)){diagnostic("WATCH_ALARM motion=restore-retained");return WATCH_SLEEP_RETAINED;}
    bool pmu_restored=pmu->resume(pmu->context);
#ifdef WATCH_NATIVE_ALARM_V2
    if(!pmu_restored)return WATCH_SLEEP_RETAINED;
#endif
    int32_t panel_restore=panel->struct_size>=RISC_DISPLAY_POWER_RESUME_STATUS_SIZE && panel->resume_status?
        panel->resume_status(panel->context):(panel->resume(panel->context)?0:RISC_LIGHT_SLEEP_PLATFORM);
    if(panel_restore==RISC_DEEP_SLEEP_RETAINED)return WATCH_SLEEP_RETAINED;
    if(!pmu_restored || panel_restore!=0){
        diagnostic(!pmu_restored?"WATCH_ALARM restore=power-failed":"WATCH_ALARM restore=panel-failed");
        return WATCH_SLEEP_FAILED;
    }
#ifdef WATCH_NATIVE_ALARM_V2
    if(!alarm_failed){
        alarm_result=watch_alarm_checked(a,a->refresh(a->context));
        if(alarm_result==ALARM_RETAINED)return WATCH_SLEEP_RETAINED;
        if(alarm_result!=ALARM_OK && alarm_result!=ALARM_PENDING)return WATCH_SLEEP_REFUSED;
    }
#else
    if(!alarm_failed)(void)a->refresh(a->context);
#endif
    return rc==RISC_LIGHT_SLEEP_OK && !deep?WATCH_SLEEP_WOKE:WATCH_SLEEP_REFUSED;
}

static int watch_alarm_sleep_prepared(const risc_display_power_api_v1 *panel,const risc_power_sleep_api_v1 *pmu,
        unsigned mode,const alarm_service_v1 *a,bool (*diagnostic)(const char *)){
    return watch_alarm_sleep_motion_prepared(panel,pmu,NULL,mode,a,diagnostic);
}
