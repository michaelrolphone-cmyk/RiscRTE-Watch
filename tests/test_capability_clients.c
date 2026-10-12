#include "WatchCapabilityClients.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned available,owned,fail_release,reads,requests;
static const void *tables[4];
static const char *const names[]={"board.battery","power.sleep","input.power-key","display.power"};
static int battery_context,key_context,sleep_context,display_context;
static bool battery_read(void *ctx,risc_battery_sample_v1 *out){assert(ctx==&battery_context);*out=(risc_battery_sample_v1){3950,73,0};reads++;return true;}
static bool key_read(void *ctx,uint32_t *out){assert(ctx==&key_context);*out=RISC_POWER_KEY_SHORT_PRESS;return true;}
static bool power_prepare(void *ctx){assert(ctx==&sleep_context);return true;}
static bool display_prepare(void *ctx){assert(ctx==&display_context);return true;}
static int32_t sleep_light(void *ctx,risc_light_sleep_result_v1 *out){assert(ctx==&sleep_context);out->wake_cause=RISC_LIGHT_SLEEP_WAKE_GPIO;return 0;}
static bool get(const char *id,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *g) {
 assert(version==1&&instance==0&&g->struct_size==sizeof(*g));requests++;
 for(unsigned n=0;n<4;n++)if(!strcmp(names[n],id)) {
  if(!(available&(1u<<n)))return false;
  assert(!(owned&(1u<<n)));owned|=1u<<n;g->slot=n;g->generation=1;g->api=tables[n];return true;
 }
 assert(!"unregistered or device-specific capability");return false;
}
static bool drop(risc_runtime_capability_v1 *g){assert(g->generation==1&&(owned&(1u<<g->slot)));if(fail_release==g->slot+1)return false;owned&=~(1u<<g->slot);g->generation=0;return true;}
static risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.acquire=get,.release=drop};
int main(void) {
 /* Exact base allocation: ASan would catch any attempt to read a PMU suffix. */
 risc_battery_gauge_api_v1 *battery=malloc(sizeof(*battery));assert(battery);
 *battery=(risc_battery_gauge_api_v1){1,sizeof(*battery),&battery_context,battery_read};
 risc_power_sleep_api_v1 sleep={.api_version=1,.struct_size=RISC_POWER_LIGHT_SLEEP_SIZE,.context=&sleep_context,.prepare_sleep=power_prepare,.resume=power_prepare,.light_sleep=sleep_light};
 risc_power_key_api_v1 key={1,sizeof(key),&key_context,key_read};
 risc_display_power_api_v1 display={.api_version=1,.struct_size=RISC_DISPLAY_POWER_LIGHT_SLEEP_SIZE,.context=&display_context,.prepare_sleep=display_prepare,.resume=display_prepare};
 tables[0]=battery;tables[1]=&sleep;tables[2]=&key;tables[3]=&display;
 for(available=0;available<16;available++) {
  watch_capability_clients c={0};requests=reads=0;
  assert(watch_capabilities_open(&c,&runtime)&&requests==4);
  assert(!!c.battery==!!(available&1));assert(!!c.sleep==!!(available&2));
  assert(!!c.key==!!(available&4));assert(!!c.display_power==!!(available&8));
  assert(watch_capabilities_can_sleep(&c)==((available&10)==10));
  if(c.battery){risc_battery_sample_v1 sample;assert(c.battery->read(c.battery->context,&sample)&&sample.percent==73&&reads==1);}
  if(c.sleep){risc_light_sleep_result_v1 r={0};assert(c.sleep->prepare_sleep(c.sleep->context)&&!c.sleep->light_sleep(c.sleep->context,&r)&&c.sleep->resume(c.sleep->context));}
  if(c.display_power)assert(c.display_power->prepare_sleep(c.display_power->context)&&c.display_power->resume(c.display_power->context));
  uint32_t events=999;assert(watch_capabilities_key_events(&c,&events));assert(events==((available&4)?RISC_POWER_KEY_SHORT_PRESS:0));
  assert(watch_capabilities_close(&c)&&!owned);assert(watch_capabilities_close(&c));
 }
 available=15;
 for(unsigned broken=0;broken<4;broken++) {
  uint32_t *head=(uint32_t*)tables[broken];head[0]=2;
  watch_capability_clients c;assert(!watch_capabilities_open(&c,&runtime));assert(watch_capabilities_close(&c)&&!owned);head[0]=1;
  uint32_t size=head[1];head[1]=8;assert(!watch_capabilities_open(&c,&runtime));assert(watch_capabilities_close(&c)&&!owned);head[1]=size;
 }
 for(fail_release=1;fail_release<=4;fail_release++) {
  watch_capability_clients c;assert(watch_capabilities_open(&c,&runtime));
  assert(!watch_capabilities_close(&c)&&owned&&c.runtime==&runtime);
  unsigned keep=fail_release;fail_release=0;assert(watch_capabilities_close(&c)&&!owned);fail_release=keep;
 }
 fail_release=0;const void *saved=tables[1];tables[1]=NULL;
 watch_capability_clients c;assert(!watch_capabilities_open(&c,&runtime));assert(watch_capabilities_close(&c)&&!owned);tables[1]=saved;
 assert(!watch_capabilities_open(NULL,&runtime)&&!watch_capabilities_open(&c,NULL));
 assert(!watch_capabilities_key_events(NULL,&available)&&!watch_capabilities_key_events(&c,NULL));
 free(battery);puts("Generic capability clients: 16 independent service combinations, base-only gauge, no-gauge sleep, API errors and retained cleanup PASS");
}
