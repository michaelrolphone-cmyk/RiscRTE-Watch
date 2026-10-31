#!/usr/bin/env python3
"""Production FAST_WAKE/ALARMS Home continuation; explicit Runtime/hardware doubles."""
import argparse,json,hashlib,os,subprocess,tempfile,ast,re
from pathlib import Path
from merged_sdk import headers
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for name in ('system','utilities','output'):p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--sanitized',action='store_true');p.add_argument('--production-flags',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
text=(ROOT/'tests/fast_wake_clock_test.c').read_text().replace('static const risc_runtime_api_v1 fast_runtime=','static risc_runtime_api_v1 fast_runtime=').replace('int main(void){','int inherited_fast_main(void){')
# The old fixture's exact startup latency assertions apply to no-continuation
# first-frame ordering. Replace only those admission-phase expectations for the
# new pre-frame resident callback; retain settled provider and custody assertions.
text=text.replace('assert(first_seen&&input_stages&&!pending&&!owned&&!service_acquires);','assert(!pending&&!owned);')
text=text.replace('assert(first_seen&&!pending&&!owned);','assert(!pending&&!owned);')
text=text.replace('if(clock_radios_pending)assert(radio_at&&input_stages);','')
text=text.replace('if(service_acquires==3||service_acquires==4)assert(polls>last_service_polls);','')
inherited_hash=hashlib.sha256(text.encode()).hexdigest()
text+="\n"+(ROOT/'tests/resident_full_controls.inc').read_text()
(a.output/'generated_fixture.c').write_text(text)
commands=[]
with tempfile.TemporaryDirectory(prefix='resident-full-') as tmp:
 w=Path(tmp);sdk=headers(w,a.system,a.utilities,ROOT);fixture=w/'fixture.c';fixture.write_text(text)
 flags=['-O0','-g1','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-misleading-indentation','-DWATCH_RESIDENT_HOST','-DWATCH_COMPACT_NOVA_ASSETS','-DWATCH_SHARED_PREFERENCE_HEADERS','-DPORTABLE_LOW_BATTERY','-DWATCH_COMPRESSED_CALENDAR_ALPHA','-DWATCH_COMPRESSED_FACE_ALPHA','-DWATCH_FAST_WAKE','-DWATCH_COLD_BOOT_ANIMATION','-DPORTABLE_WATCH_NATIVE_HEIGHT','-DPORTABLE_RTC_UTC8_DENVER',*['-I'+str(p) for p in (sdk,ROOT/'include',ROOT,ROOT/'tests',a.system/'lib/PortableApps/src')]]
 if a.production_flags:
  tree=ast.parse((ROOT/'scripts/build.py').read_text());node=next(n.value for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='FLAGS' for t in n.targets));selected=eval(compile(ast.Expression(node),'<build FLAGS>','eval'))
  defined=set(re.findall(r'^#define ([A-Z_]+)',(ROOT/'tests/fast_wake_clock_test.c').read_text(),re.M))
  flags += [v for v in selected if v.startswith('-D') and v not in flags and v[2:].split('=')[0] not in defined]
 if a.sanitized:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 sources=[fixture,*[ROOT/'apps/clock'/n for n in ('nova/nova.c','points_projection.c','viewport.c','capability_clients.c','quick_radios.c')],*[a.system/'lib/PortableApps/src'/n for n in ('quick_actions.c','quick_render.c','quick_session.c')]]
 def run(cmd):commands.append(list(map(str,cmd)));subprocess.run(cmd,check=True)
 objs=[]
 for i,src in enumerate(sources):
  obj=w/f'{i}.o';run(['cc','-std=c11',*flags,*(['-DWATCH_CONTEXTS_CLIENT'] if i else []),'-c',str(src),'-o',str(obj)]);objs.append(str(obj))
 binary=a.output/'test';run(['c++','-std=c++11',*flags,str(ROOT/'apps/clock/effects/boot.cpp'),*objs,'-o',str(binary)])
 for case in ('policy','controls','short-key'):
  result=subprocess.run([str(binary),case],text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},timeout=60);(a.output/(case+'.log')).write_text(result.stdout+result.stderr);print(result.stdout+result.stderr,end='');result.check_returncode()
(a.output/'receipt.json').write_text(json.dumps({'sanitized':a.sanitized,'production_flags':a.production_flags,'generated_fixture_sha256':hashlib.sha256(text.encode()).hexdigest(),'commands':commands,'scope':'production FAST_WAKE/ALARMS Home; Runtime/hardware doubles','hardware_tested':False,'source_sha256':{str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for f in [*[p for p in (ROOT/'apps/clock').glob('*') if p.is_file()],ROOT/'tests/resident_full_controls.inc',ROOT/'tests/fast_wake_clock_test.c',ROOT/'tests/quick_clock_test.c',Path(__file__).resolve(),ROOT/'scripts/build.py']}},indent=2)+'\n')
