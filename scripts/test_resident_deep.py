#!/usr/bin/env python3
"""Check reversible child sleep and deferred terminal transition boundaries."""
import argparse,hashlib,json,os,subprocess,tempfile
from pathlib import Path
from merged_sdk import headers
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for name in ('system','utilities','output'):p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--sanitized',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(prefix='resident-deep-') as temp:
 sdk=headers(Path(temp),a.system,a.utilities,ROOT)
 flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-DWATCH_SHARED_PREFERENCE_HEADERS','-I'+str(sdk),'-I'+str(ROOT),'-I'+str(ROOT/'include')]
 if a.sanitized:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 command=['cc',*flags,str(ROOT/'tests/resident_deep_test.c'),'-o',str(a.output/'test')];subprocess.run(command,check=True)
 result=subprocess.run([str(a.output/'test')],text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},timeout=30);(a.output/'run.log').write_text(result.stdout+result.stderr);print(result.stdout+result.stderr,end='');result.check_returncode()
 files=['tests/resident_deep_test.c','apps/clock/watch_sleep.h','apps/clock/watch_alarm_sleep.h','scripts/test_resident_deep.py']
 (a.output/'receipt.json').write_text(json.dumps({'command':command,'sanitized':a.sanitized,'hardware_tested':False,'source_sha256':{n:hashlib.sha256((ROOT/n).read_bytes()).hexdigest() for n in files}},indent=2)+'\n')
