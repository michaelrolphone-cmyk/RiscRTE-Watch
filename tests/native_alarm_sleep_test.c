#define PORTABLE_LOW_BATTERY
#define WATCH_NATIVE_ALARM_V2
#define ALARM_SERVICE_TAGGED_V2
#define WATCH_ALARM_SLEEP_RESUME
#include "watch_alarm_sleep.h"
#include <assert.h>
#include <stdio.h>
static unsigned calls,plans,steps,statuses,resumes,refreshes,restores,timed,untimed,deep,holds;
static unsigned fault,point,fault_plan,pending_at,pending_checks;static bool terminal,pending,loading;static uint32_t delta,duration,wake;
static int32_t fault_result;
static alarm_sleep_v1 issued;
static unsigned motion_pending_at,motion_checks,motion_restores;
static uint32_t preparation_ms, model_ms,light_duration;
static int32_t invoke(unsigned id){assert(!terminal);calls++;if(fault==id && (!fault_plan || plans==fault_plan)){if(fault_result==ALARM_RETAINED||fault_result==ALARM_OUTPUT||fault_result==99)terminal=true;return fault_result;}return 0;}
static int32_t prepare(void*c,alarm_sleep_v1*t){(void)c;plans++;int32_t rc=invoke(1);if(rc)return rc;if(loading&&plans==1)return ALARM_PENDING;
 if(preparation_ms){assert(point==2);*t=(alarm_sleep_v1){sizeof(*t),plans,1000+model_ms/1000,1010};issued=*t;return 0;}
 *t=(alarm_sleep_v1){sizeof(*t),plans,1000+(resumes?light_duration/1000:0),delta?1000+delta:0};issued=*t;return 0;}
static int32_t step(void*c){(void)c;steps++;return invoke(2);}
static int32_t status(void*c,alarm_status_v1*s){(void)c;statuses++;int32_t rc=invoke(3);if(rc)return rc;*s=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*s),.state=ALARM_STATE_READY};return 0;}
static int32_t refresh(void*c){(void)c;refreshes++;return invoke(5);}
static int32_t ack(void*c,const alarm_token_v1*t){(void)c;(void)t;return invoke(6);}
static int32_t stop(void*c){(void)c;return invoke(7);}
static int32_t resumed(void*c,const alarm_sleep_v1*t){(void)c;resumes++;assert(!memcmp(t,&issued,sizeof(*t)));return invoke(4);}
static alarm_service_descriptor_v2 service={.base={2,sizeof(service),NULL,status,step,refresh,ack,prepare,stop},.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=1,.features=ALARM_DESCRIPTOR_RESUME_SLEEP,.resume_sleep=resumed};
static bool prep_panel(void*c){(void)c;invoke(10);point=1;model_ms+=preparation_ms;return true;}
static bool prep_power(void*c){(void)c;invoke(11);assert(point==1);point=2;return true;}
static bool restore_power(void*c){(void)c;invoke(12);restores++;return true;}
static int32_t restore_panel(void*c){(void)c;invoke(13);restores++;return 0;}
static bool restore_old(void*c){(void)c;assert(0);return false;}
static int32_t light(void*c,risc_light_sleep_result_v1*r){(void)c;invoke(14);untimed++;r->wake_cause=wake;return 0;}
static int32_t light_for(void*c,uint32_t ms,risc_light_sleep_result_v1*r){(void)c;int rc=invoke(15);timed++;duration=light_duration=ms;r->wake_cause=wake;return rc;}
static int32_t hold(void*c){(void)c;invoke(16);holds++;return 0;}
static bool key_pending(void*c,bool*p){(void)c;invoke(17);pending_checks++;*p=pending || (pending_at && pending_checks==pending_at);return true;}
static int32_t deep_for(void*c,uint32_t ms){(void)c;invoke(18);duration=ms;deep++;terminal=true;return RISC_DEEP_SLEEP_RETAINED;}
static int32_t deep_plain(void*c){return deep_for(c,0);}
static bool diagnostic(const char*s){(void)s;invoke(19);return true;}
static risc_power_sleep_api_v1 power={.api_version=1,.struct_size=sizeof(power),.prepare_sleep=prep_power,.resume=restore_power,.light_sleep=light,.deep_sleep=deep_plain,.light_sleep_for=light_for,.sleep_wake_pending=key_pending,.deep_sleep_for=deep_for};
static risc_display_power_api_v1 panel={.api_version=1,.struct_size=sizeof(panel),.prepare_sleep=prep_panel,.resume=restore_old,.prepare_deep_sleep=hold,.resume_status=restore_panel};
static bool motion_prepare(void*c){(void)c;invoke(20);return true;}
static bool motion_pending(void*c,bool*p){(void)c;invoke(21);motion_checks++;*p=motion_pending_at&&motion_checks==motion_pending_at;return true;}
static bool motion_resume(void*c){(void)c;invoke(22);motion_restores++;return true;}
static twatch_motion_api_v1 motion={.api_version=TWATCH_MOTION_API_V1,.struct_size=sizeof(motion),.prepare_wake=motion_prepare,.wake_pending=motion_pending,.resume_wake=motion_resume};
static int32_t light_set(void*c,uint32_t ms,risc_light_sleep_result_v1*r){return ms?light_for(c,ms,r):light(c,r);}
static void reset(void){motion_pending_at=motion_checks=motion_restores=0;calls=plans=steps=statuses=resumes=refreshes=restores=timed=untimed=deep=holds=point=fault=fault_plan=pending_at=pending_checks=0;terminal=pending=loading=false;delta=5;duration=0;wake=RISC_LIGHT_SLEEP_WAKE_TIMER;fault_result=ALARM_RETAINED;preparation_ms=model_ms=light_duration=0;watch_sleep_light_ms=PORTABLE_SLEEP_LIGHT_MS;}
static int run(unsigned mode){return watch_alarm_sleep_prepared(&panel,&power,mode,&service.base,diagnostic);}
int main(void){
 (void)watch_sleep_prepared;
 reset();assert(run(PORTABLE_SLEEP_LIGHT)==WATCH_SLEEP_WOKE);assert(timed==1&&duration==4000&&resumes==1&&restores==2&&refreshes==1);
 reset();loading=true;assert(run(PORTABLE_SLEEP_LIGHT)==WATCH_SLEEP_WOKE);assert(plans==2&&steps==1&&statuses==1&&resumes==1);
 for(unsigned mode=0;mode<3;mode++){reset();delta=0;int rc=run(mode);if(mode==PORTABLE_SLEEP_DEEP)assert(rc==WATCH_SLEEP_RETAINED&&deep==1&&!duration&&!resumes&&!restores);else if(mode==PORTABLE_SLEEP_LIGHT)assert(rc==WATCH_SLEEP_WOKE&&untimed==1&&!timed&&resumes==1);else assert(rc==WATCH_SLEEP_RETAINED&&timed==1&&light_duration==PORTABLE_SLEEP_LIGHT_MS&&deep==1&&!duration&&resumes==1&&plans==2&&!restores);}
 reset();assert(run(PORTABLE_SLEEP_DEEP)==WATCH_SLEEP_RETAINED&&duration==4000&&holds==1&&!resumes&&!restores&&!refreshes);
 reset();pending=true;assert(run(PORTABLE_SLEEP_DEEP)==WATCH_SLEEP_REFUSED&&!deep&&restores==2&&!resumes);
 reset();delta=90000;assert(run(PORTABLE_SLEEP_LIGHT)==WATCH_SLEEP_WOKE&&duration==RISC_TIMED_SLEEP_MAX_MS);
 reset();delta=900;assert(run(PORTABLE_SLEEP_HYBRID)==WATCH_SLEEP_RETAINED&&timed==1&&deep==1&&resumes==1&&plans==2&&!restores&&duration==599000);
 reset();delta=900;wake=RISC_LIGHT_SLEEP_WAKE_GPIO;assert(run(PORTABLE_SLEEP_HYBRID)==WATCH_SLEEP_WOKE&&!deep&&resumes==1);
 reset();fault=1;fault_result=ALARM_BUSY;assert(run(PORTABLE_SLEEP_LIGHT)==WATCH_SLEEP_REFUSED&&!timed&&!resumes&&restores==2);
 reset();fault=1;fault_result=ALARM_STORAGE;assert(run(PORTABLE_SLEEP_LIGHT)==WATCH_SLEEP_REFUSED&&!timed&&!resumes&&restores==2&&!refreshes);
 reset();fault=1;fault_result=ALARM_PENDING;assert(run(PORTABLE_SLEEP_LIGHT)==WATCH_SLEEP_REFUSED&&plans==64&&steps==64&&statuses==64&&!resumes);
 reset();fault=15;fault_result=RISC_LIGHT_SLEEP_ACTIVE_WAKE;assert(run(PORTABLE_SLEEP_LIGHT)==WATCH_SLEEP_REFUSED&&!resumes&&restores==2);
 for(unsigned f=1;f<=5;f++)for(unsigned k=0;k<3;k++){
  reset();loading=true;fault=f;fault_result=k==0?ALARM_RETAINED:k==1?ALARM_OUTPUT:99;
  assert(run(PORTABLE_SLEEP_LIGHT)==WATCH_SLEEP_RETAINED&&terminal);
  if(f<5)assert(!restores);
  if(f<4)assert(!resumes);
  if(f==2)assert(!statuses);
 }
 reset();delta=900;for(unsigned i=0;i<32;i++)assert(run(PORTABLE_SLEEP_LIGHT)==WATCH_SLEEP_WOKE);assert(plans==32&&resumes==32&&restores==64);
 reset();service.tag=0;assert(run(PORTABLE_SLEEP_LIGHT)==WATCH_SLEEP_REFUSED&&!calls);service.tag=ALARM_SERVICE_DESCRIPTOR_TAG;
 reset();service.base.api_version=1;assert(run(PORTABLE_SLEEP_LIGHT)==WATCH_SLEEP_REFUSED&&!calls);service.base.api_version=2;
 alarm_sleep_v1 t={.rtc_seconds=1000,.deadline=999};assert(watch_alarm_duration(&t)==1);t.deadline=1000;assert(watch_alarm_duration(&t)==1);t.deadline=1001;assert(watch_alarm_duration(&t)==1);
 /* Exhaust integer boundaries, including overflow-sized UTC deltas. */
 const uint32_t ds[]={0,1,2,3,86399,86400,86401,UINT32_MAX-1000};
 const uint32_t expected[]={0,1,1000,2000,86398000,86399000,86400000,86400000};
 for(unsigned i=0;i<sizeof(ds)/sizeof(*ds);i++){
  t=(alarm_sleep_v1){.rtc_seconds=1000,.deadline=ds[i]?1000+ds[i]:0};
  assert(watch_alarm_duration(&t)==expected[i]);
 }
 /* Every modeled millisecond fraction: early polling for representable
    deadlines, except the deliberately minimum 1ms at the final sub-ms edge.
    Post-ticket delay consumes this margin; it is not an exact-time guarantee. */
 for(unsigned d=1;d<=100;d++)for(unsigned frac=0;frac<1000;frac++){
  t=(alarm_sleep_v1){.rtc_seconds=1000,.deadline=1000+d};
  uint32_t ms=watch_alarm_duration(&t);
  assert(frac+ms<=d*1000);
  if(d>1)for(unsigned lag=0;lag<=1000-frac;lag+=100)
   assert(frac+lag+ms<=d*1000);
 }
 /* An unbounded post-ticket delay can still miss the deadline: explicitly
    preserve this limitation instead of asserting impossible physical timing. */
 t=(alarm_sleep_v1){.rtc_seconds=1000,.deadline=1002};
 assert(999u+2u+watch_alarm_duration(&t)>2000u);
 /* Ordinary panel preparation happens before the service sample: varying
    delays crossing UTC seconds cannot leave a stale pre-panel duration. */
 const uint32_t preps[]={120,999,1000,1500,4999,8000};
 for(unsigned i=0;i<sizeof(preps)/sizeof(*preps);i++){
  reset();preparation_ms=preps[i];assert(run(PORTABLE_SLEEP_LIGHT)==WATCH_SLEEP_WOKE);
  assert(issued.rtc_seconds==1000+preps[i]/1000);
  assert(preps[i]+duration<=10000);assert(resumes==1);
 }
 /* Every persisted deep-timer second, including conservative alarm duration
    just before/equal/after it. No deadline must still progress Light -> Deep. */
 unsigned staged_cases=0;
 for(uint32_t seconds=60;seconds<=3600;seconds++) {
  for(unsigned variant=0;variant<4;variant++)for(unsigned key=0;key<2;key++) {
   reset();watch_sleep_light_ms=seconds*1000u;delta=variant?seconds+variant-1:0;
   wake=key?RISC_LIGHT_SLEEP_WAKE_GPIO:RISC_LIGHT_SLEEP_WAKE_TIMER;
   uint32_t alarm_ms=delta?(delta-1)*1000u:0;
   bool progress=!key && (!alarm_ms || alarm_ms>watch_sleep_light_ms);
   uint32_t expected_light=(!alarm_ms || alarm_ms>watch_sleep_light_ms)?watch_sleep_light_ms:alarm_ms;
   int rc=run(PORTABLE_SLEEP_HYBRID);
   assert(timed==1&&!untimed&&light_duration==expected_light&&resumes==1);
   assert(progress?(rc==WATCH_SLEEP_RETAINED&&deep==1&&plans==2&&!restores):
     (rc==WATCH_SLEEP_WOKE&&!deep&&plans==1&&restores==2));staged_cases++;
  }
 }
 /* Key arrival at either transition check defeats Deep. */
 reset();delta=0;pending=true;assert(run(PORTABLE_SLEEP_HYBRID)==WATCH_SLEEP_WOKE&&!deep&&!holds&&resumes==1);
 reset();delta=0;pending_at=2;assert(run(PORTABLE_SLEEP_HYBRID)==WATCH_SLEEP_REFUSED&&!deep&&holds==1&&resumes==1&&restores==2);
 /* A newly due alarm at the fresh plan restores normally; storage faults do
    not become an apparent wake, and terminal responses forbid later calls. */
 reset();delta=0;fault=1;fault_plan=2;fault_result=ALARM_BUSY;
 assert(run(PORTABLE_SLEEP_HYBRID)==WATCH_SLEEP_WOKE&&!deep&&!holds&&resumes==1&&restores==2);
 reset();delta=0;fault=1;fault_plan=2;fault_result=ALARM_STORAGE;
 assert(run(PORTABLE_SLEEP_HYBRID)==WATCH_SLEEP_REFUSED&&!deep&&!holds&&resumes==1&&restores==2&&!refreshes);
 for(unsigned kind=0;kind<3;kind++) {
  reset();delta=0;fault=1;fault_plan=2;fault_result=kind==0?ALARM_RETAINED:kind==1?ALARM_OUTPUT:99;
  assert(run(PORTABLE_SLEEP_HYBRID)==WATCH_SLEEP_RETAINED&&terminal&&!deep&&!holds&&!restores);
 }
 reset();delta=0;fault=15;fault_result=RISC_LIGHT_SLEEP_ACTIVE_WAKE;
 assert(run(PORTABLE_SLEEP_HYBRID)==WATCH_SLEEP_REFUSED&&!deep&&!resumes&&restores==2);
 reset();delta=0;power.light_sleep_for=NULL;
 assert(run(PORTABLE_SLEEP_HYBRID)==WATCH_SLEEP_REFUSED&&!timed&&!untimed&&!resumes&&!deep&&restores==2);power.light_sleep_for=light_for;
 /* Enrolled motion uses the existing multi-source API and defeats Deep at
    either pending check; no sensor capability is synthesized when absent. */
 power.light_sleep_set=light_set;power.deep_sleep_set=deep_for;
 for(unsigned stage=0;stage<3;stage++) {
  reset();delta=0;motion_pending_at=stage;
  int rc=watch_alarm_sleep_motion_prepared(&panel,&power,&motion,PORTABLE_SLEEP_HYBRID,&service.base,diagnostic);
  assert(timed==1&&resumes==1);
  if(!stage)assert(rc==WATCH_SLEEP_RETAINED&&deep==1&&!motion_restores);
  else assert(!deep&&motion_restores==1&&restores==2&&(stage==1?rc==WATCH_SLEEP_WOKE:rc==WATCH_SLEEP_REFUSED));
 }
 reset();delta=0;power.light_sleep_set=NULL;
 assert(watch_alarm_sleep_motion_prepared(&panel,&power,&motion,PORTABLE_SLEEP_HYBRID,&service.base,diagnostic)==WATCH_SLEEP_REFUSED&&!timed&&!resumes&&!deep&&motion_restores==1);
 power.deep_sleep_set=NULL;
 printf("PASS %u staged timer/deadline/GPIO cases\n",staged_cases);
 puts("PASS tagged API2 Home sleep: UTC delta/cap, Light no-schedule untimed, Hybrid no-schedule timed then Deep, timer+key, exact Light ticket once, Hybrid fresh plan, Deep no post-hold service, retained every callback");
}
