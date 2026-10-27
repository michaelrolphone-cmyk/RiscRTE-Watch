/* Explicit native Quick: finish a visible unconfirmed frame BEFORE mutation.
 * No provider revision is captured/rebased here. Ordinary capture/CAS follows
 * this barrier; a previously captured same-contact draft remains unchanged. */
#if defined(PORTABLE_QUICK_CAPABILITY_SAFE) && defined(WATCH_ATOMIC_PREFERENCES)
#define CLOCK_QUICK_PERSIST_ACTIONS (PQA_BRIGHTNESS_COMMIT|PQA_VOLUME_COMMIT|PQA_SILENT|PQA_DND|PQA_CONTEXTS|PQA_WIFI|PQA_BLUETOOTH|PQA_AIRPLANE|PQA_TONE_COMMIT)
static int clock_quick_pending_frame(uint32_t actions,uint32_t *events) {
    if(!(actions&CLOCK_QUICK_PERSIST_ACTIONS))return 1;
    pqa_state *u=&clock_quick.ui;
    bool cancel=(*events&3u)||!pqa_visible(u)||u->torch;
#ifdef WATCH_CLOCK_LAUNCHER
    cancel=cancel||picker.open;
#endif
#ifdef WATCH_CLOCK_ALARMS
    cancel=cancel||clock_alarm_modal;
#endif
#ifdef WATCH_CONTEXTS_CLIENT
    cancel=cancel||clock_context_message[0];
#endif
    if(!cancel) {
        u->save_cancelled=false;u->save_pending=true;
        risc_display_surface_v1 pending={0};uint32_t now;
        /* A failed/unknown presentation must never be followed by a write or
         * speculative display cleanup. Retain the existing display ownership. */
        if(!alive(&now)||!frame(&pending)||!draw_clock(now,&pending)||!present()) {
            if(!clock_touch_retained&&!clock_runtime_unavailable){clock_atomic_retain();}return -1;
        }
        if(!alive(&now))return -1;
#ifdef WATCH_CLOCK_LAUNCHER
        sample_launcher_touch(now,true);
        if(clock_touch_retained)return -1;
#ifdef WATCH_CONTEXTS_CLIENT
        if(clock_contexts_uncertain)return -1;
#endif
#endif
        uint32_t later=0;
        if(!clock_key_read(&later,"WATCH_KEY stage=quick-pending"))return -1;
        *events|=later;
        cancel=u->save_cancelled||(*events&3u)||risc_runtime_exit_requested_v1(rt);
#ifdef WATCH_CLOCK_ALARMS
        /* One settled boundary, never a recursive callback while painting. */
        if(!cancel&&clock_alarm.api) {
            if(!clock_alarm_foreground())return -1;
            cancel=clock_alarm_modal||u->save_cancelled;
        }
#endif
    }
    if(!cancel)return 1;
    u->save_pending=false;u->save_cancelled=false;
    pqa_cancel(u);(void)pqa_take_action(u);quick_atomic_discard();
    u->brightness=(uint8_t)clock_quick.brightness;
    u->volume=(uint8_t)clock_quick.volume;
    u->last_nonzero_volume=(uint8_t)clock_quick.restore_volume;
    u->dnd_enabled=clock_quick.dnd_enabled;
    if(!pqa_session_restore(&clock_quick,display))return -1;
    return 0;
}
static void clock_quick_pending_done(void) {
    /* Caller reaches this only after every selected mutator returned safely.
     * Retained results never clear the last completed pending frame. */
    if(!clock_quick.ui.save_pending)return;
    clock_quick.ui.save_pending=false;clock_quick.ui.save_cancelled=false;
    pqa_cancel_input(&clock_quick.ui);
}
#endif
