#!/usr/bin/env python3
"""Exact pacing and sampler bodies; mocked acquisition costs/authority boundaries."""
import argparse,subprocess,os,json,hashlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
r=Path(__file__).resolve().parents[1];text=(r/'apps/clock/crown.c').read_text()
def function(name,typ):
 start=text.index('static '+typ+' '+name+'(');i=text.index('{',start)+1;depth=1
 while depth:depth+=(text[i]=='{')-(text[i]=='}');i+=1
 return text[start:i]+'\n'
prefix=r'''
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
static bool clock_touch_retained,clock_runtime_unavailable,clock_contexts_uncertain,launcher_swipe_pending,clock_crash_modal,clock_alarm_modal;
static uint32_t launcher_sampled_at,raw,quantum=1,origin,event_at,cost;static unsigned reads,samples,yields,contexts,callbacks,last_sample;
static unsigned fault_read,fault_sample,fault_context,fault_kind,halt_read;static bool stopped,backwards;static unsigned picker;
static struct {unsigned subscription;} touch;
static void allowed(void){assert(!clock_touch_retained&&!clock_runtime_unavailable&&!clock_contexts_uncertain);++callbacks;}
static struct{bool halted;}native_clock;
static bool alive(uint32_t*out){if(clock_touch_retained||clock_runtime_unavailable)return false;allowed();++reads;if(reads==fault_read){clock_runtime_unavailable=true;return false;}*out=origin+(raw/quantum)*quantum;if(reads==halt_read)native_clock.halted=true;return true;}
static void yield_ms(uint32_t ms){allowed();assert(ms&&ms<=8);++yields;if(!stopped)raw=backwards?raw-ms:raw+ms;}
static struct{void(*yield_ms)(uint32_t);}runtime={yield_ms},*rt=&runtime;
static bool clock_broadcast_step(void){allowed();return true;}
static int clock_contexts;
static bool portable_contexts_capture(void*c){(void)c;allowed();++contexts;if(contexts==fault_context)return false;return true;}
static bool portable_contexts_retain(void*c){(void)c;return true;}
static unsigned launcher_touch_sample(void*t,void*p,uint32_t now,bool*activity){(void)t;(void)p;allowed();if(samples)assert((uint32_t)(now-last_sample)>=8);last_sample=now;++samples;raw+=cost;*activity=true;if(samples==fault_sample){if(fault_kind)clock_runtime_unavailable=true;else clock_touch_retained=true;return 0;}return raw>=event_at;}
static void launcher_touch_latch(unsigned action,bool activity){(void)activity;if(action)launcher_swipe_pending=true;}
'''
body='#ifdef WATCH_CONTEXTS_CLIENT\n'+function('clock_contexts_capture_checkpoint','bool')+'#endif\n'+function('sample_launcher_touch','bool')+function('pace_frame_policy','bool')
tests=r'''
static void reset(void){clock_touch_retained=clock_runtime_unavailable=clock_contexts_uncertain=launcher_swipe_pending=clock_crash_modal=clock_alarm_modal=false;launcher_sampled_at=raw=origin=cost=reads=samples=yields=contexts=callbacks=last_sample=fault_read=fault_sample=fault_context=fault_kind=halt_read=0;touch.subscription=1;native_clock.halted=false;quantum=1;event_at=~0u;stopped=backwards=false;}
int main(void){unsigned cases=0;
 for(unsigned age=0;age<=50;age+=(age<20?1:30))for(unsigned q=1;q<=8;q*=2)for(unsigned c=0;c<=2;++c)for(unsigned phase=0;phase<20;++phase)for(unsigned animation=0;animation<2;++animation){
  reset();raw=age;quantum=q;cost=c;event_at=phase;
  assert(pace_frame_policy(0,!animation));
  if(animation||!launcher_swipe_pending)assert((raw/quantum)*quantum>=20);
  if(age<20)assert(raw<=20+quantum+cost+8);assert(samples<=3);cases++;
 }
 reset();launcher_swipe_pending=true;assert(pace_frame_policy(0,true)&&!raw&&!samples);assert(pace_frame_policy(0,false)&&raw==20&&launcher_swipe_pending);cases++;
 for(unsigned kind=0;kind<2;++kind){reset();fault_sample=1;fault_kind=kind;assert(!pace_frame_policy(0,false));assert(samples==1);cases++;}
 for(unsigned n=1;n<=8;++n){reset();fault_read=n;bool ok=pace_frame_policy(0,false);if(clock_runtime_unavailable){assert(!ok&&reads==n);}cases++;}
#ifdef WATCH_CONTEXTS_CLIENT
 reset();fault_context=1;assert(!pace_frame_policy(0,false));assert(!samples&&clock_contexts_uncertain);cases++;
#endif
#ifdef WATCH_CRASH_REPORT
 reset();clock_crash_modal=true;event_at=0;assert(pace_frame_policy(0,false)&&raw==20&&!samples&&!launcher_swipe_pending);cases++;
#endif
 reset();clock_alarm_modal=true;event_at=0;assert(pace_frame_policy(0,false)&&raw==20&&!samples);cases++;
#ifdef WATCH_RUNTIME_FEATURES
 reset();native_clock.halted=true;assert(!pace_frame_policy(0,false)&&!callbacks&&!samples&&!yields);cases++;
 reset();halt_read=1;assert(!pace_frame_policy(0,false)&&native_clock.halted&&reads==1&&!samples&&!contexts&&!yields);assert(!clock_contexts_uncertain&&!clock_touch_retained&&!clock_runtime_unavailable);cases++;
#endif
 reset();stopped=true;assert(!pace_frame_policy(0,false)&&yields==64);cases++;
 reset();raw=100;backwards=true;assert(!pace_frame_policy(100,false)&&yields==1);cases++;
 reset();origin=UINT32_MAX-10;assert(pace_frame_policy(origin,false)&&raw==20);cases++;
 reset();clock_touch_retained=true;assert(!pace_frame_policy(0,false)&&!callbacks);reset();clock_runtime_unavailable=true;assert(!pace_frame_policy(0,false)&&!callbacks);cases+=2;
 printf("PASS %u pacing cadence/latch/deadline/authority cases\n",cases);
}
'''
source=prefix+body+tests;(a.output/'pacing.c').write_text(source);results=[]
for contexts in (False,True):
 for crash in (False,True):
  for san in (False,True):
   label=f'contexts{int(contexts)}-crash{int(crash)}-san{int(san)}';exe=a.output/label
   cmd=['cc','-std=c11','-O1','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-misleading-indentation','-DWATCH_CLOCK_LAUNCHER','-DWATCH_CLOCK_ALARMS','-DWATCH_BLE_BROADCAST']
   if contexts:cmd+=['-DWATCH_CONTEXTS_CLIENT','-DWATCH_RUNTIME_FEATURES']
   if crash:cmd+=['-DWATCH_CRASH_REPORT']
   if san:cmd+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie']
   cmd+=[str(a.output/'pacing.c'),'-o',str(exe)];subprocess.run(cmd,check=True)
   result=subprocess.run([str(exe)],capture_output=True,text=True,env=os.environ|{'ASAN_OPTIONS':'detect_leaks=0'});(a.output/(label+'.log')).write_text(result.stdout+result.stderr);assert not result.returncode,(label,result.stderr);results.append({'profile':label,'result':result.stdout.strip()});print(label,result.stdout.strip(),flush=True)
# Non-launcher compositions retain the same deadline and Contexts service path.
no_launcher=prefix+body+tests[:tests.index('int main(void)')]+r"""
int main(void){reset();assert(pace_frame_policy(0,false)&&raw==20&&!samples);
 reset();stopped=true;assert(!pace_frame_policy(0,false)&&yields==64);
 reset();fault_read=3;assert(!pace_frame_policy(0,false)&&clock_runtime_unavailable);
#ifdef WATCH_CONTEXTS_CLIENT
 reset();fault_context=1;assert(!pace_frame_policy(0,false)&&clock_contexts_uncertain);
#endif
 puts("PASS non-launcher deadline/authority/Contexts");}
"""
(a.output/'no-launcher.c').write_text(no_launcher)
for contexts in (False,True):
 for san in (False,True):
  label=f'no-launcher-contexts{int(contexts)}-san{int(san)}';exe=a.output/label
  cmd=['cc','-std=c11','-O1','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-misleading-indentation']
  if contexts:cmd+=['-DWATCH_CONTEXTS_CLIENT','-DWATCH_RUNTIME_FEATURES']
  if san:cmd+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie']
  cmd+=[str(a.output/'no-launcher.c'),'-o',str(exe)];subprocess.run(cmd,check=True)
  result=subprocess.run([str(exe)],capture_output=True,text=True,env=os.environ|{'ASAN_OPTIONS':'detect_leaks=0'});assert not result.returncode,(label,result.stderr)
  results.append({'profile':label,'result':result.stdout.strip()});print(label,result.stdout.strip(),flush=True)
(a.output/'receipt.json').write_text(json.dumps({'source_sha256':hashlib.sha256(text.encode()).hexdigest(),'results':results,'model_not_device':True},indent=2))
