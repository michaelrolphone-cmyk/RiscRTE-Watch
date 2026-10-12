#include "WatchCapabilityClients.h"
#include <string.h>
static const char *const ids[]={RISC_BATTERY_GAUGE_CAPABILITY,RISC_POWER_SLEEP_CAPABILITY,
    RISC_POWER_KEY_CAPABILITY,RISC_DISPLAY_POWER_CAPABILITY};
bool watch_capabilities_open(watch_capability_clients *c,const risc_runtime_api_v1 *rt) {
    if(!c||!rt||rt->api_version!=1||rt->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE||!rt->acquire||!rt->release)return false;
    *c=(watch_capability_clients){.runtime=rt};
    for(unsigned i=0;i<4;i++) {
        c->grants[i].struct_size=sizeof(c->grants[i]);
        /* Optional capabilities can be absent. Selection belongs to the board
         * profile; instance zero never embeds a physical device identity. */
        if(!rt->acquire(ids[i],1,0,&c->grants[i]))continue;
        c->live[i]=true;
        const void *raw=c->grants[i].api;
        if(!raw)return false;
        if(i==0) {
            const risc_battery_gauge_api_v1 *a=raw;
            if(a->api_version!=1||a->struct_size<sizeof(*a)||!a->read)return false;
            c->battery=a;
        } else if(i==1) {
            const risc_power_sleep_api_v1 *a=raw;
            if(a->api_version!=1||a->struct_size<RISC_POWER_LIGHT_SLEEP_SIZE||!a->prepare_sleep||!a->resume||!a->light_sleep)return false;
            c->sleep=a;
        } else if(i==2) {
            const risc_power_key_api_v1 *a=raw;
            if(a->api_version!=1||a->struct_size<sizeof(*a)||!a->events)return false;
            c->key=a;
        } else {
            const risc_display_power_api_v1 *a=raw;
            if(a->api_version!=1||a->struct_size<RISC_DISPLAY_POWER_LIGHT_SLEEP_SIZE||!a->prepare_sleep||!a->resume)return false;
            c->display_power=a;
        }
    }
    return true;
}
bool watch_capabilities_close(watch_capability_clients *c) {
    if(!c||!c->runtime)return true;
    for(unsigned i=4;i>0;i--)if(c->live[i-1]) {
        if(!c->runtime->release(&c->grants[i-1]))return false;
        c->live[i-1]=false;
    }
    *c=(watch_capability_clients){0};return true;
}
bool watch_capabilities_can_sleep(const watch_capability_clients *c) {
    return c&&c->sleep&&c->display_power;
}
bool watch_capabilities_key_events(const watch_capability_clients *c,uint32_t *out) {
    if(!c||!out)return false;
    *out=0;
    return !c->key || c->key->events(c->key->context,out);
}
