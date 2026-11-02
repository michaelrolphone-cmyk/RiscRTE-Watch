/* Shared diagnostic document. Native presentation/input are provider-owned;
 * the original native raster/touch test is retained as an explicit test. */
#include "RiscSceneComponentsV1.h"
#include "RiscResidentShellV1.h"
static const risc_runtime_api_v1 *br_rt;
static bool br_retained;
static risc_components_document_v1 br_doc;
__attribute__((visibility("default")))
const risc_resident_app_descriptor_v1_t risc_resident_app_descriptor_v1={
 1,sizeof(risc_resident_app_descriptor_v1_t),RISC_RESIDENT_ROLE_FOREGROUND,0};
static bool br_live(void){return !br_retained && risc_runtime_get_api(1)==br_rt;}
static void br_retain(void){
 if(br_retained)return;
 br_retained=true;
 if(risc_runtime_get_api(1)==br_rt && br_rt->retain_invocation)
  (void)br_rt->retain_invocation();
}
static bool br_release(risc_runtime_capability_v1 *g){
 if(!br_live())return false;
 if(!br_rt->release(g)||!br_live()){br_retain();return false;}
 *g=(risc_runtime_capability_v1){0};return true;
}
static bool br_get(const char *cap,unsigned api,risc_runtime_capability_v1 *g,bool *owned){
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};*owned=false;
 bool ok=br_rt->acquire(cap,api,0,g);
 if(!br_live())return false;
 if(!ok){if(g->api||g->slot||g->generation)br_retain();return false;}
 *owned=true;return true;
}
static void br_node(unsigned id,unsigned kind,const char *label,const char *value,unsigned flags){
 risc_scene_node_v1 *n=&br_doc.nodes[br_doc.node_count++];
 n->id=id;n->route=1;n->kind=kind;n->flags=flags;
 n->action=kind==RISC_SCENE_ACTION?id:0;
 snprintf(n->label,sizeof(n->label),"%s",label);
 snprintf(n->text,sizeof(n->text),"%s",value);
}
static void br_document(uint32_t revision,uint32_t seconds,const char *battery,const char *rtc,
 const char *motion,const char *status,bool native_available){
 memset(&br_doc,0,sizeof(br_doc));
 br_doc.api_version=1;br_doc.struct_size=sizeof(br_doc);br_doc.revision=revision;
 br_doc.root=1;br_doc.screen_key=1;br_doc.route_count=1;
 br_doc.routes[0].id=1;br_doc.routes[0].back_action=3;
 snprintf(br_doc.routes[0].title,sizeof(br_doc.routes[0].title),"HARDWARE TEST");
 char uptime[40];snprintf(uptime,sizeof(uptime),"UPTIME %lu S",(unsigned long)seconds);
 br_node(10,RISC_SCENE_TEXT_NODE,"Runtime",uptime,0);
 br_node(11,RISC_SCENE_TEXT_NODE,"Battery",battery,0);
 br_node(12,RISC_SCENE_TEXT_NODE,"RTC",rtc,0);
 br_node(13,RISC_SCENE_TEXT_NODE,"Motion",motion,0);
 br_node(14,RISC_SCENE_TEXT_NODE,"Status",status,0);
 br_node(15,RISC_SCENE_TEXT_NODE,"Native display / touch",
  native_available?"Color bars and raw touch coordinates available in Native Test.":"Native RGB/touch test unavailable from the authorized providers.",0);
 br_node(16,RISC_SCENE_TEXT_NODE,"Inactive tests","RADIO AUDIO SD SLEEP OFF",0);
 br_node(1,RISC_SCENE_ACTION,"MOTION","",0);
 br_node(2,RISC_SCENE_ACTION,"VIBRATE","",0);
 br_node(4,RISC_SCENE_ACTION,"NATIVE DISPLAY / TOUCH TEST","",
  native_available?0:RISC_SCENE_DISABLED);
 br_node(3,RISC_SCENE_ACTION,"BACK TO HOME","",0);
}
static bool br_event(const risc_scene_event_v1 *e){
 if(e->node==0)return e->action==3;
 for(unsigned n=0;n<br_doc.node_count;n++)
  if(br_doc.nodes[n].id==e->node)return br_doc.nodes[n].kind==RISC_SCENE_ACTION &&
   br_doc.nodes[n].action==e->action && !(br_doc.nodes[n].flags&RISC_SCENE_DISABLED);
 return false;
}
static bool br_close(const risc_scene_api_v1 *scene,uint64_t session){
 for(;;){
  if(!br_live())return false;
  int rc=scene->close(scene->context,session);
  if(!br_live())return false;
  if(rc==RISC_SCENE_OK)return true;
  if(rc!=RISC_SCENE_AGAIN){br_retain();return false;}
  br_rt->yield_ms(2);
 }
}
__attribute__((visibility("default"))) int app_module_init(void){
 br_rt=risc_runtime_get_api(1);br_retained=false;
 return !br_rt||br_rt->api_version!=1||
  br_rt->struct_size<RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE||
  !br_rt->acquire||!br_rt->release||!br_rt->health||!br_rt->yield_ms||
  !br_rt->diagnostic||!br_rt->retain_invocation?-1:0;
}
__attribute__((visibility("default"))) void app_module_fini(void){}
__attribute__((visibility("default"))) void app_main(void){
 if(!br_live())return;
 risc_runtime_capability_v1 grants[7]={{0}};bool own[7]={0};
 const risc_scene_api_v1 *scene=NULL;uint64_t session=0,sequence=0;
 const risc_battery_gauge_api_v1 *battery=NULL;
 const twatch_rtc_api_v1 *rtc=NULL;
 const twatch_motion_api_v1 *motion=NULL;const twatch_haptic_api_v1 *haptic=NULL;
 bool native_available=false,motion_tried=false,haptic_tried=false,native_requested=false;
 bool dirty=true,gate=true;uint32_t revision=0,last=0;
 char battery_text[40]="BATTERY UNAVAILABLE",rtc_text[40]="RTC UNAVAILABLE";
 char motion_text[40]="MOTION NOT STARTED",status_text[40]="SELECT A TEST";
 if(!br_get(RISC_SCENE_CAPABILITY,1,&grants[0],&own[0]))goto cleanup;
 scene=grants[0].api;
 const risc_scene_components_api_v1 *components=risc_scene_components_get_v1(scene);
 if(!components||!components->lifecycle.configure||!scene->next||!scene->snapshot||!scene->close)goto cleanup;
 if(br_get("board.battery",1,&grants[1],&own[1])){
  battery=grants[1].api;
  if(!VALID(battery,1,sizeof(*battery))||!battery->read)battery=NULL;
 }
 if(!br_live())return;
 if(br_get("rtc.clock",2,&grants[2],&own[2])){
  rtc=grants[2].api;
  if(!VALID(rtc,2,sizeof(*rtc))||!rtc->read)rtc=NULL;
 }
 if(!br_live())return;
 /* Query metadata only; release before the scene opens. No raw input is
  * subscribed and no competing display lease exists while scene owns UI. */
 if(br_get("display.output",1,&grants[5],&own[5])){
  const risc_display_output_api_v1 *d=grants[5].api;
  risc_display_info_v1 info={0};
  if(VALID(d,1,sizeof(*d))&&d->get_info&&d->acquire&&d->release&&d->submit&&d->present_status){
   bool ok=d->get_info(d->context,&info);
   if(!br_live())return;
   bringup_layout test={0};
   native_available=ok&&(info.supported_formats&RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565))&&
    bringup_layout_open(&test,info.width,info.height);
  }
  if(!br_release(&grants[5]))return;own[5]=false;
 }
 if(!br_live())return;
 if(br_get("input.touch.raw",1,&grants[6],&own[6])){
  const risc_touch_api_v1 *t=grants[6].api;
  native_available=native_available&&VALID(t,1,sizeof(*t))&&t->subscribe&&t->unsubscribe&&t->poll&&t->next&&t->snapshot;
  if(!br_release(&grants[6]))return;own[6]=false;
 }else native_available=false;
 if(!br_live())return;
 br_document(++revision,0,battery_text,rtc_text,motion_text,status_text,native_available);
 risc_scene_navigation_v1 nav={.api_version=1,.struct_size=sizeof(nav),.depth=1,.routes={1}};
 int rc=components->open(scene->context,&br_doc,&nav,&session);
 if(!br_live())return;
 if(rc!=RISC_SCENE_OK||!session){if(session||rc==RISC_SCENE_AGAIN||rc==RISC_SCENE_RETAINED){br_retain();return;}goto cleanup;}
 rc=components->lifecycle.configure(scene->context,session,0);
 if(!br_live())return;
 if(rc!=RISC_SCENE_OK){br_retain();return;}
 for(;;){
  risc_runtime_health_v1 health={.struct_size=sizeof(health)};
  if(!br_live())return;
  bool healthy=br_rt->health(&health);if(!br_live())return;if(!healthy)break;
  uint32_t flags=0;risc_scene_navigation_v1 snapshot={.api_version=1,.struct_size=sizeof(snapshot)};
  rc=scene->snapshot(scene->context,session,&snapshot,&flags);
  if(!br_live())return;
  if(rc!=RISC_SCENE_OK){br_retain();return;}
  bool settled=!(flags&(RISC_SCENE_PRESENTING|RISC_SCENE_CLOSING|RISC_SCENE_INPUT_BUSY));
  risc_scene_event_v1 event={.struct_size=sizeof(event)};
  rc=scene->next(scene->context,session,&event);
  if(!br_live())return;
  if(rc!=RISC_SCENE_OK&&rc!=RISC_SCENE_IDLE){br_retain();return;}
  if(rc==RISC_SCENE_IDLE&&settled)gate=false;
  if(rc==RISC_SCENE_OK&&event.struct_size>=sizeof(event)&&event.sequence>sequence){
   sequence=event.sequence;
   if(settled&&!gate&&event.document_revision==revision&&
      event.kind==RISC_SCENE_ACTION_EVENT&&br_event(&event)){
    gate=true;
    if(event.action==3)break;
    if(event.action==4){native_requested=true;break;}
    if(event.action==1){
     if(!motion_tried){motion_tried=true;
      if(br_get("motion.accel",1,&grants[3],&own[3])){
       motion=grants[3].api;
       if(!VALID(motion,1,TWATCH_MOTION_SAMPLE_SIZE)||!motion->read)motion=NULL;
      }
     }
     if(!br_live())return;
     snprintf(status_text,sizeof(status_text),"%s",motion?"MOTION SAMPLING ON":"MOTION UNAVAILABLE");
    }else if(event.action==2){
     if(!haptic_tried){haptic_tried=true;
      if(br_get("haptic.effect",1,&grants[4],&own[4])){
       haptic=grants[4].api;
       if(!VALID(haptic,1,sizeof(*haptic))||!haptic->effect||!haptic->stop)haptic=NULL;
      }
     }
     if(!br_live())return;
     bool ok=haptic&&haptic->effect(haptic->context,14);
     if(!br_live())return;
     snprintf(status_text,sizeof(status_text),"%s",ok?"VIBRATION REQUESTED":"VIBRATE UNAVAILABLE");
    }
    dirty=true;
   }
  }
  if(settled&&(dirty||(uint32_t)(health.uptime_ms-last)>=1000)){
   risc_battery_sample_v1 b={0};
   if(battery){
    bool ok=battery->read(battery->context,&b);if(!br_live())return;
    if(!ok)snprintf(battery_text,sizeof(battery_text),"BATTERY READ ERROR");
    else if(b.percent<=100)snprintf(battery_text,sizeof(battery_text),"BATTERY %u%% %u MV",b.percent,b.millivolts);
    else snprintf(battery_text,sizeof(battery_text),"BATTERY --%% %u MV",b.millivolts);
   }
   if(rtc){
    twatch_rtc_time_v1 date={0};bool ok=rtc->read(rtc->context,&date);if(!br_live())return;
    if(ok&&tw_valid_time(&date))snprintf(rtc_text,sizeof(rtc_text),"RTC RAW %02u:%02u:%02u",date.hour,date.minute,date.second);
    else snprintf(rtc_text,sizeof(rtc_text),"RTC UNSET");
   }
   if(motion){
    twatch_accel_sample_v1 sample={0};bool ok=motion->read(motion->context,&sample);if(!br_live())return;
    if(ok)snprintf(motion_text,sizeof(motion_text),"XYZ %d %d %d MG",sample.x,sample.y,sample.z);
    else snprintf(motion_text,sizeof(motion_text),"MOTION READ ERROR");
   }
   br_document(++revision,health.uptime_ms/1000,battery_text,rtc_text,motion_text,status_text,native_available);
   rc=components->update(scene->context,session,&br_doc);
   if(!br_live())return;
   if(rc!=RISC_SCENE_OK){br_retain();return;}
   dirty=false;last=health.uptime_ms;
  }
  br_rt->yield_ms(2);
 }
cleanup:
 if(!br_live())return;
 if(session&&!br_close(scene,session))return;
 if(haptic){
  bool ok=haptic->stop(haptic->context);if(!br_live())return;
  if(!ok){br_retain();return;}
 }
 for(unsigned n=7;n;n--)if(own[n-1]&&!br_release(&grants[n-1]))return;
 if(native_requested&&br_live())bringup_native_main();
}
