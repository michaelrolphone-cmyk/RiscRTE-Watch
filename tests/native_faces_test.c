#include "apps/clock/nova/nova.h"
#include "apps/clock/faces/picker.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static nova_watch_state state_for(unsigned n) {
    nova_watch_state state={.time={.year=2026,.month=10,.day=13,.hour=21,.minute=37,.second=49},
        .time_valid=true,.battery_valid=true,.battery_percent=73,.subsecond_ms=725,.animation_ms=13579};
    if(n==1){state.hour_24=true;state.time.hour=0;state.time.second=0;state.subsecond_ms=0;}
    if(n==2){state.time_valid=false;state.battery_valid=false;}
    static const nova_points_state ready={.status=NOVA_POINTS_READY,.phase=NOVA_PHASE_OFF_WORK,
        .now_rtc=1000,.next_count=3,.today_count=2,.previous_valid=true,
        .previous={.at_rtc=900,.hour=20,.minute=0,.kind=NOVA_POINT_WORK_END},
        .next={{.at_rtc=1100,.hour=22,.minute=30,.kind=NOVA_POINT_BEDTIME},
               {.at_rtc=1200,.hour=23,.minute=0,.kind=NOVA_POINT_CUSTOM_1,.label="WIND DOWN",.color_index=3},
               {.at_rtc=30000,.hour=8,.minute=0,.kind=NOVA_POINT_WORK_START,.day_offset=1}},
        .today={{.at_rtc=900,.hour=20,.minute=0,.kind=NOVA_POINT_WORK_END},
                {.at_rtc=1100,.hour=22,.minute=30,.kind=NOVA_POINT_BEDTIME}}};
    static const nova_points_state empty={.status=NOVA_POINTS_EMPTY},error={.status=NOVA_POINTS_ERROR};
    if(n==3)state.points=&ready;
    if(n==4)state.points=&empty;
    if(n==5)state.points=&error;
    return state;
}

static void save_image(const risc_display_surface_v1*s,unsigned id) {
 const char*dir=getenv("WATCH_NATIVE_IMAGES");if(!dir)return;
 char path[512];snprintf(path,sizeof(path),"%s/%ux%u-face-%02u.ppm",dir,s->width,s->height,id);
 FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%u %u\n255\n",s->width,s->height);
 for(unsigned y=0;y<s->height;y++)for(unsigned x=0;x<s->width;x++) {
  uint16_t q=((uint16_t*)((uint8_t*)s->pixels+y*s->stride_bytes))[x];
  unsigned char rgb[3]={(unsigned char)((q>>11)*255/31),(unsigned char)(((q>>5)&63)*255/63),(unsigned char)((q&31)*255/31)};
  assert(fwrite(rgb,1,3,f)==3);
 }assert(!fclose(f));
}
static unsigned checks,captures;
static bool capture(void*unused){(void)unused;captures++;return true;}
int main(void) {
 const unsigned dimensions[][2]={{240,240},{410,502},{160,160},{200,200},{320,240},{640,240},{240,320},{241,160}};
 for(unsigned d=0;d<sizeof(dimensions)/sizeof(*dimensions);d++) {
  unsigned width=dimensions[d][0],height=dimensions[d][1],stride=width+8;
  uint16_t*buffer=malloc((stride*height+32)*2);assert(buffer);
  risc_display_surface_v1 frame={0,buffer+16,width,height,stride*2,stride*height*2,5};
  clock_t start=clock();
  for(unsigned n=0;n<6;n++)for(unsigned id=0;id<WATCH_FACE_COUNT;id++) {
   memset(buffer,0xa5,(stride*height+32)*2);nova_watch_state state=state_for(n);state.capture_audio=capture;
   if(!nova_watch_face_render(&frame,&state,id)){fprintf(stderr,"failed %ux%u state %u face %u\n",width,height,n,id);abort();}
   for(unsigned x=0;x<16;x++){assert(buffer[x]==0xa5a5);assert(buffer[16+stride*height+x]==0xa5a5);}
   unsigned ink=0;
   for(unsigned y=0;y<height;y++) {
    for(unsigned x=width;x<stride;x++)assert(buffer[16+y*stride+x]==0xa5a5);
    for(unsigned x=0;x<width;x++)ink+=buffer[16+y*stride+x]!=0;
   }assert(ink);checks++;
   if(n==3&&(d==0||d==1))save_image(&frame,id);
  }
  printf("%ux%u: 204 native face/state renders, guards PASS, CPU %.3f ms/render\n",width,height,1000.0*(clock()-start)/CLOCKS_PER_SEC/204);
  nova_watch_picker_cache *cache=nova_watch_picker_cache_create(width,height);assert(cache);
  nova_watch_state state=state_for(3);int positions[WATCH_FACE_CATEGORY_COUNT]={0};
  for(unsigned cat=0;cat<WATCH_FACE_CATEGORY_COUNT;cat++)for(unsigned shift=0;shift<height;shift+=height/5) {
   if(!nova_watch_picker_collections_render(&frame,&state,3,-((int)cat*(int)height+(int)shift)*256,positions,NULL,3,277,cache)){fprintf(stderr,"picker failed %ux%u category %u shift %u\n",width,height,cat,shift);abort();}
  }
  for(unsigned flags=0;flags<64;flags++)assert(nova_watch_alarm_label_render(&frame,"EVENING REMINDER",flags&1,flags&2,flags&4,flags&8,flags&16,flags&32));
  assert(nova_watch_context_message(&frame,"KITCHEN EVENING ACTIVITY"));
  assert(nova_watch_sleep_status(&frame,"SLEEP ERR MOTION"));
  assert(nova_watch_render(&frame,&state));
  for(unsigned y=0;y<height;y++)for(unsigned x=width;x<stride;x++)assert(buffer[16+y*stride+x]==0xa5a5);
  for(unsigned x=0;x<16;x++){assert(buffer[x]==0xa5a5);assert(buffer[16+stride*height+x]==0xa5a5);}
  unsigned hits=0;
  for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++) {
   watch_face_picker picker={0};assert(watch_face_set_size(&picker,width,height));picker.open=true;
   watch_face_input(&picker,0,true,0,0,0,0);watch_face_input(&picker,10,true,1,1,x,y);
   bool selected=watch_face_input(&picker,20,true,0,0,0,0)==WATCH_FACE_SELECT;
   int half=(int)(132*watch_home_side(width,height)/240)/2;
   int dx=(int)x-(int)width/2,dy=(int)y-(int)watch_home_picker_center_at(width,height);
   assert(selected==(dx>-half&&dx<half&&dy>-half&&dy<half));hits+=selected;
  }assert(hits);nova_watch_picker_cache_destroy(cache);
  printf("%ux%u: native picker/alerts/status/legacy clock guards and all-pixel card hit test PASS\n",width,height);
  free(buffer);
 }
 assert(captures);printf("Native faces: %u guarded renders PASS\n",checks);
}
