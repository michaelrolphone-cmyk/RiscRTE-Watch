#include "apps/clock/nova/nova.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned long long hash(risc_display_surface_v1*s,int mask){unsigned long long h=1469598103934665603ULL;unsigned side=s->width<s->height?s->width:s->height,left=(s->width-side)/2,top=(s->height-side)/2;
 for(unsigned y=0;y<s->height;y++)for(unsigned x=0;x<s->width;x++){uint16_t p=((uint16_t*)s->pixels)[y*s->width+x];
  if(mask&&y+2>=top+side*217/240&&y<=top+side*236/240+2&&((x+2>=left+side*106/240&&x<=left+side*134/240+2)||(x+2>=left+side*203/240&&x<=left+side*231/240+2)))p=0;
  h^=p&255;h*=1099511628211ULL;h^=p>>8;h*=1099511628211ULL;}return h;}
int main(void){const unsigned sizes[][2]={{160,160},{240,240},{466,466},{410,502},{640,240},{241,160}};
 for(unsigned n=0;n<6;n++){unsigned w=sizes[n][0],h=sizes[n][1];uint16_t*p=calloc(w*h,2);assert(p);risc_display_surface_v1 s={0,p,w,h,w*2,w*h*2,5};
  for(unsigned mode=0;mode<4;mode++)for(unsigned id=0;id<WATCH_FACE_COUNT;id++){nova_watch_state state={.time={.year=2026,.month=10,.day=25,.hour=mode==1?12:11,.minute=37,.second=19},.time_valid=mode!=3,.hour_24=mode==2,.battery_valid=true,.battery_percent=42};assert(nova_watch_face_render(&s,&state,id));printf("%u %u %u %u %016llx %016llx\n",w,h,mode,id,hash(&s,0),hash(&s,1));}
 free(p);}
}
