#!/usr/bin/env python3
"""Actual Runtime/Home/Stopwatch/scene integration, minimal Home feature subset.
Physical APIs are doubles; no target or complete product acceptance assertion.
"""
import argparse,hashlib,importlib.util,json,os,shutil,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for n in ('runtime','system','utilities','output'):p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--sanitized',action='store_true');a=p.parse_args()
r,s,u,o=[getattr(a,k).resolve() for k in ('runtime','system','utilities','output')];o.mkdir(parents=True,exist_ok=False)
sys.path.insert(0,str(u/'scripts'));from lifecycle_dependencies import Closure
from merged_sdk import headers
include=headers(o,s,u,ROOT)
spec=importlib.util.spec_from_file_location('selected_sdk',s/'scripts/scene_sdk.py');sdk=importlib.util.module_from_spec(spec);spec.loader.exec_module(sdk);sdk.stage_sdk(r,s,o/'sdk')
cf=['-pipe','-std=c11','-O1','-g1','-Wall','-Wextra','-Werror','-fPIC','-fvisibility=hidden','-I'+str(include),'-I'+str(s/'lib/PortableApps/include'),'-I'+str(s/'lib/PortableApps/src'),'-I'+str(ROOT/'include'),'-I'+str(ROOT)]
san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'] if a.sanitized else []
cf+=san
if a.sanitized:cf=[('-g0' if x=='-g1' else x) for x in cf]
closure=Closure([('Runtime',r),('System',s),('Utilities',u),('Watch',ROOT),('generated-sdk',o/'sdk')],o/'dependencies.json')
compiler_tmp=o/'compiler-tmp';compiler_tmp.mkdir(exist_ok=True)
def call(cmd):closure.record(cmd);subprocess.run(cmd,check=True,env={**os.environ,'TMPDIR':str(compiler_tmp)})
f=ROOT/'test/runtime_resident';common=o/'common';common.mkdir()
def shared(name,src,flags=()):call(['cc',*cf,'-shared',*flags,*map(str,src),'-o',str(common/name)])
shared('stopwatch.elf',[f/'stopwatch.c'],['-DSTOPWATCH_SOURCE="'+str(u/'Apps/stopwatch_scene.c')+'"'])
shared('springboard.elf',[f/'launcher.c']);shared('scene-host.elf',[s/'Services/scene_host/host.c'])
for i,(name,cap) in enumerate((('display','display.output'),('touch','input.touch.raw'),('navigation','input.navigation'))):shared(name+'.elf',[f/'provider.c'],[f'-DPROVIDER_INDEX={i}',f'-DPROVIDER_ID="{name}"',f'-DPROVIDER_CAP="{cap}"','-DPROVIDER_API=1'])
shared('power-key.elf',[f/'provider.c'],['-DPROVIDER_INDEX=5','-DPROVIDER_ID="power-key"','-DPROVIDER_CAP="input.power-key"','-DPROVIDER_API=1'])
shared('scene-presentation-profile.elf',[s/'Services/scene_profile/profile.c'])
homeflags=['-DWATCH_RESIDENT_HOST','-DWATCH_CLOCK_LAUNCHER','-DWATCH_QUICK_ACTIONS','-DWATCH_SHARED_PREFERENCE_HEADERS','-DPORTABLE_WATCH_NATIVE_HEIGHT','-DWATCH_COMPRESSED_CALENDAR_ALPHA','-DWATCH_COMPRESSED_FACE_ALPHA','-Wno-unused-function','-Wno-misleading-indentation']
objects=[]
for i,src in enumerate([f/'home.c',*[ROOT/'apps/clock'/n for n in ('nova/nova.c','viewport.c','capability_clients.c')],*[s/'lib/PortableApps/src'/n for n in ('quick_actions.c','quick_render.c','quick_session.c')]]):
 obj=o/f'home-{i}.o';call(['cc',*cf,*homeflags,'-c',str(src),'-o',str(obj)]);objects.append(obj)
call(['c++','-std=c++11','-O1','-g1','-fPIC','-fvisibility=hidden',*san,'-I'+str(include),'-shared',str(ROOT/'apps/clock/effects/boot.cpp'),*map(str,objects),'-o',str(common/'default.elf')])
call(['cc',*cf,'-c',str(f/'hardware.c'),'-o',str(o/'hardware.o')])
sources=('bootstrap/Json.cpp','bootstrap/Board.cpp','bootstrap/Runtime.cpp','runtime/streams/AppStreamSessions.cpp','runtime/streams/ProviderQueueHost.cpp','runtime/drivers/ProviderGraphV2.cpp','runtime/drivers/ProviderModuleV2.cpp','runtime/storage/AppDataFiles.cpp')
exe=o/'runtime-test';call(['c++','-pipe',*san,('-g0' if a.sanitized else '-g1'),'-std=c++17','-O0','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers','-rdynamic','-I'+str(include),*['-I'+str(r/p) for p in ('src','lib/ArduinoJson/src','test/drivers/stubs','test')],*[str(r/'src'/p) for p in sources],str(f/'runtime_test.cpp'),str(o/'hardware.o'),'-ldl','-o',str(exe)])
results=[]
for name,w,h in [('watch',240,240),('amoled',410,502)]:
 stage=o/name;shutil.copytree(common,stage)
 for binary in ('default.elf','stopwatch.elf'):
  assert hashlib.sha256((stage/binary).read_bytes()).digest()==hashlib.sha256((common/binary).read_bytes()).digest()
 for mode,input_source in ((mode,source) for mode in ('demand','retained-providers') for source in ('navigation','power-key')):
  result=subprocess.run([str(exe),str(stage),str(w),str(h),'5',mode,input_source],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},text=True,capture_output=True,timeout=90)
  (stage/(mode+'-'+input_source+'.log')).write_text(result.stdout+result.stderr);print(result.stdout+result.stderr);result.check_returncode();results.append({'profile':name,'width':w,'height':h,'provider_activation':mode,'exit_input':input_source,'passed':True})
(o/'results.json').write_text(json.dumps({'scope':__doc__,'cases':results,'sanitized':a.sanitized,'home_defines':homeflags,'hardware_tested':False,'elf_sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in common.glob('*.elf')}},indent=2)+'\n')
