#define ALARM_SERVICE_TAGGED_V2 1
#include <stdbool.h>
static bool call_safe(void);
#define WATCH_ALARM_CALL_SAFE() call_safe()
#include "watch_alarm_sleep.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static unsigned calls,restores,lose_after;
static bool call_safe(void){return !lose_after||calls<lose_after;}
static int prep_rc,step_rc,status_rc,resume_rc,refresh_rc;
static alarm_sleep_v1 ticket={sizeof(ticket),7,900,905};
static int32_t prepare(void*p,alarm_sleep_v1*out){(void)p;++calls;*out=ticket;return prep_rc;}
static int32_t step(void*p){(void)p;++calls;return step_rc;}
static int32_t status(void*p,alarm_status_v1*out){(void)p;++calls;*out=(alarm_status_v1){.struct_size=sizeof(*out),.state=ALARM_STATE_READY};return status_rc;}
static int32_t refresh(void*p){(void)p;++calls;return refresh_rc;}
static int32_t stop(void*p){(void)p;return 0;}
static int32_t ack(void*p,const alarm_token_v1*t){(void)p;(void)t;return 0;}
static int32_t resume(void*p,const alarm_sleep_v1*t){(void)p;++calls;assert(!memcmp(t,&ticket,sizeof(ticket)));return resume_rc;}
static bool physical_prepare(void*p){(void)p;return true;}
static bool physical_resume(void*p){(void)p;++restores;return true;}
static bool diagnostic(const char*p){(void)p;assert(!"No diagnostics after retained alarm");return false;}
int main(void){
 alarm_service_descriptor_v2 a={.base={.api_version=2,.struct_size=sizeof(a),.status=status,.step=step,.refresh=refresh,.acknowledge=ack,.prepare_sleep=prepare,.stop_only=stop},
 .tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=1,.features=ALARM_DESCRIPTOR_RESUME_SLEEP,.resume_sleep=resume};
 assert(watch_alarm_resume_ready(&a.base));assert(watch_alarm_resume_light(&a.base,&ticket)==0&&calls==1);
 a.tag=0;assert(!watch_alarm_resume_ready(&a.base));assert(watch_alarm_resume_light(&a.base,&ticket)==ALARM_INVALID&&calls==1);a.tag=ALARM_SERVICE_DESCRIPTOR_TAG;
 a.base.api_version=1;assert(!watch_alarm_resume_ready(&a.base));a.base.api_version=2;
 a.features=0;assert(!watch_alarm_resume_ready(&a.base));a.resume_sleep=NULL;assert(!watch_alarm_resume_ready(&a.base));a.features=1;a.resume_sleep=resume;
 alarm_sleep_v1 out;calls=0;assert(watch_alarm_deadline(&a.base,&out)==0&&calls==1&&watch_alarm_duration(&out)==5000);
 prep_rc=ALARM_PENDING;
 for(unsigned i=0;i<3;i++){step_rc=i==0?ALARM_RETAINED:i==1?ALARM_OUTPUT:-99;calls=0;assert(watch_alarm_deadline(&a.base,&out)==ALARM_RETAINED&&calls==2);}
 step_rc=0;status_rc=ALARM_RETAINED;calls=0;assert(watch_alarm_deadline(&a.base,&out)==ALARM_RETAINED&&calls==3);status_rc=0;
 const risc_display_power_api_v1 panel={.api_version=1,.struct_size=sizeof(panel),.prepare_sleep=physical_prepare,.resume=physical_resume};
 const risc_power_sleep_api_v1 power={.api_version=1,.struct_size=sizeof(power),.prepare_sleep=physical_prepare,.resume=physical_resume};
 for(unsigned i=0;i<3;i++){prep_rc=i==0?ALARM_RETAINED:i==1?ALARM_OUTPUT:-99;calls=restores=0;
 assert(watch_alarm_sleep_prepared(&panel,&power,PORTABLE_SLEEP_LIGHT,&a.base,diagnostic)==WATCH_SLEEP_RETAINED&&calls==1&&restores==0);}
 prep_rc=ALARM_PENDING;step_rc=status_rc=0;
 for(unsigned n=1;n<=3;n++){lose_after=n;calls=0;assert(watch_alarm_deadline(&a.base,&out)==ALARM_RETAINED&&calls==n);}
 lose_after=1;calls=restores=0;prep_rc=ALARM_OK;
 assert(watch_alarm_sleep_prepared(&panel,&power,PORTABLE_SLEEP_LIGHT,&a.base,diagnostic)==WATCH_SLEEP_RETAINED&&calls==1&&!restores);
 lose_after=0;
 (void)watch_sleep_prepared;
 puts("Tagged alarm sleep: exact descriptor, unchanged ticket/domain delta, terminal prepare/step/status without later callbacks PASS");
}
