#include "RiscDisplayOutputV1.h"
#include "RiscTouchV1.h"
#include "RiscInputNavigationV1.h"
#include "RiscPlatformClockV1.h"
#include "RiscRealtimeV1.h"
#include "RiscPowerKeyV1.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
extern unsigned scene_test_invocation(void);
extern unsigned resident_test_phase(void);
extern unsigned resident_test_age(void);
extern void resident_test_touch_open(void);
extern void resident_test_touch_close(void);
extern void resident_test_controls_gesture(void);
extern bool resident_test_started(void);
extern bool resident_test_key_mode(void);
extern void resident_test_key_delivered(void);
extern unsigned scene_test_width(void);
extern unsigned scene_test_height(void);
extern unsigned scene_test_format(void);
extern void scene_test_frame(const void*,size_t);
extern uint64_t scene_test_millis(void);
static unsigned phase_frames,phase_nav,previous_phase;
static uint64_t event_sequence;
static unsigned event_step;
static uint8_t pixels[800*480*2+32];
static bool held,presenting,subscribed;static unsigned acquisitions,completions,nav_wait,dummy_unused __attribute__((unused));static uint64_t sequence;static unsigned polls,frame_polls,io_count,submits,releases;
static bool info(void*c,risc_display_info_v1*out){(void)c;++io_count;unsigned w=scene_test_width();*out=(risc_display_info_v1){.api_version=1,.struct_size=sizeof(*out),.width=w,.height=scene_test_height(),.supported_formats=RISC_DISPLAY_FORMAT_BIT(scene_test_format()),.preferred_format=scene_test_format()};const char* raw=getenv("ALARM_SCENE_SAFE_INSET");if(raw){char* end=NULL;unsigned long inset=strtoul(raw,&end,10);assert(end!=raw&&!*end&&inset<w/2);out->safe_insets=(risc_display_insets_v1){inset,inset,inset,inset};out->flags|=RISC_DISPLAY_INFO_ROUND;}return true;}
static bool acquire(void*c,uint32_t f,risc_display_surface_v1*out){(void)c;++io_count;assert(!held&&!presenting);held=true;++acquisitions;memset(pixels,0xa5,sizeof(pixels));unsigned w=scene_test_width(),h=scene_test_height();assert(f==scene_test_format());unsigned stride=scene_test_format()==1?(w+7)/8:w*2;*out=(risc_display_surface_v1){++sequence,pixels+16,w,h,stride,stride*h,f};return true;}
static void guards(void){unsigned bytes=(scene_test_format()==1?(scene_test_width()+7)/8:scene_test_width()*2)*scene_test_height();for(unsigned i=0;i<16;i++){assert(pixels[i]==0xa5);assert(pixels[16+bytes+i]==0xa5);}}
static void release(void*c,uint64_t frame){(void)c;++io_count;assert(held&&!presenting&&frame==sequence);held=false;++releases;guards();}
static bool submit(void*c,uint64_t frame,const risc_display_rect_v1*d,size_t n,const risc_display_present_options_v1*o,uint64_t*t){(void)c;(void)d;++io_count;assert(held&&frame==sequence);(void)o;assert(n<=1);guards();*t=sequence;frame_polls=0;held=false;presenting=true;++submits;scene_test_frame(pixels+16,(scene_test_format()==1?(scene_test_width()+7)/8:scene_test_width()*2)*scene_test_height());return true;}
static bool display_status(void*c,uint64_t t,risc_display_present_status_v1*out){(void)c;++io_count;assert(!held&&presenting&&t==sequence);out->state=++frame_polls<3?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;if(out->state==RISC_DISPLAY_PRESENT_COMPLETE){presenting=false;++completions;}return true;}
static uint64_t subscribe(void*c){(void)c;++io_count;assert(!subscribed);subscribed=true;resident_test_touch_open();event_step=0;return 1;}
static bool unsubscribe(void*c,uint64_t id){(void)c;++io_count;assert(subscribed&&id==1);subscribed=false;resident_test_touch_close();return true;}
static bool touch_poll(void*c,size_t n){(void)c;++io_count;assert(n<=2);return true;}
static int32_t touch_next(void*c,uint64_t id,risc_touch_event_v1*out){
 (void)c;assert(id==1&&subscribed);
 if(resident_test_phase()!=1||(!resident_test_started()||resident_test_age()<700)||event_step>=3)return 0;
 ++event_step;if(event_step==2)resident_test_controls_gesture();memset(out,0,sizeof(*out));out->sequence=++event_sequence;out->timestamp_ms=scene_test_millis();out->id=1;out->x=scene_test_width()/2;out->y=event_step==1?2:scene_test_height()/3;
 out->kind=event_step==1?RISC_TOUCH_EVENT_DOWN:event_step==2?RISC_TOUCH_EVENT_MOVE:RISC_TOUCH_EVENT_UP;
 return 1;
}
static bool snapshot(void*c,risc_touch_snapshot_v1*out){
 (void)c;++io_count;unsigned w=scene_test_width(),h=scene_test_height(),phase=resident_test_phase(),age=resident_test_age();
 *out=(risc_touch_snapshot_v1){.width=w,.height=h};
 if(phase==0&&age>=100&&age<300){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){1,0,(uint16_t)(age<180?w/2:w/4),(uint16_t)(h/2)};}
 if(phase==2&&age>=650&&age<750){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){1,0,(uint16_t)(w/2),(uint16_t)(h-w*23/240)};}
 return true;
}
static bool navigation(void*c,risc_input_navigation_frame_v1*out){
 (void)c;++io_count;unsigned phase=resident_test_phase();
 if(phase!=previous_phase){previous_phase=phase;phase_nav=0;phase_frames=0;}
 *out=(risc_input_navigation_frame_v1){0};assert(++polls<10000);
 if(completions>phase_frames&&resident_test_age()>100&&++nav_wait>=3){nav_wait=0;
  if(phase==1&&!resident_test_started()){out->pressed=phase_nav++?RISC_NAV_CONFIRM:RISC_NAV_DOWN;}
  else if(phase==3&&!resident_test_key_mode()&&resident_test_age()>300&&phase_nav++==0)out->pressed=RISC_NAV_HOME;
 }
 if(out->pressed)fprintf(stderr,"NAV phase=%u age=%u pressed=%u complete=%u\n",phase,resident_test_age(),out->pressed,completions);
 return true;
}
static bool foreground(void*c,const risc_input_foreground_v1*f,size_t n){(void)c;++io_count;assert(n<=1);if(n)assert(!strcmp(f->capability,"input.touch.raw"));return true;}
static bool reset(void*c){(void)c;++io_count;return true;}
static uint64_t now(void*c){(void)c;return scene_test_millis();}
static void sleep_ms(void*c,uint32_t n){(void)c;(void)n;assert(0);}
static bool brightness(void*c,uint16_t a,uint16_t b){(void)c;assert(a<=100&&b==100);return true;}
static const risc_display_output_api_v1 display={1,sizeof(display),NULL,info,acquire,release,submit,display_status,NULL,brightness};
static const risc_touch_api_v1 touch={1,sizeof(touch),NULL,subscribe,unsubscribe,touch_poll,touch_next,snapshot};
static const risc_input_navigation_api_v1 nav={1,sizeof(nav),NULL,navigation,foreground,reset};
static const risc_platform_clock_api_v1 clock_api={1,sizeof(clock_api),NULL,now,sleep_ms};
static int32_t realtime_read(void*c,risc_realtime_snapshot_v1*out){(void)c;uint64_t ms=scene_test_millis();*out=(risc_realtime_snapshot_v1){sizeof(*out),RISC_REALTIME_VALID,INT64_C(1791547200)+(int64_t)(ms/1000),0,0,ms*1000+1,ms*1000+2};return 0;}
static int32_t seed(void*c,int64_t s,uint32_t n){(void)c;(void)s;(void)n;assert(0);return -1;}
static const risc_realtime_control_api_v1 realtime={1,sizeof(realtime),NULL,realtime_read,seed};
static bool key_sent;
static bool key_events(void*c,uint32_t*out){(void)c;*out=0;if(resident_test_key_mode()&&resident_test_phase()==3&&resident_test_age()>300&&!key_sent){key_sent=true;*out=RISC_POWER_KEY_LONG_PRESS;resident_test_key_delivered();}return true;}
static const risc_power_key_api_v1 power_key={1,sizeof(power_key),NULL,key_events};
__attribute__((visibility("default"))) const void *scene_test_hardware(unsigned id){assert(id<6);const void*apis[]={&display,&touch,&nav,&clock_api,&realtime,&power_key};return apis[id];}
__attribute__((visibility("default"))) void scene_test_hardware_closed(void){assert(!held&&!presenting&&!subscribed&&acquisitions==submits+releases&&completions==submits);}
