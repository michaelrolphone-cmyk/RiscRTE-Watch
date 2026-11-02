#include <assert.h>
#include <stdio.h>
#include <string.h>
#define BRINGUP_SHARED
#include "../apps/bringup/main.c"
static unsigned tick,updates,opens,closes,releases,pulses,stops,mreads,live_mask,case_id,event_index;
static unsigned release_fail,close_mode,retains,read_fail,optional_mode;
static bool revoked;
static risc_components_document_v1 seen;
static const risc_runtime_api_v1 runtime_test;
static bool healthy(risc_runtime_health_v1 *o){assert(!revoked);o->uptime_ms=tick*100;return tick<100;}
static void yield_test(uint32_t ms){assert(!revoked);assert(ms);tick++;assert(tick<120);}
static bool log_test(const char *s){assert(s);return true;}
static bool retained(void){retains++;revoked=true;return true;}
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return !revoked&&v==1?&runtime_test:NULL;}
static bool bread(void *c,risc_battery_sample_v1 *o){(void)c;assert(!revoked);*o=(risc_battery_sample_v1){3950,73,0};return !read_fail;}
static bool rread(void *c,twatch_rtc_time_v1 *o){(void)c;assert(!revoked);*o=(twatch_rtc_time_v1){2026,11,2,1,12,30,59};return !read_fail;}
static bool mread(void *c,twatch_accel_sample_v1 *o){(void)c;assert(!revoked);mreads++;*o=(twatch_accel_sample_v1){100,-100,1000};return !read_fail;}
static bool pulse(void *c,uint8_t effect){(void)c;assert(!revoked);assert(effect==14);pulses++;return !read_fail;}
static bool stop(void *c){(void)c;assert(!revoked);stops++;return case_id!=7;}
static risc_battery_gauge_api_v1 ba={1,sizeof(ba),NULL,bread};
static twatch_rtc_api_v1 ra={.api_version=2,.struct_size=sizeof(ra),.read=rread};
static twatch_motion_api_v1 ma={.api_version=1,.struct_size=sizeof(ma),.read=mread};
static twatch_haptic_api_v1 ha={1,sizeof(ha),NULL,pulse,stop};
static int sopen(void *c,const risc_components_document_v1 *d,const risc_scene_navigation_v1 *n,uint64_t *out){
 (void)c;assert(!revoked);assert(n->depth==1);seen=*d;opens++;*out=9;return RISC_SCENE_OK;
}
static int supdate(void *c,uint64_t s,const risc_components_document_v1 *d){
 (void)c;assert(!revoked&&s==9);assert(d->revision>seen.revision);seen=*d;updates++;
 if(case_id==8){revoked=true;return RISC_SCENE_RETAINED;}return RISC_SCENE_OK;
}
static int ssnapshot(void *c,uint64_t s,risc_scene_navigation_v1 *n,uint32_t *f){
 (void)c;(void)n;assert(!revoked&&s==9);
 *f=tick<2?RISC_SCENE_PRESENTING:0;
 if(case_id==9&&tick==3)return RISC_SCENE_RETAINED;
 return RISC_SCENE_OK;
}
static int snext(void *c,uint64_t s,risc_scene_event_v1 *e){
 (void)c;assert(!revoked&&s==9);
 /* Every action follows a neutral cycle; inject stale/invalid/disabled events. */
 if(tick==0||tick==3||tick==5||tick==7||tick==9||tick==11||tick==13||tick==15){
  *e=(risc_scene_event_v1){sizeof(*e),RISC_SCENE_ACTION_EVENT,seen.revision,1,1,0,++event_index};
  if(tick==3)e->document_revision--;
  if(tick==5){e->node=99;e->action=2;}
  if(tick==7){e->node=4;e->action=4;}
  if(tick==11){e->node=2;e->action=2;}
  if(tick==13){e->node=2;e->action=2;}
  if(tick==15){e->node=3;e->action=3;}
  return RISC_SCENE_OK;
 }
 return RISC_SCENE_IDLE;
}
static int sclose(void *c,uint64_t s){
 (void)c;assert(!revoked&&s==9);closes++;
 if(close_mode==1&&closes<3)return RISC_SCENE_AGAIN;
 return close_mode==2?RISC_SCENE_RETAINED:RISC_SCENE_OK;
}
static int configure_test(void *c,uint64_t s,uint32_t f){(void)c;assert(s==9&&f==0);return RISC_SCENE_OK;}
static risc_scene_components_api_v1 sc={
 .lifecycle={.base={.api_version=1,.struct_size=sizeof(sc),.next=snext,.snapshot=ssnapshot,.close=sclose},.configure=configure_test},
 .tag=RISC_COMPONENTS_TAG,.version=1,.open=sopen,.update=supdate};
static bool acquire_test(const char *n,uint32_t v,uint64_t inst,risc_runtime_capability_v1 *g){
 assert(!revoked&&inst==0);const void *api=NULL;unsigned slot=0;
 if(!strcmp(n,"ui.scene")){assert(v==1);api=&sc;slot=1;}
 else if(!strcmp(n,"board.battery")){api=&ba;slot=2;}
 else if(!strcmp(n,"rtc.clock")){assert(v==2);api=&ra;slot=3;}
 else if(!strcmp(n,"motion.accel")){api=&ma;slot=4;}
 else if(!strcmp(n,"haptic.effect")){api=&ha;slot=5;}
 else {assert(!strcmp(n,"display.output")||!strcmp(n,"input.touch.raw"));return false;}
 if(optional_mode==1&&slot>1)return false;
 assert(!(live_mask&(1u<<slot)));live_mask|=1u<<slot;
 *g=(risc_runtime_capability_v1){sizeof(*g),slot,1,api};return true;
}
static bool release_test(risc_runtime_capability_v1 *g){
 assert(!revoked);assert(live_mask&(1u<<g->slot));
 if(release_fail==g->slot)return false;
 live_mask&=~(1u<<g->slot);releases++;return true;
}
static const risc_runtime_api_v1 runtime_test={.api_version=1,.struct_size=sizeof(runtime_test),
 .health=healthy,.yield_ms=yield_test,.diagnostic=log_test,.acquire=acquire_test,
 .release=release_test,.retain_invocation=retained};
int main(void){
 for(case_id=0;case_id<10;case_id++){
  tick=updates=opens=closes=releases=pulses=stops=mreads=live_mask=retains=event_index=0;
  revoked=false;release_fail=case_id==6?5:0;close_mode=case_id==4?1:case_id==5?2:0;
  read_fail=case_id==3;optional_mode=case_id==1?1:0;
  ba.api_version=case_id==2?99:1;ra.api_version=case_id==2?99:2;
  ma.api_version=case_id==2?99:1;ha.api_version=case_id==2?99:1;
  assert(app_module_init()==0);app_main();app_module_fini();
  assert(opens==1);
  if(case_id<5){assert(!live_mask&&!revoked);assert(closes==(case_id==4?3u:1u));
   assert(pulses==(case_id==1||case_id==2?0u:2u));
   assert(stops==(case_id==1||case_id==2?0u:1u));
   assert(updates>0);
  }else {assert(revoked);if(case_id!=8)assert(retains==1);}
  printf("shared Bringup case %u PASS\n",case_id);
 }
 return 0;
}
