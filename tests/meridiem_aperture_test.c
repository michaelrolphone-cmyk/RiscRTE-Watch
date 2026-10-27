#define WATCH_FACE_TEXT_TEST
#include "apps/clock/nova/nova.c"
#include <assert.h>
#include <stdio.h>
static unsigned calls;
static void face_text_test(const face_font*f,const char*s,int x,int y,int spacing,int fit,const uint8_t*a){
 (void)f;(void)spacing;(void)fit;(void)a;
 if(!strcmp(s,"AM")||!strcmp(s,"PM")){assert(x==120&&y==229);++calls;}
}
int main(void){
 const unsigned sizes[]={160,200,239,240,241,320,466,640};unsigned comparisons=0;
 for(unsigned n=0;n<sizeof(sizes)/sizeof(*sizes);++n){unsigned side=sizes[n],stride=side+7;size_t count=(size_t)stride*side+32;
  uint16_t *pixels=malloc(count*2);assert(pixels);risc_display_surface_v1 s={0,pixels+16,side,side,stride*2,stride*side*2,5};canvas c=CANVAS_INIT(&s,((const nova_watch_state*)NULL));
  for(unsigned pm=0;pm<2;++pm){memset(pixels,0xa5,count*2);for(unsigned y=0;y<side;++y)memset(pixels+16+y*stride,0,side*2);
   fmeridiem(&c,pm?"PM":"AM");unsigned ink=0;
   for(unsigned y=0;y<side;++y){for(unsigned x=side;x<stride;++x)assert(pixels[16+y*stride+x]==0xa5a5);
    for(unsigned x=0;x<side;++x)if(pixels[16+y*stride+x]){int64_t dx=(int64_t)2*x+1-side,dy=(int64_t)2*y+1-side,r=(int64_t)side-4;assert(dx*dx+dy*dy<=r*r);++ink;}}
   assert(ink);for(unsigned i=0;i<16;++i)assert(pixels[i]==0xa5a5&&pixels[count-1-i]==0xa5a5);
   for(unsigned id=0;id<24;++id){nova_watch_state state={.time={.year=2026,.month=10,.day=25,.hour=pm?12:11,.minute=37,.second=19},.time_valid=true,.battery_valid=true,.battery_percent=42};
    calls=0;assert(nova_watch_face_render(&s,&state,id));assert(calls==1);
    calls=0;state.hour_24=true;assert(nova_watch_face_render(&s,&state,id));assert(!calls);++comparisons;
   }
  }free(pixels);
 }
 printf("PASS %u actual face/AM-PM routing comparisons across8 native sizes; isolated actual footer pixels inside circular aperture; 24-hour marker absent; guards intact\n",comparisons);
}
