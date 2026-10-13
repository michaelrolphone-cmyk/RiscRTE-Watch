#!/usr/bin/env python3
"""Production generic bridges: immutable dependency headers, guarded full rows,
normal/ASan/UBSan fault matrices. Outputs are host evidence, not device proof."""
import argparse, hashlib, json, os, pathlib, subprocess, tempfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
SYSTEM_REF='269440c1a6b72407cc57f02f99dc4ec2c47402c0'
UTILITIES_REF='fc0bdbbbc600ce891e60c44d091fce0586ade7d8'
p=argparse.ArgumentParser()
p.add_argument('--system',type=pathlib.Path,required=True)
p.add_argument('--utilities',type=pathlib.Path,required=True)
p.add_argument('--output',type=pathlib.Path,required=True)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
def extract(repo,ref,prefix,dest):
 for name in subprocess.check_output(['git','-C',str(repo),'ls-tree','-r','--name-only',ref,prefix],text=True).splitlines():
  out=dest/name;out.parent.mkdir(parents=True,exist_ok=True)
  out.write_bytes(subprocess.check_output(['git','-C',str(repo),'show',ref+':'+name]))
common=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DPORTABLE_APP_SLEEP_LOCAL',
 '-DPORTABLE_INPUT_NAVIGATION_LOCAL','-DPORTABLE_NAVIGATION_RETENTION','-DPORTABLE_QUICK_ACTIONS']
profiles=[('basic',[],list(range(28))+list(range(46,55))),
 ('alarm',['-DPORTABLE_ALARM_CLIENT','-DWATCH_ALARM_SLEEP_RESUME'],list(range(28))+list(range(46,55))+[42,43,44,45]),
 ('full',['-DPORTABLE_ALARM_CLIENT','-DPORTABLE_MOTION_WAKE','-DWATCH_ALARM_SLEEP_RESUME','-DPORTABLE_LOW_BATTERY'],list(range(28))+list(range(46,55))+list(range(30,46)))]
runs=[];leak_limited=False
with tempfile.TemporaryDirectory(prefix='watch-bridge-') as temp:
 tmp=pathlib.Path(temp)
 extract(a.system,SYSTEM_REF,'lib/PortableApps/include/',tmp/'system')
 extract(a.utilities,UTILITIES_REF,'lib/Alarm/include/',tmp/'utilities')
 includes=[tmp/'system/lib/PortableApps/include',ROOT/'sdk/app',ROOT/'sdk/driver',ROOT/'include',ROOT/'apps/clock',tmp/'utilities/lib/Alarm/include']
 for profile,defines,cases in profiles:
  for mode,sanitize in [('normal',[]),('asan',['-fsanitize=address','-fno-omit-frame-pointer','-no-pie']),('ubsan',['-fsanitize=undefined','-fno-sanitize-recover=all'])]:
   exe=tmp/'bridge'
   subprocess.run([os.environ.get('CC','cc'),*common,*defines,*sanitize,*['-I'+str(x) for x in includes],
     str(ROOT/'apps/clock/portable_navigation.c'),str(ROOT/'apps/clock/portable_sleep.c'),str(ROOT/'tests/portable_watch_bridge_test.c'),
     '-Wl,--wrap=malloc','-Wl,--wrap=free','-o',str(exe)],check=True)
   for width,height in [(240,240),(410,502)]:
    for case in cases+list(range(100,112)):
     env=os.environ.copy()
     if leak_limited:env['ASAN_OPTIONS']='detect_leaks=0'
     command=[str(exe),str(case),str(width),str(height)]
     result=subprocess.run(command,capture_output=True,text=True,timeout=20,env=env)
     if result.returncode and 'LeakSanitizer does not work under ptrace' in result.stderr:
      leak_limited=True;env['ASAN_OPTIONS']='detect_leaks=0'
      result=subprocess.run(command,capture_output=True,text=True,timeout=20,env=env)
     if result.returncode:raise RuntimeError(f'{profile}/{mode}/{width}x{height}/case{case}\n'+result.stdout+result.stderr)
     runs.append([profile,mode,width,height,case])
   print(f'{profile} {mode}: {(len(cases)+12)*2} cases passed',flush=True)
report={'system_ref':SYSTEM_REF,'utilities_ref':UTILITIES_REF,'hardware_qualified':False,
 'leak_sanitizer_ptrace_limited':leak_limited,'runs':len(runs),'cases':runs,'sources':{str(x.relative_to(ROOT)):hashlib.sha256(x.read_bytes()).hexdigest() for x in
 [ROOT/'apps/clock/portable_navigation.c',ROOT/'apps/clock/portable_sleep.c',ROOT/'include/WatchPortableBridgeGuard.h',ROOT/'tests/portable_watch_bridge_test.c']}}
(a.output/'result.json').write_text(json.dumps(report,indent=2)+'\n')
print(f'{len(runs)} actual bridge scenarios passed')
