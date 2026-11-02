/* Extracted from RiscRTE-System-Apps 14762601b3fa7bbeeac5ed565a6825534bf74dca.
 * Hardware providers resolve by capability; storage instance 1 is the existing
 * settings namespace, not a device identity. */
#include "PortableQuickRadios.h"
static bool bind(const risc_runtime_api_v1*rt,risc_runtime_capability_v1*g,const char*name,unsigned instance) {
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return rt->acquire(name,1,instance,g);
}
static const portable_bluetooth_control_v1* ble(const risc_runtime_capability_v1*g) {
 const portable_bluetooth_control_v1*b=g->api;
 return b&&b->api_version==1&&b->struct_size>=sizeof(*b)&&b->set_enabled&&b->status?b:NULL;
}
static const wifi_api_v1* wifi(const risc_runtime_capability_v1*g) {
 const wifi_api_v1*w=g->api;
 return w&&w->api_version==1&&w->struct_size>=sizeof(*w)&&w->status&&w->disconnect_checked?w:NULL;
}
static void reflect(const pqa_radios*s,pqa_state*u) {
 u->radio_controls=true;u->radios_valid=s->valid&&s->available;
#if defined(PORTABLE_QUICK_CAPABILITY_SAFE) || defined(PORTABLE_QUICK_RADIO_AVAILABILITY)
 u->wifi_available=s->wifi_available;u->bluetooth_available=s->bluetooth_available;
 u->wifi_valid=s->valid&&s->wifi_available;u->bluetooth_valid=s->valid&&s->bluetooth_available;
#endif
 u->wifi_enabled=!!(s->flags&PORTABLE_RADIO_WIFI);u->bluetooth_enabled=!!(s->flags&PORTABLE_RADIO_BLUETOOTH);u->airplane=!!(s->flags&PORTABLE_RADIO_AIRPLANE);
}
static bool set_hardware(const wifi_api_v1*w,const portable_bluetooth_control_v1*b,uint8_t flags,bool preserve) {
 if(w && !(flags&PORTABLE_RADIO_WIFI) && (!w->disconnect_checked(w->context)||w->status(w->context)!=WIFI_LINK_DOWN))return false;
 if(!b)return true;
 uint8_t actual=PORTABLE_BLUETOOTH_RETAINED;
 const uint8_t desired=(flags&PORTABLE_RADIO_BLUETOOTH)?PORTABLE_BLUETOOTH_ON:PORTABLE_BLUETOOTH_OFF;
 /* Re-entering an ordinary app must not reset a healthy provider-owned BLE
  * lease merely to reassert the same saved preference. Explicit changes still
  * drain their owners before coming here. */
 if(preserve && b->status(b->context,&actual) && actual==desired)return true;
 return b->set_enabled(b->context,desired==PORTABLE_BLUETOOTH_ON) && b->status(b->context,&actual) && actual==desired;
}
/* Explicit read-only admission for settings editors. It observes existing
 * desired preferences and provider status without applying radio policy.
 * A false result leaves uncertain grants pinned for the caller's custody fence. */
bool pqa_radios_inspect(pqa_radios*s,pqa_state*u,const risc_runtime_api_v1*rt,
                       bool(*safe)(void*),void*context) {
 if(!s||!u||!rt||!safe||rt->api_version!=1||
    rt->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE||!rt->acquire||!rt->release||!safe(context))return false;
 pqa_radios next={0};
 risc_runtime_capability_v1 kg,wg,bg;
 bool have_k=bind(rt,&kg,RISC_KEY_VALUE_CAPABILITY,1);
 if(!safe(context))return false;
 next.valid=portable_radio_load(kg.api,&next.flags);
 if(!safe(context))return false;
 if(have_k&&!rt->release(&kg))return false;
 if(!safe(context))return false;
 bool have_w=bind(rt,&wg,"net.wifi",0);
 if(!safe(context))return false;
 bool have_b=bind(rt,&bg,"bluetooth.hci",0);
 if(!safe(context))return false;
 const wifi_api_v1*w=wifi(&wg);
 const portable_bluetooth_control_v1*b=ble(&bg);
 if(w){
  wifi_link_t state=w->status(w->context);
  if(!safe(context))return false;
  if((unsigned)state>WIFI_LINK_UP)return false;
 }
 if(b){
  uint8_t state=PORTABLE_BLUETOOTH_RETAINED;
  bool observed=b->status(b->context,&state);
  if(!safe(context))return false;
  if(!observed||state>PORTABLE_BLUETOOTH_ON)return false;
 }
 if(have_b&&!rt->release(&bg))return false;
 if(!safe(context))return false;
 if(have_w&&!rt->release(&wg))return false;
 if(!safe(context))return false;
#if defined(PORTABLE_QUICK_CAPABILITY_SAFE) || defined(PORTABLE_QUICK_RADIO_AVAILABILITY)
 next.wifi_available=w!=NULL;next.bluetooth_available=b!=NULL;next.available=w||b;
#else
 next.available=w&&b;
#endif
 *s=next;reflect(s,u);
 if(!s->valid)u->error_flags|=PQA_ERROR_RADIO|PQA_ERROR_SAVE;
 return true;
}
bool pqa_radios_load(pqa_radios*s,pqa_state*u,const risc_runtime_api_v1*rt) {
 *s=(pqa_radios){0};risc_runtime_capability_v1 kg,wg,bg;
 (void)bind(rt,&kg,RISC_KEY_VALUE_CAPABILITY,1);s->valid=portable_radio_load(kg.api,&s->flags);
 if(kg.api&&!rt->release(&kg))return false;
 (void)bind(rt,&wg,"net.wifi",0);(void)bind(rt,&bg,"bluetooth.hci",0);
 const wifi_api_v1*w=wifi(&wg);const portable_bluetooth_control_v1*b=ble(&bg);
 /* An unreadable/corrupt desired state must not leave a previously enabled
  * controller running behind disabled controls. Prove each available radio Off without
  * overwriting the bad record; failed cleanup retains the invocation. */
 #if defined(PORTABLE_QUICK_CAPABILITY_SAFE) || defined(PORTABLE_QUICK_RADIO_AVAILABILITY)
 s->wifi_available=w!=NULL;s->bluetooth_available=b!=NULL;s->available=w||b;
#else
 s->available=w&&b;
#endif
 bool ok=set_hardware(w,b,s->valid?s->flags:0,true);
 if(!s->valid)u->error_flags|=PQA_ERROR_RADIO|PQA_ERROR_SAVE;
 if(bg.api&&!rt->release(&bg))ok=false;
 if(wg.api&&!rt->release(&wg))ok=false;
 reflect(s,u);if(!ok)u->error_flags|=PQA_ERROR_RADIO;
 return ok;
}
#ifdef PORTABLE_QUICK_ATOMIC_PREFERENCES
bool pqa_radios_apply_user(pqa_radios*s,pqa_state*u,const risc_runtime_api_v1*rt,uint32_t actions,pqa_user_preference_save save,void *context,bool(*safe)(void*)) {
#else
bool pqa_radios_apply(pqa_radios*s,pqa_state*u,const risc_runtime_api_v1*rt,uint32_t actions) {
#endif
 if(!(actions&(PQA_WIFI|PQA_BLUETOOTH|PQA_AIRPLANE)))return true;
 if(!s->valid||!s->available){u->error_flags|=PQA_ERROR_RADIO;return true;}
#if defined(PORTABLE_QUICK_CAPABILITY_SAFE) || defined(PORTABLE_QUICK_RADIO_AVAILABILITY)
 if(((actions&PQA_WIFI)&&!s->wifi_available)||((actions&PQA_BLUETOOTH)&&!s->bluetooth_available)){
  u->error_flags|=PQA_ERROR_RADIO;return true;
 }
#endif
 uint8_t next=s->flags;
 if(actions&PQA_AIRPLANE) {
  if(next&PORTABLE_RADIO_AIRPLANE)next=(uint8_t)((next>>3)&3u);
  else next=(uint8_t)(PORTABLE_RADIO_AIRPLANE|((next&3u)<<3));
 } else {
  uint8_t bit=(actions&PQA_WIFI)?PORTABLE_RADIO_WIFI:PORTABLE_RADIO_BLUETOOTH;
  next^=bit;
  if(next&bit)next&=(uint8_t)~(PORTABLE_RADIO_AIRPLANE|PORTABLE_RADIO_PREVIOUS_WIFI|PORTABLE_RADIO_PREVIOUS_BLUETOOTH);
 }
 risc_runtime_capability_v1 wg,bg;
 (void)bind(rt,&wg,"net.wifi",0);(void)bind(rt,&bg,"bluetooth.hci",0);
#ifndef PORTABLE_QUICK_ATOMIC_PREFERENCES
 risc_runtime_capability_v1 kg;(void)bind(rt,&kg,RISC_KEY_VALUE_CAPABILITY,1);
#endif
 const wifi_api_v1*w=wifi(&wg);const portable_bluetooth_control_v1*b=ble(&bg);
 /* A provider disappearing between reflection and explicit action is not
  * success. Do not persist an unconfirmed transition. */
#if defined(PORTABLE_QUICK_CAPABILITY_SAFE) || defined(PORTABLE_QUICK_RADIO_AVAILABILITY)
 bool same=(w!=NULL)==s->wifi_available&&(b!=NULL)==s->bluetooth_available;
#else
 bool same=w&&b;
#endif
 bool changed=same&&set_hardware(w,b,next,false);bool ok=true;
#ifdef PORTABLE_QUICK_ATOMIC_PREFERENCES
 const uint8_t record[]={0x51,1,next,(uint8_t)(next^0xa5u)};
 bool saved=changed&&save&&save(context,PORTABLE_RADIO_KEY,record,4,NULL);
 if(!safe||!safe(context))return false;
 if(saved){
#else
 if(changed&&portable_radio_save(kg.api,next)){
#endif
 s->flags=next;u->error_flags&=~(PQA_ERROR_RADIO|PQA_ERROR_SAVE);}
 else {
  /* An explicit operation failed. Prove restoration; otherwise retain the
   * invocation instead of claiming Off or proceeding into native sleep. */
  u->error_flags|=PQA_ERROR_RADIO|PQA_ERROR_SAVE;
  ok=set_hardware(w,b,s->flags,false)&&same;
 }
#ifndef PORTABLE_QUICK_ATOMIC_PREFERENCES
 if(kg.api&&!rt->release(&kg))ok=false;
#endif
 if(bg.api&&!rt->release(&bg))ok=false;
 if(wg.api&&!rt->release(&wg))ok=false;
 reflect(s,u);return ok;
}
bool pqa_radios_suspend(const risc_runtime_api_v1*rt) {
 risc_runtime_capability_v1 g;if(!bind(rt,&g,"bluetooth.hci",0))return true;
 const portable_bluetooth_control_v1*b=ble(&g);uint8_t state=PORTABLE_BLUETOOTH_RETAINED;
 bool ok=b&&b->set_enabled(b->context,false)&&b->status(b->context,&state)&&state==PORTABLE_BLUETOOTH_OFF;
 return rt->release(&g)&&ok;
}
bool pqa_radios_resume(pqa_radios*s,pqa_state*u,const risc_runtime_api_v1*rt) {return pqa_radios_load(s,u,rt);}

#ifdef PORTABLE_QUICK_ATOMIC_PREFERENCES
bool pqa_radios_apply(pqa_radios*s,pqa_state*u,const risc_runtime_api_v1*rt,uint32_t actions) {
 (void)s;(void)rt;if(actions&(PQA_WIFI|PQA_BLUETOOTH|PQA_AIRPLANE))u->error_flags|=PQA_ERROR_SAVE|PQA_ERROR_RADIO;return true;
}
#endif
