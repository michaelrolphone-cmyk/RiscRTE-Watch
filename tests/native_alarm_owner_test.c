#define WATCH_NATIVE_ALARM_V2
#define ALARM_SERVICE_TAGGED_V2
#define WATCH_NATIVE_TIME_AUTHORITY
#define WATCH_RUNTIME_FEATURES
#define WATCH_CLOCK_ALARMS
#define WATCH_CLOCK_LAUNCHER
#define WATCH_ALARM_SLEEP_RESUME
#define WATCH_SHARED_PREFERENCE_HEADERS
#include "apps/clock/crown.c"
#include <assert.h>
#include <stdio.h>
static unsigned retained,calls;
static int32_t result;
static bool retain(void){retained++;return true;}
static bool release(risc_runtime_capability_v1*g){(void)g;calls++;return false;}
static int32_t stop(void*c){(void)c;calls++;return result;}
static int32_t get_status(void*c,alarm_status_v1*s){(void)c;calls++;*s=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*s),.state=ALARM_STATE_READY};return result;}
static int32_t ack(void*c,const alarm_token_v1*t){(void)c;(void)t;calls++;return result;}
static int32_t plan(void*c,alarm_sleep_v1*t){(void)c;(void)t;return 0;}
static int32_t resume_plan(void*c,const alarm_sleep_v1*t){(void)c;(void)t;return 0;}
static const alarm_service_descriptor_v2 descriptor={.base={2,sizeof(descriptor),NULL,get_status,stop,stop,ack,plan,stop},.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=1,.features=ALARM_DESCRIPTOR_RESUME_SLEEP,.resume_sleep=resume_plan};
static bool acquire(const char*n,uint32_t api,uint64_t instance,risc_runtime_capability_v1*g){assert(!strcmp(n,"alarm.service")&&api==2&&!instance);calls++;g->api=&descriptor.base;return true;}
static risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.retain_invocation=retain,.release=release,.acquire=acquire};
static void setup(void){rt=&runtime;native_clock=(watch_clock_runtime){.runtime=rt};clock_alarm=(portable_alarm_client){.api=&descriptor.base,.grant={.api=&descriptor.base}};clock_alarm_failed_cleaned=false;retained=calls=0;result=ALARM_RETAINED;}
int main(void){
 (void)watch_runtime_promote;
 setup();assert(clock_alarm_open_optional()&&calls==1&&clock_alarm.api==&descriptor.base&&clock_alarm_attempted);
 setup();assert(!portable_alarm_pump(&clock_alarm)&&calls==1&&clock_alarm.retained);assert(!clock_alarm_failure()&&retained==1&&native_clock.halted);clock_alarm_finish();clock_exit_checkpoint(7);assert(calls==1&&retained==1);
 setup();assert(!portable_alarm_refresh(&clock_alarm));assert(!clock_alarm_failure()&&calls==1&&retained==1);
 setup();alarm_token_v1 token={0};assert(!portable_alarm_acknowledge(&clock_alarm,&token));assert(!clock_alarm_failure()&&calls==1&&retained==1);
 setup();clock_alarm_failure();assert(calls==1&&retained==1); // failure-stop retention before diagnostic/yield
 setup();result=ALARM_OK;clock_display_settled=true;clock_alarm_finish();assert(calls==2&&retained==1&&native_clock.halted); // status then uncertain release
 puts("PASS production Home API2 client-to-Runtime fence: step/refresh/ack/stop/release; no post-retention cleanup/checkpoint callback");
}
