#!/usr/bin/env python3
"""Compile the exact Home alive() body against current and legacy Runtime ABI."""
import argparse,json,os,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--runtime-sdk',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
r=Path(__file__).resolve().parents[1];s=(r/'apps/clock/crown.c').read_text();start=s.index('static bool alive(uint32_t *ms) {');end=s.index('\n}\n',start)+3;body=s[start:end]
prefix='''#include <assert.h>
#include <stdio.h>
#include "RiscRuntimeV1.h"
static const risc_runtime_api_v1 *rt;
static bool clock_touch_retained,clock_runtime_unavailable;
static unsigned hc,mc;static bool ok=true;static uint32_t tick;
static bool health(risc_runtime_health_v1*h){assert(!clock_touch_retained);hc++;h->uptime_ms=tick;return ok;}
#ifdef RISC_RUNTIME_MONOTONIC_V1_SIZE
static bool monotonic(uint32_t*p){assert(!clock_touch_retained);mc++;if(!ok)return false;*p=tick;return true;}
#endif
'''
test='''
int main(void){
 risc_runtime_api_v1 api={.api_version=1,.struct_size=sizeof(api),.health=health};rt=&api;uint32_t out=0;
 tick=123;assert(alive(&out)&&out==123&&hc==1&&!mc);
#ifdef RISC_RUNTIME_MONOTONIC_V1_SIZE
 api.monotonic_ms=monotonic;
 for(unsigned n=0;n<100000;n++){tick=n;assert(alive(&out)&&out==n);}
 assert(mc==100000&&hc==1);tick=UINT32_MAX;assert(alive(&out)&&out==UINT32_MAX);tick=0;assert(alive(&out)&&out==0);
 ok=false;assert(!alive(&out)&&hc==1&&clock_runtime_unavailable);clock_runtime_unavailable=false;ok=true;
 api.struct_size=RISC_RUNTIME_MONOTONIC_V1_SIZE-1;assert(alive(&out)&&hc==2);
 api.struct_size=sizeof(api);api.monotonic_ms=NULL;assert(alive(&out)&&hc==3);
 api.monotonic_ms=monotonic;
#endif
 unsigned h=hc,m=mc;clock_touch_retained=true;assert(!alive(&out)&&hc==h&&mc==m);
 clock_touch_retained=false;ok=false;
#ifdef RISC_RUNTIME_MONOTONIC_V1_SIZE
 api.monotonic_ms=NULL;
#endif
 assert(!alive(&out));puts("PASS exact Home clock: current/legacy/null/failure/wrap/retained boundaries");return 0;
}
'''
rows=[]
with tempfile.TemporaryDirectory(dir=a.output,prefix='clock-') as tmp:
 t=Path(tmp)
 for legacy in [False,True]:
  source=prefix+ ('#undef RISC_RUNTIME_MONOTONIC_V1_SIZE\n' if legacy else '')+'#include "RiscRuntimeClockV1.h"\n'+body+test
  (t/'test.c').write_text(source)
  for san in [False,True]:
   f=['-std=c11','-O2','-Wall','-Wextra','-Werror']
   if legacy:f+=['-Wno-unused-function']
   if san:f+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie']
   exe=t/'test';subprocess.run(['cc',*f,'-I'+str(a.runtime_sdk),'-I'+str(r/'sdk/app'),str(t/'test.c'),'-o',str(exe)],check=True)
   v=subprocess.run([exe],check=True,capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
   rows.append({'legacy_suffix_disabled':legacy,'sanitized':san,'result':v.stdout.strip()});print(rows[-1],flush=True)
(a.output/'clock-receipt.json').write_text(json.dumps(rows,indent=2)+'\n')
