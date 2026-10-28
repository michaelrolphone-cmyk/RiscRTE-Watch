#include "apps/clock/nova/nova.h"
#include "apps/clock/faces/catalog.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned calls,fail_at;
static const uint16_t *observed_buffer;static size_t observed_size;static uint64_t callback_trace;
static bool capture(void *p){(void)p;++calls;callback_trace^=calls;callback_trace*=UINT64_C(1099511628211);
 for(unsigned i=0;i<17;i++){size_t at=(observed_size-1)*i/16;callback_trace^=observed_buffer[at];callback_trace*=UINT64_C(1099511628211);}return calls!=fail_at;}
static void record(const uint16_t *b,size_t n,bool ok,unsigned tag){
 uint64_t h=UINT64_C(1469598103934665603);for(size_t i=0;i<n;i++){h^=b[i];h*=UINT64_C(1099511628211);}
 printf("%u %u %u %llu %llu\n",tag,ok,calls,(unsigned long long)h,(unsigned long long)callback_trace);
}
int main(void){
 const unsigned dimensions[][2]={{160,160},{200,200},{240,240},{241,160},{320,240},{466,466},{410,502},{640,240},{240,320}};
 unsigned tag=0;const unsigned failures[]={1,2,17,257,1024};
 for(unsigned dim=0;dim<sizeof(dimensions)/sizeof(*dimensions);dim++){
  unsigned w=dimensions[dim][0],h=dimensions[dim][1],stride=w+8;size_t n=stride*h+32;
  uint16_t *b=malloc(n*2);assert(b);risc_display_surface_v1 s={0,b+16,w,h,stride*2,stride*h*2,5};
  nova_watch_state state={.time={.year=2026,.month=10,.day=25,.hour=11,.minute=59,.second=58},.time_valid=true,.battery_valid=true,.battery_percent=99,.animation_ms=813,.subsecond_ms=813,.capture_audio=capture};
  observed_buffer=b;observed_size=n;
  for(unsigned mode=0;mode<8;mode++)for(unsigned id=0;id<WATCH_FACE_COUNT;id++){
   state.hour_24=mode==1;state.time_valid=mode!=2;state.battery_valid=mode!=2;fail_at=mode>=3?failures[mode-3]:0;calls=0;callback_trace=0;memset(b,0xa5,n*2);
   bool ok=nova_watch_face_render(&s,&state,id);record(b,n,ok,tag++);
   for(unsigned i=0;i<16;i++)assert(b[i]==0xa5a5&&b[16+stride*h+i]==0xa5a5);
   for(unsigned y=0;y<h;y++)for(unsigned x=w;x<stride;x++)assert(b[16+y*stride+x]==0xa5a5);
  }
  fail_at=0;state.time_valid=state.battery_valid=true;
  nova_watch_picker_cache *cache=nova_watch_picker_cache_create(w,h);assert(cache);
  int positions[WATCH_FACE_CATEGORY_COUNT]={0};unsigned side=w<h?w:h;
  for(unsigned cat=0;cat<WATCH_FACE_CATEGORY_COUNT;cat++)for(unsigned step=0;step<36;step++){
   positions[cat]=-(int)(step*(side*108/240)*256/4);calls=0;callback_trace=0;memset(b,0xa5,n*2);
   bool ok=nova_watch_picker_collections_render(&s,&state,0,-(int)(cat*h*256+(step%4)*h*64),positions,NULL,0,256,cache);
   assert(ok);record(b,n,ok,tag++);
   for(unsigned i=0;i<16;i++)assert(b[i]==0xa5a5&&b[16+stride*h+i]==0xa5a5);
   for(unsigned y=0;y<h;y++)for(unsigned x=w;x<stride;x++)assert(b[16+y*stride+x]==0xa5a5);
  }
  nova_watch_picker_cache_destroy(cache);free(b);
 }
 return 0;
}
