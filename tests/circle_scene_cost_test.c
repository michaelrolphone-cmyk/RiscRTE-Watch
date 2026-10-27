#include "apps/clock/nova/nova.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static unsigned calls;static bool capture(void*x){(void)x;++calls;return true;}
int main(void){unsigned w=466;uint16_t*b=calloc(w*w,2);assert(b);risc_display_surface_v1 s={0,b,w,w,w*2,w*w*2,5};nova_watch_state state={.time={2026,10,25,11,59,58},.time_valid=true,.battery_valid=true,.battery_percent=99,.capture_audio=capture};
 const unsigned ids[]={0,1,2,8,12,24,33};
 for(unsigned k=0;k<7;k++){calls=0;clock_t began=clock();for(unsigned f=0;f<100;f++){state.animation_ms=f*20;state.subsecond_ms=f*20%1000;assert(nova_watch_face_render(&s,&state,ids[k]));}printf("face%u cpu_seconds=%.9f frames=100 checkpoints=%u\n",ids[k],(double)(clock()-began)/CLOCKS_PER_SEC,calls);}
 nova_watch_picker_cache*cache=nova_watch_picker_cache_create(w,w);assert(cache);int positions[WATCH_FACE_CATEGORY_COUNT]={0};calls=0;clock_t began=clock();for(unsigned f=0;f<100;f++){state.animation_ms=f*20;state.subsecond_ms=f*20%1000;positions[0]=-(int)(f%36)*(int)(w*108/240)*64;assert(nova_watch_picker_collections_render(&s,&state,0,0,positions,NULL,0,256,cache));}printf("picker cpu_seconds=%.9f frames=100 checkpoints=%u\n",(double)(clock()-began)/CLOCKS_PER_SEC,calls);nova_watch_picker_cache_destroy(cache);free(b);return 0;}
