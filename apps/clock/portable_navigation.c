/* App-local logical key navigation. Physical input and emergency reboot remain
 * exclusively provider-owned; no battery or board-private API is inspected. */
#include "PortableNavigation.h"
#include "RiscPowerKeyV1.h"
#include "WatchPortableBridgeGuard.h"
#include <string.h>

static risc_runtime_capability_v1 grant;
static const risc_power_key_api_v1 *key;
static watch_portable_bridge_guard guard;
static bool suppressed=true,neutral;

bool portable_input_navigation_retained(void) {return guard.retained;}

static bool reset(void *context) {
    (void)context;
    neutral=false;
    if(guard.retained || !key)return false;
    /* A modal/foreground owner must keep its own completed input events. */
    if(suppressed)return true;
    uint32_t discarded=0;
    return watch_bridge_bool(&guard,key->events(key->context,&discarded));
}
static bool poll(void *context,risc_input_navigation_frame_v1 *out) {
    (void)context;
    if(guard.retained || !key || !out)return false;
    *out=(risc_input_navigation_frame_v1){0};
    if(suppressed)return true;
    uint32_t events=0;
    if(!watch_bridge_bool(&guard,key->events(key->context,&events))){neutral=false;return false;}
    /* Completed short events become Back only after an event-free sample.
     * Long, combined and unknown events disarm; they never synthesize Back. */
    if(!events)neutral=true;
    else if(neutral && events==RISC_POWER_KEY_SHORT_PRESS) {
        out->pressed=out->released=RISC_NAV_BACK;neutral=false;
    } else neutral=false;
    return true;
}
static bool foreground(void *context,const risc_input_foreground_v1 *claims,size_t count) {
    (void)context;
    suppressed=true;neutral=false;
    if(guard.retained || !key || count>RISC_INPUT_NAVIGATION_MAX_FOREGROUND || (count && !claims))return false;
    bool overlap=false;
    for(size_t i=0;i<count;i++) {
        if(!claims[i].capability || !claims[i].api_version)return false;
        if(!strcmp(claims[i].capability,"input.navigation") ||
           !strcmp(claims[i].capability,RISC_POWER_KEY_CAPABILITY))overlap=true;
    }
    if(overlap)return true;
    /* The old owner has released its claim. Discard boundary events before
     * accepting new input, but leave failure suppressed. */
    suppressed=false;
    if(!reset(NULL)){suppressed=true;return false;}
    return true;
}
static const risc_input_navigation_api_v1 api={1,sizeof(api),NULL,poll,foreground,reset};

const risc_input_navigation_api_v1 *portable_input_navigation_open(const risc_runtime_api_v1 *runtime) {
    if(guard.retained || watch_bridge_grant_populated(&grant) || !watch_bridge_runtime_valid(runtime))return NULL;
    guard.runtime=runtime;suppressed=true;neutral=false;key=NULL;
    if(!watch_bridge_acquire(&guard,RISC_POWER_KEY_CAPABILITY,1,0,&grant))return NULL;
    key=grant.api;
    if(!key || key->api_version!=1 || key->struct_size<sizeof(*key) || !key->events) {
        if(watch_bridge_release(&guard,&grant))key=NULL;
        return NULL;
    }
    return &api;
}
void portable_input_navigation_close(const risc_runtime_api_v1 *runtime) {
    (void)runtime;
    if(guard.retained)return;
    suppressed=true;neutral=false;
    if(watch_bridge_grant_populated(&grant) && !watch_bridge_release(&guard,&grant))return;
    key=NULL;
}
