#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dlfcn.h>
#include "../apps/clock/nova/nova.h"
#include "../apps/clock/faces/catalog.h"
typedef bool(*render_fn)(uint16_t*,unsigned,unsigned,const nova_watch_state*,unsigned);
typedef struct{unsigned calls,fail;} observer;
static bool capture(void*p){observer*o=p;return ++o->calls!=o->fail;}
int main(int argc,char**argv){assert(argc==3);void*h[2];render_fn render[2];uint16_t*p[2];for(unsigned k=0;k<2;k++){h[k]=dlopen(argv[k+1],RTLD_NOW|RTLD_LOCAL);assert(h[k]);render[k]=dlsym(h[k],"probe_face");assert(render[k]);p[k]=malloc(466*466*2);assert(p[k]);}
 unsigned pairs=0;for(unsigned cat=0;cat<WATCH_FACE_CATEGORY_COUNT;cat++)for(unsigned i=0;i<watch_face_pages[cat].count;i++){
 unsigned id=watch_face_pages[cat].ids[i];double elapsed[2]={0};for(unsigned n=0;n<30;n++){observer o[2]={{0,0},{0,0}};if(n>=25)o[0].fail=o[1].fail=(n-24)*50;
 nova_watch_state s={.time={.year=2026,.month=10,.day=13,.hour=21,.minute=37,.second=49+n/20},.time_valid=true,.battery_valid=true,.battery_percent=73,.subsecond_ms=(n%20)*50,.animation_ms=n*50,.capture_audio=capture};bool ok[2];
 for(unsigned order=0;order<2;order++){unsigned k=(order+n)%2;memset(p[k],0xa5,466*466*2);s.capture_context=&o[k];clock_t t=clock();ok[k]=render[k](p[k],466,466,&s,id);if(n>=5&&n<25)elapsed[k]+=1000.0*(clock()-t)/CLOCKS_PER_SEC;}
 assert(ok[0]==ok[1]&&o[0].calls==o[1].calls&&!memcmp(p[0],p[1],466*466*2));pairs++;}
 printf("face%u reference_ms=%.6f candidate_ms=%.6f ratio=%.6f\n",id,elapsed[0]/20,elapsed[1]/20,elapsed[1]/elapsed[0]);fflush(stdout);}
 printf("PASS %u face framebuffer/callback pairs including checkpoint failures\n",pairs);for(unsigned k=0;k<2;k++){free(p[k]);dlclose(h[k]);}}
