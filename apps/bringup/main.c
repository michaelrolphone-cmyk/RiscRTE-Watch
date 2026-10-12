/* Hardware-neutral first-boot diagnostics. All physical access is through
 * generic capabilities. A failed optional test never prevents the first frame.
 * Home owns normal navigation; returning from this child re-enters Home. */
#include "RiscRuntimeV1.h"
#include "WatchViewport.h"
#include "RiscBatteryGaugeV1.h"
#include "RiscPowerKeyV1.h"
#include "twatch_calendar.h"
#include <stdio.h>
#include <string.h>

static const uint8_t numbers[10][5]={
 {62,81,73,69,62},{0,66,127,64,0},{98,81,73,73,70},{34,65,73,73,54},{24,20,18,127,16},
 {39,69,69,69,57},{60,74,73,73,48},{1,113,9,5,3},{54,73,73,73,54},{6,73,73,41,30}};
static const uint8_t letters[26][5]={
 {126,9,9,9,126},{127,73,73,73,54},{62,65,65,65,34},{127,65,65,34,28},
 {127,73,73,73,65},{127,9,9,9,1},{62,65,73,73,122},{127,8,8,8,127},
 {0,65,127,65,0},{32,64,65,63,1},{127,8,20,34,65},{127,64,64,64,64},
 {127,2,12,2,127},{127,4,8,16,127},{62,65,65,65,62},{127,9,9,9,6},
 {62,65,81,33,94},{127,9,25,41,70},{70,73,73,73,49},{1,1,127,1,1},
 {63,64,64,64,63},{31,32,64,32,31},{63,64,56,64,63},{99,20,8,20,99},
 {3,4,120,4,3},{97,81,73,69,67}};
static uint8_t glyph(char c,unsigned x){
 if(c>='A'&&c<='Z')return letters[c-'A'][x];
 if(c>='0'&&c<='9')return numbers[c-'0'][x];
 if(c=='-')return 8;if(c==':')return x==2?36:0;
 if(c=='.')return x==2?64:0;if(c=='/')return (uint8_t)(64u>>x);
 if(c=='%'){const uint8_t a[]={99,19,8,100,99};return a[x];}return 0;
}
static void box(risc_display_surface_v1 *s,unsigned x,unsigned y,unsigned w,unsigned h,uint16_t c){
 for(unsigned yy=y;yy<y+h&&yy<s->height;yy++){
  uint8_t *p=(uint8_t*)s->pixels+yy*s->stride_bytes;
  for(unsigned xx=x;xx<x+w&&xx<s->width;xx++){p[xx*2]=(uint8_t)c;p[xx*2+1]=(uint8_t)(c>>8);}
 }
}
static void text(risc_display_surface_v1 *s,unsigned x,unsigned y,const char *v,unsigned scale,uint16_t c){
 for(unsigned k=0;v[k]&&k<36;k++)for(unsigned col=0;col<5;col++)for(unsigned row=0;row<7;row++)
  if(glyph(v[k],col)&(1u<<row))box(s,x+(k*6+col)*scale,y+row*scale,scale,scale,c);
}
static bool render(risc_display_surface_v1 *s,uint32_t uptime,const char *battery,const char *rtc,
 const char *motion,const char *status,uint16_t tx,uint16_t ty,unsigned touches){
 if(!s||!s->pixels||s->width!=240||s->height!=240||s->pixel_format!=RISC_DISPLAY_FORMAT_RGB565||
    s->stride_bytes<480||s->stride_bytes>UINT32_MAX/240||s->size_bytes<(size_t)s->stride_bytes*240)return false;
 box(s,0,0,240,240,0x0841);text(s,12,12,"HARDWARE TEST",2,0xffff);
 text(s,12,32,"RISC RTE  BOOT 0.1.0",1,0x7bef);
 box(s,12,45,72,4,0xf800);box(s,84,45,72,4,0x07e0);box(s,156,45,72,4,0x001f);
 char b[40];snprintf(b,sizeof(b),"UPTIME %lu S",(unsigned long)uptime);text(s,12,58,b,1,0xffff);
 text(s,12,74,battery,1,0xffff);text(s,12,90,rtc,1,0xffff);
 snprintf(b,sizeof(b),"TOUCH %u  X%u Y%u",touches,tx,ty);text(s,12,106,b,1,0x07ff);
 text(s,12,122,motion,1,0xffff);text(s,12,141,status,1,0xffe0);
 text(s,12,154,"RADIO AUDIO SD SLEEP OFF",1,0x7bef);
 box(s,10,172,105,28,0x2124);box(s,125,172,105,28,0x2124);
 text(s,27,182,"MOTION",1,0xffff);text(s,151,182,"VIBRATE",1,0xffff);
 box(s,10,209,220,25,0x032c);text(s,80,218,"BACK TO HOME",1,0xffff);return true;
}
static bool get(const risc_runtime_api_v1 *rt,const char *cap,unsigned version,risc_runtime_capability_v1 *g){
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return rt->acquire(cap,version,0,g);
}
#define VALID(a,v,extent) ((a)&&(a)->api_version==(v)&&(a)->struct_size>=(extent))
__attribute__((visibility("default"))) void app_main(void){
 const risc_runtime_api_v1 *rt=risc_runtime_get_api(1);
 if(!rt||rt->api_version!=1||rt->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE||!rt->health||!rt->yield_ms||!rt->diagnostic||!rt->acquire||!rt->release)return;
 risc_runtime_capability_v1 g[6]={{0}};bool live[6]={0};watch_viewport v={0};
 const risc_touch_api_v1 *touch=NULL;const risc_battery_gauge_api_v1 *battery=NULL;
 const twatch_rtc_api_v1 *rtc=NULL;const twatch_motion_api_v1 *motion=NULL;const twatch_haptic_api_v1 *haptic=NULL;
 uint64_t sub=0;bool pending=false,dirty=true,leave=false,down=false,motion_tried=false;
 unsigned action=0,touches=0;uint8_t contact=0;uint16_t tx=0,ty=0,dx=0,dy=0;
 uint32_t last=0,started=0;uint64_t token=0;
 char battery_text[40]="BATTERY UNAVAILABLE",rtc_text[40]="RTC UNSET",motion_text[40]="MOTION NOT STARTED",status_text[40]="TAP BUTTONS TO TEST";
 live[0]=get(rt,"display.output",1,&g[0]);if(!live[0]||!watch_viewport_open(&v,g[0].api))goto cleanup;
 live[1]=get(rt,"input.touch.raw",1,&g[1]);touch=g[1].api;
 if(!live[1]||!VALID(touch,1,sizeof(*touch))||!touch->subscribe||!touch->unsubscribe||!touch->poll||!touch->next||!touch->snapshot)goto cleanup;
 sub=touch->subscribe(touch->context);if(!sub)goto cleanup;
 risc_touch_snapshot_v1 snap={0};if(!touch->snapshot(touch->context,&snap))goto cleanup;
 live[2]=get(rt,"board.battery",1,&g[2]);battery=g[2].api;
 if(!VALID(battery,1,sizeof(*battery))||!battery->read)battery=NULL;
 live[3]=get(rt,"rtc.clock",2,&g[3]);rtc=g[3].api;
 if(!VALID(rtc,2,sizeof(*rtc))||!rtc->read)rtc=NULL;
 rt->diagnostic("WATCH_TEST opened display=ready touch=ready optional-tests=idle");
 for(;;){
  risc_runtime_health_v1 health={.struct_size=sizeof(health)};if(!rt->health(&health))break;
  if(pending){risc_display_present_status_v1 s={0};
   if(!v.display.present_status(v.display.context,token,&s)||s.state==RISC_DISPLAY_PRESENT_FAILED||s.state==RISC_DISPLAY_PRESENT_SUPERSEDED)break;
   if(s.state==RISC_DISPLAY_PRESENT_COMPLETE)pending=false;
   else if((uint32_t)(health.uptime_ms-started)>10000){rt->diagnostic("WATCH_TEST display=timeout");break;}
  }
  if(!touch->poll(touch->context,1)){down=false;strcpy(status_text,"TOUCH I/O ERROR");dirty=true;}
  for(unsigned i=0;i<8;i++){risc_touch_event_v1 e={0};int n=touch->next(touch->context,sub,&e);
   if(!n)break;if(n<0){down=false;if(!touch->snapshot(touch->context,&snap))leave=true;break;}
   tx=e.x;ty=e.y;uint16_t x=tx,y=ty;if(!watch_viewport_map(&v,&x,&y)){down=false;continue;}dirty=true;
   if(e.kind==RISC_TOUCH_EVENT_DOWN){down=true;contact=e.id;dx=x;dy=y;touches++;}
   if(e.kind==RISC_TOUCH_EVENT_UP&&down&&contact==e.id){down=false;
    int ax=(int)x-dx,ay=(int)y-dy;
    if(ax>-16&&ax<16&&ay>-16&&ay<16){if(y>=209)leave=true;else if(y>=172&&y<200)action=x<120?1:2;}
   }
  }
  if(!pending&&leave)break;
  if(!pending&&action){
   if(action==1){if(!motion_tried){motion_tried=true;live[4]=get(rt,"motion.accel",1,&g[4]);motion=g[4].api;
     if(!VALID(motion,1,TWATCH_MOTION_SAMPLE_SIZE)||!motion->read)motion=NULL;}
    strcpy(status_text,motion?"MOTION SAMPLING ON":"MOTION UNAVAILABLE");rt->diagnostic(motion?"WATCH_TEST motion=started":"WATCH_TEST motion=unavailable");
   }else{if(!live[5]){live[5]=get(rt,"haptic.effect",1,&g[5]);haptic=g[5].api;
     if(!VALID(haptic,1,sizeof(*haptic))||!haptic->effect||!haptic->stop)haptic=NULL;}
    bool ok=haptic&&haptic->effect(haptic->context,14);strcpy(status_text,ok?"VIBRATION PULSE REQUESTED":"VIBRATION UNAVAILABLE");rt->diagnostic(ok?"WATCH_TEST haptic=pulse-requested":"WATCH_TEST haptic=unavailable");
   }action=0;dirty=true;
  }
  if(!pending&&(dirty||(uint32_t)(health.uptime_ms-last)>=1000)){
   risc_battery_sample_v1 b={0};if(battery&&battery->read(battery->context,&b)){
    if(b.percent<=100)snprintf(battery_text,sizeof(battery_text),"BATTERY %u%% %u MV",b.percent,b.millivolts);
    else snprintf(battery_text,sizeof(battery_text),"BATTERY --%% %u MV",b.millivolts);
   }
   twatch_rtc_time_v1 date={0};if(rtc&&rtc->read(rtc->context,&date)&&tw_valid_time(&date))snprintf(rtc_text,sizeof(rtc_text),"RTC RAW %02u:%02u:%02u",date.hour,date.minute,date.second);
   if(motion){twatch_accel_sample_v1 a={0};if(motion->read(motion->context,&a))snprintf(motion_text,sizeof(motion_text),"XYZ %d %d %d MG",a.x,a.y,a.z);else strcpy(motion_text,"MOTION READ ERROR");}
   risc_display_surface_v1 s={0};if(!v.display.acquire(v.display.context,RISC_DISPLAY_FORMAT_RGB565,&s))break;
   if(!render(&s,health.uptime_ms/1000,battery_text,rtc_text,motion_text,status_text,tx,ty,touches))break;
   if(!v.display.submit(v.display.context,s.frame,NULL,0,NULL,&token))break;
   pending=true;dirty=false;started=last=health.uptime_ms;
  }
  rt->yield_ms(pending?1:10);
 }
cleanup:
 /* Finish an accepted native frame before returning so Home never races it. */
 if(pending&&v.native){for(unsigned n=0;n<10000;n++){risc_display_present_status_v1 s={0};
   if(!v.display.present_status(v.display.context,token,&s)||s.state!=RISC_DISPLAY_PRESENT_ACTIVE)break;rt->yield_ms(1);}}
 if(haptic&&!haptic->stop(haptic->context)){rt->diagnostic("WATCH_TEST retained=haptic-stop");for(;;)rt->yield_ms(100);}
 if(sub&&!touch->unsubscribe(touch->context,sub)){rt->diagnostic("WATCH_TEST retained=touch");for(;;)rt->yield_ms(100);}
 watch_viewport_close(&v);
 for(unsigned n=6;n>0;n--)if(live[n-1]&&!rt->release(&g[n-1])){rt->diagnostic("WATCH_TEST retained=release");for(;;)rt->yield_ms(100);}
 rt->diagnostic("WATCH_TEST return=home");
}
