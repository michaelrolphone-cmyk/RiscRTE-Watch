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
 u->wifi_enabled=!!(s->flags&PORTABLE_RADIO_WIFI);u->bluetooth_enabled=!!(s->flags&PORTABLE_RADIO_BLUETOOTH);u->airplane=!!(s->flags&PORTABLE_RADIO_AIRPLANE);
}
static bool set_hardware(const wifi_api_v1*w,const portable_bluetooth_control_v1*b,uint8_t flags,bool preserve) {
 if(!(flags&PORTABLE_RADIO_WIFI) && (!w->disconnect_checked(w->context)||w->status(w->context)!=WIFI_LINK_DOWN))return false;
 uint8_t actual=PORTABLE_BLUETOOTH_RETAINED;
 const uint8_t desired=(flags&PORTABLE_RADIO_BLUETOOTH)?PORTABLE_BLUETOOTH_ON:PORTABLE_BLUETOOTH_OFF;
 /* Re-entering an ordinary app must not reset a healthy provider-owned BLE
  * lease merely to reassert the same saved preference. Explicit changes still
  * drain their owners before coming here. */
 if(preserve && b->status(b->context,&actual) && actual==desired)return true;
 return b->set_enabled(b->context,desired==PORTABLE_BLUETOOTH_ON) && b->status(b->context,&actual) && actual==desired;
}
bool pqa_radios_load(pqa_radios*s,pqa_state*u,const risc_runtime_api_v1*rt) {
 *s=(pqa_radios){0};risc_runtime_capability_v1 kg,wg,bg;
 (void)bind(rt,&kg,RISC_KEY_VALUE_CAPABILITY,1);s->valid=portable_radio_load(kg.api,&s->flags);
 if(kg.api&&!rt->release(&kg))return false;
 (void)bind(rt,&wg,"net.wifi",0);(void)bind(rt,&bg,"bluetooth.hci",0);
 const wifi_api_v1*w=wifi(&wg);const portable_bluetooth_control_v1*b=ble(&bg);
 /* An unreadable/corrupt desired state must not leave a previously enabled
  * controller running behind disabled controls. Prove both radios Off without
  * overwriting the bad record; failed cleanup retains the invocation. */
 s->available=w&&b;bool ok=!s->available||set_hardware(w,b,s->valid?s->flags:0,true);
 if(!s->valid)u->error_flags|=PQA_ERROR_RADIO|PQA_ERROR_SAVE;
 if(bg.api&&!rt->release(&bg))ok=false;
 if(wg.api&&!rt->release(&wg))ok=false;
 reflect(s,u);if(!ok)u->error_flags|=PQA_ERROR_RADIO;
 return ok;
}
bool pqa_radios_apply(pqa_radios*s,pqa_state*u,const risc_runtime_api_v1*rt,uint32_t actions) {
 if(!(actions&(PQA_WIFI|PQA_BLUETOOTH|PQA_AIRPLANE)))return true;
 if(!s->valid||!s->available){u->error_flags|=PQA_ERROR_RADIO;return true;}
 uint8_t next=s->flags;
 if(actions&PQA_AIRPLANE) {
  if(next&PORTABLE_RADIO_AIRPLANE)next=(uint8_t)((next>>3)&3u);
  else next=(uint8_t)(PORTABLE_RADIO_AIRPLANE|((next&3u)<<3));
 } else {
  uint8_t bit=(actions&PQA_WIFI)?PORTABLE_RADIO_WIFI:PORTABLE_RADIO_BLUETOOTH;
  next^=bit;
  if(next&bit)next&=(uint8_t)~(PORTABLE_RADIO_AIRPLANE|PORTABLE_RADIO_PREVIOUS_WIFI|PORTABLE_RADIO_PREVIOUS_BLUETOOTH);
 }
 risc_runtime_capability_v1 wg,bg,kg;
 (void)bind(rt,&wg,"net.wifi",0);(void)bind(rt,&bg,"bluetooth.hci",0);(void)bind(rt,&kg,RISC_KEY_VALUE_CAPABILITY,1);
 const wifi_api_v1*w=wifi(&wg);const portable_bluetooth_control_v1*b=ble(&bg);
 bool changed=w&&b&&set_hardware(w,b,next,false);bool ok=true;
 if(changed&&portable_radio_save(kg.api,next)){s->flags=next;u->error_flags&=~(PQA_ERROR_RADIO|PQA_ERROR_SAVE);}
 else {
  /* An explicit operation failed. Prove restoration; otherwise retain the
   * invocation instead of claiming Off or proceeding into native sleep. */
  u->error_flags|=PQA_ERROR_RADIO|PQA_ERROR_SAVE;
  ok=w&&b&&set_hardware(w,b,s->flags,false);
 }
 if(kg.api&&!rt->release(&kg))ok=false;
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
