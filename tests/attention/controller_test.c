#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "RiscRuntimeV1.h"
#include "RiscResidentAttentionV1.h"
#include "twatch_caps.h"
#include "AlarmServiceV1.h"
#include "AlarmRecords.h"
#define WATCH_CLOCK_ALARMS
static risc_runtime_api_v1 runtime;
static const risc_runtime_api_v1 *rt=&runtime;
static risc_resident_client_v1 clock_resident;
static risc_resident_attention_client_v1 clock_resident_extended;
static bool clock_resident_terminal,clock_resident_child,clock_resident_dispatching;
static uint32_t clock_resident_keys,clock_resident_armed_at;
static const twatch_rtc_api_v1 *rtc;
static struct {uint64_t subscription;risc_runtime_capability_v1 grant;} touch;
static int device_clients;
static bool clock_alarm_error_seen;
static uint32_t clock_alarm_seen_snapshot;
static int32_t clock_alarm_seen_error;
static struct {const alarm_service_v1 *api;alarm_status_v1 status;} clock_alarm;
static bool live=true,key_ok=true,rtc_ok=true,replace_key,replace_status,replace_rtc;
static unsigned acquire_mode,release_mode;
static uint32_t keys,now_ms,key_calls,status_calls,rtc_calls,acquires,releases,pumps,opens,closes;
static alarm_status_v1 status_value;
static int32_t status_result;
static twatch_rtc_time_v1 rtc_value;
static bool clock_resident_alive(void){return live&&!clock_resident_terminal;}
static bool alive(uint32_t*out){*out=now_ms;return live;}
static bool watch_capabilities_key_events(const int*c,uint32_t*out){(void)c;key_calls++;*out=keys;keys=0;if(replace_key)live=false;return key_ok;}
static int32_t alarm_status(void*c,alarm_status_v1*out){(void)c;status_calls++;*out=status_value;if(replace_status)live=false;return status_result;}
static bool rtc_read(void*c,twatch_rtc_time_v1*out){(void)c;rtc_calls++;*out=rtc_value;if(replace_rtc)live=false;return rtc_ok;}
static alarm_service_v1 alarm_api={.api_version=1,.struct_size=sizeof(alarm_api),.status=alarm_status};
static twatch_rtc_api_v1 rtc_api={.api_version=2,.struct_size=sizeof(rtc_api),.read=rtc_read};
static bool clock_resident_alarm_ready(void){return live;}
static bool launcher_touch_open(void*t){(void)t;opens++;return true;}
static bool launcher_touch_close(void*t){(void)t;closes++;return true;}
static bool clock_resident_alarm_pump(void){pumps++;return true;}
static bool clock_resident_background_pause(void){return true;}
static int32_t test_deadline(void*a,alarm_sleep_v1*d){(void)a;*d=(alarm_sleep_v1){sizeof(*d),status_value.snapshot,100,110};return ALARM_OK;}
static bool portable_alarm_status(void*c){(void)c;clock_alarm.status=status_value;return true;}
static bool acquire(const char*n,uint32_t v,uint64_t i,risc_runtime_capability_v1*g){assert(!strcmp(n,"rtc.clock")&&v==2&&!i);acquires++;if(acquire_mode==1)return false;if(acquire_mode==2){g->struct_size=1;return false;}if(acquire_mode==3){g->slot=2;return false;}g->api=&rtc_api;g->slot=1;g->generation=1;if(acquire_mode==4)g->struct_size=1;return true;}
static bool release(risc_runtime_capability_v1*g){releases++;memset(g,0,sizeof(*g));g->struct_size=sizeof(*g);if(release_mode==1)g->slot=2;if(release_mode==2)g->struct_size=1;return true;}
#include "apps/clock/clock_resident_attention.inc"
static void reset(void){
 live=key_ok=rtc_ok=true;replace_key=replace_status=replace_rtc=false;keys=0;now_ms=1000;acquire_mode=release_mode=0;
 key_calls=status_calls=rtc_calls=acquires=releases=pumps=opens=closes=0;
 clock_resident_terminal=clock_resident_dispatching=false;clock_resident_child=true;clock_resident_keys=clock_resident_armed_at=0;
 clock_attention_enabled=true;clock_attention_observing=clock_attention_alarm=clock_attention_sampled=false;
 clock_attention_rtc_grant=(risc_runtime_capability_v1){.struct_size=sizeof(clock_attention_rtc_grant)};clock_attention_rtc=rtc=&rtc_api;
 clock_alarm.api=&alarm_api;clock_alarm_error_seen=false;clock_attention_deadline=110;clock_attention_snapshot=7;
 status_value=(alarm_status_v1){.api_version=1,.struct_size=sizeof(status_value),.state=ALARM_STATE_READY,.snapshot=7};status_result=ALARM_OK;
 rtc_value=(twatch_rtc_time_v1){.year=2000,.month=1,.day=1,.minute=1,.second=40};
 runtime.acquire=acquire;runtime.release=release;alarm_api.prepare_sleep=test_deadline;
}
static uint32_t observe(uint32_t now){risc_resident_attention_v1 o={sizeof(o),7,0,now};assert(clock_attention_observe(NULL,now,&o)==RISC_RESIDENT_OK);assert(!acquires&&!releases&&!pumps&&!opens&&!closes);return o.pending_flags;}
static void retained(void){risc_resident_attention_v1 o={sizeof(o),7,0,1000};assert(clock_attention_observe(NULL,1000,&o)==RISC_RESIDENT_RETAINED);unsigned n=key_calls+status_calls+rtc_calls;assert(clock_attention_observe(NULL,1001,&o)==RISC_RESIDENT_RETAINED);assert(n==key_calls+status_calls+rtc_calls);}
int main(void){unsigned cases=0;
 reset();for(unsigned i=0;i<500;i++){rtc_value.second=(uint8_t)(40+i/100);assert(!observe(1000+i*10));}assert(rtc_calls==5);cases++;
 reset();rtc_value.second=50;assert(observe(1000)==1);assert(observe(1001)==1);cases++;
 reset();assert(!observe(1000));rtc_value.second=55;assert(observe(2000)==1);cases++;
 reset();assert(!observe(1000));rtc_value.second=39;assert(observe(2000)==1);cases++;
 reset();rtc_value.month=13;assert(observe(1000)==1);cases++;
 reset();rtc_ok=false;assert(observe(1000)==1);cases++;
 reset();status_value.state=ALARM_STATE_LOADING;assert(observe(1000)==1);cases++;
 reset();status_value.snapshot++;assert(observe(1000)==1);cases++;
 reset();status_value.occurrence=(alarm_token_v1){1,2,3,4};assert(observe(1000)==1);assert(status_value.occurrence.generation==4);cases++;
 reset();status_value.output_uncertain=1;retained();assert(!key_calls&&!rtc_calls);cases++;
 reset();keys=2;assert(!observe(100));keys=2;assert(observe(300)==2);assert(clock_resident_keys==2&&observe(301)==2);cases++;
 reset();keys=1;assert(observe(300)==4);assert(clock_resident_keys==1);cases++;
 reset();keys=3;assert(observe(300)==6);cases++;
 reset();key_ok=false;retained();cases++;
 reset();replace_key=true;retained();cases++;
 reset();replace_status=true;retained();assert(!rtc_calls);cases++;
 reset();replace_rtc=true;retained();cases++;
 reset();status_result=-99;retained();cases++;
 reset();status_value.struct_size=1;retained();cases++;
 reset();clock_attention_observing=true;{risc_resident_attention_v1 o={sizeof(o),7,0,0};assert(clock_attention_observe(NULL,0,&o)==RISC_RESIDENT_BUSY);assert(!key_calls);}cases++;
 reset();status_result=ALARM_BUSY;keys=2;{risc_resident_attention_v1 o={sizeof(o),7,0,300};assert(clock_attention_observe(NULL,300,&o)==RISC_RESIDENT_BUSY&&!o.pending_flags);assert(clock_resident_keys==0&&!key_calls&&!rtc_calls);}cases++;
 reset();clock_attention_sampled=true;clock_attention_at=UINT32_MAX-500;clock_attention_seconds=100;rtc_value.second=41;assert(!observe(500));cases++;
 reset();clock_alarm_error_seen=true;clock_alarm_seen_snapshot=7;clock_alarm_seen_error=-5;status_value.state=ALARM_STATE_BLOCKED;status_value.error=-5;assert(!observe(1000)&&!rtc_calls);cases++;
 reset();clock_alarm.api=NULL;assert(!observe(1000)&&!rtc_calls&&!status_calls);cases++;
 reset();rtc=NULL;clock_attention_rtc=NULL;assert(clock_attention_prime());assert(acquires==1&&pumps==1&&opens==1&&closes==1);assert(clock_attention_close()&&releases==1);cases++;
 reset();assert(clock_attention_prime());assert(!acquires&&clock_attention_deadline==110&&clock_attention_sampled);cases++;
 reset();clock_resident_keys=3;clock_attention_alarm=true;assert(clock_attention_reject(NULL,2)==0&&clock_resident_keys==1&&clock_attention_alarm);assert(clock_attention_reject(NULL,4)==0&&!clock_resident_keys&&clock_attention_alarm);cases++;
 reset();clock_resident_keys=3;assert(clock_attention_reject(NULL,1)==RISC_RESIDENT_INVALID&&clock_resident_keys==3);assert(clock_attention_reject(NULL,0)==RISC_RESIDENT_INVALID);cases++;
 reset();clock_resident_keys=2;clock_resident_dispatching=true;assert(clock_attention_reject(NULL,2)==RISC_RESIDENT_BUSY&&clock_resident_keys==2);cases++;
 reset();clock_attention_rtc_grant.slot=1;assert(!clock_attention_close()&&!releases);cases++;
 reset();clock_attention_rtc_grant.struct_size=1;assert(!clock_attention_close()&&!releases);cases++;
 reset();status_value.error=-99;retained();cases++;
 for(unsigned mode=1;mode<=4;mode++){reset();rtc=NULL;clock_attention_rtc=NULL;acquire_mode=mode;assert(!clock_attention_prime()&&acquires==1&&!opens&&!pumps);cases++;}
 for(unsigned mode=1;mode<=2;mode++){reset();clock_attention_rtc_grant.api=&rtc_api;clock_attention_rtc_grant.slot=clock_attention_rtc_grant.generation=1;release_mode=mode;assert(!clock_attention_close()&&releases==1);cases++;}
 printf("Home attention %u cases PASS: quiet continuity, fresh RTC, exact intent, no observer mutation/custody loss\n",cases);return 0;
}
