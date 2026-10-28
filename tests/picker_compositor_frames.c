#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dlfcn.h>
#include "../apps/clock/nova/nova.h"
#include "../apps/clock/faces/catalog.h"
typedef struct {void*h;void*(*create)(unsigned,unsigned);void(*destroy)(void*);bool(*render)(uint16_t*,unsigned,unsigned,const nova_watch_state*,unsigned,int,const int*,unsigned,unsigned,void*);} module;
static module open_module(const char*path){module m={0};m.h=dlopen(path,RTLD_NOW|RTLD_LOCAL);if(!m.h){fprintf(stderr,"%s\n",dlerror());abort();}m.create=dlsym(m.h,"probe_create");m.destroy=dlsym(m.h,"probe_destroy");m.render=dlsym(m.h,"probe_render");assert(m.create&&m.destroy&&m.render);return m;}
typedef struct{unsigned calls,fail_at;} observer;
static bool capture(void*p){observer*o=p;o->calls++;return !o->fail_at||o->calls<o->fail_at;}
int main(int argc,char**argv){assert(argc==3);module m[2]={open_module(argv[1]),open_module(argv[2])};unsigned dimensions[][2]={{466,466},{240,240},{240,466},{410,502},{640,240},{300,300},{160,160}};unsigned speeds[]={1,2,8,32},pairs=0;
for(unsigned d=0;d<7;d++)for(unsigned cat=0;cat<WATCH_FACE_CATEGORY_COUNT;cat++)for(unsigned speed=0;speed<4;speed++){
 unsigned w=dimensions[d][0],h=dimensions[d][1],side=w<h?w:h,frames=d?12:100;uint16_t*p[2]={malloc(w*h*2),malloc(w*h*2)};void*c[2]={m[0].create(w,h),m[1].create(w,h)};assert(p[0]&&p[1]&&c[0]&&c[1]);double elapsed[2]={0};const watch_face_page*page=watch_face_page_for(cat);unsigned extent=(page->count-1)*(side*108u/240u);if(!extent)extent=1;
 for(unsigned n=0;n<frames;n++){int positions[WATCH_FACE_CATEGORY_COUNT]={0};unsigned phase=(n*speeds[speed])%(2*extent);positions[cat]=-(int)(phase<=extent?phase:2*extent-phase)*256;unsigned selected=page->ids[0];nova_watch_state s={.time={.year=2026,.month=10,.day=13,.hour=21,.minute=37+n/50,.second=(48+n/20)%60},.time_valid=true,.battery_valid=true,.battery_percent=n<25?73:42,.subsecond_ms=(n%20)*50,.animation_ms=n*50,.hour_24=n>=75,.capture_audio=capture};observer o[2]={{0,0},{0,0}};bool ok[2];
  for(unsigned order=0;order<2;order++){unsigned k=(order+n)%2;s.capture_context=&o[k];memset(p[k],0xa5,w*h*2);clock_t t=clock();ok[k]=m[k].render(p[k],w,h,&s,selected,-(int)(cat*h*256),positions,selected,n%10?256:277,c[k]);elapsed[k]+=1000.0*(clock()-t)/CLOCKS_PER_SEC;}
  if(ok[0]!=ok[1]||o[0].calls!=o[1].calls||memcmp(p[0],p[1],w*h*2)){fprintf(stderr,"MISMATCH %ux%u cat%u speed%u frame%u callbacks%u/%u result%d/%d\n",w,h,cat,speeds[speed],n,o[0].calls,o[1].calls,ok[0],ok[1]);abort();}pairs++;
 }
 printf("%ux%u category%u speed%u pairs%u reference_ms=%.4f planned_ms=%.4f ratio=%.4f\n",w,h,cat,speeds[speed],frames,elapsed[0]/frames,elapsed[1]/frames,elapsed[1]/elapsed[0]);fflush(stdout);for(unsigned k=0;k<2;k++){m[k].destroy(c[k]);free(p[k]);}
}
/* Actual two-category transitions exercise ordered overlap and category wrap. */
for(unsigned cat=0;cat<WATCH_FACE_CATEGORY_COUNT;cat++)for(unsigned fault=0;fault<6;fault++){
 unsigned w=466,h=466;uint16_t*p[2]={malloc(w*h*2),malloc(w*h*2)};void*c[2]={m[0].create(w,h),m[1].create(w,h)};assert(p[0]&&p[1]&&c[0]&&c[1]);
 for(unsigned n=0;n<20;n++){int positions[WATCH_FACE_CATEGORY_COUNT];for(unsigned j=0;j<WATCH_FACE_CATEGORY_COUNT;j++)positions[j]=-(int)(j*17)*256;unsigned fails[]={0,1,60,200,600,1200};observer o[2]={{0,fails[fault]},{0,fails[fault]}};bool ok[2];nova_watch_state s={.time={.year=2026,.month=10,.day=13,.hour=21,.minute=37,.second=49},.time_valid=true,.battery_valid=true,.battery_percent=73,.subsecond_ms=n*50,.capture_audio=capture};for(unsigned k=0;k<2;k++){memset(p[k],0xa5,w*h*2);s.capture_context=&o[k];ok[k]=m[k].render(p[k],w,h,&s,0,-(int)((cat*h+n*h/20)*256),positions,0,256,c[k]);}assert(ok[0]==ok[1]&&o[0].calls==o[1].calls&&!memcmp(p[0],p[1],w*h*2));pairs++;}
 for(unsigned k=0;k<2;k++){m[k].destroy(c[k]);free(p[k]);}
}
printf("PASS %u complete picker framebuffer/callback pairs, independent renderer/cache state, original publication reference\n",pairs);dlclose(m[0].h);dlclose(m[1].h);}
