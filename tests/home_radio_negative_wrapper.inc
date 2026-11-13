static bool clock_radios_apply_user(uint32_t actions) {
#ifdef WATCH_SHARED_HOME_BOOT
    bool detached=false;
    if((actions&(PQA_WIFI|PQA_BLUETOOTH|PQA_AIRPLANE))&&device_clients.live[0]) {
        if(!clock_canvas_live()||held)return false;
        battery_api=NULL;device_clients.battery=NULL;
        bool released=rt->release(&device_clients.grants[0]);
        if(!clock_canvas_live())return false;
        if(!released||device_clients.grants[0].api||device_clients.grants[0].slot||device_clients.grants[0].generation) {
            clock_atomic_retain();return false;
        }
        device_clients.live[0]=false;detached=true;
    }
#endif
    bool applied=pqa_radios_apply_user(&clock_radios,&clock_quick.ui,rt,actions,quick_atomic_save,NULL,quick_atomic_guard);
#ifdef WATCH_SHARED_HOME_BOOT
    if(!clock_canvas_live())return false;
    /* False means radio restoration or cleanup was not proved. Do not acquire
     * another provider, restore a stale pointer, or continue ordinary cleanup. */
    if(!applied){clock_atomic_retain();return false;}
    if(detached) {
        risc_runtime_capability_v1 *grant=&device_clients.grants[0];
        *grant=(risc_runtime_capability_v1){.struct_size=sizeof(*grant)};
        bool acquired=rt->acquire(RISC_BATTERY_GAUGE_CAPABILITY,1,0,grant);
        if(!clock_canvas_live())return false;
        if(!acquired) {
            if(grant->api||grant->slot||grant->generation){clock_atomic_retain();return false;}
            rt->diagnostic("HOME battery rebind refused after radio edit");
            return false;
        }
        device_clients.live[0]=true;
        const risc_battery_gauge_api_v1 *api=grant->api;
        if(!api||api->api_version!=1||api->struct_size<sizeof(*api)||!api->read) {
            clock_atomic_retain();return false;
        }
        device_clients.battery=api;battery_api=api;
    }
#endif
    return applied;
}