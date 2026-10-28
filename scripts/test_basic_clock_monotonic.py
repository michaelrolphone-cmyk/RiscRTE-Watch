#!/usr/bin/env python3
"""Actual basic clock app_main/render with small modeled capability tables."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
def main():
 p=argparse.ArgumentParser();p.add_argument('--watch',type=Path,required=True);p.add_argument('--clock-sdk',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
 root=Path(__file__).resolve().parents[1];a.output.mkdir(parents=True,exist_ok=True)
 sources=[root/'tests/basic_clock_monotonic.c',a.watch/'apps/clock/main.c',a.watch/'apps/clock/render.c']
 cmd=['cc','-std=c11','-Wall','-Wextra','-Werror','-O1','-g',*(['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if a.sanitize else []),*['-I'+str(x) for x in [a.clock_sdk,a.watch/'sdk/app',a.watch/'sdk/driver',a.watch/'include',a.watch/'apps/clock']],*map(str,sources),'-o',str(a.output/'execute')]
 subprocess.run(cmd,check=True)
 run=subprocess.run([str(a.output/'execute')],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'})
 (a.output/'stdout.log').write_text(run.stdout);(a.output/'stderr.log').write_text(run.stderr);run.check_returncode()
 lines=run.stdout.splitlines();assert len(lines)==21 and all(x.startswith('PASS basic clock ') for x in lines)
 inputs=[*sources,a.clock_sdk/'RiscRuntimeClockV1.h',a.clock_sdk/'RiscRuntimeV1.h',a.watch/'apps/clock/render.h',*sorted((a.watch/'sdk/driver').glob('*.h')),*sorted((a.watch/'include').glob('*.h'))]
 sha=lambda f:hashlib.sha256(f.read_bytes()).hexdigest()
 receipt={'passed':True,'cases':21,'sanitize':a.sanitize,'actual_app_main':True,'actual_renderer':True,'physical_tested':False,'command':cmd,'source_sha256':{str(x):sha(x) for x in inputs if x.exists()},'stdout_sha256':sha(a.output/'stdout.log'),'scope':['zero and wrap valid','1000ms refresh boundary','pending completion and strict >10000ms timeout across wrap','current suffix failure never retries health','legacy prefix and null suffix use health','RTC invalid remains unset','submit/layout failure releases held frame then RTC/display grants','current/legacy pixels, yield counts, diagnostics and unwind event order identical'],'limits':['Modeled 240x240 display and RTC; not Crown or device execution','Existing cleanup after clock failure preserved, not strengthened','ASan/UBSan only; leak detection disabled']}
 (a.output/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps({'passed':True,'cases':21,'sanitize':a.sanitize}))
if __name__=='__main__':main()
