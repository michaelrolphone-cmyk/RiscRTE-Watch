#!/usr/bin/env python3
"""Qualify the actual Settings adapter and generic bridge. --system-ref
extracts immutable inputs, including fixture callbacks, before compilation."""
import argparse,hashlib,json,os,pathlib,subprocess,tempfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system',type=pathlib.Path,required=True)
p.add_argument('--system-ref');p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args()
base=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DPORTABLE_CONTEXTS_CLIENT','-DPORTABLE_BLE_BROADCAST','-DPORTABLE_QUICK_RADIOS','-DPORTABLE_LOW_BATTERY','-DPORTABLE_SLEEP_SETTINGS','-DPORTABLE_NOVA_UI','-DPORTABLE_TOUCH_ROTATION=0','-DPORTABLE_INPUT_NAVIGATION=','-DPORTABLE_INPUT_NAVIGATION_LOCAL','-DPORTABLE_NAVIGATION_RETENTION']
cases=list(range(12))+[13,14,15];runs=[];leak_limited=False;source_hashes={}
system_ref=subprocess.check_output(['git','-C',str(a.system),'rev-parse',a.system_ref or 'HEAD'],text=True).strip()
with tempfile.TemporaryDirectory(prefix='watch-adapter-bridge-') as temp:
 temp=pathlib.Path(temp);system=a.system
 if a.system_ref:
  system=temp/'system'
  prefixes=['Apps/','lib/PortableApps/','lib/NativeApps/include/','test/native_apps/']
  names=subprocess.check_output(['git','-C',str(a.system),'ls-tree','-r','--name-only',system_ref,*prefixes],text=True).splitlines()
  for name in names:
   out=system/name;out.parent.mkdir(parents=True,exist_ok=True)
   out.write_bytes(subprocess.check_output(['git','-C',str(a.system),'show',system_ref+':'+name]))
 includes=[system/'lib/PortableApps/include',system/'lib/NativeApps/include',system/'test/native_apps',ROOT/'sdk/driver',ROOT/'include']
 sources=[system/'Apps/settings.c',ROOT/'tests/portable_watch_adapter_bridge_test.c',ROOT/'apps/clock/portable_navigation.c',*[system/'lib/PortableApps/src'/n for n in ('quick_actions.c','quick_session.c','quick_radios.c','quick_render.c')]]
 # Hash all transitive source/header inputs, including the textually included
 # adapter and fixtures, rather than only compiler command-line translation units.
 for origin,label,paths in [(system,'system',[system/'lib/PortableApps',system/'lib/NativeApps/include',system/'test/native_apps',system/'Apps']),
                           (ROOT,'bridge',[ROOT/'apps/clock/portable_navigation.c',ROOT/'include/WatchPortableBridgeGuard.h',ROOT/'sdk/driver/RiscPowerKeyV1.h',ROOT/'tests/portable_watch_adapter_bridge_test.c'])]:
  for path in paths:
   for source in ([path] if path.is_file() else path.rglob('*')):
    if source.is_file() and source.suffix in ('.c','.h','.inc','.cpp'):
     source_hashes[label+'/'+str(source.relative_to(origin))]=hashlib.sha256(source.read_bytes()).hexdigest()
 for width,height,definitions in [(240,240,[]),(410,502,['-DPORTABLE_WATCH_NATIVE_HEIGHT','-DSETTINGS_FIXTURE_WIDTH=410','-DSETTINGS_FIXTURE_HEIGHT=502','-DPORTABLE_ASYNC_RGB565'])]:
  for mode,flags in [('normal',[]),('asan',['-fsanitize=address','-fno-omit-frame-pointer','-no-pie']),('ubsan',['-fsanitize=undefined','-fno-sanitize-recover=all'])]:
   exe=temp/'adapter'
   subprocess.run([os.environ.get('CC','cc'),*base,*definitions,*flags,*['-I'+str(x) for x in includes],*map(str,sources),'-Wl,--wrap=malloc','-Wl,--wrap=free','-o',str(exe)],check=True)
   for case in cases:
    env=os.environ.copy()
    if leak_limited:env['ASAN_OPTIONS']='detect_leaks=0'
    result=subprocess.run([str(exe),str(case)],capture_output=True,text=True,timeout=30,env=env)
    if result.returncode and 'LeakSanitizer does not work under ptrace' in result.stderr:
     leak_limited=True;env['ASAN_OPTIONS']='detect_leaks=0'
     result=subprocess.run([str(exe),str(case)],capture_output=True,text=True,timeout=30,env=env)
    if result.returncode:raise RuntimeError(f'{width}x{height} {mode} case={case}\n'+result.stdout+result.stderr)
    runs.append([width,height,mode,case])
   print(f'Actual adapter + generic bridge {width}x{height} {mode}: {len(cases)} passed',flush=True)
a.output.mkdir(parents=True,exist_ok=True)
(a.output/'adapter-result.json').write_text(json.dumps({'leak_sanitizer_ptrace_limited':leak_limited,'runs':len(runs),'cases':runs,'system_head':system_ref,'system_immutable':bool(a.system_ref),'source_sha256':source_hashes},indent=2)+'\n')
