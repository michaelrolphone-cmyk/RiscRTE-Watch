/* Generic RGB Home resident adapter. Child applications retain only semantic
 * state; this single shared Home owns native controls and physical policy.
 * Ordinary deployments without an admitted host keep their legacy behavior. */
#ifdef WATCH_RESIDENT_HOST
#include "RiscResidentShellV1.h"
static risc_resident_client_v1 clock_resident;
static bool clock_resident_enabled,clock_resident_child,clock_resident_dispatching;
static bool clock_resident_terminal,clock_resident_continued,clock_resident_handoff;
static bool clock_resident_sleep_pending,clock_resident_manual_sleep;
static int32_t clock_resident_policy(risc_resident_reply_v1 *reply,uint32_t now,bool force);
static uint32_t clock_resident_activity,clock_resident_policy_at,clock_resident_armed_at,clock_resident_keys;
static char clock_resident_next[193];
static bool clock_resident_alive(void) {
    if(clock_resident_terminal)return false;
    if(risc_runtime_get_api(1)!=rt){clock_resident_terminal=true;return false;}
#ifdef WATCH_RUNTIME_FEATURES
    if(native_clock.halted){clock_resident_terminal=true;return false;}
#endif
#ifdef WATCH_ATOMIC_PREFERENCES
    if(clock_atomic_retained){clock_resident_terminal=true;return false;}
#endif
    return true;
}
static int32_t clock_resident_retain(void) {
    if(clock_resident_alive()) {
        clock_resident_terminal=true;
        if(rt->struct_size>=RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE&&rt->retain_invocation)(void)rt->retain_invocation();
    }
    return RISC_RESIDENT_RETAINED;
}
static bool clock_resident_settled(void) {
    if(!clock_resident_alive()||held)return false;
#ifdef WATCH_CLOCK_ALARMS
    if(!clock_display_settled||clock_alarm_modal)return false;
#endif
#ifdef WATCH_ATOMIC_PREFERENCES
    if(quick_atomic_running||clock_quick_scope)return false;
#endif
    return true;
}
static bool clock_resident_pause(void) {
    if(!clock_resident_alive()||held)return false;
#ifdef WATCH_CLOCK_ALARMS
    if(!clock_display_settled||clock_alarm_modal)return false;
#endif
#ifdef WATCH_ATOMIC_PREFERENCES
    if(!clock_atomic_finish()||!clock_resident_alive())return false;
#endif
#if defined(WATCH_BLE_BROADCAST) || defined(WATCH_CONTEXTS_CLIENT)
    if(!clock_background_pause()||!clock_resident_alive())return false;
#endif
    if(!launcher_touch_close(&touch)||!clock_resident_alive())return false;
    watch_face_close(&picker);picker_open_pending=picker_select_pending=false;
    launcher_swipe_pending=launcher_activity_pending=false;
    pqa_cancel(&clock_quick.ui);(void)pqa_take_action(&clock_quick.ui);
    return true;
}
static bool clock_resident_preferences(void) {
    if(!pqa_session_load(&clock_quick,rt)||!clock_resident_alive())return false;
#ifdef WATCH_QUICK_RADIOS
    if(!pqa_radios_load(&clock_radios,&clock_quick.ui,rt)||!clock_resident_alive())return false;
#endif
    return reload_time_format()&&clock_resident_alive();
}
/* Reuse the existing alarm reconciler and native modal UI at a settled host
 * checkpoint, including continuation before FAST_WAKE's first outer turn. */
static bool clock_resident_alarm_ready(void) {
#ifdef WATCH_CLOCK_ALARMS
    if(!clock_alarm_attempted&&(!clock_alarm_open_optional()||!clock_resident_alive()))return false;
#endif
    return clock_resident_alive();
}
static bool clock_resident_alarm_pump(void) {
#ifdef WATCH_CLOCK_ALARMS
    if(!clock_alarm_foreground()||!clock_resident_alive())return false;
#endif
    return true;
}
static bool clock_resident_background_pause(void) {
#if defined(WATCH_BLE_BROADCAST) || defined(WATCH_CONTEXTS_CLIENT)
    if(!clock_background_pause()||!clock_resident_alive())return false;
#endif
    return clock_resident_alive();
}
static bool clock_resident_actions(uint32_t events) {
    if(events&3u)pqa_close(&clock_quick.ui);
#ifdef WATCH_ATOMIC_PREFERENCES
    if(!clock_quick_scope&&(pqa_visible(&clock_quick.ui)||pqa_capture(&clock_quick.ui)||clock_quick.ui.pending)) {
        if(!quick_atomic_modal_begin())return false;
        clock_quick_scope=true;
    }
    ++quick_atomic_contact;
    uint32_t actions=quick_atomic_take_action();
    if(!quick_atomic_capture(actions))return false;
    quick_atomic_show_queue_error();
#else
    uint32_t actions=pqa_take_action(&clock_quick.ui);
#endif
    bool changed=false;
    if(!pqa_session_apply(&clock_quick,rt,display,actions,&changed)||!clock_resident_alive())return false;
#ifdef WATCH_CONTEXTS_CLIENT
    if(actions&PQA_CONTEXTS)portable_contexts_settings_changed(&clock_contexts,clock_quick.ui.contexts_valid);
#endif
#ifdef WATCH_QUICK_RADIOS
#ifdef WATCH_ATOMIC_PREFERENCES
    if(!pqa_radios_apply_user(&clock_radios,&clock_quick.ui,rt,actions,quick_atomic_save,NULL,quick_atomic_guard))return false;
#else
    if(!pqa_radios_apply(&clock_radios,&clock_quick.ui,rt,actions))return false;
#endif
    if(!clock_resident_alive())return false;
#endif
#ifdef WATCH_CLOCK_ALARMS
    if(changed&&clock_alarm.api){(void)clock_alarm.api->refresh(clock_alarm.api->context);if(!clock_resident_alive())return false;}
#endif
    if(!clock_quick.ui.radio_controls&&(actions&PQA_WIFI)) {
        memcpy(clock_resident_next,"wifi_settings.elf",sizeof("wifi_settings.elf"));
        int32_t rc=clock_resident.request_foreground_exit(clock_resident.invocation);
        if(!clock_resident_alive()||rc!=RISC_RESIDENT_OK)return false;
        pqa_close(&clock_quick.ui);
    }
#ifdef WATCH_ATOMIC_PREFERENCES
    if(clock_quick_scope&&!pqa_visible(&clock_quick.ui)&&!pqa_capture(&clock_quick.ui)) {
        if(!quick_atomic_close())return false;
        clock_quick_scope=false;
    }
#endif
    return clock_resident_alive();
}
static int32_t clock_resident_controls(risc_resident_reply_v1 *reply) {
    if(!clock_resident_preferences()||!clock_resident_alarm_ready())return clock_resident_retain();
    if(!launcher_touch_open(&touch)||!clock_resident_alive())return clock_resident_retain();
    /* Never adopt an inherited contact as a button press. The normal touch
     * adapter requires neutral before this already-open native drawer. */
    pqa_cancel(&clock_quick.ui);(void)pqa_take_action(&clock_quick.ui);
    clock_quick.ui.target_q8=pqa_open_q8(&clock_quick.ui);
    clock_quick.ui.clock_valid=false;
    uint32_t now;
    for(;;) {
        if(!clock_resident_alive()||!alive(&now))return clock_resident_retain();
        if(!clock_resident_alarm_pump()||!alive(&now)||!clock_resident_alive())return clock_resident_retain();
        sample_launcher_touch(now,true);
        if(!clock_resident_alive())return clock_resident_retain();
        uint32_t events=0;
        if(!clock_key_read(&events,"WATCH_KEY stage=resident-controls")||!clock_resident_actions(events))return clock_resident_retain();
        if(events&2u)clock_resident_manual_sleep=true;
        else if(events&1u) {
            int32_t rc=clock_resident.request_foreground_exit(clock_resident.invocation);
            if(!clock_resident_alive()||rc!=RISC_RESIDENT_OK)return clock_resident_retain();
        }
        risc_display_surface_v1 surface={0};
        if(!frame(&surface)||!draw_clock(now,&surface)||!present()||!clock_resident_alive())return clock_resident_retain();
        if(!pqa_visible(&clock_quick.ui)&&!pqa_capture(&clock_quick.ui)&&!clock_quick.ui.pending)break;
        rt->yield_ms(10);
    }
#ifdef WATCH_ATOMIC_PREFERENCES
    if(!clock_atomic_finish())return clock_resident_retain();
#endif
    if(!clock_resident_background_pause()||!launcher_touch_close(&touch)||!clock_resident_alive())return clock_resident_retain();
    launcher_swipe_pending=launcher_activity_pending=false;picker_open_pending=picker_select_pending=false;
    reply->flags=RISC_RESIDENT_REPLY_REDRAW|RISC_RESIDENT_REPLY_CONFIGURATION_CHANGED;
    clock_resident_activity=now;
    if(clock_resident_manual_sleep){clock_resident_manual_sleep=false;return clock_resident_policy(reply,now,true);}
    return clock_resident_next[0]?RISC_RESIDENT_EXIT:RISC_RESIDENT_OK;
}
/* Policy runs only after the child closes its scene. Light retains the child;
 * a qualified Deep transition defers until the foreground has cleanly unloaded. */
static int32_t clock_resident_policy(risc_resident_reply_v1 *reply,uint32_t now,bool force) {
    uint32_t keys=clock_resident_keys;clock_resident_keys=0;
    if(!clock_resident_preferences()||!clock_resident_alarm_ready())return clock_resident_retain();
#ifdef WATCH_CLOCK_ALARMS
    if(!launcher_touch_open(&touch)||!clock_resident_alive()||!clock_resident_alarm_pump()||!alive(&now)||!clock_resident_alive())return clock_resident_retain();
    if(!clock_resident_background_pause()||!launcher_touch_close(&touch)||!clock_resident_alive())return clock_resident_retain();
    reply->flags|=RISC_RESIDENT_REPLY_REDRAW;
#ifdef WATCH_FAST_WAKE
    if(clock_alarm_input_cancelled){clock_alarm_input_cancelled=false;clock_resident_activity=now;force=false;keys=0;}
#endif
#endif
    if(keys&1u) {
        int32_t rc=clock_resident.request_foreground_exit(clock_resident.invocation);
        if(!clock_resident_alive()||rc!=RISC_RESIDENT_OK)return clock_resident_retain();
        return RISC_RESIDENT_EXIT;
    }
    if(keys&2u)force=true;
#ifdef PORTABLE_LOW_BATTERY
    if(!clock_low_battery_poll(now)||!clock_resident_alive())return clock_resident_retain();
    uint32_t idle_ms=clock_quick.idle_ms;
#else
    uint32_t idle_ms=60000u;
#endif
    if((force||(idle_ms&&(uint32_t)(now-clock_resident_activity)>=idle_ms))&&watch_capabilities_can_sleep(&device_clients)) {
#ifdef WATCH_RUNTIME_FEATURES
        if(!watch_runtime_stage(&native_clock)||!clock_resident_alive())return clock_resident_retain();
#endif
        watch_sleep_defer_deep=true;
        int result=sleep_cycle();watch_sleep_defer_deep=false;
        if(result==WATCH_SLEEP_RETAINED||!clock_resident_alive())return clock_resident_retain();
#ifdef WATCH_RUNTIME_FEATURES
        if(!watch_runtime_abandon(&native_clock)||!clock_resident_alive())return clock_resident_retain();
#endif
        if(result==WATCH_SLEEP_DEFERRED) {
            clock_resident_sleep_pending=true;
            int32_t rc=clock_resident.request_foreground_exit(clock_resident.invocation);
            if(!clock_resident_alive()||rc!=RISC_RESIDENT_OK)return clock_resident_retain();
            return RISC_RESIDENT_EXIT;
        }
        if(!result)return clock_resident_retain();
#ifdef WATCH_QUICK_RADIOS
        if(!pqa_radios_resume(&clock_radios,&clock_quick.ui,rt)||!clock_resident_alive())return clock_resident_retain();
#endif
        if(!alive(&now))return clock_resident_retain();
        clock_resident_activity=now;reply->flags|=RISC_RESIDENT_REPLY_REDRAW;
    }
    clock_resident_policy_at=now;
    return RISC_RESIDENT_OK;
}
static int32_t clock_resident_dispatch(void *context,const risc_resident_request_v1 *request,risc_resident_reply_v1 *reply) {
    (void)context;
    if(!clock_resident_alive())return RISC_RESIDENT_RETAINED;
    if(!clock_resident_child||clock_resident_dispatching||!request||request->struct_size!=sizeof(*request)||!reply||reply->struct_size<sizeof(*reply))return RISC_RESIDENT_INVALID;
    reply->flags=0;
    if(!clock_resident_settled())return RISC_RESIDENT_BUSY;
    uint32_t now;if(!alive(&now)||!clock_resident_alive())return clock_resident_retain();
    if(request->reason==RISC_RESIDENT_CHECKPOINT_POLL) {
        /* The Home-owned power-key capability is distinct from the scene's
         * touch/USB navigation claims. Latch intent without changing child
         * focus; input/persistence inhibition postpones its POLICY boundary. */
        uint32_t keys=0;
        if(!clock_key_read(&keys,"WATCH_KEY stage=resident-poll")||!clock_resident_alive())return clock_resident_retain();
        if(keys&3u) {
            clock_resident_activity=now;
            if((uint32_t)(now-clock_resident_armed_at)>=250u)clock_resident_keys|=keys&3u;
        }
        if(request->flags&(RISC_RESIDENT_POLL_ACTIVITY|RISC_RESIDENT_POLL_INHIBIT_IDLE))clock_resident_activity=now;
        if(!(request->flags&RISC_RESIDENT_POLL_INHIBIT_POLICY)&&(clock_resident_keys||(uint32_t)(now-clock_resident_policy_at)>=1000u))reply->flags=RISC_RESIDENT_REPLY_POLICY_REQUEST;
        return RISC_RESIDENT_OK;
    }
    if(request->reason!=RISC_RESIDENT_CHECKPOINT_CONTROLS&&request->reason!=RISC_RESIDENT_CHECKPOINT_POLICY)return RISC_RESIDENT_BUSY;
    clock_resident_dispatching=true;
    int32_t rc=request->reason==RISC_RESIDENT_CHECKPOINT_CONTROLS?clock_resident_controls(reply):clock_resident_policy(reply,now,false);
    if(!clock_resident_alive())return RISC_RESIDENT_RETAINED;
    clock_resident_dispatching=false;return rc;
}
static void clock_resident_failed(void *context,const risc_resident_failure_v1 *failure) {
    (void)context;(void)failure;
    if(!clock_resident_alive())return;
    memcpy(sleep_status,"APP FAILED TO START",sizeof("APP FAILED TO START"));
}
static bool clock_resident_init(void) {
    clock_resident=(risc_resident_client_v1){.struct_size=sizeof(clock_resident)};
    clock_resident_enabled=clock_resident_child=clock_resident_dispatching=clock_resident_terminal=clock_resident_continued=clock_resident_handoff=false;
    clock_resident_sleep_pending=clock_resident_manual_sleep=false;watch_sleep_defer_deep=watch_sleep_deep_deferred=false;
    clock_resident_next[0]=0;clock_resident_activity=clock_resident_policy_at=clock_resident_armed_at=clock_resident_keys=0;
    if(rt->struct_size<RISC_RUNTIME_RESIDENT_SHELL_V1_SIZE||!rt->resident_shell)return true;
    bool found=rt->resident_shell(&clock_resident);if(!clock_resident_alive())return false;
    if(!found)return true;
    if(clock_resident.api_version!=1||clock_resident.struct_size<sizeof(clock_resident)||clock_resident.role!=RISC_RESIDENT_ROLE_HOST||!clock_resident.invocation||!clock_resident.register_shell||!clock_resident.run_foreground||!clock_resident.request_foreground_exit||!rt->retain_invocation){(void)clock_resident_retain();return false;}
    const risc_resident_callbacks_v1 callbacks={.api_version=1,.struct_size=sizeof(callbacks),.dispatch=clock_resident_dispatch,.failed=clock_resident_failed};
    int32_t rc=clock_resident.register_shell(clock_resident.invocation,&callbacks);
    if(!clock_resident_alive())return false;
    if(rc!=RISC_RESIDENT_OK){(void)clock_resident_retain();return false;}
    clock_resident_enabled=true;return true;
}
static int32_t clock_resident_run(const char *path) {
    if(!clock_resident_alive())return RISC_RESIDENT_RETAINED;
    clock_resident_continued=clock_resident_handoff=false;
    if(!clock_resident_enabled)return path&&rt->request_launch(path)?RISC_RESIDENT_HANDOFF:RISC_RESIDENT_DENIED;
    if(held)return RISC_RESIDENT_BUSY;
#ifdef WATCH_CLOCK_ALARMS
    if(!clock_display_settled||clock_alarm_modal)return RISC_RESIDENT_BUSY;
#endif
    if(!clock_resident_pause())return clock_resident_retain();
    char queued_path[193];
    for(;;) {
        clock_resident_next[0]=0;clock_resident_child=true;
        uint32_t now,discarded=0;
        if(!clock_key_read(&discarded,"WATCH_KEY stage=resident-entry-discard")||!clock_resident_alive()||!alive(&now))return clock_resident_retain();
        clock_resident_keys=0;clock_resident_activity=clock_resident_policy_at=clock_resident_armed_at=now;
        risc_resident_result_v1 result={.struct_size=sizeof(result)};
        int32_t rc=clock_resident.run_foreground(clock_resident.invocation,path,&result);
        if(!clock_resident_alive())return RISC_RESIDENT_RETAINED;
        if(rc==RISC_RESIDENT_RETAINED||result.status==RISC_RESIDENT_RETAINED)return clock_resident_retain();
        clock_resident_child=false;
        if(rc==RISC_RESIDENT_HANDOFF){clock_resident_sleep_pending=false;clock_resident_handoff=true;return rc;}
        if(rc!=RISC_RESIDENT_OK&&rc!=RISC_RESIDENT_NO_PENDING) {
            clock_resident_sleep_pending=false;clock_resident_next[0]=0;
            if(rc!=RISC_RESIDENT_BUSY&&rc!=RISC_RESIDENT_DENIED&&rc!=RISC_RESIDENT_INVALID&&rc!=RISC_RESIDENT_INCOMPATIBLE&&rc!=RISC_RESIDENT_FAILED)return clock_resident_retain();
            if(!launcher_touch_open(&touch)||!clock_resident_alive())return clock_resident_retain();
            return rc;
        }
        if(clock_resident_sleep_pending) {
            clock_resident_sleep_pending=false;
#ifdef WATCH_RUNTIME_FEATURES
            if(!watch_runtime_stage(&native_clock)||!clock_resident_alive())return clock_resident_retain();
#endif
            unsigned prior=sleep_mode;sleep_mode=PORTABLE_SLEEP_DEEP;
            int slept=sleep_cycle();sleep_mode=prior;
            if(slept==WATCH_SLEEP_RETAINED||!clock_resident_alive())return clock_resident_retain();
#ifdef WATCH_RUNTIME_FEATURES
            if(!watch_runtime_abandon(&native_clock)||!clock_resident_alive())return clock_resident_retain();
#endif
            if(!slept)return clock_resident_retain();
        }
        if(clock_resident_next[0]){memcpy(queued_path,clock_resident_next,sizeof(queued_path));path=queued_path;continue;}
        if(!launcher_touch_open(&touch)||!clock_resident_alive())return clock_resident_retain();
        clock_resident_continued=rc==RISC_RESIDENT_OK;
        return rc;
    }
}
__attribute__((visibility("default"))) const risc_resident_app_descriptor_v1_t risc_resident_app_descriptor_v1={1,sizeof(risc_resident_app_descriptor_v1_t),RISC_RESIDENT_ROLE_HOST,0};
#endif
