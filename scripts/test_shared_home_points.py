#!/usr/bin/env python3
"""Compare legacy and provider-zone UTC Points projections without storage writes."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
from merged_sdk import headers
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--utilities',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
a=p.parse_args();root=Path(__file__).resolve().parents[1];out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
sdk=headers(out,a.system_apps,a.utilities,root);runs=[]
for mode in ('normal','sanitized'):
 flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if mode=='sanitized' else []
 binary=out/mode
 command=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-DWATCH_SHARED_HOME_CIVIL','-DPORTABLE_RTC_UTC8_DENVER','-I'+str(sdk),'-I'+str(root/'include'),str(root/'tests/shared_home_points_test.c'),str(root/'apps/clock/points_projection.c'),str(a.system_apps/'lib/PortableApps/src/PortableTimeZone.c'),str(a.system_apps/'lib/PortableApps/src/PortableTimeZoneCatalog.c'),'-o',str(binary)]
 subprocess.run(command,check=True)
 result=subprocess.run([str(binary)],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
 (out/(mode+'.log')).write_text(result.stdout+result.stderr)
 if result.returncode:print(result.stdout+result.stderr);result.check_returncode()
 print(result.stdout,end='');runs.append({'mode':mode,'command':command,'output':result.stdout})
 files=['apps/clock/points_projection.c','apps/clock/points_projection.h','tests/shared_home_points_test.c','scripts/test_shared_home_points.py','sdk/app/PointsUtcSchedule.h','dependencies.lock.json']
(out/'receipt.json').write_text(json.dumps({'runs':runs,'hardware_tested':False,'sha256':{path:hashlib.sha256((root/path).read_bytes()).hexdigest() for path in files}},indent=2)+'\n')
