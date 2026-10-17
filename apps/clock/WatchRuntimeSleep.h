#pragma once
#include "RiscRuntimeSleepV1.h"
#include "watch_sleep.h"
/* App-owned settled boundary. Runtime settles only explicitly selected native
 * services; mapped providers retain hardware ownership and CPU readiness stays
 * authoritative. Ordinary returns restore wake/pad state before this resume. */
static int watch_runtime_sleep_begin(const risc_runtime_api_v1 *rt,uint64_t *token) {
    const int32_t rc=risc_runtime_sleep_prepare_v1(rt,token);
    if(rc==RISC_RUNTIME_SLEEP_RETAINED)return WATCH_SLEEP_RETAINED;
    if(rc==RISC_RUNTIME_SLEEP_OK)return WATCH_SLEEP_WOKE;
    watch_sleep_stage=12;watch_sleep_detail=rc;
    if(rt && rt->diagnostic)rt->diagnostic("WATCH_SLEEP native-services=refused");
    return WATCH_SLEEP_REFUSED;
}
static int watch_runtime_sleep_end(const risc_runtime_api_v1 *rt,uint64_t token,int result) {
    if(result==WATCH_SLEEP_RETAINED)return result;
    const int32_t rc=risc_runtime_sleep_resume_v1(rt,token);
    if(rc==RISC_RUNTIME_SLEEP_OK || rc==RISC_RUNTIME_SLEEP_MEDIA_UNAVAILABLE)return result;
    /* Unknown or refused restoration cannot release the app, provider graph,
     * or borrowed frame custody. Runtime also retains an abandoned token. */
    watch_sleep_stage=12;watch_sleep_detail=rc;
    return WATCH_SLEEP_RETAINED;
}
