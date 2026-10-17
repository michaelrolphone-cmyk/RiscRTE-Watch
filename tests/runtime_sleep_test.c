/* Production Home with exact old/new Runtime prefixes and sleep-custody mocks. */
#define WATCH_TEST_RUNTIME_EXTERN
#define main inherited_quick_main
#include "quick_clock_test.c"
#undef main
static risc_runtime_api_v1 selected;
static const risc_runtime_api_v1 *selected_api;
static unsigned begins,ends,panel_resumes,power_resumes,stopped_grants,stopped_releases;
static int32_t begin_result,end_result,power_result;
static bool fenced;
static void authority(void){assert(!fenced);}
static bool checked_release(risc_runtime_capability_v1 *g){authority();return release_cap(g);}
static bool checked_health(risc_runtime_health_v1 *h){authority();return health(h);}
static void checked_yield(uint32_t n){authority();yield(n);}
static bool checked_diagnostic(const char *s){return diagnostic(s);}
static bool idle_touch(void *c,risc_touch_snapshot_v1 *s){authority();(void)c;*s=(risc_touch_snapshot_v1){.width=TEST_WIDTH,.height=TEST_HEIGHT};return true;}
static void fence(void){fenced=true;stopped_grants=grants;stopped_releases=ungrants;}
static int32_t begin(uint64_t *token){
 authority();assert(!owned&&!pending&&!held&&!touch.subscription&&last_complete==submitted&&submitted);
 assert(!hardware_brightness);for(size_t i=0;i<sizeof(completed_pixels)/sizeof(*completed_pixels);++i)assert(!completed_pixels[i]);
 begins++;*token=0;
 if(begin_result==RISC_RUNTIME_SLEEP_OK)*token=0x12340000u+begins;
 if(begin_result==RISC_RUNTIME_SLEEP_RETAINED)fence();
 return begin_result;
}
static int32_t end(uint64_t token){
 authority();assert(token==0x12340000u+begins&&begins==ends+1);
 assert(panel_resumes==begins&&power_resumes==begins&&!owned&&!pending);
 ends++;if(end_result!=RISC_RUNTIME_SLEEP_OK&&end_result!=RISC_RUNTIME_SLEEP_MEDIA_UNAVAILABLE)fence();
 return end_result;
}
static bool panel_resume(void*c){authority();panel_resumes++;return resume(c);}
static bool power_resume(void*c){authority();power_resumes++;return resume(c);}
static int32_t enter(void*c,risc_light_sleep_result_v1*r){
 authority();assert(begins==ends+1);(void)sleep_now(c,r);
 if(power_result==RISC_LIGHT_SLEEP_RETAINED)fence();
 return power_result;
}
static int32_t enter_timed(void*c,uint32_t ms,risc_light_sleep_result_v1*r){assert(ms==300000);return enter(c,r);}
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){assert(v==1);return selected_api;}
static void setup(void){
 selected=runtime;selected.struct_size=sizeof(selected);selected.sleep_prepare=begin;selected.sleep_resume=end;
 selected.release=checked_release;selected.health=checked_health;selected.yield_ms=checked_yield;selected.diagnostic=checked_diagnostic;
 selected_api=&selected;scenario=7;ta.snapshot=idle_touch;da.resume=panel_resume;pa.resume=power_resume;pa.light_sleep=enter;pa.light_sleep_for=enter_timed;
 begin_result=end_result=RISC_RUNTIME_SLEEP_OK;power_result=RISC_LIGHT_SLEEP_ACTIVE_WAKE;
}
static void compatibility(void){
 uint64_t token=99;assert(risc_runtime_sleep_prepare_v1(NULL,&token)==RISC_RUNTIME_SLEEP_CONTEXT&&!token);
 assert(risc_runtime_sleep_prepare_v1(&selected,NULL)==RISC_RUNTIME_SLEEP_CONTEXT);
 selected.api_version=2;assert(risc_runtime_sleep_prepare_v1(&selected,&token)==RISC_RUNTIME_SLEEP_CONTEXT);selected.api_version=1;
 const size_t sizes[]={RISC_RUNTIME_CAPABILITIES_V1_SIZE,RISC_RUNTIME_INITIAL_DEFAULT_V1_SIZE,RISC_RUNTIME_SERVICE_CHECKPOINT_V1_SIZE,RISC_RUNTIME_SLEEP_V1_SIZE-1};
 for(unsigned i=0;i<sizeof(sizes)/sizeof(*sizes);++i){
  risc_runtime_api_v1 *old=malloc(sizes[i]);assert(old);memcpy(old,&selected,sizes[i]);old->struct_size=(uint32_t)sizes[i];
  token=99;assert(risc_runtime_sleep_prepare_v1(old,&token)==RISC_RUNTIME_SLEEP_OK&&!token);
  assert(risc_runtime_sleep_resume_v1(old,0)==RISC_RUNTIME_SLEEP_OK);free(old);
 }
 selected.sleep_prepare=NULL;assert(risc_runtime_sleep_prepare_v1(&selected,&token)==RISC_RUNTIME_SLEEP_UNSUPPORTED);
 selected.sleep_prepare=begin;selected.sleep_resume=NULL;assert(risc_runtime_sleep_prepare_v1(&selected,&token)==RISC_RUNTIME_SLEEP_UNSUPPORTED);
 assert(!begins&&!ends);
}
int main(int argc,char **argv){
 assert(argc==2);setup();const char *mode=argv[1];
 if(!strcmp(mode,"helper")){compatibility();goto done;}
 risc_runtime_api_v1 *legacy=NULL;
 if(!strcmp(mode,"old")){
  const size_t n=RISC_RUNTIME_SERVICE_CHECKPOINT_V1_SIZE;void *bytes=malloc(n);assert(bytes);memcpy(bytes,&selected,n);legacy=bytes;
  legacy->struct_size=(uint32_t)n;selected_api=legacy;pa.light_sleep=sleep_now;pa.light_sleep_for=sleep_timed;
 }else if(!strcmp(mode,"missing"))selected.sleep_resume=NULL;
 else if(!strcmp(mode,"busy"))begin_result=RISC_RUNTIME_SLEEP_BUSY;
 else if(!strcmp(mode,"unsupported"))begin_result=RISC_RUNTIME_SLEEP_UNSUPPORTED;
 else if(!strcmp(mode,"context"))begin_result=RISC_RUNTIME_SLEEP_CONTEXT;
 else if(!strcmp(mode,"prepare-retained"))begin_result=RISC_RUNTIME_SLEEP_RETAINED;
 else if(!strcmp(mode,"resume-retained"))end_result=RISC_RUNTIME_SLEEP_RETAINED;
 else if(!strcmp(mode,"resume-busy"))end_result=RISC_RUNTIME_SLEEP_BUSY;
 else if(!strcmp(mode,"resume-context"))end_result=RISC_RUNTIME_SLEEP_CONTEXT;
 else if(!strcmp(mode,"resume-unknown"))end_result=-99;
 else if(!strcmp(mode,"removed-card"))end_result=RISC_RUNTIME_SLEEP_MEDIA_UNAVAILABLE;
 else if(!strcmp(mode,"power-retained"))power_result=RISC_LIGHT_SLEEP_RETAINED;
 else assert(!strcmp(mode,"ok"));
 app_main();
 assert(ready&&!owned&&!pending&&!held);
 if(fenced){assert(grants==stopped_grants&&ungrants==stopped_releases&&grants>ungrants);}
 else{
  assert(grants==ungrants&&subscribed==unsubscribed&&!allocations&&!launches);
  assert(hardware_brightness==40); /* refused native preparation restores a complete frame */
  if(!strcmp(mode,"old")){assert(!begins&&!ends&&sleeps);}
  else if(!strcmp(mode,"missing")){assert(!begins&&!ends&&!sleeps);}
  else if(begin_result!=RISC_RUNTIME_SLEEP_OK){assert(begins&&!ends&&!sleeps);}
  else assert(begins&&ends==begins&&sleeps==begins);
 }
 if(!strcmp(mode,"power-retained"))assert(!ends&&!panel_resumes&&!power_resumes);
 if(!strcmp(mode,"prepare-retained"))assert(!ends&&!sleeps&&!panel_resumes&&!power_resumes);
 free(legacy);
 done:printf("Home native sleep %ux%u %s PASS (host-only)\n",TEST_WIDTH,TEST_HEIGHT,mode);
}
