#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "PortableQuickRadios.h"
#include "PortableQuickLayout.h"
#include "PortableQuickRender.h"
static uint16_t pixels[TEST_WIDTH*TEST_HEIGHT];
static uint32_t snapshots[4];
static unsigned present,malformed,grants,writes,ble_sets,wifi_disconnects;
static bool corrupt,save_ok,safe_ok,release_ok,ble_fail,wifi_fail;
static uint8_t record,ble_state;
static bool inspection_mode,inspection_stopped,inspection_no_storage;
static unsigned inspection_calls,inspection_cutoff,inspection_releases,inspection_release_failure;
static void inspect_observe(void){if(inspection_mode){assert(!inspection_stopped);++inspection_calls;}}
static wifi_link_t link_state;
static int32_t get(void*c,const char*k,void*out,uint32_t capacity,uint32_t*n){inspect_observe();
 (void)c;assert(!strcmp(k,PORTABLE_RADIO_KEY)&&capacity>=4);*n=4;
 uint8_t b[]={0x51,1,record,(uint8_t)(record^0xa5u^(corrupt?1:0))};memcpy(out,b,4);return RISC_KEY_VALUE_OK;
}
static int32_t put(void*c,const char*k,const void*p,uint32_t n){inspect_observe();(void)c;(void)k;(void)p;(void)n;assert(!"Unscoped write");return RISC_KEY_VALUE_IO;}
static bool disconnect(void*c){inspect_observe();(void)c;++wifi_disconnects;if(wifi_fail)return false;link_state=WIFI_LINK_DOWN;return true;}
static wifi_link_t status(void*c){inspect_observe();(void)c;return link_state;}
static bool enable(void*c,bool on){inspect_observe();(void)c;++ble_sets;if(ble_fail)return false;ble_state=on;return true;}
static bool bt_status(void*c,uint8_t*out){inspect_observe();(void)c;*out=ble_state;return true;}
static const risc_key_value_v1 kv={1,sizeof(kv),NULL,get,put};
static const wifi_api_v1 wifi_api={.api_version=1,.struct_size=sizeof(wifi_api),.status=status,.disconnect_checked=disconnect};
static const portable_bluetooth_control_v1 bt_api={.api_version=1,.struct_size=sizeof(bt_api),.set_enabled=enable,.status=bt_status};
static const struct {uint32_t version,size;} bad={2,8};
static bool acquire(const char*n,uint32_t v,uint64_t instance,risc_runtime_capability_v1*g){inspect_observe();
 assert(v==1);const void*p=NULL;unsigned bit=0;
 if(!strcmp(n,RISC_KEY_VALUE_CAPABILITY)){assert(instance==1);if(inspection_mode&&inspection_no_storage)return false;p=&kv;}
 else {assert(instance==0);if(!strcmp(n,"net.wifi")){bit=1;p=&wifi_api;}else{assert(!strcmp(n,"bluetooth.hci"));bit=2;p=&bt_api;}
 if(!(present&bit))return false;if(malformed&bit)p=&bad;}
 g->api=p;++grants;return true;
}
static bool release(risc_runtime_capability_v1*g){inspect_observe();assert(g->api&&grants);if(inspection_mode&&++inspection_releases==inspection_release_failure){inspection_stopped=true;return false;}if(!release_ok)return false;g->api=NULL;--grants;return true;}
static const risc_runtime_api_v1 rt={.api_version=1,.struct_size=sizeof(rt),.acquire=acquire,.release=release};
static bool save(void*c,const char*k,const void*p,uint32_t n,const uint8_t*restore){inspect_observe();
 (void)c;assert(!strcmp(k,PORTABLE_RADIO_KEY)&&n==4&&!restore);++writes;
 uint8_t decoded=0;assert(portable_radio_decode(p,n,&decoded));if(save_ok)record=decoded;return save_ok;
}
static bool safe(void*c){(void)c;if(inspection_mode&&inspection_cutoff&&inspection_calls>=inspection_cutoff){inspection_stopped=true;return false;}return safe_ok;}
static void reset(unsigned mask,uint8_t flags){
 inspection_mode=inspection_stopped=inspection_no_storage=false;
 inspection_calls=inspection_cutoff=inspection_releases=inspection_release_failure=0;
 present=mask;malformed=grants=writes=ble_sets=wifi_disconnects=0;
 corrupt=ble_fail=wifi_fail=false;save_ok=safe_ok=release_ok=true;
 record=flags;ble_state=!!(flags&2);link_state=(flags&1)?WIFI_LINK_UP:WIFI_LINK_DOWN;
}
static uint32_t tap(pqa_state*u,unsigned tile){
 pqa_layout l=pqa_layout_for(u);pqa_rect r=l.tiles[tile];
 u->position_q8=u->target_q8=pqa_open_q8(u);
 assert(pqa_input(u,100,true,1,1,r.x+r.w/2,r.y+r.h/2,true));
 assert(pqa_input(u,120,true,0,1,r.x+r.w/2,r.y+r.h/2,true));return pqa_take_action(u);
}
static uint32_t raster(pqa_state*u){
 memset(pixels,0,sizeof(pixels));
 risc_display_surface_v1 surface={.pixels=pixels,.width=TEST_WIDTH,.height=TEST_HEIGHT,
 .stride_bytes=TEST_WIDTH*2,.size_bytes=sizeof(pixels),.pixel_format=RISC_DISPLAY_FORMAT_RGB565};
 assert(pqa_render(&surface,u,"12:34",true,75));
 uint32_t hash=2166136261u;for(unsigned i=0;i<sizeof(pixels)/sizeof(pixels[0]);++i)hash=(hash^pixels[i])*16777619u;
 return hash;
}
static void check_ui(pqa_state*u,unsigned mask,unsigned width,unsigned height){
 assert(pqa_set_dimensions(u,width,height));assert(u->wifi_valid==!!(mask&1)&&u->bluetooth_valid==!!(mask&2));
 assert(tap(u,3)==((mask&1)?PQA_WIFI:0));assert(tap(u,4)==((mask&2)?PQA_BLUETOOTH:0));
 assert(tap(u,2)==(mask?PQA_AIRPLANE:0));
 uint32_t before=raster(u);u->radios_valid=false;
 assert(!tap(u,3)&&!tap(u,4)&&!tap(u,2));
 if(mask)assert(raster(u)!=before);u->radios_valid=mask!=0;
}
static unsigned check_inspection(void){
 unsigned cases=0,complete_calls=0;
 const uint8_t flags[]={0,1,2,3,4|8|16};
 for(unsigned mask=0;mask<4;++mask)for(unsigned f=0;f<sizeof(flags);++f)for(unsigned observed=0;observed<4;++observed){
  reset(mask,flags[f]);inspection_mode=true;
  ble_state=!!(observed&2);link_state=(observed&1)?WIFI_LINK_UP:WIFI_LINK_DOWN;
  pqa_state u;pqa_init(&u);pqa_radios s;
  assert(pqa_radios_inspect(&s,&u,&rt,safe,NULL));
  assert(!grants&&!writes&&!ble_sets&&!wifi_disconnects&&record==flags[f]);
  assert(ble_state==!!(observed&2)&&link_state==((observed&1)?WIFI_LINK_UP:WIFI_LINK_DOWN));
  assert(s.flags==flags[f]&&s.valid&&s.available==(mask!=0));
  assert(u.wifi_available==!!(mask&1)&&u.bluetooth_available==!!(mask&2));
  assert(u.wifi_valid==!!(mask&1)&&u.bluetooth_valid==!!(mask&2));
  if(mask==3)complete_calls=inspection_calls;
  ++cases;
 }
 for(unsigned mask=0;mask<4;++mask){
  reset(mask,3);inspection_mode=true;corrupt=true;
  pqa_state u;pqa_init(&u);pqa_radios s;
  assert(pqa_radios_inspect(&s,&u,&rt,safe,NULL)&&!s.valid&&!u.radios_valid);
  assert(!grants&&!writes&&!ble_sets&&!wifi_disconnects&&record==3);
  assert(ble_state==1&&link_state==WIFI_LINK_UP);++cases;
 }
 for(unsigned badmask=1;badmask<4;++badmask){
  reset(3,3);inspection_mode=true;malformed=badmask;
  pqa_state u;pqa_init(&u);pqa_radios s;
  assert(pqa_radios_inspect(&s,&u,&rt,safe,NULL));
  assert(u.wifi_available==!(badmask&1)&&u.bluetooth_available==!(badmask&2));
  assert(!grants&&!writes&&!ble_sets&&!wifi_disconnects);++cases;
 }
 for(unsigned cutoff=1;cutoff<=complete_calls;++cutoff){
  reset(3,3);inspection_mode=true;inspection_cutoff=cutoff;
  pqa_state u;pqa_init(&u);pqa_radios s;
  assert(!pqa_radios_inspect(&s,&u,&rt,safe,NULL));
  assert(inspection_stopped&&inspection_calls==cutoff&&!writes&&!ble_sets&&!wifi_disconnects);++cases;
 }
 for(unsigned fail=1;fail<=3;++fail){
  reset(3,3);inspection_mode=true;inspection_release_failure=fail;
  pqa_state u;pqa_init(&u);pqa_radios s;
  assert(!pqa_radios_inspect(&s,&u,&rt,safe,NULL));
  assert(inspection_stopped&&grants&&!writes&&!ble_sets&&!wifi_disconnects);++cases;
 }
 for(unsigned bad=0;bad<2;++bad){
  reset(3,3);inspection_mode=true;
  if(bad)ble_state=PORTABLE_BLUETOOTH_RETAINED;else link_state=(wifi_link_t)99;
  pqa_state u;pqa_init(&u);pqa_radios s;
  assert(!pqa_radios_inspect(&s,&u,&rt,safe,NULL));
  assert(grants==2&&!writes&&!ble_sets&&!wifi_disconnects);++cases;
 }
 {reset(3,3);inspection_mode=inspection_no_storage=true;
  pqa_state u;pqa_init(&u);pqa_radios s;
  assert(pqa_radios_inspect(&s,&u,&rt,safe,NULL)&&!s.valid&&!u.radios_valid);
  assert(!grants&&!writes&&!ble_sets&&!wifi_disconnects);++cases;}
 {reset(3,3);inspection_mode=true;safe_ok=false;
  pqa_state u;pqa_init(&u);pqa_radios s;
  assert(!pqa_radios_inspect(&s,&u,&rt,safe,NULL)&&!inspection_calls&&!grants);
  assert(!pqa_radios_inspect(&s,&u,&rt,NULL,NULL)&&!inspection_calls);++cases;}
 printf("Read-only radio inspection: %u absence/policy/status/custody cases PASS\\n",cases);
 return cases;
}
int main(void){
 unsigned inspected=check_inspection();(void)inspected;
 unsigned scenarios=0;const uint8_t flags[]={0,1,2,3,4|8|16};
 for(unsigned mask=0;mask<4;++mask)for(unsigned f=0;f<sizeof(flags);++f){
  reset(mask,flags[f]);pqa_state u;pqa_init(&u);pqa_radios s;
  assert(pqa_radios_load(&s,&u,&rt)&&!grants&&!writes&&!ble_sets);
  assert(s.available==(mask!=0)&&s.flags==flags[f]);
  check_ui(&u,mask,TEST_WIDTH,TEST_HEIGHT);if(flags[f]==3)snapshots[mask]=raster(&u);++scenarios;
  unsigned before=ble_sets;assert(pqa_radios_resume(&s,&u,&rt)&&ble_sets==before&&!grants);++scenarios;
  const uint32_t actions[]={PQA_WIFI,PQA_BLUETOOTH,PQA_AIRPLANE};
  for(unsigned a=0;a<3;++a){
   reset(mask,flags[f]);pqa_init(&u);assert(pqa_radios_load(&s,&u,&rt));
   uint8_t old=record;bool available=a==2?mask!=0:!!(mask&(1u<<a));
   assert(pqa_radios_apply_user(&s,&u,&rt,actions[a],save,NULL,safe)&&!grants);
   assert(writes==(unsigned)available);if(!available)assert(record==old&&s.flags==old);
   else assert(record==s.flags);
   ++scenarios;
   reset(mask,flags[f]);pqa_init(&u);assert(pqa_radios_load(&s,&u,&rt));save_ok=false;
   assert(pqa_radios_apply_user(&s,&u,&rt,actions[a],save,NULL,safe)&&!grants&&record==flags[f]&&s.flags==flags[f]);
   if(mask&2)assert(ble_state==!!(flags[f]&2));++scenarios;
  }
 }
 for(unsigned mask=0;mask<4;++mask){
  reset(mask,3);corrupt=true;pqa_state u;pqa_init(&u);pqa_radios s;
  assert(pqa_radios_load(&s,&u,&rt)&&!grants&&!writes&&!s.valid&&!u.radios_valid&&!u.wifi_valid&&!u.bluetooth_valid);
  if(mask&1)assert(link_state==WIFI_LINK_DOWN);if(mask&2)assert(ble_state==0);
  assert(record==3);++scenarios;
  reset(mask,3);assert(pqa_radios_suspend(&rt)&&!grants);if(mask&2)assert(!ble_state);++scenarios;
 }
 for(unsigned badmask=1;badmask<4;++badmask){
  reset(3,3);malformed=badmask;pqa_state u;pqa_init(&u);pqa_radios s;
  assert(pqa_radios_load(&s,&u,&rt)&&!grants);check_ui(&u,3^badmask,TEST_WIDTH,TEST_HEIGHT);++scenarios;
 }
 {reset(3,3);pqa_state u;pqa_init(&u);pqa_radios s;assert(pqa_radios_load(&s,&u,&rt));
  present=1;assert(!pqa_radios_apply_user(&s,&u,&rt,PQA_AIRPLANE,save,NULL,safe)&&!grants&&!writes&&record==3);++scenarios;}
 {reset(2,2);pqa_state u;pqa_init(&u);pqa_radios s;assert(pqa_radios_load(&s,&u,&rt));
  ble_fail=true;assert(!pqa_radios_apply_user(&s,&u,&rt,PQA_BLUETOOTH,save,NULL,safe)&&!grants&&!writes&&record==2);++scenarios;}
 {reset(1,1);pqa_state u;pqa_init(&u);pqa_radios s;assert(pqa_radios_load(&s,&u,&rt));
  wifi_fail=true;assert(pqa_radios_apply_user(&s,&u,&rt,PQA_WIFI,save,NULL,safe)&&!grants&&!writes&&record==1);++scenarios;}
 {reset(3,3);pqa_state u;pqa_init(&u);pqa_radios s;assert(pqa_radios_load(&s,&u,&rt));
  safe_ok=false;assert(!pqa_radios_apply_user(&s,&u,&rt,PQA_AIRPLANE,save,NULL,safe)&&grants==2);++scenarios;}
 {reset(2,2);pqa_state u;pqa_init(&u);pqa_radios s;assert(pqa_radios_load(&s,&u,&rt));
  release_ok=false;assert(!pqa_radios_apply_user(&s,&u,&rt,PQA_BLUETOOTH,save,NULL,safe)&&grants==1);++scenarios;}
 for(unsigned i=0;i<4;++i){for(unsigned j=0;j<i;++j)assert(snapshots[i]!=snapshots[j]);
  printf("Native radio availability mask=%u raster=%08x\\n",i,snapshots[i]);}
 printf("Independent radios: %u production policy/input, lease, atomic rollback/custody cases PASS\n",scenarios);return 0;
}
