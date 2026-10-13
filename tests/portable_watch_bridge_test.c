/* Execute production bridge translation units against strict logical services.
 * Each terminal case gets a fresh process; no test resets production fences. */
#include "PortableNavigation.h"
#include "PortableAppSleep.h"
#include "RiscPowerKeyV1.h"
#include "RiscPowerSleepV1.h"
#include "RiscDisplayPowerV1.h"
#include "PortableTapSettings.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
bool portable_input_navigation_retained(void);
static unsigned scenario,width,height,stride,calls,grants,acquires,releases,frames,submits,queries,yields;
static unsigned sleeps,deep_calls,prepares,resumes,motion_prepares,motion_resumes;
#ifdef PORTABLE_ALARM_CLIENT
static unsigned alarm_prepares,alarm_resumes,refreshes;
#endif
static unsigned allocs,frees,retains,key_reads,events,brightness=67;
static bool retained,healthy=true,frame_live,pending,timer_wake,fault_used;
static uint8_t memory[502*(410*2+18)+64],original[502*410*2];
static uint32_t last_ms;
static unsigned io(void){assert(!retained);return ++calls;}
void *__real_malloc(size_t);
void __real_free(void *);
void *__wrap_malloc(size_t n){assert(!retained);allocs++;return __real_malloc(n);}
void __wrap_free(void *p){assert(!retained);if(p)frees++;__real_free(p);}
static bool health(risc_runtime_health_v1 *out){io();assert(out->struct_size==sizeof(*out));return healthy;}
static bool retain(void){assert(!retained);retained=true;retains++;return true;}
static bool diagnostic(const char *s){io();assert(s);if(scenario==48){healthy=false;return false;}return true;}
static void yield(uint32_t n){io();assert(n==1);yields++;if(scenario==53)healthy=false;}
static bool key_events(void *c,uint32_t *out){(void)c;io();key_reads++;if(scenario==104){healthy=false;return false;}if(scenario==105)return false;*out=events;events=0;return true;}
static risc_power_key_api_v1 key={1,sizeof(key),NULL,key_events};
static bool prepare(void *c){(void)c;io();prepares++;assert(brightness==0&&!frame_live&&!pending);if(scenario==12)return false;if(scenario==13){healthy=false;return false;}return true;}
static bool resume(void *c){(void)c;io();resumes++;return scenario!=27;}
static bool wake_pending(void *c,bool *out){(void)c;io();*out=scenario==24;return true;}
static int32_t light(void *c,uint32_t ms,risc_light_sleep_result_v1 *out){
 (void)c;io();sleeps++;last_ms=ms;assert(brightness==0&&!pending&&!frame_live);
 for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width*2;x++)assert(memory[32+y*stride+x]==0);
 if(scenario==14)return RISC_LIGHT_SLEEP_RETAINED;
 if(scenario==15)return RISC_LIGHT_SLEEP_CONTEXT;
 if(scenario==16)return RISC_LIGHT_SLEEP_ACTIVE_WAKE;
 out->wake_cause=timer_wake?RISC_LIGHT_SLEEP_WAKE_TIMER:RISC_LIGHT_SLEEP_WAKE_GPIO;return 0;
}
static int32_t untimed(void *c,risc_light_sleep_result_v1 *out){return light(c,0,out);}
static int32_t deep(void *c){(void)c;io();deep_calls++;assert(!pending&&!frame_live);return scenario==21?0:scenario==22?RISC_DEEP_SLEEP_CONTEXT:RISC_DEEP_SLEEP_ACTIVE_WAKE;}
static int32_t timed_deep(void *c,uint32_t ms){last_ms=ms;return deep(c);}
static risc_power_sleep_api_v1 power={1,sizeof(power),NULL,prepare,resume,untimed,deep,light,wake_pending,timed_deep,light,timed_deep};
static int32_t panel_deep(void *c){(void)c;io();return scenario==20?RISC_DEEP_SLEEP_CONTEXT:0;}
static int32_t panel_resume_status(void *c){(void)c;io();resumes++;return scenario==17?RISC_DEEP_SLEEP_RETAINED:scenario==18?RISC_DEEP_SLEEP_CONTEXT:0;}
static risc_display_power_api_v1 panel={1,sizeof(panel),NULL,prepare,resume,panel_deep,panel_resume_status};
static int32_t kv_get(void *c,const char *k,void *buf,uint32_t cap,uint32_t *n){
 (void)c;(void)buf;(void)cap;io();assert(!strcmp(k,PORTABLE_TAP_KEY));*n=0;
 if(scenario==36)return RISC_KEY_VALUE_CONTEXT;
 if(scenario==37)return RISC_KEY_VALUE_IO;
 return RISC_KEY_VALUE_NOT_FOUND;
}
static int32_t kv_put(void*c,const char*k,const void*b,uint32_t n){(void)c;(void)k;(void)b;(void)n;assert(!"unexpected write");return 0;}
static risc_key_value_v1 kv={1,sizeof(kv),NULL,kv_get,kv_put};
static bool motion_prepare(void *c){(void)c;io();motion_prepares++;return scenario!=38;}
static bool motion_pending(void *c,bool *out){(void)c;io();*out=scenario==39;return true;}
static bool motion_resume(void *c){(void)c;io();motion_resumes++;return scenario!=40;}
static bool tap_info(void *c,twatch_tap_info_v1 *out){(void)c;io();*out=(twatch_tap_info_v1){sizeof(*out),99,0,50,7};return true;}
static bool tap_configure(void *c,uint16_t n){(void)c;io();assert(n==7);return scenario!=41;}
static bool observe_begin(void *c,uint16_t n){(void)c;(void)n;assert(!"unexpected observe");return false;}
static bool observe(void *c,twatch_tap_observation_v1 *out){(void)c;(void)out;assert(!"unexpected observe");return false;}
static twatch_motion_api_v1 motion={.api_version=1,.struct_size=sizeof(motion),.prepare_wake=motion_prepare,
 .wake_pending=motion_pending,.resume_wake=motion_resume,.tap_info=tap_info,.tap_configure=tap_configure,
 .tap_observe_begin=observe_begin,.tap_observe=observe};
static bool acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *out){
 io();acquires++;assert(version==1 && out->struct_size==sizeof(*out));const void *api=NULL;
 if(scenario==47||scenario==111){out->struct_size=0;return false;}
 if(!strcmp(name,RISC_POWER_KEY_CAPABILITY)){assert(!instance);api=&key;if(scenario==101)return false;}
 else if(!strcmp(name,RISC_POWER_SLEEP_CAPABILITY)){assert(!instance);api=&power;if(scenario==1)return false;}
 else if(!strcmp(name,RISC_DISPLAY_POWER_CAPABILITY)){assert(!instance);api=&panel;if(scenario==2)return false;}
 else if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY)){assert(instance==1);api=&kv;if(scenario==30)return false;}
 else if(!strcmp(name,TWATCH_MOTION_CAPABILITY)){assert(!instance);api=&motion;if(scenario==31)return false;}
 else assert(!"undeclared/private capability");
 *out=(risc_runtime_capability_v1){sizeof(*out),acquires,1,scenario==109?NULL:api};grants++;
 if(scenario==3 || (scenario==4 && api==&panel) || scenario==102 || scenario==109 ||
    (scenario==32 && api==&kv) || (scenario==33 && api==&motion))return false;
 return true;
}
static bool release(risc_runtime_capability_v1 *g){
 io();releases++;assert(g->api && grants);
 if(scenario==110){memset(g,0,sizeof(*g));return false;}
 if(scenario==5 || scenario==103 || (scenario==34&&g->api==&kv) || (scenario==35&&g->api==&motion))return false;
 grants--;return true;
}
static risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.health=health,.yield_ms=yield,
 .diagnostic=diagnostic,.acquire=acquire,.release=release,.retain_invocation=retain};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){assert(v==1);return &runtime;}
unsigned portable_quick_brightness(void){assert(!retained);return 67;}
uint32_t portable_quick_sleep_light_ms(void){assert(!retained);return 123456;}
static bool info(void*c,risc_display_info_v1*out){(void)c;io();*out=(risc_display_info_v1){.api_version=1,.struct_size=sizeof(*out),.width=width,.height=height};return true;}
static bool frame_acquire(void*c,uint32_t format,risc_display_surface_v1*out){
 (void)c;io();assert(format==RISC_DISPLAY_FORMAT_RGB565&&!frame_live&&!pending);
 if(scenario==51&&frames==1&&!fault_used){fault_used=true;return false;}
 frames++;frame_live=true;
 *out=(risc_display_surface_v1){frames,memory+32,width,height,stride,stride*height,RISC_DISPLAY_FORMAT_RGB565};
 if(scenario==6)out->height--;
 if(scenario==7)out->stride_bytes=width*2-1;
 if(scenario==8)out->size_bytes=(height-1)*stride+width*2-1;
 if(scenario==46)out->size_bytes=(height-1)*stride+width*2;
 if(scenario==9)out->stride_bytes=UINT32_MAX;
 if(scenario==10){healthy=false;return false;}
 if(scenario==11&&frames==2)out->height++;
 return true;
}
static void frame_release(void*c,risc_display_frame_v1 frame){(void)c;io();assert(frame_live&&frame==frames);frame_live=false;}
static bool submit(void*c,risc_display_frame_v1 frame,const risc_display_rect_v1 *damage,size_t n,
 const risc_display_present_options_v1 *options,risc_display_present_token_v1 *token){
 (void)c;(void)options;io();assert(frame_live&&frame==frames&&!damage&&!n);
 if(scenario==49&&!fault_used){fault_used=true;return false;}
 frame_live=false;pending=true;submits++;queries=0;*token=submits;return true;
}
static bool status(void*c,risc_display_present_token_v1 token,risc_display_present_status_v1*out){
 (void)c;io();assert(pending&&token==submits);queries++;
 if(scenario==25)return false;
 if(scenario==26){out->state=RISC_DISPLAY_PRESENT_ACTIVE;return true;}
 if(queries<3){out->state=RISC_DISPLAY_PRESENT_ACTIVE;return true;}
 pending=false;out->state=scenario==50&&submits==1?RISC_DISPLAY_PRESENT_FAILED:RISC_DISPLAY_PRESENT_COMPLETE;return true;
}
static bool set_brightness(void*c,uint16_t value,uint16_t max){(void)c;io();assert(max==100);if((scenario==52||scenario==54)&&!fault_used){fault_used=true;if(scenario==54)brightness=value;return false;}if(value){assert(!pending);for(unsigned y=0;y<height;y++)assert(!memcmp(memory+32+y*stride,original+y*width*2,width*2));}brightness=value;return true;}
static risc_display_output_api_v1 display={1,sizeof(display),NULL,info,frame_acquire,frame_release,submit,status,NULL,set_brightness};
#ifdef PORTABLE_ALARM_CLIENT
static int32_t alarm_prepare(void*c,alarm_sleep_v1*out){(void)c;io();alarm_prepares++;*out=(alarm_sleep_v1){sizeof(*out),alarm_prepares,1000,scenario==43?1010:0};if(scenario==42)return ALARM_BUSY;if(scenario==44){healthy=false;return ALARM_RTC;}return 0;}
static int32_t alarm_step(void*c){(void)c;io();return 0;}
static int32_t alarm_status(void*c,alarm_status_v1*out){(void)c;io();out->state=ALARM_STATE_READY;return 0;}
static int32_t alarm_refresh(void*c){(void)c;io();refreshes++;return 0;}
static int32_t alarm_resume(void*c,const alarm_sleep_v1*out){(void)c;io();assert(out->snapshot==alarm_prepares);alarm_resumes++;return 0;}
static alarm_service_sleep_v1 alarms={{1,sizeof(alarms),NULL,alarm_status,alarm_step,alarm_refresh,NULL,alarm_prepare,NULL},alarm_resume};
#endif
static void check_guards(void){
 for(unsigned x=0;x<32;x++)assert(memory[x]==0xa5);
 for(unsigned y=0;y<height;y++)for(unsigned x=width*2;x<stride;x++)assert(memory[32+y*stride+x]==0xa5);
 for(unsigned x=0;x<32;x++)assert(memory[32+height*stride+x]==0xa5);
}
static int sleep_once(void){
#ifdef PORTABLE_ALARM_CLIENT
 return portable_app_alarm_sleep(&runtime,&display,NULL,(scenario==45||scenario==48)?NULL:&alarms.base);
#else
 return portable_app_sleep(&runtime,&display,NULL);
#endif
}
static void navigation_test(void){
 if(scenario==106)key.api_version=2;
 if(scenario==107)key.events=NULL;
 if(scenario==108)key.struct_size=8;
 const risc_input_navigation_api_v1 *nav=portable_input_navigation_open(&runtime);
 if(scenario>=106&&scenario<=108){assert(!nav&&!retained&&!grants);return;}
 if(scenario==101){assert(!nav&&!retained&&!grants);return;}
 if(scenario==102||scenario==109||scenario==111){assert(!nav&&retained);return;}
 assert(nav);
 if(scenario==103||scenario==110){portable_input_navigation_close(&runtime);assert(retained);return;}
 risc_input_foreground_v1 touch={"input.touch.raw",1},modal={RISC_POWER_KEY_CAPABILITY,1};
 events=RISC_POWER_KEY_SHORT_PRESS;
 bool fg=nav->foreground(nav->context,&touch,1);
 if(scenario==104){assert(!fg&&retained);return;}
 if(scenario==105){assert(!fg&&!retained);portable_input_navigation_close(&runtime);return;}
 assert(fg&&!events);
 risc_input_navigation_frame_v1 out;
 events=RISC_POWER_KEY_SHORT_PRESS;assert(nav->poll(NULL,&out)&&!out.pressed);
 assert(nav->poll(NULL,&out)&&!out.pressed);
 events=RISC_POWER_KEY_SHORT_PRESS;assert(nav->poll(NULL,&out)&&out.pressed==RISC_NAV_BACK&&out.released==RISC_NAV_BACK&&!out.buttons);
 assert(nav->poll(NULL,&out));events=RISC_POWER_KEY_LONG_PRESS;assert(nav->poll(NULL,&out)&&!out.pressed);
 events=RISC_POWER_KEY_SHORT_PRESS;assert(nav->poll(NULL,&out)&&!out.pressed);
 assert(nav->poll(NULL,&out));events=3;assert(nav->poll(NULL,&out)&&!out.pressed);
 assert(nav->foreground(NULL,&modal,1));unsigned reads=key_reads;events=2;
 assert(nav->reset(NULL)&&nav->poll(NULL,&out)&&!out.pressed&&events==2&&key_reads==reads);
 assert(nav->foreground(NULL,&touch,1)&&!events);assert(nav->poll(NULL,&out));
 events=2;assert(nav->reset(NULL)&&!events);events=2;assert(nav->poll(NULL,&out)&&!out.pressed);
 for(unsigned i=0;i<4;i++){portable_input_navigation_close(&runtime);portable_input_navigation_close(&runtime);assert(!grants);nav=portable_input_navigation_open(&runtime);assert(nav);}
 portable_input_navigation_close(&runtime);assert(!grants);
}
int main(int argc,char **argv){
 assert(argc==4);scenario=(unsigned)atoi(argv[1]);width=(unsigned)atoi(argv[2]);height=(unsigned)atoi(argv[3]);
 if(scenario>=100){navigation_test();if(retained){unsigned before=calls;assert(portable_input_navigation_retained());portable_input_navigation_close(&runtime);assert(!portable_input_navigation_open(&runtime)&&calls==before);}printf("navigation %u passed\n",scenario);return 0;}
 stride=width*2+18;memset(memory,0xa5,sizeof(memory));
 for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width*2;x++)memory[32+y*stride+x]=original[y*width*2+x]=(uint8_t)(1+(x+y)%254);
 timer_wake=scenario>=19&&scenario<=24;
 int rc=sleep_once();check_guards();
 bool terminal=scenario==3||scenario==4||scenario==5||scenario==10||scenario==13||scenario==14||scenario==15||scenario==17||scenario==18||scenario==20||scenario==21||scenario==22||scenario==25||scenario==26||scenario==47||scenario==48||scenario==53;
#ifdef PORTABLE_MOTION_WAKE
 terminal=terminal||scenario==32||scenario==33||scenario==34||scenario==35||scenario==36||scenario==40;
#endif
#ifdef PORTABLE_ALARM_CLIENT
 terminal=terminal||scenario==44;
#endif
 if(terminal){assert(rc==-2&&retained&&retains==1&&frees==0);unsigned before=calls;assert(sleep_once()==-2&&calls==before);}
 else {
  assert(!retained&&!grants&&!frame_live&&!pending&&allocs==frees);
  if(scenario==0||scenario==24||scenario==31||scenario==37||scenario==39||scenario==43||scenario==45||scenario==46){assert(rc==1&&brightness==67&&submits==2);}
  if(scenario==0) {
#ifdef PORTABLE_LOW_BATTERY
   assert(last_ms==123456);
#else
#ifdef PORTABLE_ALARM_CLIENT
   assert(last_ms==300000);
#else
   assert(last_ms==300000);
#endif
#endif
#ifdef PORTABLE_ALARM_CLIENT
   assert(alarm_prepares==1&&alarm_resumes==1&&refreshes==1);
#endif
#ifdef PORTABLE_MOTION_WAKE
   assert(motion_prepares==1&&motion_resumes==1);
#endif
  }
  if(scenario==43)assert(last_ms==10000);
  if(scenario==0)for(unsigned n=0;n<4;n++){assert(sleep_once()==1&&brightness==67&&!grants);check_guards();}
  if(scenario>=6&&scenario<=9)assert(rc==-1&&!sleeps&&!submits);
  if(scenario==11 || (scenario>=49&&scenario<=52) || scenario==54)assert(rc==-1&&!sleeps&&brightness==67);
  if(scenario==12||scenario==16||scenario==19||scenario==23||scenario==38||scenario==41||scenario==42)assert(rc==0&&brightness==67);
  if(scenario==1||scenario==2||scenario==30)assert(rc==0&&!sleeps);
  if(scenario==27)assert(rc==-1);
 }
 printf("sleep %u %ux%u passed: rc=%d calls=%u submits=%u frees=%u retained=%u\n",scenario,width,height,rc,calls,submits,frees,retains);
 return 0;
}
