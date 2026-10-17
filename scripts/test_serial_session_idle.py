#!/usr/bin/env python3
"""Execute exact Home idle caller with real serial Files session/timeout logic."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess,tempfile
from merged_sdk import headers
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for name in ('system','utilities','runtime','output'):p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--home',type=Path,default=ROOT)
p.add_argument('--expect-baseline',action='store_true')
p.add_argument('--normal-only',action='store_true')
p.add_argument('--dimensions',choices=('both','410x502','240x240'),default='both')
p.add_argument('--prepare-only',action='store_true')
a=p.parse_args();w=a.home.resolve();a.output.mkdir(parents=True,exist_ok=True)
variant='baseline' if a.expect_baseline else 'fixed'
receipt={'scope':'Actual Home controller/renderer and real RiscSerial::Files lease/parser. Runtime service/sleep adapter, device APIs and time are modeled. No target, native Runtime scheduler, USB browser or physical-device qualification.','variant':variant,'home_head':subprocess.check_output(['git','-C',str(w),'rev-parse','HEAD'],text=True).strip(),'runtime_head':subprocess.check_output(['git','-C',str(a.runtime),'rev-parse','HEAD'],text=True).strip(),'runs':[],'compilations':[],'inputs':{},'hardware_tested':False,'leak_detection':False}
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def save(): (a.output/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
for path in [w/'apps/clock/crown.c',ROOT/'tests/serial_session_idle.inc',ROOT/'tests/serial_session_idle_bridge.cpp',Path(__file__).resolve(),a.runtime/'src/runtime/serial/Files.cpp',a.runtime/'src/runtime/serial/Files.h']:
 receipt['inputs'][str(path)]=digest(path)
if a.prepare_only:
 save();print('Prepared exact-source inputs; no compiler started.');raise SystemExit(0)
with tempfile.TemporaryDirectory(prefix='home-serial-idle-') as temp:
 work=Path(temp);sdk=headers(work,a.system,a.utilities,w)
 original=w/'tests/fast_wake_clock_test.c'
 source=original.read_text();assert source.count('static const risc_runtime_api_v1 fast_runtime=')==1
 source=source.replace('static const risc_runtime_api_v1 fast_runtime=','static risc_runtime_api_v1 fast_runtime=').replace('int main(void){','int inherited_fast_main(void){')
 source='#include <stdbool.h>\nstatic bool probe_long;\n'+source.replace('ended=true;','if(!probe_long)ended=true;')
 source+=(ROOT/'tests/serial_session_idle.inc').read_text()
 fixture=work/'serial_session_idle.c';fixture.write_text(source)
 receipt['generated_fixture_sha256']=digest(fixture);receipt['inputs'][str(original)]=digest(original)
 sources=[fixture,*[w/'apps/clock'/n for n in ('nova/nova.c','points_projection.c','viewport.c','capability_clients.c','quick_radios.c')],*[a.system/'lib/PortableApps/src'/n for n in ('quick_actions.c','quick_render.c','quick_session.c','PortableAutomaticProvision.c','PortableNetworkSession.c')]]
 dims=[(240,240),(410,502)] if a.dimensions=='both' else [tuple(map(int,a.dimensions.split('x')))]
 for width,height in dims:
  for sanitized in ([False] if a.normal_only else [False,True]):
   flags=['-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-misleading-indentation','-DWATCH_COMPACT_NOVA_ASSETS','-DWATCH_SHARED_PREFERENCE_HEADERS','-DPORTABLE_LOW_BATTERY','-DWATCH_COMPRESSED_CALENDAR_ALPHA','-DWATCH_COMPRESSED_FACE_ALPHA','-DWATCH_FAST_WAKE','-DWATCH_COLD_BOOT_ANIMATION','-DPORTABLE_WATCH_NATIVE_HEIGHT','-DPORTABLE_RTC_UTC8_DENVER','-DPORTABLE_PREFERENCE_STORE','-DWATCH_ATOMIC_PREFERENCES','-DPORTABLE_QUICK_ATOMIC_PREFERENCES','-DPORTABLE_QUICK_RADIOS','-DWATCH_AUTOMATIC_PROVISION','-DWATCH_CRASH_REPORT','-DCRASH_REPORT_SESSION_FILES=1','-DWATCH_CRASH_REPORT_NAMESPACE=61',f'-DTEST_WIDTH={width}u',f'-DTEST_HEIGHT={height}u',*['-I'+str(q) for q in (sdk,w/'include',w,w/'tests',a.system/'lib/PortableApps/src',a.runtime/'src',a.runtime/'sdk/app',a.runtime/'sdk/driver')]]
   if sanitized:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
   objects=[];compilation={'dimensions':[width,height],'sanitized':sanitized,'commands':[]};receipt['compilations'].append(compilation)
   def run(command):
    command=list(map(str,command));compilation['commands'].append(command);save();subprocess.run(command,check=True)
   all_sources=[*sources,ROOT/'tests/serial_session_idle_bridge.cpp',a.runtime/'src/runtime/serial/Files.cpp',w/'apps/clock/effects/boot.cpp']
   for i,src in enumerate(all_sources):
    obj=work/f'{i}.o';dep=work/f'{i}.d';cpp=src.suffix=='.cpp'
    compile_flags=flags
    if src in (ROOT/'tests/serial_session_idle_bridge.cpp',a.runtime/'src/runtime/serial/Files.cpp'):
     compile_flags=[f for f in flags if not f.startswith('-I')]+['-I'+str(a.runtime/q) for q in ('src','sdk/app','sdk/driver')]
    run(['c++' if cpp else 'cc','-std=c++17' if cpp else '-std=c11',*compile_flags,*(['-DWATCH_CONTEXTS_CLIENT'] if i else []),'-MMD','-MF',dep,'-c',src,'-o',obj]);objects.append(obj)
    for q in dep.read_text().replace('\\\n',' ').split(':',1)[1].split():
     path=Path(q)
     if path!=fixture:receipt['inputs'][str(path)]=digest(path)
   binary=work/'test';run(['c++',*flags,*objects,'-o',binary]);compilation['binary_sha256']=digest(binary)
   for case in ('old-signal','lease-timeout','renewed-session','disconnect','no-session','manual-busy','blanking-arrival'):
    result=subprocess.run([str(binary),case,variant],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'},timeout=120)
    log=a.output/f'{width}x{height}-{"sanitized" if sanitized else "normal"}-{case}.log'
    log.write_text(result.stdout+result.stderr);print(result.stdout+result.stderr,end='',flush=True)
    receipt['runs'].append({'dimensions':[width,height],'sanitized':sanitized,'case':case,'returncode':result.returncode,'stdout':result.stdout,'stderr':result.stderr});save();result.check_returncode()
   for obj in objects:obj.unlink()
   binary.unlink()
save()
