#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <dlfcn.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "apps/clock/nova/nova.h"
typedef struct {
 bool(*face)(risc_display_surface_v1*,const nova_watch_state*,unsigned);
 bool(*picker)(risc_display_surface_v1*,const nova_watch_state*,unsigned);
 void(*admission_selection)(unsigned);void(*release)(void);void(*control)(unsigned,unsigned,bool);void(*info)(uint64_t*);const void*(*image)(void);
} module;
#ifndef WATCH_DAMAGE_UNSUPPORTED_RETRY
#define WATCH_DAMAGE_UNSUPPORTED_RETRY 4u
#endif
static module selected,reference;
static unsigned calls,fail_at,late;static bool failed;static uint16_t pixels[640*640],oracle[640*640],frozen[640*640],owned[640*640];static size_t bytes,owner_bytes;
static bool capture(void*p){(void)p;if(failed)late++;calls++;uint64_t info[8]={0};selected.info(info);if((fail_at&&calls>=fail_at)||info[2]){if(!failed){memcpy(frozen,pixels,bytes);owner_bytes=(size_t)info[5];if(owner_bytes)memcpy(owned,selected.image(),owner_bytes);}failed=true;selected.control(0,0,true);return false;}return true;}
static uint64_t now_ns(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC_RAW,&t);return(uint64_t)t.tv_sec*1000000000u+t.tv_nsec;}
static module load(const char*path){void*h=dlopen(path,RTLD_NOW|RTLD_LOCAL);if(!h){fprintf(stderr,"%s\n",dlerror());abort();}module m={0};*(void**)&m.face=dlsym(h,"probe_face");*(void**)&m.picker=dlsym(h,"probe_picker");*(void**)&m.release=dlsym(h,"probe_release");*(void**)&m.control=dlsym(h,"probe_control");*(void**)&m.info=dlsym(h,"probe_info");*(void**)&m.image=dlsym(h,"probe_image");*(void**)&m.admission_selection=dlsym(h,"probe_admission_selection");assert(m.admission_selection);assert(m.face&&m.picker&&m.release&&m.control&&m.info&&m.image);return m;}
static nova_watch_state state(unsigned tick){nova_watch_state s={.time={.year=2026,.month=10,.day=26,.hour=9,.minute=2+(58+tick/20)/60,.second=(58+tick/20)%60},.time_valid=true,.battery_valid=true,.battery_percent=73,.subsecond_ms=tick%20*50,.animation_ms=tick*50};return s;}
static risc_display_surface_v1 surface(unsigned side,void*p){return(risc_display_surface_v1){0,p,side,side,side*2,side*side*2,5};}
static void reset(void){selected.control(0,0,false);selected.release();reference.release();calls=fail_at=late=0;failed=false;bytes=466*466*2;owner_bytes=0;}
static void pair(nova_watch_state*s,unsigned face,unsigned side){risc_display_surface_v1 a=surface(side,pixels),b=surface(side,oracle);s->capture_audio=NULL;assert(selected.face(&a,s,face));assert(reference.face(&b,s,face));assert(!memcmp(pixels,oracle,a.size_bytes));}
int main(int argc,char**argv){assert(argc==3);reference=load(argv[1]);selected=load(argv[2]);unsigned pairs=0;
 for(unsigned face=0;face<WATCH_FACE_COUNT;face++){reset();uint64_t t[2]={0};for(unsigned n=0;n<80;n++){nova_watch_state s=state(n);risc_display_surface_v1 a=surface(466,pixels),b=surface(466,oracle);uint64_t start,dt[2]={0};if(n&1){start=now_ns();assert(reference.face(&b,&s,face));dt[0]=now_ns()-start;}start=now_ns();assert(selected.face(&a,&s,face));dt[1]=now_ns()-start;if(!(n&1)){start=now_ns();assert(reference.face(&b,&s,face));dt[0]=now_ns()-start;}assert(!memcmp(pixels,oracle,a.size_bytes));if(n>=5){t[0]+=dt[0];t[1]+=dt[1];}pairs++;}printf("face%u baseline_ms=%.4f selected_ms=%.4f ratio=%.4f\n",face,t[0]/75000000.,t[1]/75000000.,(double)t[1]/t[0]);}
 unsigned sizes[]={240,300,466,640};for(unsigned k=0;k<4;k++)for(unsigned face=0;face<8;face++){reset();for(unsigned n=0;n<12;n++){nova_watch_state s=state(n*5);pair(&s,face,sizes[k]);pairs++;}}
 /* Copied state variation, including rollovers and optional metadata changes. */
 for(unsigned face=0;face<WATCH_FACE_COUNT;face++){reset();for(unsigned n=0;n<16;n++){nova_watch_state v=state(n*17);v.hour_24=!!(n&1);v.time_valid=!!(n&2);v.battery_valid=!!(n&4);v.battery_percent=n*7;v.time.hour=n%3==0?0:n%3==1?12:23;v.time.minute=n&1?59:0;v.time.second=n&2?59:0;v.contexts.enabled=true;v.contexts.ready=!!(n&1);v.contexts.room_valid=true;v.contexts.event_valid=true;strcpy(v.contexts.room,n&1?"Room":"Other room");strcpy(v.contexts.event,n&2?"Event":"Other event");pair(&v,face,466);pairs++;}}
 /* Count failed collectors independently of wall-clock noise. */
 reset();{unsigned planned=0;for(unsigned n=0;n<80;n++){uint64_t q[8]={0};selected.info(q);planned+=q[7]==0;nova_watch_state v=state(n);pair(&v,31,466);pairs++;}assert(planned==(80+WATCH_DAMAGE_UNSUPPORTED_RETRY-1)/WATCH_DAMAGE_UNSUPPORTED_RETRY);printf("unsupported admission: %u planning attempts in 80 full frames\n",planned);}
 /* Actual barrier and synthetic same-selection supported-stream rediscovery.
  * Current unknown collections always reach raw rounded battery/collection draws. */
 reset();{nova_watch_state v=state(0);pair(&v,31,466);uint64_t status[8]={0};selected.info(status);assert(status[7]==WATCH_DAMAGE_UNSUPPORTED_RETRY-1);selected.admission_selection(0);for(unsigned n=0;n<WATCH_DAMAGE_UNSUPPORTED_RETRY;n++){pair(&v,0,466);pairs++;}selected.info(status);assert(status[3]&&!status[7]);pair(&v,31,466);pair(&v,0,466);selected.info(status);assert(!status[7]&&status[3]);pair(&v,31,466);pair(&v,31,300);selected.info(status);assert(!status[7]);}
 /* Caller overlay and picker writes never mutate the completed-image owner. */
 reset();for(unsigned n=0;n<32;n++){nova_watch_state s=state(n);pair(&s,n%8,466);pairs++;risc_display_surface_v1 a=surface(466,pixels),b=surface(466,oracle);assert(selected.picker(&a,&s,n));assert(reference.picker(&b,&s,n));assert(!memcmp(pixels,oracle,a.size_bytes));pairs++;memset(pixels,0xa5,sizeof(pixels));pair(&s,n%8,466);pairs++;}
 /* Picker-first cannot allocate the independent full-face damage owner. */
 reset();uint64_t before[8]={0},after[8]={0};selected.info(before);nova_watch_state s=state(0);risc_display_surface_v1 f=surface(466,pixels);assert(selected.picker(&f,&s,8));selected.info(after);assert(before[0]==after[0]&&!after[6]);
 for(unsigned fail=1;fail<=2;fail++){reset();selected.info(before);selected.control((unsigned)before[0]+fail,0,false);for(unsigned n=0;n<4;n++){s=state(n);pair(&s,n,466);pairs++;}selected.info(after);assert(after[0]-before[0]==fail);selected.control(0,0,false);selected.release();}
 for(unsigned retain=1;retain<=2;retain++){reset();selected.info(before);selected.control(0,(unsigned)before[0]+retain,false);s=state(0);s.capture_audio=capture;calls=0;assert(!selected.face(&f,&s,0));selected.info(after);assert(failed&&!late&&after[0]-before[0]==retain&&after[1]==before[1]);assert(!memcmp(pixels,frozen,bytes));selected.control(0,0,false);selected.release();failed=false;}
 unsigned faults=0;for(unsigned mode=0;mode<3;mode++){reset();s=state(0);s.time.second=45;s.subsecond_ms=500;s.capture_audio=capture;unsigned fault_face=mode==2?31:0;if(mode)for(unsigned i=0;i<(mode==2?1u:4u);i++)assert(selected.face(&f,&s,fault_face));calls=0;assert(selected.face(&f,&s,fault_face));unsigned limit=calls;printf("fault_mode%u positive_callbacks%u\n",mode,limit);
 for(unsigned point=1;point<=limit+1;point++){reset();s.capture_audio=capture;unsigned fault_face=mode==2?31:0;if(mode)for(unsigned i=0;i<(mode==2?1u:4u);i++)assert(selected.face(&f,&s,fault_face));calls=0;fail_at=point;bool ok=selected.face(&f,&s,fault_face);if(failed){selected.info(after);assert(!ok&&!late&&!after[3]&&!memcmp(pixels,frozen,bytes));if(owner_bytes)assert(!memcmp(selected.image(),owned,owner_bytes));}else assert(ok);selected.control(0,0,false);fail_at=0;failed=false;s.capture_audio=NULL;pair(&s,0,466);faults++;}}
 reset();printf("PASS %u exact frame pairs; %u complete positive-trace fault positions; allocation-terminal/OOM/picker/overlay/release cases\n",pairs,faults);return 0;}
