#pragma once
/* Local client lifetime guard. A terminal fence is invocation-long: callers
 * must stop provider I/O and retain allocations/grants until Runtime restart. */
#include "RiscRuntimeV1.h"
typedef struct {
    const risc_runtime_api_v1 *runtime;
    bool retained;
} watch_portable_bridge_guard;
static inline void watch_bridge_retain(watch_portable_bridge_guard *g) {
    if(g->retained)return;
    g->retained=true;
#ifdef RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE
    if(g->runtime && g->runtime->struct_size>=RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE && g->runtime->retain_invocation)
        (void)g->runtime->retain_invocation();
#endif
}
static inline bool watch_bridge_runtime_valid(const risc_runtime_api_v1 *rt) {
    return rt && rt->api_version==RISC_RUNTIME_API_V1 &&
        rt->struct_size>=RISC_RUNTIME_CAPABILITIES_V1_SIZE &&
        rt->acquire && rt->release && rt->health && rt->diagnostic && rt->yield_ms;
}
static inline bool watch_bridge_grant_populated(const risc_runtime_capability_v1 *g) {
    return g->api || g->slot || g->generation;
}
/* False bool APIs do not encode CONTEXT. A fresh failed owner health check
 * distinguishes a lost invocation from an ordinary healthy refusal. */
static inline bool watch_bridge_bool(watch_portable_bridge_guard *g,bool ok) {
    if(g->retained)return false;
    if(!ok) {
        risc_runtime_health_v1 health={.struct_size=sizeof(health)};
        if(!g->runtime->health(&health))watch_bridge_retain(g);
    }
    return ok;
}
static inline bool watch_bridge_acquire(watch_portable_bridge_guard *g,
        const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *out) {
    if(g->retained)return false;
    *out=(risc_runtime_capability_v1){.struct_size=sizeof(*out)};
    bool ok=g->runtime->acquire(name,version,instance,out);
    if(!ok && (watch_bridge_grant_populated(out) || out->struct_size!=sizeof(*out))) {watch_bridge_retain(g);return false;}
    return watch_bridge_bool(g,ok);
}
static inline bool watch_bridge_release(watch_portable_bridge_guard *g,risc_runtime_capability_v1 *grant) {
    if(g->retained)return false;
    if(!watch_bridge_grant_populated(grant))return true;
    risc_runtime_capability_v1 original=*grant;
    if(!g->runtime->release(&original)){watch_bridge_retain(g);return false;}
    *grant=(risc_runtime_capability_v1){0};return true;
}
