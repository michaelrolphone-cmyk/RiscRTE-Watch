#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "apps/clock/nova/nova.h"
static nova_watch_picker_cache *test_cache_create(unsigned width,unsigned height);
static void test_cache_destroy(nova_watch_picker_cache *cache);
#define nova_watch_picker_cache_create test_cache_create
#define nova_watch_picker_cache_destroy test_cache_destroy
static inline void *test_picker_malloc(size_t n);
static inline void test_picker_free(void*p);
#define malloc test_picker_malloc
#define free test_picker_free
#define WATCH_CLOCK_LAUNCHER 1
#define WATCH_QUICK_ACTIONS 1
#include "twatch_power.h" /* Legacy test fixture only; Home uses independent APIs. */
#include "apps/clock/crown.c"
#undef malloc
#undef free
#undef nova_watch_picker_cache_create
#undef nova_watch_picker_cache_destroy
#ifndef TEST_WIDTH
#define TEST_WIDTH 240u
#define TEST_HEIGHT 240u
#endif
#define TEST_LOGICAL_HEIGHT ((TEST_HEIGHT*240u+TEST_WIDTH-1u)/TEST_WIDTH)
static unsigned allocations,cache_allocations;
static uint16_t pixels[TEST_WIDTH*TEST_HEIGHT],completed_pixels[TEST_WIDTH*TEST_HEIGHT],expected_pixels[TEST_WIDTH*TEST_HEIGHT];
static void fixture_expected_face(bool with_status) {
 risc_display_surface_v1 expected={0,expected_pixels,TEST_WIDTH,TEST_HEIGHT,TEST_WIDTH*2,sizeof(expected_pixels),5};
 assert(nova_watch_face_render(&expected,&face,picker.selected));
 if(with_status&&sleep_status[0])assert(nova_watch_sleep_status(&expected,sleep_status));
}
static unsigned last_complete,swipe_submitted,swipe_time;
static unsigned clock_ms,submitted,released,grants,ungrants,subscribed,unsubscribed,polls,launches,sleeps,scenario;
static bool ready,owned,pending,launched;
static unsigned touch_step,ready_time,face_writes,face_reads,frame_started;
static inline void*test_picker_malloc(size_t n){
 assert(n==TEST_WIDTH*TEST_HEIGHT*2u);
 void*p=malloc(n);if(p){allocations++;memset(p,0xa5,n);}return p;
}
static inline void test_picker_free(void*p){if(p){assert(allocations);allocations--;}free(p);}
static nova_watch_picker_cache *test_cache_create(unsigned width,unsigned height){
 cache_allocations++;if(scenario==16)return NULL;
 nova_watch_picker_cache *p=nova_watch_picker_cache_create(width,height);
 if(p)allocations++;
 return p;
}
static void test_cache_destroy(nova_watch_picker_cache *p){
 if(p){assert(allocations);allocations--;}nova_watch_picker_cache_destroy(p);
}
static unsigned hardware_brightness,quick_writes;
static bool quick_saved[3];static uint8_t quick_values[3];
static int quick_key(const char*k){return !strcmp(k,"brightness")?0:!strcmp(k,"alarm_volume")?1:!strcmp(k,"quick_volume")?2:-1;}
#ifdef WATCH_QUICK_RADIOS
static uint8_t radio_bytes[4],ble_state;static bool radio_saved;
static bool ble_enable(void*c,bool on){(void)c;ble_state=on?1:0;return true;}
static bool ble_status(void*c,uint8_t*out){(void)c;*out=ble_state;return true;}
static bool wifi_off(void*c){(void)c;return true;}
static wifi_link_t wifi_status(void*c){(void)c;return WIFI_LINK_DOWN;}
static const portable_bluetooth_control_v1 qa_ble={.api_version=1,.struct_size=sizeof(qa_ble),.set_enabled=ble_enable,.status=ble_status};
static const wifi_api_v1 qa_wifi={.api_version=1,.struct_size=sizeof(qa_wifi),.status=wifi_status,.disconnect_checked=wifi_off};
#endif
#ifdef PORTABLE_LOW_BATTERY
static uint8_t low_bytes[3][5];static uint32_t low_lengths[3];static unsigned low_writes[3];
static unsigned low_percent=55,low_flags;static bool low_read_fail;
static int low_key(const char *key){return !strcmp(key,PORTABLE_LOW_BATTERY_KEY)?0:!strcmp(key,PORTABLE_SLEEP_IDLE_KEY)?1:!strcmp(key,PORTABLE_SLEEP_DEEP_KEY)?2:-1;}
#endif
static bool saved_face;static uint8_t saved_blob[4];
static bool health(risc_runtime_health_v1 *h){h->uptime_ms=clock_ms;return !launched && clock_ms<(scenario==24?10000u:scenario>=8?6000u:3000u);}
static void yield(uint32_t n){assert(n>=1&&n<=20);clock_ms+=n;}
static bool diagnostic(const char*s){if(strstr(s,"ready")){ready=true;ready_time=clock_ms;}return true;}
static bool info(void*c,risc_display_info_v1*i){(void)c;*i=(risc_display_info_v1){.width=TEST_WIDTH,.height=TEST_HEIGHT,.supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565)};return true;}
static bool acquire_frame(void*c,uint32_t f,risc_display_surface_v1*s){(void)c;assert(f==5&&!owned&&!pending);owned=true;*s=(risc_display_surface_v1){1,pixels,TEST_WIDTH,TEST_HEIGHT,TEST_WIDTH*2,sizeof(pixels),5};return true;}
static void release_frame(void*c,uint64_t f){(void)c;assert(f==1&&owned);owned=false;released++;}
static bool submit_frame(void*c,uint64_t f,const risc_display_rect_v1*d,size_t n,const risc_display_present_options_v1*o,uint64_t*t){(void)c;(void)d;(void)n;(void)o;assert(owned&&!pending&&f==1);
 if(face_writes&&scenario!=11&&scenario!=15){
  assert(!picker.open&&!picker_scratch);
  fixture_expected_face(true);
  assert(!memcmp(pixels,expected_pixels,sizeof(pixels)));
 }
 owned=false;pending=true;frame_started=clock_ms;*t=++submitted;return true;}
static bool status(void*c,uint64_t t,risc_display_present_status_v1*s){(void)c;assert(t==submitted&&pending);if((scenario==14||scenario==19)&&clock_ms-frame_started<95){s->state=RISC_DISPLAY_PRESENT_ACTIVE;return true;}pending=false;s->state=scenario==5 && touch_step>=2?RISC_DISPLAY_PRESENT_FAILED:RISC_DISPLAY_PRESENT_COMPLETE;
 if(s->state==RISC_DISPLAY_PRESENT_COMPLETE){memcpy(completed_pixels,pixels,sizeof(pixels));last_complete=submitted;}return true;}
static bool brightness(void*c,uint16_t a,uint16_t b){(void)c;assert(a<=100&&b==100);hardware_brightness=a;if(a)assert(submitted&&!pending);return true;}
static bool prepare(void*c){(void)c;assert(!owned&&!pending&&!touch.subscription);
#ifdef WATCH_QUICK_RADIOS
assert(!ble_state);
#endif
return true;}
static bool resume(void*c){(void)c;return true;}
static bool key(void*c,uint32_t*e){(void)c;*e=(scenario==7&&ready&&!sleeps&&clock_ms>2400)||(scenario==24&&ready&&!sleeps&&clock_ms-ready_time>2400)?2:((scenario==10&&clock_ms-ready_time==2000)||(scenario==20&&clock_ms-ready_time>=960&&clock_ms-ready_time<1000))&&ready?1:0;return true;}
static int32_t sleep_now(void*c,risc_light_sleep_result_v1*r){(void)c;(void)r;assert(!touch.subscription);sleeps++;return RISC_LIGHT_SLEEP_ACTIVE_WAKE;}
static int32_t sleep_timed(void*c,uint32_t ms,risc_light_sleep_result_v1*r){assert(ms==
#ifdef PORTABLE_LOW_BATTERY
watch_sleep_light_ms
#else
300000
#endif
);return sleep_now(c,r);}
static bool wake_pending(void*c,bool*p){(void)c;*p=false;return true;}
static bool rtc_read(void*c,twatch_rtc_time_v1*t){(void)c;*t=(twatch_rtc_time_v1){2026,10,4,0,0,40,0};return true;}
static bool battery(void*c,risc_battery_sample_v1*s){(void)c;
#ifdef PORTABLE_LOW_BATTERY
if(low_read_fail)return false;
*s=(risc_battery_sample_v1){3900,(uint8_t)low_percent,(uint8_t)low_flags};
#else
*s=(risc_battery_sample_v1){3900,55,0};
#endif
return true;}
static uint64_t subscribe(void*c){(void)c;subscribed++;return subscribed;}
static bool unsubscribe(void*c,uint64_t h){(void)c;assert(h);unsubscribed++;return true;}
static bool poll(void*c,size_t n){(void)c;assert(n==1);polls++;touch_step++;return true;}
static int32_t next(void*c,uint64_t h,risc_touch_event_v1*e){(void)c;(void)h;(void)e;return scenario==3&&touch_step==3?-1:0;}
static bool snapshot(void*c,risc_touch_snapshot_v1*s){
 (void)c;*s=(risc_touch_snapshot_v1){.width=240,.height=240};
 unsigned step=touch_step;
 if(scenario>=8){
  if(!ready)return true;
  unsigned age=clock_ms-ready_time;
  unsigned x=120,y=92;bool down=false;
  if(scenario>=21) {
   if(age>=20&&age<120){down=true;x=120;y=age<40?10:age<60?80:age<80?160:235;}
   if(scenario==21){
    if(age>=650&&age<730){down=true;x=age<690?44:182;y=90;}
    if(age>=1000&&age<1080){down=true;x=110;y=62;}
    if(age>=1400&&age<1480){down=true;x=51;y=139;}
    if(age>=1800&&age<1880){down=true;x=193;y=185;}
    if(age>=2200&&age<2280){down=true;x=100;y=100;}
   }
   if(scenario==24){
    if(age>=650&&age<730){down=true;x=122;y=185;}
    if(age>=950&&age<1030){down=true;x=193;y=139;}
    if(age>=1250&&age<1330){down=true;x=193;y=139;}
    if(age>=1550&&age<1630){down=true;x=51;y=185;}
    if(age>=1850&&age<1930){down=true;x=120;y=222;}
   }
   if(scenario==23){if(age>=650&&age<730){down=true;x=182;y=62;}if(age>=1000&&age<1080){down=true;x=120;y=222;}}
   if(down){s->contact_count=1;s->contacts[0]=(risc_touch_contact_v1){1,0,(uint16_t)x,(uint16_t)y};}
   return true;
  }
  if(age>=20&&age<660)down=true;
  if(scenario==14){
   if(age>=1000&&age<1009){down=true;x=120;}
   if(age>=1016&&age<1025){down=true;x=220;}
  }
  if(scenario==9||scenario==10||scenario==11||scenario==15){
   if(age>=720&&age<940){down=true;x=age<820?120:12;}
   if(age>=1800&&age<1840){down=true;x=120;}
  }
  if(scenario==19||scenario==20){
   if(age>=720&&age<940){down=true;y=age<820?200:40;}
   if(age>=950&&age<980){down=true;y=92;}
   if(scenario==19&&age>=2400&&age<2440){down=true;y=92;}
  }
  if(down){s->contact_count=1;s->contacts[0]=(risc_touch_contact_v1){1,0,(uint16_t)x,(uint16_t)y};}
  return true;
 }
 if(scenario==7)return true;
 if(scenario==2 && step<3){s->contact_count=1;s->contacts[0]=(risc_touch_contact_v1){1,0,120,120};return true;}
 if(scenario==2)step-=3;
 if(step==3){swipe_submitted=submitted;swipe_time=clock_ms;}
 if(step<2)return true;
 if(scenario==1 && step>=3)return true;
 s->contact_count=scenario==4&&step>=3?2:1;
 s->contacts[0]=(risc_touch_contact_v1){1,0,step<3?120:80,120};return true;
}
static risc_touch_api_v1 ta={1,sizeof(ta),NULL,subscribe,unsubscribe,poll,next,snapshot};
static twatch_panel_power_v1 da={{1,TWATCH_PANEL_LIGHT_SLEEP_SIZE,NULL,info,acquire_frame,release_frame,submit_frame,status,NULL,brightness},prepare,resume,NULL,NULL};
static twatch_pmu_api_v1 pa={{1,sizeof(pa),NULL,battery},key,prepare,resume,sleep_now,NULL,sleep_timed,wake_pending,NULL,NULL,NULL};
static twatch_rtc_api_v1 ra={2,sizeof(ra),NULL,rtc_read,NULL,NULL,NULL};
static int32_t face_get(void*c,const char*k,void*b,uint32_t n,uint32_t*z){(void)c;*z=0;
#ifdef PORTABLE_LOW_BATTERY
int low=low_key(k);if(low>=0){*z=low_lengths[low];if(!*z)return RISC_KEY_VALUE_NOT_FOUND;if(n<*z)return RISC_KEY_VALUE_BUFFER_SMALL;memcpy(b,low_bytes[low],*z);return RISC_KEY_VALUE_OK;}
#endif
#ifdef WATCH_QUICK_RADIOS
if(!strcmp(k,PORTABLE_RADIO_KEY)){assert(n>=4);if(!radio_saved)return RISC_KEY_VALUE_NOT_FOUND;memcpy(b,radio_bytes,4);*z=4;return 0;}
#endif
int qi=quick_key(k);if(qi>=0){assert(n==1);if(!quick_saved[qi])return RISC_KEY_VALUE_NOT_FOUND;*(uint8_t*)b=quick_values[qi];*z=1;return 0;}if(!strcmp(k,PORTABLE_TIME_FORMAT_KEY)){if(scenario==17||scenario==18){assert(n==4);uint8_t f[]={0x54,1,1,scenario==17?0xa4:0};memcpy(b,f,4);*z=4;return 0;}return RISC_KEY_VALUE_NOT_FOUND;}if(strcmp(k,WATCH_FACE_KEY))return RISC_KEY_VALUE_NOT_FOUND;face_reads++;if(scenario==15&&face_writes)return RISC_KEY_VALUE_IO;if(!saved_face)return RISC_KEY_VALUE_NOT_FOUND;assert(n==4);memcpy(b,saved_blob,4);*z=4;return 0;}
#ifdef WATCH_ATOMIC_PREFERENCES
static bool atomic_publishing;
#ifdef PORTABLE_PREFERENCE_STORE
enum { PREF_OK, PREF_MISSING, PREF_BAD_API, PREF_BAD_GRANT, PREF_BAD_FLAGS,
 PREF_STATE_IO, PREF_READ_IO, PREF_SUBMIT_IO, PREF_COLLECT_IO, PREF_BAD_RECEIPT,
 PREF_DIRTY_MISSING, PREF_PARTIAL };
static unsigned preference_flags=15,preference_fault,generic_acquires,atomic_acquires;
static risc_key_value_atomic_receipt_v1 preference_receipt;
#endif
#endif
static int32_t face_put(void*c,const char*k,const void*b,uint32_t n){(void)c;
#ifdef WATCH_ATOMIC_PREFERENCES
assert(atomic_publishing);
#endif
#ifdef PORTABLE_LOW_BATTERY
int low=low_key(k);if(low>=0){assert(n<=5);low_writes[low]++;low_lengths[low]=n;memcpy(low_bytes[low],b,n);return RISC_KEY_VALUE_OK;}
#endif
#ifdef WATCH_QUICK_RADIOS
if(!strcmp(k,PORTABLE_RADIO_KEY)){assert(n==4);memcpy(radio_bytes,b,4);radio_saved=true;return 0;}
#endif
int qi=quick_key(k);if(qi>=0){assert(n==1);quick_writes++;if(scenario==23)return RISC_KEY_VALUE_IO;quick_values[qi]=*(const uint8_t*)b;quick_saved[qi]=true;return 0;}assert(!strcmp(k,WATCH_FACE_KEY)&&n==4);face_writes++;if(scenario==11)return RISC_KEY_VALUE_IO;memcpy(saved_blob,b,4);saved_face=true;return 0;}
static const risc_key_value_v1 face_kv={1,sizeof(face_kv),NULL,face_get,face_put};
static risc_power_sleep_api_v1 test_power;
static risc_power_key_api_v1 test_key;
static risc_display_power_api_v1 test_display_power;
#ifdef WATCH_ATOMIC_PREFERENCES
static uint64_t atomic_revision=7,atomic_token;
static uint32_t atomic_count;static char atomic_keys[2][16];static uint8_t atomic_values[2][64];static uint32_t atomic_sizes[2];
static int32_t atomic_state(void*c,risc_key_value_atomic_state_v1*s){(void)c;assert(!owned&&!pending);
 unsigned flags=3;
#ifdef PORTABLE_PREFERENCE_STORE
 if(preference_fault==PREF_STATE_IO)return RISC_ATOMIC_IO;
 flags=preference_fault==PREF_BAD_FLAGS?0:preference_flags;
#endif
 *s=(risc_key_value_atomic_state_v1){sizeof(*s),flags,1,16,64,4096,1024,0,atomic_revision};return RISC_ATOMIC_OK;}
static int32_t atomic_get(void*c,uint64_t rev,const char*k,void*out,uint32_t cap,uint32_t*size){
 assert(!owned&&!pending);*size=0;
#ifdef PORTABLE_PREFERENCE_STORE
 if(preference_fault==PREF_READ_IO)return RISC_ATOMIC_IO;
#endif
 if(rev!=atomic_revision)return RISC_ATOMIC_STALE;
 uint8_t data[64];uint32_t n=0;int rc=face_get(c,k,data,quick_key(k)>=0?1:!strcmp(k,WATCH_FACE_KEY)||!strcmp(k,PORTABLE_TIME_FORMAT_KEY)?4:sizeof(data),&n);
 if(rc==RISC_KEY_VALUE_NOT_FOUND)return RISC_ATOMIC_NOT_FOUND;
 if(rc)return RISC_ATOMIC_IO;
 *size=n;if(cap<n)return RISC_ATOMIC_BUFFER_SMALL;memcpy(out,data,n);return RISC_ATOMIC_OK;
}
static int32_t atomic_submit(void*c,uint64_t rev,const risc_key_value_atomic_mutation_v1*m,uint32_t count,const void*intent,uint32_t bytes,uint64_t*token){
 (void)c;assert(!owned&&!pending&&!atomic_token&&count&&count<=2&&bytes==176);*token=0;
 const uint8_t*p=intent;assert(((!memcmp(p,"PFI0001",8)&&p[8]==PORTABLE_PREFERENCE_USER)||(!memcmp(p,"PFA0001",8)&&p[8]==PORTABLE_PREFERENCE_AUTOMATIC)));
 unsigned intended=0;
#ifdef PORTABLE_PREFERENCE_STORE
 if(count==2){assert(!strcmp(m[0].key,"quick_volume")||!strcmp(m[0].key,"quick_bright"));intended=1;}
#endif
 assert(!strcmp((const char*)p+12,m[intended].key));
#ifdef PORTABLE_PREFERENCE_STORE
 if(preference_fault==PREF_SUBMIT_IO)return RISC_ATOMIC_IO;
#endif
 if(rev!=atomic_revision)return RISC_ATOMIC_STALE;
 if(scenario==23)return RISC_ATOMIC_IO;
 for(unsigned i=0;i<count;i++){assert(m[i].kind==RISC_ATOMIC_PUT&&strlen(m[i].key)<16&&m[i].size<=64);strcpy(atomic_keys[i],m[i].key);memcpy(atomic_values[i],m[i].value,m[i].size);atomic_sizes[i]=m[i].size;}
 atomic_count=count;*token=atomic_token=1;
#ifdef PORTABLE_PREFERENCE_STORE
 if(!(preference_flags&RISC_PREFERENCE_STORE_ATOMIC_GROUP)) {
  atomic_publishing=true;
  for(unsigned i=0;i<count;i++) {
   if(preference_fault==PREF_PARTIAL&&i==1)break;
   assert(!face_put(NULL,atomic_keys[i],atomic_values[i],atomic_sizes[i]));
  }
  atomic_publishing=false;
  bool partial=preference_fault==PREF_PARTIAL&&count>1;
  ++atomic_revision;
  preference_receipt=(risc_key_value_atomic_receipt_v1){sizeof(preference_receipt),partial?RISC_ATOMIC_UNCERTAIN:RISC_ATOMIC_OK,atomic_revision,partial?0:atomic_revision};
 }
#endif
 return RISC_ATOMIC_OK;
}
static int32_t atomic_collect(void*c,uint64_t token,risc_key_value_atomic_receipt_v1*r){
 (void)c;assert(!owned&&!pending&&token==atomic_token&&token);
#ifdef PORTABLE_PREFERENCE_STORE
 if(preference_fault==PREF_COLLECT_IO)return RISC_ATOMIC_IO;
 if(preference_fault==PREF_BAD_RECEIPT){*r=(risc_key_value_atomic_receipt_v1){0};return RISC_ATOMIC_OK;}
 if(!(preference_flags&RISC_PREFERENCE_STORE_ATOMIC_GROUP)){*r=preference_receipt;atomic_token=0;return RISC_ATOMIC_OK;}
#endif
 atomic_publishing=true;
 for(unsigned i=0;i<atomic_count;i++)assert(!face_put(NULL,atomic_keys[i],atomic_values[i],atomic_sizes[i]));
 atomic_publishing=false;atomic_token=0;
 *r=(risc_key_value_atomic_receipt_v1){sizeof(*r),RISC_ATOMIC_OK,++atomic_revision,atomic_revision};return RISC_ATOMIC_OK;
}
static int32_t atomic_cancel(void*c,uint64_t t){(void)c;assert(t==atomic_token);return RISC_ATOMIC_OK;}
static const risc_key_value_atomic_v1 atomic_api={1,sizeof(atomic_api),NULL,atomic_state,atomic_get,atomic_submit,atomic_collect,atomic_cancel};
#endif
static bool acquire_cap(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){(void)v;
#ifdef WATCH_ATOMIC_PREFERENCES
#ifdef PORTABLE_PREFERENCE_STORE
if(!strcmp(n,RISC_KEY_VALUE_ATOMIC_CAPABILITY)){atomic_acquires++;return false;}
if(!strcmp(n,RISC_PREFERENCE_STORE_CAPABILITY)) {
 assert(v==1&&id==1);generic_acquires++;
 if(preference_fault==PREF_MISSING)return false;
 static risc_key_value_atomic_v1 selected;selected=atomic_api;
 if(preference_fault==PREF_BAD_API)selected.collect=NULL;
 g->api=&selected;g->slot=1;g->generation=preference_fault==PREF_BAD_GRANT?0:1;grants++;
 return preference_fault!=PREF_DIRTY_MISSING;
}
#else
if(!strcmp(n,RISC_KEY_VALUE_ATOMIC_CAPABILITY)){assert(id==1);g->api=&atomic_api;g->slot=1;g->generation=1;grants++;return true;}
#endif
#endif
if(!strcmp(n,"power.sleep")){test_power=(risc_power_sleep_api_v1){1,sizeof(test_power),pa.base.context,pa.prepare_sleep,pa.resume,pa.light_sleep,pa.deep_sleep,pa.light_sleep_for,pa.sleep_wake_pending,pa.deep_sleep_for,pa.light_sleep_set,pa.deep_sleep_set};g->api=&test_power;grants++;return true;}
if(!strcmp(n,"input.power-key")){test_key=(risc_power_key_api_v1){1,sizeof(test_key),pa.base.context,pa.key_events};g->api=&test_key;grants++;return true;}
if(!strcmp(n,"display.power")){test_display_power=(risc_display_power_api_v1){1,sizeof(test_display_power),da.base.context,da.prepare_sleep,da.resume,da.prepare_deep_sleep,da.resume_status};g->api=&test_display_power;grants++;return true;}

#ifdef WATCH_QUICK_RADIOS
if(!strcmp(n,"net.wifi")){assert(id==0);g->api=&qa_wifi;grants++;return true;}
if(!strcmp(n,"bluetooth.hci")){assert(id==0);g->api=&qa_ble;grants++;return true;}
#endif
if(!strcmp(n,RISC_KEY_VALUE_CAPABILITY)){if(scenario<8)return false;assert(id==1);grants++;g->api=&face_kv;return true;}assert(!id);grants++;if(!strcmp(n,"display.output"))g->api=&da;else if(!strcmp(n,"board.battery"))g->api=&pa;else if(!strcmp(n,"rtc.clock"))g->api=&ra;else{assert(!strcmp(n,"input.touch.raw"));g->api=&ta;}return true;}
static bool release_cap(risc_runtime_capability_v1*g){assert(g->api&&!owned&&!pending);g->api=NULL;g->slot=0;g->generation=0;ungrants++;return true;}
static bool launch(const char*p){assert(!strcmp(p,"springboard.elf")&&!owned&&!pending&&!touch.subscription);assert(submitted==last_complete&&submitted==swipe_submitted&&clock_ms==swipe_time);
 assert(!memcmp(completed_pixels,pixels,sizeof(pixels)));
 fixture_expected_face(true);
  assert(!memcmp(pixels,expected_pixels,sizeof(pixels)));
 launches++;if(scenario==6)return false;launched=true;return true;}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=RISC_RUNTIME_SERVICE_CHECKPOINT_V1_SIZE,.health=health,.yield_ms=yield,.diagnostic=diagnostic,.request_launch=launch,.acquire=acquire_cap,.release=release_cap};
#ifndef WATCH_TEST_RUNTIME_EXTERN
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){assert(v==1);return &runtime;}
#endif
int main(void){
 for(unsigned run=0;run<
#ifdef WATCH_QUICK_RADIOS
 4
#else
 3
#endif
 ;run++){
  scenario=21+run;if(scenario!=22){memset(quick_saved,0,sizeof(quick_saved));memset(quick_values,0,sizeof(quick_values));}
#ifdef WATCH_QUICK_RADIOS
  ble_state=0;if(scenario!=22)radio_saved=false;
#endif
  quick_writes=face_writes=face_reads=ready_time=cache_allocations=0;assert(!allocations);saved_face=false;
  clock_ms=submitted=released=grants=ungrants=subscribed=unsubscribed=polls=launches=sleeps=touch_step=0;
  ready=owned=pending=launched=false;last_complete=swipe_submitted=swipe_time=0;app_main();
  if(!(ready&&!owned&&!pending&&grants==ungrants&&subscribed==unsubscribed&&!allocations&&!launches))fprintf(stderr,"Quick case %u %ux%u ready=%d owned=%d pending=%d grants=%u/%u subscribed=%u/%u allocations=%u launches=%u\n",scenario,TEST_WIDTH,TEST_HEIGHT,ready,owned,pending,grants,ungrants,subscribed,unsubscribed,allocations,launches);
  assert(ready&&!owned&&!pending&&grants==ungrants&&subscribed==unsubscribed&&!allocations&&!launches);
  if(scenario==21){assert(quick_values[0]==50&&quick_values[1]==0&&quick_values[2]==100);assert(hardware_brightness==50&&!clock_quick.ui.torch);assert(quick_writes==4);}
  if(scenario==22){assert(clock_quick.brightness==50&&clock_quick.volume==0&&clock_quick.restore_volume==100);assert(hardware_brightness==50&&!quick_writes);}
#ifdef WATCH_QUICK_RADIOS
  if(scenario==24){assert(sleeps==1&&ble_state==1&&clock_quick.ui.bluetooth_enabled&&!clock_quick.ui.wifi_enabled&&!clock_quick.ui.airplane&&radio_saved);}
#endif
  if(scenario==23){assert(clock_quick.brightness==40&&hardware_brightness==40&&!quick_saved[0]);assert(clock_quick.ui.error_flags&PQA_ERROR_SAVE);}
 }
 puts("Current Clock quick controls: top edge, notification volume, brightness, Silent restoration, torch off, reload and failed save passed");
return 0;
}
