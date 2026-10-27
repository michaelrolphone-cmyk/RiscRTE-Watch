/* Real Home and renderer with the established lifecycle/provider fixture. */
#include <stdint.h>
static int32_t native_zone_get(void *,const char *,void *,uint32_t,uint32_t *);
#define WATCH_TEST_TIMEZONE_GET native_zone_get
#define WATCH_NATIVE_TIME_AUTHORITY
#define WATCH_FAST_WAKE_TEST_ENTRY inherited_external_time_main
#include "fast_wake_clock_test.c"

static unsigned zone_reads;
static uint8_t zone_record[PORTABLE_TIMEZONE_RECORD_BYTES];
static int32_t zone_status=RISC_KEY_VALUE_NOT_FOUND;
static int32_t native_zone_get(void *context,const char *key,void *buffer,uint32_t capacity,uint32_t *size) {
 (void)context;assert(!strcmp(key,PORTABLE_TIMEZONE_KEY));zone_reads++;*size=0;
 if(zone_status!=RISC_KEY_VALUE_OK)return zone_status;
 *size=sizeof(zone_record);if(capacity<*size)return RISC_KEY_VALUE_BUFFER_SMALL;
 memcpy(buffer,zone_record,*size);return RISC_KEY_VALUE_OK;
}
static void saved_zone(const char *id) {
 assert(portable_timezone_find(id,strlen(id)+1)>=0&&strlen(id)<PORTABLE_TIMEZONE_ID_BYTES);
 memset(zone_record,0,sizeof(zone_record));zone_record[0]='T';zone_record[1]='Z';zone_record[2]=1;
 memcpy(zone_record+4,id,strlen(id));zone_record[3]=0xa5;
 for(unsigned i=0;i<sizeof(zone_record);i++)if(i!=3)zone_record[3]^=zone_record[i];
 zone_status=RISC_KEY_VALUE_OK;
}
static void expected_civil(unsigned year,unsigned month,unsigned day,unsigned weekday,unsigned hour,unsigned minute,unsigned second) {
 assert(face.time_valid&&face.time.year==year&&face.time.month==month&&face.time.day==day&&
  face.time.weekday==weekday&&face.time.hour==hour&&face.time.minute==minute&&face.time.second==second);
}
static int32_t changed_zone_wake(void *context,uint32_t ms,risc_light_sleep_result_v1 *result) {
 int32_t rc=motion_sleep(context,ms,result);saved_zone("Asia/Tokyo");return rc;
}
static unsigned wake_fault;
static void corrupt_native(unsigned fault) {
 switch(fault) {
 case 0:native_time.struct_size=0;break;
 case 1:native_time.reserved=1;break;
 case 2:native_time.nanoseconds=1000000000u;break;
 case 3:native_time.nanoseconds=1;break;
 case 4:native_time.monotonic_before_us=2;native_time.monotonic_after_us=1;break;
 case 5:native_time.validity=2;break;
 case 6:native_time.epoch_seconds=-1;break;
 case 7:native_time.epoch_seconds=(int64_t)INT32_MAX+1;break;
 case 8:native_time.validity=RISC_REALTIME_UNSET;native_time.epoch_seconds=1;break;
 case 9:native_time.validity=RISC_REALTIME_UNSET;native_time.epoch_seconds=0;native_time.nanoseconds=1000;break;
 case 10:native_read_result=RISC_REALTIME_IO;break;
 case 11:native_read_result=RISC_REALTIME_INVALID;break;
 case 12:native_read_result=RISC_REALTIME_CONTEXT;break;
 default:assert(false);
 }
}
static int32_t fault_sleep(void *context,uint32_t ms,risc_light_sleep_result_v1 *result) {
 int32_t rc=motion_sleep(context,ms,result);corrupt_native(wake_fault);return rc;
}
static void no_external_time(void) {
 assert(!rtc_acquires&&!rtc_read_calls&&!seed_calls&&!rtc_reconcile_errors);
 assert(!clock_rtc_reconcile_pending&&!clock_rtc_retry_delayed&&!native_clock.rtc_reconcile_pending);
 assert(!native_clock.fallback&&strcmp(sleep_status,"TIME CHECK ERR"));
}
int main(void) {
 /* Every boot cause, with and without a retained checkpoint; cold animation
  * and normal reentry use Runtime time without trying a discoverable RTC. */
 unsigned startup_cases=0;
 for(unsigned valid=0;valid<2;valid++)for(unsigned cause=0;cause<=RISC_BOOT_DEEP_OTHER;cause++)
 for(unsigned retained=0;retained<2;retained++)for(unsigned initial=0;initial<2;initial++) {
  reset_case();boot_cause=cause;initial_default_entry=initial;
  checkpoint_result=retained?RISC_RETAINED_WAKE_OK:RISC_RETAINED_WAKE_ABSENT;
  native_time.validity=valid?RISC_REALTIME_VALID:RISC_REALTIME_UNSET;
  if(!valid)native_time.epoch_seconds=0;
  risc_realtime_snapshot_v1 original=native_time;
  app_main();check_clean();no_external_time();
  assert(first_seen&&face.time_valid==(bool)valid&&!first_time_warning&&!sleep_status[0]);
  assert(!memcmp(&native_time,&original,sizeof(original))&&native_reads);
  startup_cases++;
 }
 /* Repeated actual light resumes preserve native evolution, not RTC drift
  * correction. UNSET stays zero and visible rather than being seeded. */
 for(unsigned valid=0;valid<2;valid++)for(int direction=-1;direction<=1;direction+=2) {
  reset_case();requested_wakes=2;contexts_enabled=true;
  native_time.validity=valid?RISC_REALTIME_VALID:RISC_REALTIME_UNSET;
  if(!valid)native_time.epoch_seconds=0;
  light_wake_drift=valid?direction*300:0;int64_t initial=native_time.epoch_seconds;
  app_main();check_clean();no_external_time();
  assert(wake_count==2&&soft_frames==2&&input_stages==3&&contexts_starts==3);
  assert(face.time_valid==(bool)valid&&native_time.epoch_seconds==initial+2*light_wake_drift);
 }
 /* Independent exact epochs prove native UTC and shared IANA preference use,
  * not merely agreement between two copies of the legacy converter. */
 const struct {const char *zone;int64_t epoch;unsigned year,month,day,weekday,hour,minute,second;} cases[]={
  {"UTC",1792900000,2026,10,25,0,3,46,40},
  {"America/Denver",1792900000,2026,10,24,6,21,46,40},
  {"Asia/Tokyo",1792900000,2026,10,25,0,12,46,40},
  {"America/Denver",1772960399,2026,3,8,0,1,59,59},
  {"America/Denver",1772960400,2026,3,8,0,3,0,0},
  {"America/Denver",1793519999,2026,11,1,0,1,59,59},
  {"America/Denver",1793520000,2026,11,1,0,1,0,0}
 };
 for(unsigned i=0;i<sizeof(cases)/sizeof(*cases);i++){
  reset_case();saved_zone(cases[i].zone);native_time.epoch_seconds=cases[i].epoch;
  app_main();check_clean();no_external_time();
  expected_civil(cases[i].year,cases[i].month,cases[i].day,cases[i].weekday,cases[i].hour,cases[i].minute,cases[i].second);
  assert(native_time.epoch_seconds==cases[i].epoch&&!face_writes&&!quick_writes);
 }
 zone_status=RISC_KEY_VALUE_NOT_FOUND;reset_case();native_time.epoch_seconds=1792900000;
 app_main();check_clean();no_external_time();expected_civil(2026,10,25,0,3,46,40);
 for(unsigned invalid=0;invalid<2;invalid++){
  reset_case();saved_zone("UTC");if(invalid)zone_status=RISC_KEY_VALUE_IO;else zone_record[3]^=1;
  app_main();check_clean();no_external_time();assert(!face.time_valid&&!strcmp(sleep_status,"TIME ZONE ERR"));
 }
 zone_status=RISC_KEY_VALUE_NOT_FOUND;reset_case();requested_wakes=1;light_wake_drift=0;
 native_time.epoch_seconds=1792900000;pa.light_sleep_set=changed_zone_wake;
 app_main();check_clean();no_external_time();expected_civil(2026,10,25,0,12,46,40);
 assert(wake_count==1&&soft_frames==1);
 zone_status=RISC_KEY_VALUE_NOT_FOUND;
 for(unsigned fault=0;fault<13;fault++) {
  reset_case();corrupt_native(fault);app_main();no_external_time();
  assert(!first_seen&&!submitted&&!confirmations&&!service_acquires&&!points_loads);
  if(fault==12){assert(retained_fence&&native_clock.halted&&retains==1);retained_reset();}
  else check_clean();
 }
 /* Reject each failed/malformed post-sleep snapshot before wake input/frame.
  * CONTEXT retains invocation custody and the fixture forbids all later I/O. */
 for(unsigned fault=0;fault<13;fault++) {
  reset_case();requested_wakes=1;wake_fault=fault;pa.light_sleep_set=fault_sleep;
  app_main();no_external_time();assert(first_seen&&wake_count==1&&!soft_frames);
  if(fault==12){assert(retained_fence&&native_clock.halted&&retains==1);retained_reset();}
  else check_clean();
 }
 /* Even a stale fallback pointer cannot cause external reads in native mode. */
 reset_case();native_clock=(watch_clock_runtime){.runtime=&fast_runtime,.time=&native_api,.fallback=&ra};
 twatch_rtc_time_v1 date;uint16_t fraction;
 assert(watch_runtime_load_zone(&native_clock,&fast_kv)==PORTABLE_TIMEZONE_MISSING);
 assert(watch_runtime_read(&native_clock,&date,&fraction)&&!rtc_read_calls);
 assert(watch_runtime_seed(&native_clock,&ra)&&!native_clock.fallback&&!seed_calls&&!rtc_read_calls);
 native_read_result=RISC_REALTIME_CONTEXT;
 assert(!watch_runtime_read(&native_clock,&date,&fraction)&&native_clock.halted&&retains==1);
 unsigned before=native_reads;assert(!watch_runtime_read(&native_clock,&date,&fraction)&&native_reads==before);
 retained_reset();
 printf("Native time authority: %u cold/deep/reentry/UNSET cases, repeated light resumes, 26 startup/wake malformed/I/O/CONTEXT cases, no RTC acquisitions/reads or seeds, strict retained custody; exact UTC/Denver/Tokyo/DST and timezone wake reload PASS\n",startup_cases);
 return 0;
}
