#pragma once
#include "RiscTouchV1.h"
/* Watch deployment policy, not a raw-provider ABI change. The confirmed
 * touch-direction variant uses identity coordinates in every GUI client. */
typedef struct {
    risc_runtime_capability_v1 grant;
    const risc_touch_api_v1 *api;
    watch_viewport_touch viewport;
    uint64_t subscription;
    bool neutral, down;
#ifdef WATCH_HOME_CANVAS
    bool canvas,navigation_neutral;
    unsigned navigation_focus;
#endif
    uint8_t id;
    uint16_t x, y;
} watch_launcher_touch;
static bool launcher_touch_close(watch_launcher_touch *t) {
    if(!clock_canvas_live())return false;
    if (t->subscription && !t->api->unsubscribe(t->api->context,t->subscription)) return false;
    t->subscription=0;
#ifdef WATCH_HOME_CANVAS
    if(t->canvas&&!clock_canvas_pause())return false;
#endif
    if (t->grant.api && !rt->release(&t->grant)) return false;
    *t=(watch_launcher_touch){0}; return true;
}
static bool launcher_touch_open(watch_launcher_touch *t) {
    *t=(watch_launcher_touch){.grant={.struct_size=sizeof(t->grant)}};
#ifdef WATCH_HOME_CANVAS
    if(clock_canvas_selected){
        if(!clock_canvas_wake())return false;
        t->canvas=true;t->api=&clock_canvas_touch.api;
    }else
#endif
    {
        if (!rt->acquire("input.touch.raw",1,0,&t->grant)) return false;
        t->api=t->grant.api;
    }
    if (!t->api || t->api->api_version!=1 || t->api->struct_size<sizeof(*t->api) ||
        !t->api->subscribe || !t->api->unsubscribe || !t->api->poll || !t->api->next || !t->api->snapshot) return false;
#ifdef WATCH_HOME_CANVAS
    if(!t->canvas)
#endif
    {
        if (!watch_viewport_touch_open(&t->viewport,&clock_viewport,t->api)) return false;
        t->api=&t->viewport.api;
    }
    t->subscription=t->api->subscribe(t->api->context);
    risc_touch_snapshot_v1 s={0};
    if (!t->subscription || !t->api->snapshot(t->api->context,&s)) return false;
    t->neutral=s.contact_count==0; return true;
}
/* Drain queued notifications, then use the provider's authoritative snapshot.
 * A gap/multitouch/id replacement cancels every gesture until neutral. */
typedef struct {
    uint32_t now;
    uint16_t x,y;
    uint8_t count,id;
    bool valid,activity;
#ifdef WATCH_HOME_CANVAS
    bool controls;
    risc_input_navigation_frame_v1 navigation;
#endif
} watch_launcher_observation;
/* Capture does not mutate either gesture recognizer. The renderer may collect
 * these observations, then replay them in order after all frame writes stop. */
static watch_launcher_observation launcher_touch_capture(watch_launcher_touch *t,uint32_t now) {
    watch_launcher_observation o={.now=now};
    if(!t->subscription)return o;
    bool ok=t->api->poll(t->api->context,1);
    for(unsigned n=0;n<RISC_TOUCH_QUEUE_LENGTH;n++) {
        risc_touch_event_v1 e={0};int32_t count=t->api->next(t->api->context,t->subscription,&e);
        if(count<=0){if(count<0)ok=false;break;}o.activity=true;
    }
#ifdef WATCH_HOME_CANVAS
    if(t->canvas){
        /* Snapshot adoption is only for this snapshot consumer, after its
         * event queue has drained. The alarm's ordered reducer does not do it. */
        if(!clock_canvas_live())return o;
        if(!home_canvas_touch_sync(&clock_canvas_touch))ok=false;
        o.navigation=home_canvas_touch_navigation(&clock_canvas_touch,&o.controls);
        if(!o.navigation.buttons)t->navigation_neutral=true;
        else if(!t->navigation_neutral)o.navigation.pressed=o.navigation.released=0;
        o.activity|=o.controls||o.navigation.buttons||o.navigation.pressed||o.navigation.released;
    }
#endif
    risc_touch_snapshot_v1 s={0};
    o.valid=t->api->snapshot(t->api->context,&s)&&ok&&s.width==clock_viewport.width&&s.height==clock_viewport.height;
    o.count=s.contact_count;o.id=s.contacts[0].id;o.x=s.contacts[0].x;o.y=s.contacts[0].y;
    if(o.count)o.activity=true;
    return o;
}

#ifdef WATCH_HOME_CANVAS
#include "PortableQuickLayout.h"
static unsigned clock_canvas_navigation_focus;
static unsigned launcher_canvas_navigation(watch_face_picker*p,const watch_launcher_observation*o){
 uint32_t key=o->navigation.pressed;
 if(!o->valid||o->count){return WATCH_FACE_NONE;}
#ifdef WATCH_QUICK_ACTIONS
 if(o->controls||(!p->open&&!pqa_visible(&clock_quick.ui)&&(key&RISC_NAV_UP))){
  pqa_cancel_input(&clock_quick.ui);clock_quick.ui.target_q8=pqa_open_q8(&clock_quick.ui);
  return WATCH_FACE_NONE;
 }
 if(pqa_visible(&clock_quick.ui)){
  if(key&(RISC_NAV_BACK|RISC_NAV_HOME)){pqa_close(&clock_quick.ui);return (key&RISC_NAV_HOME)?WATCH_FACE_LAUNCHER:WATCH_FACE_NONE;}
  if(key&(RISC_NAV_UP|RISC_NAV_DOWN|RISC_NAV_PAGE_BACK|RISC_NAV_PAGE_FORWARD)){
   unsigned back=!!(key&(RISC_NAV_UP|RISC_NAV_PAGE_BACK));
   clock_canvas_navigation_focus=(clock_canvas_navigation_focus+(back?8u:1u))%9u;
  }
  unsigned focus=clock_canvas_navigation_focus;
  static const char*const labels[]={"QUICK BRIGHTNESS","QUICK VOLUME","QUICK SILENT","QUICK DND","QUICK AIRPLANE","QUICK WI-FI","QUICK BLUETOOTH","QUICK TORCH","QUICK CONTEXTS"};
  if(key)memcpy(sleep_status,labels[focus],strlen(labels[focus])+1);
  if(focus<2&&(key&(RISC_NAV_LEFT|RISC_NAV_RIGHT))){
   pqa_state*u=&clock_quick.ui;bool valid=focus?u->volume_valid:u->brightness_valid;
   if(valid){
    int value=(focus?u->volume:u->brightness)+((key&RISC_NAV_RIGHT)?10:-10);
    int minimum=focus?0:10;if(value<minimum)value=minimum;if(value>100)value=100;
    if(focus){u->volume=u->action_volume=(uint8_t)value;u->pending|=PQA_VOLUME_COMMIT;}
    else{u->brightness=u->action_brightness=(uint8_t)value;u->pending|=PQA_BRIGHTNESS_COMMIT;}
   }
  }else if((key&RISC_NAV_CONFIRM)&&focus>=2){
   pqa_state*u=&clock_quick.ui;pqa_layout l=pqa_layout_for(u);
   pqa_rect r=focus==8?l.contexts:l.tiles[focus-2];
   /* Use the same controller validation and explicit action provenance as
    * touch; unavailable fields cannot synthesize an authorized write. */
   if(u->position_q8==pqa_open_q8(u)&&u->target_q8==pqa_open_q8(u)){
    pqa_cancel_input(u);(void)pqa_input(u,o->now,true,0,0,0,0,true);
    (void)pqa_input(u,o->now,true,1,255,r.x+r.w/2,r.y+r.h/2,true);
    (void)pqa_input(u,o->now,true,0,255,r.x+r.w/2,r.y+r.h/2,true);
   }
  }
  return WATCH_FACE_NONE;
 }
#endif
 if(key&RISC_NAV_HOME)return WATCH_FACE_LAUNCHER;
 if(key&RISC_NAV_BACK){if(p->open){watch_face_close(p);return WATCH_FACE_NONE;}return WATCH_FACE_LAUNCHER;}
 if(!p->open){
  if(key&(RISC_NAV_CONFIRM|RISC_NAV_DOWN)){
   p->open=true;p->category=(uint8_t)watch_face_category_for(p->selected);
   p->target=(uint8_t)watch_face_index_for(p->category,p->selected);p->position=-(int)p->target*watch_face_pitch(p);p->snapping=true;
   p->category_position=-(int)p->category*watch_category_pitch(p);p->category_positions[p->category]=p->position;
   watch_face_reset_contact(p);p->last_tick=o->now;return WATCH_FACE_OPEN;
  }
  if(key&(RISC_NAV_LEFT|RISC_NAV_RIGHT|RISC_NAV_PAGE_BACK|RISC_NAV_PAGE_FORWARD))return WATCH_FACE_LAUNCHER;
  return WATCH_FACE_NONE;
 }
 if(key&(RISC_NAV_UP|RISC_NAV_DOWN)){
  p->category_positions[p->category]=p->position;
  p->category=(uint8_t)((p->category+((key&RISC_NAV_UP)?WATCH_FACE_CATEGORY_COUNT-1:1))%WATCH_FACE_CATEGORY_COUNT);
  p->category_target=p->category;p->category_position=-(int)p->category*watch_category_pitch(p);p->category_velocity=0;
  p->position=p->category_positions[p->category];p->target=(uint8_t)watch_face_nearest(p);p->snapping=true;
 }
 if(key&(RISC_NAV_LEFT|RISC_NAV_RIGHT|RISC_NAV_PAGE_BACK|RISC_NAV_PAGE_FORWARD)){
  unsigned count=watch_face_page_for(p->category)->count;
  unsigned back=!!(key&(RISC_NAV_LEFT|RISC_NAV_PAGE_BACK));
  p->target=(uint8_t)((p->target+(back?count-1:1))%count);p->snapping=true;p->velocity=0;
 }
 if(key&RISC_NAV_CONFIRM){
  p->position=-(int)p->target*watch_face_pitch(p);p->pulse_face=watch_face_page_for(p->category)->ids[p->target];
  p->pulse_started=o->now;p->pulse_active=true;return WATCH_FACE_SELECT;
 }
 return WATCH_FACE_NONE;
}
#endif

static unsigned launcher_touch_observe(watch_face_picker *picker,const watch_launcher_observation *o) {
    uint32_t now=o->now;bool valid=o->valid;
#ifdef WATCH_HOME_CANVAS
    if(o->controls||o->navigation.pressed)return launcher_canvas_navigation(picker,o);
#endif
#ifdef WATCH_QUICK_ACTIONS
    bool consumed=pqa_input(&clock_quick.ui,now,valid,o->count,o->id,o->x,o->y,!picker->open);
    if(consumed){watch_face_reset_contact(picker);return WATCH_FACE_NONE;}
    if(clock_quick.ui.route==PQA_REPLAY) {
        (void)watch_face_input(picker,clock_quick.ui.start_ms,true,0,0,0,0);
        (void)watch_face_input(picker,clock_quick.ui.start_ms,true,1,clock_quick.ui.start_id,(unsigned)clock_quick.ui.start_x,(unsigned)clock_quick.ui.start_y);
    }
#endif
    return watch_face_input(picker,now,valid,o->count,o->id,o->x,o->y);
}
static unsigned launcher_touch_sample(watch_launcher_touch *t,watch_face_picker *picker,uint32_t now,bool *activity) {
    *activity=false;if(!t->subscription)return WATCH_FACE_NONE;
    watch_launcher_observation o=launcher_touch_capture(t,now);
    *activity=o.activity;
    return launcher_touch_observe(picker,&o);
}
