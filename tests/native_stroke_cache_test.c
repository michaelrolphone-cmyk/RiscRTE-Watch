#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static unsigned allocations,frees;static int fail_alloc;
static void *cache_alloc(size_t n){allocations++;return fail_alloc?NULL:malloc(n);}
static void cache_free(void*p){frees++;free(p);}
#define WATCH_STROKE_ALLOC cache_alloc
#define WATCH_STROKE_FREE cache_free
#define WATCH_STROKE_TEST
#include "../apps/clock/nova/nova.c"
static unsigned calls,fail_at;
static bool capture(void*p){(void)p;calls++;return !fail_at||calls<fail_at;}
static uint32_t rng=12345;static unsigned random_value(void){rng=rng*1664525u+1013904223u;return rng;}
static uint16_t a[466*466],b[466*466];
static nova_watch_state state={.time={.year=2026,.month=10,.day=13,.hour=21,.minute=37,.second=49},.time_valid=true,.battery_valid=true,.battery_percent=73,.subsecond_ms=725,.animation_ms=13579,.capture_audio=capture};
static void pair_face(unsigned face){risc_display_surface_v1 f={0,a,466,466,932,sizeof(a),5};stroke_enabled=false;reuse_test_observe=false;calls=0;bool ok=nova_watch_face_render(&f,&state,face);unsigned before=calls;f.pixels=b;stroke_enabled=true;reuse_test_observe=true;calls=0;assert(nova_watch_face_render(&f,&state,face)==ok);assert(before==calls);assert(!memcmp(a,b,sizeof(a)));}
int main(void){unsigned pairs=0;
for(unsigned id=0;id<WATCH_FACE_COUNT;id++)for(unsigned n=0;n<3;n++){state.subsecond_ms=725+n*50;pair_face(id);pairs++;}
/* Dynamic underlay/color, negative/fractional transformed positions, dimensions,
 * narrow+wide geometry, clipping, zero-length strokes and repeated hits. */
for(unsigned i=0;i<1200;i++){unsigned dim=80+(i%3)*193;int x1=(int)(random_value()%12000)-3000,y1=(int)(random_value()%12000)-3000,x2=i%7?x1+(int)(random_value()%3000)-1500:x1,y2=i%7?y1+(int)(random_value()%3000)-1500:y1,width=1+i%500;for(unsigned n=0;n<dim*dim;n++)a[n]=b[n]=(uint16_t)random_value();uint16_t color=(uint16_t)random_value();reuse_test_observe=true;for(unsigned warm=0;warm<3;warm++)native_reuse_width(dim,dim);canvas ca={(uint8_t*)a,dim*2,dim,dim,capture,NULL,0,false,{0}},cb=ca;cb.pixels=(uint8_t*)b;calls=0;stroke_enabled=false;reuse_test_observe=false;fline(&ca,x1,y1,x2,y2,width,color);unsigned before=calls;calls=0;stroke_enabled=true;reuse_test_observe=true;fline(&cb,x1,y1,x2,y2,width,color);assert(before==calls);assert(!memcmp(a,b,dim*dim*2));/* Repeat samekey overnewbackground. */for(unsigned n=0;n<dim*dim;n++)a[n]=b[n]=(uint16_t)random_value();ca.operations=cb.operations=0;calls=0;stroke_enabled=false;reuse_test_observe=false;fline(&ca,x1,y1,x2,y2,width,color^0xffff);before=calls;calls=0;stroke_enabled=true;reuse_test_observe=true;fline(&cb,x1,y1,x2,y2,width,color^0xffff);assert(before==calls);assert(!memcmp(a,b,dim*dim*2));pairs+=2;}
for(unsigned id=0;id<2;id++){unsigned face=id?8:0;fail_at=0;pair_face(face);unsigned faults[]={1,2,10,60,120,300,1000};for(unsigned k=0;k<7;k++){fail_at=faults[k];pair_face(face);pairs++;}}
fail_at=0;nova_watch_raster_cache_release();unsigned oldalloc=allocations;fail_alloc=1;pair_face(0);pair_face(8);pair_face(0);assert(!stroke_cache&&allocations==oldalloc+1);nova_watch_raster_cache_release();fail_alloc=0;pair_face(0);pair_face(0);pair_face(0);assert(stroke_cache);nova_watch_raster_cache_release();nova_watch_raster_cache_release();assert(!stroke_cache);
printf("PASS %u full-buffer/callback pairs; allocation OOM fallback/retry/idempotent release; allocation=%zu bytes\n",pairs,sizeof(stroke_store));
unsigned ids[]={0,1,7,8,13,16};for(unsigned j=0;j<6;j++){double ms[2]={0};unsigned hits=0,misses=0;for(unsigned n=0;n<80;n++){state.time.second=(58+n/20)%60;state.time.minute=37+(58+n/20)/60;state.subsecond_ms=(n%20)*50;unsigned oh=stroke_hits,om=stroke_misses;for(unsigned k=0;k<2;k++){stroke_enabled=k;reuse_test_observe=k;risc_display_surface_v1 f={0,k?b:a,466,466,932,sizeof(a),5};clock_t t=clock();assert(nova_watch_face_render(&f,&state,ids[j]));ms[k]+=1000.0*(clock()-t)/CLOCKS_PER_SEC;}assert(!memcmp(a,b,sizeof(a)));hits+=stroke_hits-oh;misses+=stroke_misses-om;}printf("face%u progression80 host off %.3f on %.3f ms hits%u misses%u bytes%u entries%u\n",ids[j],ms[0]/80,ms[1]/80,hits,misses,stroke_cache->used,stroke_cache->count);}
unsigned speeds[]={1,2,8,32};for(unsigned speed=0;speed<4;speed++){nova_watch_raster_cache_release();
nova_watch_picker_cache*c[2]={nova_watch_picker_cache_create(466,466),nova_watch_picker_cache_create(466,466)};assert(c[0]&&c[1]);double ms[2]={0};unsigned hits=0,misses=0;for(unsigned n=0;n<100;n++){int positions[WATCH_FACE_CATEGORY_COUNT]={0};positions[0]=-(int)((n<50?n:99-n)*speeds[speed])*256;unsigned oh=stroke_hits,om=stroke_misses;for(unsigned k=0;k<2;k++){stroke_enabled=k;reuse_test_observe=k;risc_display_surface_v1 f={0,k?b:a,466,466,932,sizeof(a),5};clock_t t=clock();unsigned beforefaces=face_test_calls;uint32_t beforegeometry=reuse_serial;assert(nova_watch_picker_collections_render(&f,&state,0,0,positions,NULL,0,256,c[k]));if(k)assert(reuse_serial-beforegeometry==face_test_calls-beforefaces+1u);ms[k]+=1000.0*(clock()-t)/CLOCKS_PER_SEC;}assert(!memcmp(a,b,sizeof(a)));hits+=stroke_hits-oh;misses+=stroke_misses-om;}printf("moving picker speed%u forward/reverse100 full-buffer pairs host off %.3f on %.3f ms hits%u misses%u bytes%u entries%u\n",speeds[speed],ms[0]/100,ms[1]/100,hits,misses,stroke_cache->used,stroke_cache->count);nova_watch_picker_cache_destroy(c[0]);nova_watch_picker_cache_destroy(c[1]);nova_watch_raster_cache_release();}return 0;}
