#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "RiscRuntimeV1.h"
#include "render.h"
void app_main(void);
static risc_runtime_api_v1 runtime;
static uint16_t pixels[240*240];
static uint32_t times[8];static unsigned count,at,health_calls,clock_calls,frames,releases,grants,reads,statuses,yields,scenario;
static char events[128],diagnostics[256];static unsigned used;static bool held;
static void event(char x){assert(used+1<sizeof(events));events[used++]=x;events[used]=0;}
static bool tick(uint32_t*out){if(at==count)return false;*out=times[at++];return true;}
static bool clock_read(uint32_t*out){++clock_calls;return tick(out);}
static bool health(risc_runtime_health_v1*out){assert(out->struct_size==sizeof(*out));++health_calls;return tick(&out->uptime_ms);}
static void yield(uint32_t ms){assert(ms==1||ms==20);++yields;assert(yields<30);}
static bool diagnostic(const char*s){assert(strlen(diagnostics)+strlen(s)+2<sizeof(diagnostics));strcat(diagnostics,s);strcat(diagnostics,";");return true;}
static bool info(void*c,risc_display_info_v1*out){(void)c;*out=(risc_display_info_v1){.width=240,.height=240,.supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565)};return true;}
static bool acquire_frame(void*c,uint32_t format,risc_display_surface_v1*out){(void)c;assert(!held&&format==RISC_DISPLAY_FORMAT_RGB565);held=true;event('A');*out=(risc_display_surface_v1){.frame=1,.pixels=pixels,.width=scenario==5?239:240,.height=240,.stride_bytes=480,.size_bytes=sizeof(pixels),.pixel_format=format};return true;}
static void release_frame(void*c,risc_display_frame_v1 frame){(void)c;assert(held&&frame==1);held=false;++releases;event('F');}
static bool submit(void*c,risc_display_frame_v1 frame,const risc_display_rect_v1*d,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*out){(void)c;assert(held&&frame==1&&!d&&!n&&o->queue_policy==RISC_DISPLAY_QUEUE_FIFO);event('S');if(scenario==4)return false;held=false;++frames;*out=frames;return true;}
static bool status(void*c,risc_display_present_token_v1 t,risc_display_present_status_v1*out){(void)c;assert(t&&t==frames);++statuses;out->state=scenario==2?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;return true;}
static bool brightness(void*c,uint16_t a,uint16_t b){(void)c;assert(a==40&&b==100);return true;}
static bool rtc_read(void*c,twatch_rtc_time_v1*out){(void)c;++reads;*out=(twatch_rtc_time_v1){2026,1,1,4,0,0,0};return scenario!=6;}
static risc_display_output_api_v1 display={.api_version=1,.struct_size=sizeof(display),.get_info=info,.acquire=acquire_frame,.release=release_frame,.submit=submit,.present_status=status,.set_brightness=brightness};
static twatch_rtc_api_v1 rtc={.api_version=2,.struct_size=sizeof(rtc),.read=rtc_read};
static bool acquire(const char*n,uint32_t v,uint64_t instance,risc_runtime_capability_v1*out){assert(!instance);if(!strcmp(n,"display.output")){assert(v==1);out->api=&display;out->slot=1;event('D');}else{assert(!strcmp(n,"rtc.clock")&&v==2);out->api=&rtc;out->slot=2;event('R');}return true;}
static bool release(risc_runtime_capability_v1*g){assert(!held);++grants;event(g->slot==2?'r':'d');g->api=NULL;return true;}
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){assert(v==1);return &runtime;}
static uint64_t digest(void){uint64_t h=1469598103934665603ull;for(unsigned i=0;i<sizeof(pixels)/sizeof(*pixels);++i){h^=pixels[i];h*=1099511628211ull;}return h;}
int main(void){
 for(scenario=0;scenario<7;++scenario){char expected_events[128]={0},expected_diag[256]={0};uint64_t expected_hash=0;unsigned expected_yields=0;
  for(unsigned mode=0;mode<3;++mode){
   at=health_calls=clock_calls=frames=releases=grants=reads=statuses=yields=used=0;held=false;events[0]=diagnostics[0]=0;memset(pixels,0,sizeof(pixels));
   count=4;times[0]=0;times[1]=1;times[2]=999;times[3]=1000;
   if(scenario==1){times[0]=UINT32_MAX-500;times[1]=UINT32_MAX-499;times[2]=498;times[3]=499;}
   if(scenario==2){count=3;times[0]=UINT32_MAX-100;times[1]=9899;times[2]=9900;}
   if(scenario==3)count=0;
   if(scenario==4||scenario==5)count=1;
   runtime=(risc_runtime_api_v1){.api_version=1,.struct_size=mode==0?RISC_RUNTIME_CAPABILITIES_V1_SIZE:sizeof(runtime),.health=health,.yield_ms=yield,.diagnostic=diagnostic,.acquire=acquire,.release=release,.monotonic_ms=mode==2?NULL:clock_read};
   app_main();assert(!held&&grants==2);assert(events[used-2]=='r'&&events[used-1]=='d');
   if(mode==1){assert(!health_calls&&clock_calls);}else{assert(!clock_calls&&health_calls);}
   if(scenario<2||scenario==6)assert(frames==2&&reads==2&&statuses==1&&releases==0);
   if(scenario==2){assert(frames==1&&statuses==2&&yields==2&&!releases);assert(strstr(diagnostics,"present-timeout"));}
   if(scenario==3)assert(!frames&&!reads&&!releases&&!yields&&clock_calls+health_calls==1);
   if(scenario==4||scenario==5)assert(!frames&&releases==1&&!yields);
   if(scenario==6)assert(strstr(diagnostics,"ready time=unset"));
   if(mode==0){strcpy(expected_events,events);strcpy(expected_diag,diagnostics);expected_hash=digest();expected_yields=yields;}
   else assert(!strcmp(events,expected_events)&&!strcmp(diagnostics,expected_diag)&&digest()==expected_hash&&yields==expected_yields);
   printf("PASS basic clock scenario=%u mode=%u frames=%u health=%u clock=%u events=%s pixels=%016llx\n",scenario,mode,frames,health_calls,clock_calls,events,(unsigned long long)digest());
  }
 }
 return 0;
}
