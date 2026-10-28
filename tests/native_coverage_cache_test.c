#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static unsigned allocations,frees,terminal_allocs,terminal_frees;static int fail_alloc;static int in_render,terminal;
static void *cache_alloc(size_t n){allocations++;if(in_render&&terminal)terminal_allocs++;return fail_alloc?NULL:malloc(n);}
static void cache_free(void*p){frees++;if(in_render&&terminal)terminal_frees++;free(p);}
#define WATCH_COVERAGE_ALLOC cache_alloc
#define WATCH_COVERAGE_FREE cache_free
#define WATCH_COVERAGE_TEST
#include "../apps/clock/nova/nova.c"
static unsigned calls,fail_at;
static bool capture(void*p){(void)p;calls++;bool ok=!fail_at||calls<fail_at;if(!ok)terminal=1;return ok;}
static uint32_t rng=12345;static unsigned random_value(void){rng=rng*1664525u+1013904223u;return rng;}
static uint16_t a[466*466],b[466*466];
static nova_watch_state state={.time={.year=2026,.month=10,.day=13,.hour=21,.minute=37,.second=49},.time_valid=true,.battery_valid=true,.battery_percent=73,.subsecond_ms=725,.animation_ms=13579,.capture_audio=capture};
static void pair_face(unsigned face){risc_display_surface_v1 f={0,a,466,466,932,sizeof(a),5};coverage_enabled=false;reuse_test_observe=false;calls=0;terminal=0;in_render=1;bool ok=nova_watch_face_render(&f,&state,face);in_render=0;assert(!terminal_allocs&&!terminal_frees);unsigned before=calls;f.pixels=b;coverage_enabled=true;reuse_test_observe=true;calls=0;terminal=0;in_render=1;assert(nova_watch_face_render(&f,&state,face)==ok);in_render=0;assert(!terminal_allocs&&!terminal_frees);assert(before==calls);assert(!memcmp(a,b,sizeof(a)));}
int main(void){unsigned pairs=0;
/* Full renderer fails during line work before its first native glyph. */
risc_display_surface_v1 early={0,a,466,466,932,sizeof(a),5};coverage_enabled=true;reuse_test_observe=true;for(unsigned warm=0;warm<3;warm++){native_reuse_width(466,466);}assert(native_reuse_allowed(466,466));fail_at=60;calls=0;terminal=0;in_render=1;assert(!nova_watch_face_render(&early,&state,0));in_render=0;
printf("early-line terminal allocator calls=%u free calls=%u capturecalls=%u\n",terminal_allocs,terminal_frees,calls);fflush(stdout);
assert(terminal&&terminal_allocs==0&&terminal_frees==0);assert(!coverage_attempted);
nova_watch_raster_cache_release();fail_at=0;terminal=0;
for(unsigned warm=0;warm<3;warm++){native_reuse_width(466,466);}assert(nova_watch_face_render(&early,&state,0));assert(coverage_cache);unsigned oldhits=coverage_hits,oldmisses=coverage_misses,oldused=coverage_used,oldcount=coverage_count;fail_at=60;calls=0;terminal=0;in_render=1;assert(!nova_watch_face_render(&early,&state,0));in_render=0;assert(!terminal_allocs&&!terminal_frees);assert(coverage_hits==oldhits&&coverage_misses==oldmisses&&coverage_used==oldused&&coverage_count==oldcount);nova_watch_raster_cache_release();fail_at=0;terminal=0;


for(unsigned id=0;id<WATCH_FACE_COUNT;id++)for(unsigned n=0;n<3;n++){state.subsecond_ms=725+n*50;pair_face(id);pairs++;}
/* Immutable bank identity survives a reused scratch address, fractional phase,
 * clipping, scale, opacity and changing destination background. */
uint8_t bank[4][64*64],scratch[64*64];for(unsigned j=0;j<4;j++)for(unsigned i=0;i<sizeof(bank[j]);i++)bank[j][i]=(uint8_t)random_value();
for(unsigned i=0;i<600;i++){unsigned j=i%4,w=8+i%57,h=8+(i*7)%57;int sx=64+(i*97)%700,sy=64+(i*71)%700,left=(int)(i*53%32000)-8000,top=(int)(i*67%32000)-8000;unsigned opacity=i%256;uint16_t color=(uint16_t)random_value();memcpy(scratch,bank[j],sizeof(scratch));for(unsigned n=0;n<80*80;n++)a[n]=b[n]=(uint16_t)random_value();reuse_test_observe=true;for(unsigned warm=0;warm<3;warm++)native_reuse_width(80,80);canvas ca={(uint8_t*)a,160,80,80,capture,NULL,0,false,{0}},cb=ca;cb.pixels=(uint8_t*)b;calls=0;coverage_enabled=false;reuse_test_observe=false;native_alpha_keyed(&ca,scratch,w,h,left,top,sx,sy,color,opacity,bank,j);unsigned before=calls;calls=0;coverage_enabled=true;reuse_test_observe=true;native_alpha_keyed(&cb,scratch,w,h,left,top,sx,sy,color,opacity,bank,j);assert(before==calls);assert(!memcmp(a,b,80*80*2));pairs++;}
/* Warm cache and every chosen callback fault preserve output and terminal state. */
for(unsigned id=0;id<2;id++){unsigned face=id?16:0;fail_at=0;pair_face(face);unsigned faults[]={1,2,10,60,120,300,1000};for(unsigned k=0;k<7;k++){fail_at=faults[k];pair_face(face);pairs++;}}
fail_at=0;nova_watch_raster_cache_release();assert(!coverage_cache);unsigned oldalloc=allocations;fail_alloc=1;pair_face(0);pair_face(16);pair_face(0);assert(!coverage_cache&&allocations==oldalloc+1);nova_watch_raster_cache_release();fail_alloc=0;pair_face(0);pair_face(0);pair_face(0);assert(coverage_cache);nova_watch_raster_cache_release();nova_watch_raster_cache_release();assert(!coverage_cache);
printf("PASS %u full-buffer/callback pairs; allocation OOM fallback, retry after release, idempotent release; allocation size=%zu metadata=%zu\n",pairs,sizeof(coverage_store),sizeof(coverage_entry)*COVERAGE_SLOTS);
/* Paired same-process host timing; cold transitions and warm frames both count. */
for(unsigned id=0;id<WATCH_FACE_COUNT;id+=id<1?1:7){double ms[2]={0};for(unsigned n=0;n<30;n++)for(unsigned k=0;k<2;k++){coverage_enabled=k;reuse_test_observe=k;risc_display_surface_v1 f={0,a,466,466,932,sizeof(a),5};clock_t t=clock();assert(nova_watch_face_render(&f,&state,id));ms[k]+=1000.0*(clock()-t)/CLOCKS_PER_SEC;}printf("face %u host off %.3f on %.3f ms\n",id,ms[0]/30,ms[1]/30);}
nova_watch_picker_cache *c[2]={nova_watch_picker_cache_create(466,466),nova_watch_picker_cache_create(466,466)};assert(c[0]&&c[1]);double ms[2]={0};unsigned hits=0,misses=0;for(unsigned n=0;n<50;n++){int positions[WATCH_FACE_CATEGORY_COUNT]={0};positions[0]=-(int)(n%25*8)*256;unsigned oldhits=coverage_hits,oldmiss=coverage_misses;for(unsigned k=0;k<2;k++){coverage_enabled=k;reuse_test_observe=k;risc_display_surface_v1 f={0,k?b:a,466,466,932,sizeof(a),5};clock_t t=clock();assert(nova_watch_picker_collections_render(&f,&state,0,0,positions,NULL,0,256,c[k]));ms[k]+=1000.0*(clock()-t)/CLOCKS_PER_SEC;}assert(!memcmp(a,b,sizeof(a)));hits+=coverage_hits-oldhits;misses+=coverage_misses-oldmiss;}
printf("moving picker50 full-buffer pairs host off %.3f on %.3f ms; hits%u misses%u occupancy%u entries%u\n",ms[0]/50,ms[1]/50,hits,misses,coverage_used,coverage_count);nova_watch_picker_cache_destroy(c[0]);nova_watch_picker_cache_destroy(c[1]);nova_watch_raster_cache_release();return 0;}
