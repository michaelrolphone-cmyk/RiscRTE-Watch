/* Actual Home with an explicit product-selected initial local calendar. */
#include <stdint.h>
static int32_t default_zone_get(void *,const char *,void *,uint32_t,uint32_t *);
#define WATCH_TEST_TIMEZONE_GET default_zone_get
#define WATCH_NATIVE_TIME_AUTHORITY
#define WATCH_FAST_WAKE_TEST_ENTRY inherited_external_time_main
#include "fast_wake_clock_test.c"
static uint8_t zone_bytes[PORTABLE_TIMEZONE_RECORD_BYTES];
static int zone_result=RISC_KEY_VALUE_NOT_FOUND;
static int32_t default_zone_get(void *c,const char *key,void *out,uint32_t cap,uint32_t *size){
 (void)c;assert(!strcmp(key,PORTABLE_TIMEZONE_KEY));*size=0;
 if(zone_result!=RISC_KEY_VALUE_OK)return zone_result;
 *size=sizeof(zone_bytes);assert(cap>=*size);memcpy(out,zone_bytes,*size);return RISC_KEY_VALUE_OK;
}
static void zone(const char *id){
 memset(zone_bytes,0,sizeof(zone_bytes));zone_bytes[0]='T';zone_bytes[1]='Z';zone_bytes[2]=1;
 memcpy(zone_bytes+4,id,strlen(id));zone_bytes[3]=0xa5;
 for(unsigned i=0;i<sizeof(zone_bytes);i++)if(i!=3)zone_bytes[3]^=zone_bytes[i];
 zone_result=RISC_KEY_VALUE_OK;
}
static void unset(void){native_time.validity=RISC_REALTIME_UNSET;native_time.epoch_seconds=0;native_time.nanoseconds=0;}
static unsigned fault;
static int32_t faulty_seed(void *context,int64_t epoch,uint32_t ns){
 if(fault==1)return RISC_REALTIME_IO;
 if(fault==2)return RISC_REALTIME_CONTEXT;
 int rc=native_seed(context,epoch,ns);
 if(fault==3)native_read_result=RISC_REALTIME_IO;
 if(fault==4)native_time.epoch_seconds++;
 if(fault==4)native_time.nanoseconds=1000;
 if(fault==5)native_time.validity=RISC_REALTIME_UNSET,native_time.epoch_seconds=0;
 return rc;
}
int main(void){
 unsigned cases=0;
 for(unsigned valid=0;valid<2;valid++)for(unsigned cause=0;cause<=RISC_BOOT_DEEP_OTHER;cause++)
 for(unsigned initial=0;initial<2;initial++){
  reset_case();zone_result=RISC_KEY_VALUE_NOT_FOUND;boot_cause=cause;initial_default_entry=initial;
  if(!valid)unset();
  int64_t original=native_time.epoch_seconds;
  app_main();check_clean();assert(first_seen&&face.time_valid&&!rtc_acquires&&!rtc_read_calls&&seed_calls==!valid);
  assert(native_time.epoch_seconds==(valid?original:INT64_C(1767225600)));
  if(!valid)assert(face.time.year==2026&&face.time.month==1&&face.time.day==1&&!face.time.hour&&!face.time.minute&&!face.time.second);
  cases++;
 }
 const struct {const char *id;int64_t utc;} zones[]={{"UTC",1767225600},{"America/Denver",1767250800},{"Asia/Tokyo",1767193200}};
 for(unsigned i=0;i<sizeof(zones)/sizeof(*zones);i++){
  reset_case();zone(zones[i].id);unset();requested_wakes=2;light_wake_drift=300;
  app_main();check_clean();assert(seed_calls==1&&wake_count==2&&native_time.epoch_seconds==zones[i].utc+600);
  assert(face.time_valid&&face.time.year==2026&&face.time.month==1&&face.time.day==1&&face.time.hour==0&&face.time.minute==10);
 }
 /* Existing Settings time is native authority, never overwritten on reentry. */
 reset_case();zone("America/Denver");native_time.epoch_seconds=1792900000;
 app_main();check_clean();assert(!seed_calls&&face.time_valid&&face.time.year==2026&&face.time.month==10&&face.time.day==24&&face.time.hour==21&&face.time.minute==46);
 for(unsigned missing=0;missing<2;missing++){
  reset_case();zone("UTC");unset();if(missing)zone_result=RISC_KEY_VALUE_IO;else zone_bytes[3]^=1;
  app_main();check_clean();assert(!seed_calls&&!face.time_valid&&!strcmp(sleep_status,"TIME ZONE ERR"));
 }
 zone_result=RISC_KEY_VALUE_NOT_FOUND;
 for(fault=1;fault<=5;fault++){
  reset_case();unset();risc_realtime_control_api_v1 api=native_api;api.seed=faulty_seed;
  native_clock=(watch_clock_runtime){.runtime=&fast_runtime,.time=&api};
  assert(watch_runtime_load_zone(&native_clock,&fast_kv)==PORTABLE_TIMEZONE_MISSING);
  assert(!watch_runtime_initialize_unset(&native_clock));
  if(fault==2){assert(native_clock.halted&&retains==1);retained_reset();}else assert(!native_clock.halted);
 }
 /* General policy: verify local midnight across the complete shared catalog. */
 for(unsigned i=0;i<portable_timezone_count();i++){
  reset_case();unset();zone(portable_timezone_get(i)->id);
  native_clock=(watch_clock_runtime){.runtime=&fast_runtime,.time=&native_api};
  assert(watch_runtime_load_zone(&native_clock,&fast_kv)==PORTABLE_TIMEZONE_LOADED);
  assert(watch_runtime_initialize_unset(&native_clock)&&seed_calls==1);
  twatch_rtc_time_v1 date={0};uint16_t fraction=0;assert(watch_runtime_read(&native_clock,&date,&fraction));
  assert(date.year==2026&&date.month==1&&date.day==1&&!date.hour&&!date.minute&&!date.second);
  assert(watch_runtime_initialize_unset(&native_clock)&&seed_calls==1);
 }
 printf("Explicit initial time: %u actual Home cold/deep/reentry cases, UTC/Denver/Tokyo repeated light wake, Settings precedence, bad-zone and seed/readback/custody refusals, %u shared zones PASS\n",cases,portable_timezone_count());
 return 0;
}
