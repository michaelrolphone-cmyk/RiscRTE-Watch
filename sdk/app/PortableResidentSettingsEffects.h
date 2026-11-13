#pragma once
/* Shared host-owned settings executor. Bind only at child-entry with validated
 * provider tables; caller owns settled dispatch and invocation boundaries.
 * Same protocol on raster/paper hosts. No product selection or Runtime proxy. */
#include "RiscRuntimeV1.h"
#include "RiscDisplayOutputV1.h"
#include "AlarmServiceV1.h"
#include "RiscResidentSettingsEffectsV1.h"
#include "PortableQuickPreferences.h"
#include "PortableRadioPolicy.h"
#include "PortableBluetoothControl.h"
#include "WifiApi.h"
typedef struct {
 const risc_runtime_api_v1*runtime;
 const risc_display_output_api_v1*display;
 const alarm_service_v1*alarm;
 bool (*alive)(void*);void (*retain)(void*);void*context;
 uint32_t supported;unsigned result,id,brightness;
 bool running,progressing,retained;
} portable_resident_settings_effects;
static inline bool prse_alive(portable_resident_settings_effects*s){return s&&!s->retained&&s->runtime&&risc_runtime_get_api(1)==s->runtime&&s->alive&&s->alive(s->context);}
static inline int32_t prse_retain(portable_resident_settings_effects*s){if(!s->retained){s->retained=true;if(s->retain)s->retain(s->context);}return RISC_RESIDENT_RETAINED;}
static inline void prse_clear(portable_resident_settings_effects*s){s->result=s->id=0;}
static bool prse_empty(const risc_runtime_capability_v1*g){return !g->api&&!g->slot&&!g->generation;}
static int prse_acquire(portable_resident_settings_effects*s,const char*name,unsigned id,risc_runtime_capability_v1*g){
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};
 bool ok=s->runtime->acquire(name,1,id,g);if(!prse_alive(s))return -1;
 if(g->struct_size!=sizeof(*g)||(ok?(!g->api||!g->slot||!g->generation):!prse_empty(g))){(void)prse_retain(s);return -1;}
 return ok?0:1;
}
static bool prse_release(portable_resident_settings_effects*s,risc_runtime_capability_v1*g){
 if(!prse_alive(s))return false;
 if(prse_empty(g))return true;
 risc_runtime_capability_v1 copy=*g;bool ok=s->runtime->release(&copy);
 if(!prse_alive(s))return false;
 if(!ok||copy.struct_size!=sizeof(copy)||!prse_empty(&copy)){(void)prse_retain(s);return false;}
 *g=(risc_runtime_capability_v1){0};return true;
}
static bool prse_supported(portable_resident_settings_effects*s,unsigned effect){
 if(!(s->supported&(1u<<effect)))return false;
 if(effect==RISC_SETTINGS_BRIGHTNESS)return s->display&&s->display->set_brightness;

 if(effect==RISC_SETTINGS_WIFI||effect==RISC_SETTINGS_BLUETOOTH)return true;


 if(effect==RISC_SETTINGS_ALARM)return s->alarm&&s->alarm->refresh&&s->alarm->status&&s->alarm->step;

 return false;
}

static unsigned prse_apply(portable_resident_settings_effects*s,unsigned effect){

 if(effect==RISC_SETTINGS_ALARM){
  int rc=s->progressing?s->alarm->step(s->alarm->context):s->alarm->refresh(s->alarm->context);if(!prse_alive(s))return 0;
  if(rc<ALARM_FOREGROUND||rc>ALARM_PENDING||rc==ALARM_OUTPUT){(void)prse_retain(s);return 0;}
  alarm_status_v1 st={.struct_size=sizeof(st)};
  int sr=s->alarm->status(s->alarm->context,&st);if(!prse_alive(s))return 0;
  if(sr!=ALARM_OK||st.api_version!=1||st.struct_size<sizeof(st)||st.state>ALARM_STATE_CUE||st.output_uncertain||st.error<ALARM_FOREGROUND||st.error>ALARM_PENDING||st.error==ALARM_OUTPUT){(void)prse_retain(s);return 0;}
  return rc==ALARM_PENDING?RISC_SETTINGS_PENDING:rc==ALARM_OK?RISC_SETTINGS_APPLIED:RISC_SETTINGS_FAILED;
 }

 risc_runtime_capability_v1 kg={0};int ac=prse_acquire(s,RISC_KEY_VALUE_CAPABILITY,1,&kg);if(ac)return ac<0?0:RISC_SETTINGS_FAILED;
 const risc_key_value_v1*kv=kg.api;
 if(kv->api_version!=1||kv->struct_size<sizeof(*kv)||!kv->get){(void)prse_retain(s);return 0;}
 uint8_t bytes[4]={0};uint32_t n=0;
 int rc=kv->get(kv->context,effect==RISC_SETTINGS_BRIGHTNESS?PQA_BRIGHTNESS_KEY:PORTABLE_RADIO_KEY,bytes,effect==RISC_SETTINGS_BRIGHTNESS?1:sizeof(bytes),&n);
 if(!prse_alive(s))return 0;
 if(rc>0||rc<RISC_KEY_VALUE_BUSY||rc==RISC_KEY_VALUE_CONTEXT||(!rc&&n>sizeof(bytes))){(void)prse_retain(s);return 0;}
 bool valid=false;uint8_t value=0;
 if(effect==RISC_SETTINGS_BRIGHTNESS){value=rc==RISC_KEY_VALUE_NOT_FOUND?PQA_BRIGHTNESS_DEFAULT:bytes[0];valid=rc==RISC_KEY_VALUE_NOT_FOUND||(!rc&&n==1&&value>=10&&value<=100);}
 else {value=PORTABLE_RADIO_WIFI;valid=rc==RISC_KEY_VALUE_NOT_FOUND||(!rc&&portable_radio_decode(bytes,n,&value));}
 if(!prse_release(s,&kg))return 0;
 if(!valid)return RISC_SETTINGS_FAILED;
 if(effect==RISC_SETTINGS_BRIGHTNESS){
  bool ok=s->display->set_brightness(s->display->context,value,100);if(!prse_alive(s))return 0;
  /* The display Boolean has no clean-failure custody discriminator. */
  if(!ok){(void)prse_retain(s);return 0;}
  s->brightness=value;return RISC_SETTINGS_APPLIED;
 }

 risc_runtime_capability_v1 g={0};ac=prse_acquire(s,effect==RISC_SETTINGS_WIFI?"net.wifi":"bluetooth.hci",0,&g);if(ac)return ac<0?0:RISC_SETTINGS_FAILED;
 unsigned result=RISC_SETTINGS_FAILED;
 if(effect==RISC_SETTINGS_WIFI){
  const wifi_api_v1*w=g.api;if(w->api_version!=1||w->struct_size<WIFI_MANAGEMENT_V1_SIZE||!w->status||!w->disconnect_checked){(void)prse_retain(s);return 0;}
  if(value&PORTABLE_RADIO_WIFI)result=RISC_SETTINGS_APPLIED; /* permission is not auto-connect */
  else {
   bool ok=w->disconnect_checked(w->context);if(!prse_alive(s))return 0;
   if(!ok){(void)prse_retain(s);return 0;}
   wifi_link_t state=w->status(w->context);if(!prse_alive(s))return 0;
   if(state<WIFI_LINK_DOWN||state>WIFI_LINK_UP){(void)prse_retain(s);return 0;}
   result=state==WIFI_LINK_DOWN?RISC_SETTINGS_APPLIED:RISC_SETTINGS_FAILED;
  }
 }else{
  const portable_bluetooth_control_v1*b=g.api;if(b->api_version!=1||b->struct_size<sizeof(*b)||!b->status||!b->set_enabled){(void)prse_retain(s);return 0;}
  uint8_t state=PORTABLE_BLUETOOTH_RETAINED,wanted=(value&PORTABLE_RADIO_BLUETOOTH)?PORTABLE_BLUETOOTH_ON:PORTABLE_BLUETOOTH_OFF;
  bool ok=b->status(b->context,&state);if(!prse_alive(s))return 0;
  if(!ok||state>=PORTABLE_BLUETOOTH_RETAINED){(void)prse_retain(s);return 0;}
  if(state!=wanted){
   ok=b->set_enabled(b->context,wanted==PORTABLE_BLUETOOTH_ON);if(!prse_alive(s))return 0;
   /* status is the published copied custody discriminator after refusal. */
   bool read=b->status(b->context,&state);if(!prse_alive(s))return 0;
   if(!read||state>=PORTABLE_BLUETOOTH_RETAINED){(void)prse_retain(s);return 0;}
  }
  result=ok&&state==wanted?RISC_SETTINGS_APPLIED:RISC_SETTINGS_FAILED;
 }
 if(!prse_release(s,&g))return 0;
 return result;


}
static bool prse_reason(uint32_t reason){return (reason&UINT32_C(0xffff0000))==UINT32_C(0x53450000);}
static int32_t prse_dispatch(portable_resident_settings_effects*s,const risc_resident_request_v1*q){
 if(!prse_alive(s))return prse_retain(s);
 if(s->running)return prse_retain(s);
 if(q->flags||q->reserved)return RISC_RESIDENT_INVALID;
 if(q->reason==RISC_SETTINGS_PROGRESS(RISC_SETTINGS_ALARM)){
  if(s->id!=RISC_SETTINGS_ALARM||s->result!=RISC_SETTINGS_PENDING)return RISC_RESIDENT_BUSY;
  s->running=true;s->progressing=true;
  unsigned result=prse_apply(s,RISC_SETTINGS_ALARM);
  if(!prse_alive(s)||!result)return prse_retain(s);
  s->progressing=false;s->running=false;s->result=result;
  return RISC_RESIDENT_OK;
 }
 for(unsigned effect=1;effect<=4;effect++){
  if(q->reason==RISC_SETTINGS_QUERY(effect))return prse_supported(s,effect)?RISC_RESIDENT_OK:RISC_RESIDENT_BUSY;
  for(unsigned result=1;result<=3;result++)if(q->reason==RISC_SETTINGS_RESULT(effect,result))return s->id==effect&&s->result==result?RISC_RESIDENT_OK:RISC_RESIDENT_BUSY;
  if(q->reason==RISC_SETTINGS_APPLY(effect)){
   if(!prse_supported(s,effect)||s->result==RISC_SETTINGS_PENDING)return RISC_RESIDENT_BUSY;
   prse_clear(s);s->running=true;
   unsigned result=prse_apply(s,effect);
   if(!prse_alive(s)||!result)return prse_retain(s);
   s->running=false;s->id=effect;s->result=result;
   return RISC_RESIDENT_OK;
  }
 }
 return RISC_RESIDENT_BUSY;
}
