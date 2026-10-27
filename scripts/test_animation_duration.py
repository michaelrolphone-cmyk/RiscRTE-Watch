#!/usr/bin/env python3
"""Execute exact production animation bodies with modeled clocks/providers."""
import argparse, hashlib, json, os, subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
r=Path(__file__).resolve().parents[1];s=(r/'apps/clock/crown.c').read_text()
def function(name):
 start=s.index('static bool '+name+'(');brace=s.index('{',start);depth=1;i=brace+1
 while depth:
  depth+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[start:i]+'\n'
prefix=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "transitions/PortableTransition.h"
#define WATCH_CLOCK_LAUNCHER
#define RISC_DISPLAY_FORMAT_RGB565 1
static bool clock_touch_retained,clock_runtime_unavailable,launcher_swipe_pending;
static uint32_t launcher_sampled_at;
static struct {unsigned subscription;} touch;
static bool sample_launcher_touch(uint32_t now,bool force){(void)now;(void)force;return true;}
static uint32_t raw,origin,quantum,cost;static unsigned reads,yields,frames,boot_end,bright,clocks;
static unsigned fail_read,back_read,fail_frame,fail_present;static bool stalled,retain_present;
static uint16_t pixels[4];static unsigned held;
static struct {unsigned width,height;} clock_viewport={2,2};
typedef struct {unsigned token;void*pixels;unsigned width,height,stride_bytes;size_t size_bytes;unsigned pixel_format;} risc_display_surface_v1;
static bool alive(uint32_t*out){if(clock_touch_retained||clock_runtime_unavailable)return false;reads++;if(reads==fail_read){clock_runtime_unavailable=true;return false;}if(reads==back_read)raw-=100;*out=origin+(raw/quantum)*quantum;return true;}
static void yield_ms(uint32_t ms){assert(!clock_touch_retained&&!clock_runtime_unavailable);assert(++yields<100000);if(!stalled)raw+=ms;}
static struct {void(*yield_ms)(uint32_t);} runtime={yield_ms},*rt=&runtime;
static bool brightness(void*c,uint16_t a,uint16_t b){(void)c;(void)a;(void)b;assert(!clock_touch_retained);bright++;return true;}
static void release(void*c,unsigned h){(void)c;(void)h;assert(!clock_touch_retained);}
static struct {void*context;bool(*set_brightness)(void*,uint16_t,uint16_t);void(*release)(void*,unsigned);} provider={0,brightness,release},*display=&provider;
static bool frame(risc_display_surface_v1*s){assert(!clock_touch_retained&&!clock_runtime_unavailable);frames++;if(frames==fail_frame)return false;*s=(risc_display_surface_v1){0,pixels,2,2,4,8,1};held=1;return true;}
static bool draw_clock(uint32_t now,risc_display_surface_v1*s){(void)now;(void)s;clocks++;if(!stalled)raw+=cost;return true;}
static bool watch_boot_render(risc_display_surface_v1*s,unsigned age){memset(s->pixels,0,s->size_bytes);assert(age<=1800);if(age==1800)boot_end++;if(!stalled)raw+=cost;return true;}
static bool present(void){assert(!clock_touch_retained&&!clock_runtime_unavailable);held=0;if(frames==fail_present){clock_touch_retained=retain_present;return false;}if(!stalled)raw+=cost;return true;}
static unsigned clock_brightness(void){return 40;}
typedef struct {uint32_t last;bool observed;} clock_animation_progress;
'''
test=r'''
static void reset(void){raw=0;origin=0;quantum=1;cost=0;reads=yields=frames=boot_end=bright=clocks=0;fail_read=back_read=fail_frame=fail_present=0;stalled=retain_present=clock_touch_retained=clock_runtime_unavailable=false;launcher_swipe_pending=true;}
int main(void){unsigned lanes=0;bool(*functions[])(void)={startup,logo_to_clock,return_to_clock,hold_boot_frame};
 for(unsigned f=0;f<4;f++)for(unsigned c=0;c<4;c++)for(unsigned q=0;q<3;q++)for(unsigned pending=0;pending<2;pending++)for(unsigned wrap=0;wrap<2;wrap++){
  reset();cost=(unsigned[]){0,1,8,50}[c];quantum=(unsigned[]){1,4,16}[q];launcher_swipe_pending=pending;origin=wrap?UINT32_MAX-100:0;
  assert(functions[f]());assert(launcher_swipe_pending==(bool)pending);assert((raw>=(unsigned[]){2230,180,60,250}[f]));
  if(f==0){assert(boot_end>=2&&bright==1&&clocks);if(!cost&&quantum==1)assert(frames>=100&&frames<=103);}
  lanes++;
 }
 for(unsigned f=0;f<4;f++){
  reset();stalled=true;assert(!functions[f]());assert(yields<=84);lanes++;
  reset();origin=1000;cost=1;back_read=3;assert(!functions[f]());lanes++;
  reset();fail_read=3;assert(!functions[f]());assert(clock_runtime_unavailable&&reads==3);lanes++;
  if(f<3){reset();fail_frame=1;assert(!functions[f]());assert(frames==1);lanes++;
   reset();fail_present=f==2?2:1;retain_present=true;assert(!functions[f]());assert(clock_touch_retained&&frames==(f==2?2u:1u));lanes++;}
 }
 reset();clock_animation_progress p={0,false};uint32_t out=99;assert(clock_animation_now(&p,&out)&&out==0);stalled=true;out=99;assert(!clock_animation_now(&p,&out)&&out==99&&yields==64);lanes++;
 reset();assert(pace_frame(0)&&raw==0&&launcher_swipe_pending);assert(pace_animation_frame(0)&&raw==20&&launcher_swipe_pending);lanes++;
 reset();p=(clock_animation_progress){0,false};clock_touch_retained=true;assert(!clock_animation_now(&p,&out)&&!reads&&!yields);reset();clock_runtime_unavailable=true;assert(!clock_animation_now(&p,&out)&&!reads&&!yields);lanes+=2;
 printf("PASS %u actual animation duration/progress scenarios\n",lanes);return 0;
}
'''
source=prefix+''.join(function(x) for x in ['pace_frame_policy','pace_frame','pace_animation_frame','clock_animation_now','hold_boot_frame','logo_to_clock','return_to_clock','startup'])+test
(a.output/'animation.c').write_text(source);rows=[]
for san in [False,True]:
 exe=a.output/('animation-san' if san else 'animation');flags=['-std=c11','-O2','-Wall','-Wextra','-Werror']
 if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie']
 subprocess.run(['cc',*flags,'-I'+str(r/'apps/clock'),str(a.output/'animation.c'),'-o',str(exe)],check=True)
 result=subprocess.run([exe],check=True,text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});print(result.stdout.strip());rows.append({'sanitized':san,'result':result.stdout.strip()})
(a.output/'evidence.json').write_text(json.dumps({'source_sha256':hashlib.sha256(s.encode()).hexdigest(),'source_revision':subprocess.check_output(['git','-C',str(r),'rev-parse','HEAD'],text=True).strip(),'dirty':bool(subprocess.check_output(['git','-C',str(r),'status','--porcelain'],text=True)),'modeled_provider_timing_not_device_measurement':True,'results':rows},indent=2)+'\n')
