#!/usr/bin/env python3
"""Actual Watch alarm-sleep header qualification; no hardware."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
from merged_sdk import headers
p=argparse.ArgumentParser()
for n in ('system','utilities','output'):p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args();root=Path(__file__).resolve().parents[1];a.output.mkdir(parents=True,exist_ok=True)
sdk=headers(a.output,a.system,a.utilities,root);runs=[]
for mode in ('normal','sanitized'):
 flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if mode=='sanitized' else []
 binary=a.output/mode
 cmd=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-I'+str(sdk),'-I'+str(root/'include'),'-I'+str(root/'apps/clock'),str(root/'tests/shared_home_alarm_sleep_test.c'),'-o',str(binary)]
 subprocess.run(cmd,check=True)
 r=subprocess.run([str(binary)],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
 print(r.stdout,end='');runs.append(dict(mode=mode,command=cmd,output=r.stdout))
(a.output/'receipt.json').write_text(json.dumps(dict(hardware_tested=False,runs=runs,source_sha256={p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in ('apps/clock/watch_alarm_sleep.h','tests/shared_home_alarm_sleep_test.c')}),indent=2)+'\n')
