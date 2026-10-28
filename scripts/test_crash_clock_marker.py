#!/usr/bin/env python3
"""Exact production crash-step body: typed marker failure stops all later work."""
from pathlib import Path
import argparse,subprocess,os,tempfile,json
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True);r=Path(__file__).resolve().parents[1]
s=(r/'apps/clock/clock_crash.inc').read_text();body=s[s.index('static bool clock_crash_step(void) {'):s.rindex('\n#endif')]
macro=next(l for l in (r/'apps/clock/crown.c').read_text().splitlines() if l.startswith('#define CLOCK_WAKE_MARK(stage) (clock_wake_mark'))
fixture=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static bool clock_runtime_unavailable,clock_handoff,clock_contexts_handoff,launcher_swipe_pending,clock_alarm_modal,clock_crash_modal,launcher_activity_pending;
static unsigned clock_deferred_phase=4,held,operations,mark_count,fail_mark;
static bool clock_display_settled=true;
static struct {bool open;} picker;
static struct {int ui;} clock_quick;
static struct {bool pending,presented;char text[2];unsigned text_size;} clock_crash={.pending=true,.text_size=1};
typedef struct {unsigned ignored;} risc_display_surface_v1;
typedef struct {bool valid;unsigned count;uint8_t id;} watch_launcher_observation;
static int touch;
static void op(void){assert(!clock_runtime_unavailable);operations++;}
static bool pqa_visible(void*p){(void)p;op();return false;}
static bool crash_session_read(void*p){(void)p;op();return true;}
static void clock_wake_mark(const char*s){(void)s;op();if(++mark_count==fail_mark)clock_runtime_unavailable=true;}
static bool clock_crash_prepare(void*p){(void)p;op();return true;}
static bool frame(void*p){(void)p;op();return true;}
static bool nova_watch_crash_report(void*p,const char*t,unsigned size,unsigned offset,uint32_t*next){(void)p;(void)t;(void)offset;op();*next=size;return true;}
static bool present(void){op();return true;}
static bool clock_key_read(uint32_t*e,const char*s){(void)s;op();*e=1;return true;}
static bool clock_crash_safe(void*p){(void)p;op();return true;}
static bool alive(uint32_t*n){op();*n=0;return true;}
static watch_launcher_observation launcher_touch_capture(void*p,unsigned n){(void)p;(void)n;op();return (watch_launcher_observation){.valid=true};}
static void yield(unsigned n){(void)n;op();}
static const struct {void(*yield_ms)(unsigned);} runtime={yield},*rt=&runtime;
static bool crash_session_ack(void*p){(void)p;op();return true;}
static void watch_face_reset_contact(void*p){(void)p;op();}
static int pqa_input(void*p,unsigned n,bool v,unsigned c,unsigned id,unsigned x,unsigned y,bool final){(void)p;(void)n;(void)v;(void)c;(void)id;(void)x;(void)y;(void)final;op();return 0;}
static bool crash_session_checkpoint(void*p){(void)p;op();return true;}
'''
main='''
int main(void){for(unsigned fault=0;fault<=2;fault++){fail_mark=fault;mark_count=operations=0;clock_runtime_unavailable=false;clock_crash.pending=true;clock_crash.presented=false;clock_crash_modal=false;bool ok=clock_crash_step();assert(ok==(fault==0));assert(clock_runtime_unavailable==(fault!=0));assert(mark_count==(fault?fault:2));printf("crash marker fault=%u operations=%u stop=%u\\n",fault,operations,!ok);}return 0;}
'''
rows=[]
with tempfile.TemporaryDirectory(dir=a.output) as tmp:
 t=Path(tmp);(t/'test.c').write_text(fixture+'\n'+macro+'\n'+body+'\n'+main)
 for san in [False,True]:
  f=['-std=c11','-O1','-Wall','-Wextra','-Werror']+(['-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie'] if san else [])
  subprocess.run(['cc',*f,str(t/'test.c'),'-o',str(t/'test')],check=True);v=subprocess.run([t/'test'],check=True,text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});rows.append({'sanitized':san,'stdout':v.stdout});print(v.stdout,end='')
(a.output/'receipt.json').write_text(json.dumps(rows,indent=2)+'\n')
