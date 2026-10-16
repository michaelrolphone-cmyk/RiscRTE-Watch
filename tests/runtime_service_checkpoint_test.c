/* Production Home plus delayed presentation. These are host timing/provider
 * doubles; no hardware or serial endpoint is involved. */
#define WATCH_TEST_RUNTIME_EXTERN
#define main inherited_quick_main
#include "quick_clock_test.c"
#undef main

static risc_runtime_api_v1 selected;
static const risc_runtime_api_v1 *selected_api;
static unsigned service_calls,pending_yields,stopped_grants,stopped_releases;
static int32_t service_result;
static bool authority_stopped;

static void check_authority(void){assert(!authority_stopped);}
static bool checked_health(risc_runtime_health_v1 *out){check_authority();return health(out);}
static void checked_yield(uint32_t ms){check_authority();if(pending)pending_yields++;yield(ms);}
static bool checked_acquire(const char *name,uint32_t version,uint64_t id,risc_runtime_capability_v1 *out){
 check_authority();return acquire_cap(name,version,id,out);
}
static bool checked_release(risc_runtime_capability_v1 *grant){check_authority();return release_cap(grant);}
static bool checked_diagnostic(const char *text){check_authority();return diagnostic(text);}
static bool checked_launch(const char *path){check_authority();return launch(path);}
static bool idle_touch(void *context,risc_touch_snapshot_v1 *out){
 check_authority();(void)context;*out=(risc_touch_snapshot_v1){.width=TEST_WIDTH,.height=TEST_HEIGHT};return true;
}
static int32_t service_checkpoint(void){
 check_authority();
 /* Accepted presentations must have COMPLETED, not merely relinquished held. */
 assert(ready&&!held&&!owned&&!pending&&submitted&&last_complete==submitted);
 service_calls++;
 if(service_result!=RISC_RUNTIME_CHECKPOINT_OK&&service_result!=RISC_RUNTIME_CHECKPOINT_BUSY){
  stopped_grants=grants;stopped_releases=ungrants;authority_stopped=true;
 }
 return service_result;
}
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){assert(version==1);return selected_api;}

static void setup(void){
 assert(!allocations&&!held);
 quick_writes=face_writes=face_reads=ready_time=cache_allocations=0;saved_face=false;
 clock_ms=submitted=released=grants=ungrants=subscribed=unsubscribed=polls=launches=sleeps=touch_step=0;
 ready=owned=pending=launched=false;last_complete=swipe_submitted=swipe_time=0;
 service_calls=pending_yields=stopped_grants=stopped_releases=0;authority_stopped=false;
 scenario=14; /* Each presentation remains ACTIVE for 95 modeled ms. */
 memset(quick_saved,0,sizeof(quick_saved));memset(quick_values,0,sizeof(quick_values));
 selected=runtime;selected.health=checked_health;selected.yield_ms=checked_yield;
 selected.acquire=checked_acquire;selected.release=checked_release;
 selected.diagnostic=checked_diagnostic;selected.request_launch=checked_launch;
 selected.service_checkpoint=service_checkpoint;selected_api=&selected;
 ta.snapshot=idle_touch;service_result=RISC_RUNTIME_CHECKPOINT_OK;
}

static void helper_compatibility(void){
 setup();
 assert(risc_runtime_service_checkpoint_v1(NULL)==RISC_RUNTIME_CHECKPOINT_CONTEXT);
 selected.api_version=2;
 assert(risc_runtime_service_checkpoint_v1(&selected)==RISC_RUNTIME_CHECKPOINT_CONTEXT);
 selected.api_version=1;selected.service_checkpoint=NULL;
 assert(risc_runtime_service_checkpoint_v1(&selected)==RISC_RUNTIME_CHECKPOINT_OK);
 selected.service_checkpoint=service_checkpoint;
 /* Allocate exactly the old suffix boundary. ASan catches even a speculative
  * read of service_checkpoint beyond the legacy table's allocation. */
 const size_t sizes[]={RISC_RUNTIME_CAPABILITIES_V1_SIZE,RISC_RUNTIME_INITIAL_DEFAULT_V1_SIZE,
                      RISC_RUNTIME_SERVICE_CHECKPOINT_V1_SIZE-1};
 for(unsigned i=0;i<sizeof(sizes)/sizeof(sizes[0]);i++){
  risc_runtime_api_v1 *legacy=malloc(sizes[i]);assert(legacy);
  memcpy(legacy,&selected,sizes[i]);legacy->struct_size=(uint32_t)sizes[i];
  assert(risc_runtime_service_checkpoint_v1(legacy)==RISC_RUNTIME_CHECKPOINT_OK);
  free(legacy);
 }
 assert(!service_calls);
}

static void ordinary_presentation_and_pacing(void){
 setup();rt=&selected;ready=true;hardware_brightness=67;
 assert(watch_viewport_open(&clock_viewport,&da.base));display=&clock_viewport.display;
 risc_display_surface_v1 surface={0};
 assert(frame(&surface));
 assert(present());
 assert(pending_yields>=95&&!service_calls&&!pending&&!held&&!owned);
 unsigned before=pending_yields;
 assert(pace_frame(clock_ms));
 assert(!service_calls&&pending_yields==before);
 rt->yield_ms(1);assert(!service_calls);
 watch_viewport_close(&clock_viewport);
}

static void home_case(const char *mode){
 setup();
 risc_runtime_api_v1 *legacy=NULL;
 if(!strcmp(mode,"old")){
  size_t bytes=RISC_RUNTIME_INITIAL_DEFAULT_V1_SIZE;
  legacy=malloc(bytes);assert(legacy);memcpy(legacy,&selected,bytes);
  legacy->struct_size=(uint32_t)bytes;selected_api=legacy;
 }else if(!strcmp(mode,"null"))selected.service_checkpoint=NULL;
 else if(!strcmp(mode,"busy"))service_result=RISC_RUNTIME_CHECKPOINT_BUSY;
 else if(!strcmp(mode,"context"))service_result=RISC_RUNTIME_CHECKPOINT_CONTEXT;
 else if(!strcmp(mode,"retained"))service_result=RISC_RUNTIME_CHECKPOINT_RETAINED;
 else if(!strcmp(mode,"unknown"))service_result=-99;
 else assert(!strcmp(mode,"ok"));
 app_main();
 assert(ready&&pending_yields>=95&&!pending&&!owned&&!held&&!allocations);
 if(authority_stopped){
  assert(service_calls==1&&grants==stopped_grants&&ungrants==stopped_releases);
  assert(grants>ungrants); /* No speculative provider cleanup after the fence. */
 }else{
  assert(grants==ungrants&&subscribed==unsubscribed&&!launches);
  assert(!strcmp(mode,"old")||!strcmp(mode,"null")?!service_calls:service_calls>1);
 }
 free(legacy);
}

int main(int argc,char **argv){
 assert(argc==2);
 if(!strcmp(argv[1],"helper"))helper_compatibility();
 else if(!strcmp(argv[1],"presentation"))ordinary_presentation_and_pacing();
 else home_case(argv[1]);
 printf("Home service checkpoint %ux%u %s: PASS (host-only)\n",TEST_WIDTH,TEST_HEIGHT,argv[1]);
 return 0;
}
