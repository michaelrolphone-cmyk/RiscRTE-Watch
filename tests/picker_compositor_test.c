#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static unsigned plan_allocations,plan_frees;static int plan_fail;
static void*plan_alloc(size_t n){plan_allocations++;return plan_fail?NULL:malloc(n);}
static void plan_free(void*p){plan_frees++;free(p);}
#define WATCH_PICKER_PLAN_ALLOC plan_alloc
#define WATCH_PICKER_PLAN_FREE plan_free
#include "../apps/clock/nova/nova.c"
static uint32_t random_state=12345;static unsigned random_next(void){random_state=random_state*1664525u+1013904223u;return random_state;}
typedef struct {unsigned calls,fail_at;} observer;
static bool capture(void*p){observer*o=p;o->calls++;return !o->fail_at||o->calls<o->fail_at;}
static void compare(unsigned w,unsigned h,int size,int x,int y,int radius,int border,unsigned opacity,bool legacy,unsigned fail,unsigned fallback){
 unsigned stride=w+7,n=stride*h+32;uint16_t*a=malloc(n*2),*b=malloc(n*2),*src=malloc((legacy?240u*240u:(unsigned)size*size)*2);uint32_t*columns=malloc((unsigned)size*4);assert(a&&b&&src&&columns);
 for(unsigned i=0;i<n;i++){a[i]=b[i]=(uint16_t)random_next();}
 for(unsigned i=0;i<(legacy?240u*240u:(unsigned)size*size);i++)src[i]=(uint16_t)random_next();
 observer oa={0,fail},ob=oa;canvas ca={(uint8_t*)(a+16),stride*2,h,w,capture,&oa,0,false,{0}},cb=ca;cb.pixels=(uint8_t*)(b+16);cb.context=&ob;
 picker_composite_reference(&ca,src,size,x,y,radius,border,opacity,0x8a3f,legacy);picker_composite_planned(&cb,src,size,x,y,radius,border,opacity,0x8a3f,legacy,fallback==1?NULL:columns,fallback==2?(unsigned)size-1:(unsigned)size);
 assert(!memcmp(a,b,n*2));assert(ca.failed==cb.failed&&ca.operations==cb.operations&&oa.calls==ob.calls);free(a);free(b);free(src);free(columns);
}
int main(void){unsigned cases=0;const unsigned widths[]={160,240,300,466,640};for(unsigned i=0;i<1600;i++){unsigned w=widths[i%5],h=i%3?466:240;int size=1+(int)(random_next()%278);int x=(int)(random_next()%(w+size))-size,y=(int)(random_next()%(h+size))-size,r=(int)(random_next()%((unsigned)size/2+1)),border=(int)(random_next()%((unsigned)r+7));compare(w,h,size,x,y,r,border,i%256,i&1,0,i%17==0?1:i%19==0?2:0);cases++;}
for(unsigned fail=1;fail<=100;fail++){compare(466,466,278,94,39,29,4,203,false,fail,0);cases++;}
/* Unusual geometry falls back instead of altering the established raster. */
for(unsigned i=0;i<8;i++){compare(240,300,128,-11,40,70+(int)i,4,255,true,0,0);cases++;}
for(unsigned i=0;i<8;i++){compare(2048,2048,1221,(int)i*200-700,(int)i*177-400,(int)(i*87),1+(int)i*31,i&1?255:0,false,0,0);cases++;}
compare(466,466,278,20,35,29,4,300,false,0,0);cases++;
plan_fail=1;nova_watch_picker_cache*c=nova_watch_picker_cache_create(466,466);assert(c&&!c->composition_columns&&plan_allocations==1);uint16_t*p=calloc(466*466,2);assert(p);risc_display_surface_v1 frame={0,p,466,466,932,466*466*2,5};nova_watch_state s={.time={2026,1,1,0,0,0},.time_valid=true};int positions[WATCH_FACE_CATEGORY_COUNT]={0};assert(nova_watch_picker_collections_render(&frame,&s,0,0,positions,NULL,0,256,c));assert(plan_allocations==1&&plan_frees==0);nova_watch_picker_cache_destroy(c);plan_fail=0;c=nova_watch_picker_cache_create(466,466);assert(c&&c->composition_columns&&plan_allocations==2);assert(nova_watch_picker_collections_render(&frame,&s,0,0,positions,NULL,0,256,c));assert(plan_allocations==2);nova_watch_picker_cache_destroy(c);assert(plan_frees==1);nova_watch_picker_cache_destroy(NULL);free(p);nova_watch_raster_cache_release();printf("PASS %u full-buffer/guard/callback/terminal cases; optional plan OOM fallback and no per-frame allocation;466plan_bytes=%u\n",cases,((466u*143u+239u)/240u)*4);}
