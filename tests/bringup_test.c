#include "RiscRuntimeV1.h"
#include "WatchViewport.h"
#include "twatch_caps.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static uint32_t now_,width_,height_,live_,frames_,pulses_,motion_reads_,event_,ready_,ticks_;
static bool borrowed_,subscribed_,pending_,optional_;
static uint16_t *pixels_;
static bool dinfo(void*c,risc_display_info_v1*out){(void)c;*out=(risc_display_info_v1){.api_version=1,.struct_size=sizeof(*out),.width=width_,.height=height_,.supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565)};return true;}
static bool dacq(void*c,uint32_t f,risc_display_surface_v1*out){(void)c;assert(f==RISC_DISPLAY_FORMAT_RGB565);if(borrowed_||pending_)return false;borrowed_=true;*out=(risc_display_surface_v1){frames_+1,pixels_,width_,height_,width_*2,width_*height_*2,RISC_DISPLAY_FORMAT_RGB565};return true;}
static void drel(void*c,uint64_t t){(void)c;assert(borrowed_&&t);borrowed_=false;}
static bool dsub(void*c,uint64_t t,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,uint64_t*out){(void)c;(void)r;(void)o;assert(borrowed_&&t&&n==0);borrowed_=false;pending_=true;ready_=now_+3;*out=++frames_;return true;}
static bool dstatus(void*c,uint64_t t,risc_display_present_status_v1*out){(void)c;assert(t&&t<=frames_);if(now_>=ready_)pending_=false;out->state=pending_?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;return true;}
static const risc_display_output_api_v1 disp_={.api_version=1,.struct_size=sizeof(disp_),.get_info=dinfo,.acquire=dacq,.release=drel,.submit=dsub,.present_status=dstatus};
static uint64_t tsub(void*c){(void)c;assert(!subscribed_);subscribed_=true;return 1;}
static bool tunsub(void*c,uint64_t t){(void)c;assert(subscribed_&&t==1);subscribed_=false;return true;}
static bool tpoll(void*c,size_t n){(void)c;assert(n==1);ticks_++;return true;}
static int32_t tnext(void*c,uint64_t id,risc_touch_event_v1*out){(void)c;assert(id==1&&subscribed_);
 if(event_>=6||ticks_<20+event_*15)return 0;
 unsigned x=event_<2?180:event_<4?60:120,y=event_<4?185:220;
 unsigned side=width_<height_?width_:height_;
 *out=(risc_touch_event_v1){.id=1,.sequence=event_+1,.kind=(event_%2)?RISC_TOUCH_EVENT_UP:RISC_TOUCH_EVENT_DOWN,.x=(width_-side)/2+(x*side+239)/240,.y=(height_-side)/2+(y*side+239)/240};event_++;return 1;
}
static bool tsnap(void*c,risc_touch_snapshot_v1*out){(void)c;*out=(risc_touch_snapshot_v1){.width=width_,.height=height_};return true;}
static const risc_touch_api_v1 touch_={1,sizeof(touch_),NULL,tsub,tunsub,tpoll,tnext,tsnap};
static bool bread(void*c,risc_battery_sample_v1*out){(void)c;*out=(risc_battery_sample_v1){3950,73,0};return true;}
static const risc_battery_gauge_api_v1 battery_={1,sizeof(battery_),NULL,bread};
static bool rread(void*c,twatch_rtc_time_v1*out){(void)c;(void)out;return false;}
static const twatch_rtc_api_v1 rtc_={.api_version=2,.struct_size=sizeof(rtc_),.read=rread};
static bool mread(void*c,twatch_accel_sample_v1*out){(void)c;*out=(twatch_accel_sample_v1){100,-100,1000};motion_reads_++;return true;}
static const twatch_motion_api_v1 motion_={.api_version=1,.struct_size=sizeof(motion_),.read=mread};
static bool pulse(void*c,uint8_t effect){(void)c;assert(effect==14);pulses_++;return true;}
static bool stop(void*c){(void)c;return true;}
static const twatch_haptic_api_v1 haptic_={1,sizeof(haptic_),NULL,pulse,stop};
static bool acq(const char*name,uint32_t version,uint64_t instance,risc_runtime_capability_v1*g){
 const char*names[]={"display.output","input.touch.raw","board.battery","rtc.clock","motion.accel","haptic.effect"};
 const void*tables[]={&disp_,&touch_,&battery_,&rtc_,&motion_,&haptic_};assert(instance==0);
 for(unsigned i=0;i<6;i++)if(!strcmp(name,names[i])){assert(version==(i==3?2u:1u));if(!optional_&&i>1)return false;
  if(i>3)assert(frames_>0);assert(!(live_&(1u<<i)));live_|=1u<<i;g->api=tables[i];g->slot=i+1;g->generation=1;return true;}
 assert(0);return false;
}
static bool rel(risc_runtime_capability_v1*g){assert(g->slot>0&&g->slot<=6&&(live_&(1u<<(g->slot-1))));if(g->slot==1)assert(!borrowed_&&!pending_);live_&=~(1u<<(g->slot-1));return true;}
static bool health_(risc_runtime_health_v1*h){h->uptime_ms=now_;return now_<30000;}
static void yield_(uint32_t ms){now_+=ms;}
static bool log_(const char*s){assert(s);return true;}
static risc_runtime_api_v1 rt_={.api_version=1,.struct_size=sizeof(rt_),.health=health_,.yield_ms=yield_,.diagnostic=log_,.acquire=acq,.release=rel};
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t version){return version==1?&rt_:NULL;}
#include "../apps/bringup/main.c"
int main(void){
 for(unsigned dims=0;dims<3;dims++)for(unsigned optional=0;optional<2;optional++){
  width_=dims==0?240:dims==1?410:502;height_=dims==0?240:dims==1?502:410;
  now_=live_=frames_=pulses_=motion_reads_=event_=ready_=ticks_=0;borrowed_=subscribed_=pending_=false;optional_=optional;
  pixels_=calloc(width_*height_,2);assert(pixels_);app_main();assert(!live_&&!borrowed_&&!subscribed_&&!pending_);
  assert(frames_>1&&event_==6);assert(pulses_==optional);assert(!!motion_reads_==!!optional);
  free(pixels_);
 }
 puts("Bring-up child: actual entry point, three geometries, missing optional hardware, delayed frames, touch actions and complete return cleanup PASS");
}
