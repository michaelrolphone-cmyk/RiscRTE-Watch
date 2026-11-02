#include "PortableContextActions.h"

#ifdef ALARM_SERVICE_TAGGED_V2
/* Reuse the original countdown writer/schema. Only service observations are
 * wrapped so a retained refresh cannot be followed by workflow reporting. */
static int32_t clock_context_timer_status(void*context,alarm_status_v1*out) {
    (void)context;
    if(!clock_alarm_custody_live())return ALARM_RETAINED;
    int32_t rc=clock_alarm.api->status(clock_alarm.api->context,out);
    if(!clock_canvas_live())return ALARM_RETAINED;
    if(!portable_alarm_result(&clock_alarm,rc)||!clock_alarm_custody_live())return ALARM_RETAINED;
    if(rc==ALARM_OK&&(!portable_alarm_result(&clock_alarm,out->error)||out->output_uncertain||
       out->api_version!=1||out->struct_size<sizeof(*out)||out->state>ALARM_STATE_CUE)){
        clock_alarm.retained=true;(void)clock_alarm_custody_live();return ALARM_RETAINED;
    }
    return rc;
}
static int32_t clock_context_timer_refresh(void*context) {
    (void)context;return clock_alarm_refresh_checked()?ALARM_OK:ALARM_RETAINED;
}
static bool clock_context_timer(uint32_t duration,risc_runtime_capability_v1*grant) {
    if(!clock_alarm.api||!clock_alarm_custody_live())return false;
    alarm_service_v1 observed=*clock_alarm.api;
    observed.status=clock_context_timer_status;observed.refresh=clock_context_timer_refresh;
    bool ok=portable_context_timer(rt,&observed,duration,grant);
    return clock_alarm_custody_live()&&ok;
}
#else
static bool clock_context_timer(uint32_t duration,risc_runtime_capability_v1*grant) {
    return portable_context_timer(rt,clock_alarm.api,duration,grant);
}
#endif

static uint32_t clock_context_checked;
static bool clock_context_rules(void){
 const contexts_fingerprint_service_v1*fp=contexts_fingerprint_api(clock_contexts.api);if(!fp)return true;
 uint32_t now=face.animation_ms;if(clock_context_checked&&(uint32_t)(now-clock_context_checked)<500u)return true;clock_context_checked=now;
 contexts_evaluate_v1 q={.struct_size=sizeof(q)};q.observation.enabled=clock_contexts.settings_valid&&clock_contexts.policy.enabled;
 q.observation.time_valid=face.time_valid;q.observation.hour=face.time.hour;
 risc_battery_sample_v1 battery={0};if(battery_api&&battery_api->read&&battery_api->read(battery_api->context,&battery)&&battery.percent<=100&&!(battery.flags&RISC_BATTERY_PROFILE_MISSING)){q.observation.battery_valid=true;q.observation.battery=battery.percent;q.observation.charging=!!(battery.flags&RISC_BATTERY_CHARGING);}
 if(!fp->fingerprint(fp->base.context,CONTEXTS_FP_EVALUATE,&q))return false;
 if(q.result.changed){
  if(!clock_background_pause())return false;
  risc_runtime_capability_v1 g={.struct_size=sizeof(g)};uint16_t success=0;
#ifdef WATCH_ATOMIC_PREFERENCES
  bool opened=home_automatic_begin(PORTABLE_AUTOMATIC_HOME_RULES);const risc_key_value_v1 *settings=&home_automatic.view;
#else
  bool opened=rt->acquire(RISC_KEY_VALUE_CAPABILITY,1,1,&g);const risc_key_value_v1 *settings=g.api;
#endif
  for(unsigned f=0;f<CR_SETTINGS;f++)if(q.result.changed&(1u<<f))if(opened&&portable_context_setting(settings,f,q.result.settings[f],clock_contexts.low_battery,watch_face_save))success|=1u<<f;
#ifdef WATCH_ATOMIC_PREFERENCES
  if(!home_automatic_close()){clock_atomic_retain();return false;}
#else
  if(g.api&&!rt->release(&g))return false;
#endif
  for(unsigned i=0;i<CR_RULES;i++){bool changed=false,complete=true;for(unsigned f=0;f<CR_SETTINGS;f++)if(q.result.winners[f]==(int)i&&(q.result.changed&(1u<<f))){changed=true;complete&=!!(success&(1u<<f));}if(changed){contexts_workflow_v1 report={.struct_size=sizeof(report),.rule=i,.result=complete?CR_APPLIED:CR_PARTIAL};if(!fp->fingerprint(fp->base.context,CONTEXTS_FP_WORKFLOW,&report))return false;}}
  if(opened){if(!pqa_session_load(&clock_quick,rt)||!pqa_session_restore(&clock_quick,display)||!pqa_radios_resume(&clock_radios,&clock_quick.ui,rt))return false;
   g=(risc_runtime_capability_v1){.struct_size=sizeof(g)};if(!rt->acquire(RISC_KEY_VALUE_CAPABILITY,1,1,&g))return false;unsigned selected=picker.selected;(void)portable_sleep_load(g.api,&sleep_mode);if(watch_face_load(g.api,&selected)==RISC_KEY_VALUE_OK)picker.selected=(uint8_t)selected;if(!rt->release(&g))return false;
  }
  if(clock_alarm.api&&!clock_alarm_refresh_checked())return false;
 }
 if(clock_context_message[0])return true;
 /* This ABI cannot peek at a workflow before claiming it. Defer only claim,
  * including timer/message steps, until handoff is admissible; evaluation,
  * settings, capture and projection above continue normally. */
 if(!clock_navigation_ready)return true;
 contexts_workflow_v1 action={.struct_size=sizeof(action),.claim=true};if(!fp->fingerprint(fp->base.context,CONTEXTS_FP_WORKFLOW,&action))return false;
 if(action.available){
  if(!clock_background_pause())return false;
  bool ok=false;
  if(action.step.kind==CR_MESSAGE){memcpy(clock_context_message,action.step.text,33);ok=true;}
  else if(action.step.kind==CR_TIMER){risc_runtime_capability_v1 g={0};ok=clock_context_timer(action.step.value,&g);if(!clock_alarm_custody_live()||g.api)return false;}
  else if(action.step.kind==CR_LAUNCH){ok=clock_launch_request(action.step.text);if(!clock_launch_live())return false;if(ok&&!clock_launch_returned())clock_contexts_handoff=true;}
  action.claim=false;action.result=ok?CR_RAN:CR_FAILED;if(!fp->fingerprint(fp->base.context,CONTEXTS_FP_WORKFLOW,&action))return false;
 }
 return true;
}
/* Copied service observations only. Source apps retain model storage ownership;
 * ordinary app handoff performs one bounded export at a time. */
static bool clock_contexts_failure(void) {
    clock_contexts_uncertain=true;
#ifdef WATCH_RUNTIME_FEATURES
    watch_runtime_retain(rt);
#endif
    return false;
}
static bool clock_contexts_recover_export(void) {
    if(!clock_contexts.api)return true;
    contexts_status_v1 s={.struct_size=sizeof(s)};
    if(!clock_contexts.api->status(clock_contexts.api->context,&s))return clock_contexts_failure();
    /* Successful owners clear active before return. Failed owner launch/init
     * becomes visible failure instead of an automatic launch loop. */
    if(s.export_active&&!clock_contexts.api->finish_export(clock_contexts.api->context,
        s.export_active,CONTEXTS_EXPORT_UNSUPPORTED))return clock_contexts_failure();
    return true;
}
static void clock_contexts_project(const contexts_status_v1 *s) {
    nova_context_state v={0};v.enabled=clock_contexts.settings_valid&&clock_contexts.policy.enabled;
    v.ready=s->audio.model_state==CONTEXTS_MODEL_READY||s->radio.model_state==CONTEXTS_MODEL_READY;
    v.paused=s->state!=CONTEXTS_LIVE;v.ambiguous=s->audio.room_ambiguous||s->radio.room_ambiguous;
    const contexts_source_status_v1 *room=NULL,*event=NULL;
    if(s->audio.current&&s->audio.room_valid&&!s->audio.room_ambiguous)room=&s->audio;
    if(s->radio.current&&s->radio.room_valid&&!s->radio.room_ambiguous) {
        if(room&&strncmp(room->room_name,s->radio.room_name,17)){room=NULL;v.ambiguous=true;}
        else if(!room)room=&s->radio;
    }
    if(room&&memchr(room->room_name,0,17)&&!v.ambiguous) {
        memcpy(v.room,room->room_name,17);v.source=(uint8_t)room->source;v.room_valid=true;
    }
    if(s->audio.current&&s->audio.event_valid&&!s->audio.event_ambiguous)event=&s->audio;
    else if(s->radio.current&&s->radio.event_valid&&!s->radio.event_ambiguous)event=&s->radio;
    if(event&&memchr(event->event_name,0,17)){memcpy(v.event,event->event_name,17);v.event_valid=true;}
    const contexts_fingerprint_service_v1*fp=contexts_fingerprint_api(clock_contexts.api);
    if(fp){contexts_fingerprint_status_v1 q={.struct_size=sizeof(q)};if(fp->fingerprint(fp->base.context,CONTEXTS_FP_STATUS,&q)){v.ready=q.generation!=0;v.room_valid=q.room_slot>=0;v.event_valid=q.event_slot>=0;v.ambiguous=q.room_ambiguous;memcpy(v.room,q.room_name,17);memcpy(v.event,q.event_name,17);}}
    face.contexts=v;
}
static bool clock_contexts_tick(void) {
    if(!clock_contexts.api)return true;
    if(clock_contexts_uncertain||held||!clock_display_settled)return false;
    if(clock_alarm_modal||picker.open||pqa_visible(&clock_quick.ui)||pqa_capture(&clock_quick.ui)) {
        if(!clock_contexts_pause())return false;
    } else if(!portable_contexts_step(&clock_contexts,true,true))return clock_contexts_failure();
    contexts_status_v1 s={.struct_size=sizeof(s)};
    if(!clock_contexts.api->status(clock_contexts.api->context,&s))return clock_contexts_failure();
    if(!contexts_fingerprint_api(clock_contexts.api)&&clock_contexts.settings_valid&&clock_contexts.policy.enabled&&!s.export_active&&!s.export_pending) {
        uint32_t needed=(s.audio.model_state==CONTEXTS_MODEL_EMPTY?CONTEXTS_AUDIO:0u)|
            (s.radio.model_state==CONTEXTS_MODEL_EMPTY?CONTEXTS_RADIO:0u);
        if(needed) {
            if(!clock_background_pause()||!clock_contexts.api->request_export(clock_contexts.api->context,needed)||
               !clock_contexts.api->status(clock_contexts.api->context,&s))return clock_contexts_failure();
        }
    }
    clock_contexts_project(&s);
    if(s.export_pending&&!s.export_active&&!clock_alarm_modal&&!picker.open&&!pqa_visible(&clock_quick.ui)) {
        /* Keep the export pending; BUSY is not an unsupported owner. */
        if(!clock_navigation_ready)return true;
        unsigned source=s.export_pending&CONTEXTS_AUDIO?CONTEXTS_AUDIO:CONTEXTS_RADIO;
        if(!clock_background_pause()||!clock_contexts.api->begin_export(clock_contexts.api->context,source))return clock_contexts_failure();
        if(clock_launch_request(source==CONTEXTS_AUDIO?"audio_spectrum.elf":"waterfall.elf")) {
            if(clock_launch_returned())return true;
            clock_contexts_handoff=true;
#ifdef WATCH_BLE_BROADCAST
            clock_handoff=true;
#endif
            return true;
        }
        if(!clock_launch_live())return false;
        return clock_contexts.api->finish_export(clock_contexts.api->context,source,CONTEXTS_EXPORT_UNSUPPORTED);
    }
    if(clock_alarm_modal||picker.open||pqa_visible(&clock_quick.ui)||pqa_capture(&clock_quick.ui))return true;
    if(clock_contexts.rules_available)return clock_context_rules();
    uint16_t applied=0;
#ifdef WATCH_ATOMIC_PREFERENCES
    if(!portable_contexts_apply_room_using(&clock_contexts,&applied,watch_face_save,home_automatic_preset))return clock_contexts_failure();
#else
    if(!portable_contexts_apply_room(&clock_contexts,&applied,watch_face_save))return clock_contexts_failure();
#endif
    if(applied) {
        if(!pqa_session_load(&clock_quick,rt)||!pqa_session_restore(&clock_quick,display))return false;
        if(clock_alarm.api && (applied&(PORTABLE_CONTEXT_VOLUME|PORTABLE_CONTEXT_DND|PORTABLE_CONTEXT_ALERT)))
            if(!clock_alarm_refresh_checked())return false;
        if(applied&(PORTABLE_CONTEXT_SLEEP|PORTABLE_CONTEXT_FACE)) {
            risc_runtime_capability_v1 g={.struct_size=sizeof(g)};
            if(!rt->acquire(RISC_KEY_VALUE_CAPABILITY,1,1,&g))return false;
            if(applied&PORTABLE_CONTEXT_SLEEP)(void)portable_sleep_load(g.api,&sleep_mode);
            if(applied&PORTABLE_CONTEXT_FACE){unsigned selected=picker.selected;if(watch_face_load(g.api,&selected)==RISC_KEY_VALUE_OK)picker.selected=(uint8_t)selected;}
            if(!rt->release(&g))return clock_contexts_failure();
        }
    }
    return true;
}
