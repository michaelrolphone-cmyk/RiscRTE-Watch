#ifndef WATCH_SLEEP_DEFAULTS_H
#define WATCH_SLEEP_DEFAULTS_H
/* Deployment-selected fallbacks, not persisted preferences or board policy.
 * Always let the pinned portable reader validate an existing record first. */
#include "PortableSleepPolicy.h"
#ifndef WATCH_DEFAULT_SLEEP_MODE
#define WATCH_DEFAULT_SLEEP_MODE PORTABLE_SLEEP_HYBRID
#endif
#ifndef WATCH_DEFAULT_IDLE_SECONDS
#define WATCH_DEFAULT_IDLE_SECONDS 60u
#endif
#if WATCH_DEFAULT_SLEEP_MODE < PORTABLE_SLEEP_LIGHT || WATCH_DEFAULT_SLEEP_MODE > PORTABLE_SLEEP_HYBRID
#error WATCH_DEFAULT_SLEEP_MODE must be light, deep or hybrid
#endif
#if WATCH_DEFAULT_IDLE_SECONDS < 5 || WATCH_DEFAULT_IDLE_SECONDS > 3600
#error WATCH_DEFAULT_IDLE_SECONDS must be between 5 and 3600
#endif
#define WATCH_DEFAULT_IDLE_MS (WATCH_DEFAULT_IDLE_SECONDS * 1000u)
#if WATCH_DEFAULT_SLEEP_MODE == PORTABLE_SLEEP_DEEP
#define WATCH_DEFAULT_SLEEP_NAME "deep"
#elif WATCH_DEFAULT_SLEEP_MODE == PORTABLE_SLEEP_LIGHT
#define WATCH_DEFAULT_SLEEP_NAME "light"
#else
#define WATCH_DEFAULT_SLEEP_NAME "hybrid"
#endif
static inline int watch_sleep_load(const risc_key_value_v1 *kv,unsigned *mode) {
    int loaded=portable_sleep_load(kv,mode);
    if(mode && loaded!=PORTABLE_SLEEP_LOADED)*mode=WATCH_DEFAULT_SLEEP_MODE;
    return loaded;
}
static inline int watch_sleep_timer_load(const risc_key_value_v1 *kv,bool deep,uint32_t *milliseconds) {
    int loaded=portable_sleep_timer_load(kv,deep,milliseconds);
    if(milliseconds && !deep && loaded!=PORTABLE_SLEEP_LOADED)*milliseconds=WATCH_DEFAULT_IDLE_MS;
    return loaded;
}
static inline const char *watch_sleep_fallback_diagnostic(int loaded) {
    switch(loaded) {
    case PORTABLE_SLEEP_MISSING:return "WATCH_CLOCK settings=missing default=" WATCH_DEFAULT_SLEEP_NAME;
    case PORTABLE_SLEEP_INVALID:return "WATCH_CLOCK settings=invalid default=" WATCH_DEFAULT_SLEEP_NAME;
    case PORTABLE_SLEEP_UNAVAILABLE:return "WATCH_CLOCK settings=unavailable default=" WATCH_DEFAULT_SLEEP_NAME;
    default:return 0;
    }
}
#endif
