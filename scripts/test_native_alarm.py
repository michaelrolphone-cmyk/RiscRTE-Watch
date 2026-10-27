#!/usr/bin/env python3
"""Owning API2 sleep/client tests; no physical or whole-product claim."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
from merged_sdk import headers
from build_inputs import verify_dependency,file_manifest
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--clock-edit-authority',action='store_true');p.add_argument('--native-quick-actions',action='store_true',help='Use explicit native Quick dependency profile and matching client flags');p.add_argument('--system',type=Path,required=True);p.add_argument('--utilities',type=Path,required=True);p.add_argument('--output',type=Path,default=ROOT/'build/native-alarm');a=p.parse_args()
if a.native_quick_actions and not a.clock_edit_authority:p.error('--native-quick-actions requires --clock-edit-authority')
s=a.system.resolve();u=a.utilities.resolve();o=a.output.resolve();o.mkdir(parents=True,exist_ok=True)
profile_path='profiles/native-quick-actions.json' if a.native_quick_actions else 'profiles/native-clock-edit.json' if a.clock_edit_authority else 'profiles/native-alarm-v2.json'
profile=json.loads((ROOT/profile_path).read_text())
verify_dependency(s,profile['system'],['lib/PortableApps']);verify_dependency(u,profile['utilities'],['lib/Alarm/include'])
inc=headers(o,s,u,ROOT);results=[]
for source in ('native_alarm_sleep_test.c','native_alarm_owner_test.c','test_generic_sleep.c'):
 for sanitized in (False,True):
  binary=o/(source+str(int(sanitized)));command=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-Wl,--gc-sections',*(['-DALARM_SERVICE_CLOCK_EDIT_V2'] if a.clock_edit_authority else []),*(['-DPORTABLE_QUICK_CAPABILITY_SAFE','-DPORTABLE_QUICK_SAFE_CONTENT'] if a.native_quick_actions else []),*(['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []),*['-I'+str(x) for x in (inc,ROOT/'include',ROOT/'apps/clock',ROOT)],str(ROOT/'tests'/source),'-o',str(binary)]
  subprocess.run(command,check=True);result=subprocess.run([binary],check=True,text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'});print(result.stdout,end='');results.append({'source':source,'sanitized':sanitized,'command':command,'stdout':result.stdout})
(o/'receipt.json').write_text(json.dumps({'profile':profile,'native_quick_actions':a.native_quick_actions,'clock_edit_authority':a.clock_edit_authority,'hardware_tested':False,'whole_product_tested':False,'leak_detection':False,'inputs':file_manifest(ROOT,['apps/clock/watch_alarm_sleep.h','apps/clock/clock_alarm.inc','apps/clock/crown.c',profile_path,'tests/native_alarm_sleep_test.c','tests/native_alarm_owner_test.c','scripts/test_native_alarm.py']),'results':results},indent=2)+'\n')
