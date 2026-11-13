#!/usr/bin/env python3
"""Actual Runtime alarm/idle checkpoint transport; host/native safety are explicit doubles."""
import argparse,hashlib,json,os,shlex,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');p.add_argument('--cxx',default='c++');a=p.parse_args();r=a.runtime.resolve();o=a.output.resolve();o.mkdir(parents=True,exist_ok=True)
(o/'sdk').mkdir(exist_ok=True)
for name in ('RiscResidentSettingsEffectsV1.h',):shutil.copy2(ROOT/'sdk/app'/name,o/'sdk'/name)
hashfile=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
original=r/'test/resident_policy_test.cpp';text=original.read_text();anchor='void childScenario(){\n';assert text.count(anchor)==1
text=text.replace(anchor,anchor+(ROOT/'tests/resident_effects_runtime.inc').read_text())
text='#include "RiscResidentSettingsEffectsV1.h"\n'+text
fixture=o/'fixture.cpp';fixture.write_text(text)
flags=['-O0','-g1','-gz','-pipe','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers'];
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
incs=['-I'+str(x) for x in (r/'src',r/'sdk/app',r/'sdk/driver',r/'sdk/hardware',r/'lib/ArduinoJson/src',r/'test/drivers/stubs',r/'test',o/'sdk')]
commands=[];inputs={}
def execute(cmd):
 commands.append(list(map(str,cmd)));subprocess.run(cmd,check=True)
def deps(compiler,src,extra=()):
 raw=subprocess.check_output([compiler,'-std=c++17' if src.suffix=='.cpp' else '-std=c11',*flags,*incs,*extra,'-MM',str(src)],text=True)
 for name in shlex.split(raw.replace('\\\n',' ').split(':',1)[1]):
  f=Path(name).resolve();inputs[str(f)]=hashfile(f)
for role in range(3):
 src=r/'test/fixtures/resident_policy_app.c';extra=['-DRESIDENT_POLICY_ROLE='+str(role)];deps('cc',src,extra);execute(['cc','-std=c11',*flags,*incs,*extra,'-fPIC','-fvisibility=hidden','-shared',str(src),'-o',str(o/f'role-{role}.elf')])
for name in ('stream_session_provider','stream_session_root'):
 src=r/f'test/fixtures/{name}.c';deps('cc',src);execute(['cc','-std=c11',*flags,*incs,'-fPIC','-fvisibility=hidden','-shared',str(src),'-o',str(o/(name+'.elf'))])
src=r/'test/resident_native_contexts_test.cpp';deps(a.cxx,src,['-DRESIDENT_NATIVE_CONTEXTS_PROVIDER']);execute([a.cxx,'-std=c++17',*flags,*incs,'-DRESIDENT_NATIVE_CONTEXTS_PROVIDER','-fPIC','-fvisibility=hidden','-shared',str(src),'-o',str(o/'probe.elf')])
sources=[r/s for s in ('src/bootstrap/Json.cpp','src/bootstrap/Board.cpp','src/bootstrap/Runtime.cpp','src/runtime/streams/AppStreamSessions.cpp','src/runtime/streams/ProviderQueueHost.cpp','src/runtime/drivers/ProviderGraphV2.cpp','src/runtime/drivers/ProviderModuleV2.cpp','src/ports/esp32s3/CpuPort.cpp')]+[fixture]
for src in sources:deps(a.cxx,src)
for src in (original,ROOT/'tests/resident_effects_runtime.inc',Path(__file__).resolve()):inputs[str(src)]=hashfile(src)
execute([a.cxx,'-std=c++17',*flags,*incs,'-rdynamic',*map(str,sources),'-ldl','-o',str(o/'test')])
cases=('effects-roundtrip','effects-old-host','effects-flags','retained-effects')
for case in cases:
 for role,name in enumerate(('host','child','next')):shutil.copy2(o/f'role-{role}.elf',o/(name+'.elf'))
 result=subprocess.run([str(o/'test'),str(o),case],text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},timeout=60);(o/(case+'.log')).write_text(result.stdout+result.stderr);print(result.stdout+result.stderr,end='');result.check_returncode()
assert inputs=={f:hashfile(f) for f in inputs},'Compiled inputs changed'
(o/'receipt.json').write_text(json.dumps({'cases':len(cases),'sanitized':a.sanitize,'commands':commands,'compiled_inputs':inputs,'binaries':{str(f):hashfile(f) for f in o.iterdir() if f.suffix=='.elf' or f.name=='test'},'scope':__doc__,'physical_execution':False},indent=2)+'\n')
