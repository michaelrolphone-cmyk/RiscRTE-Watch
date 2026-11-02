#!/usr/bin/env python3
"""Qualify the actual Home against independent absent/malformed capabilities.
Original lifecycle fixtures remain byte-identical; derived fixture edits are
explicit, exact-match checked and hashed in the receipt.
"""
import argparse,ast,hashlib,json,os,re,subprocess,tempfile
from pathlib import Path
from merged_sdk import headers
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for name in ('system','utilities','output'):p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--width',type=int,default=240);p.add_argument('--height',type=int,default=240)
p.add_argument('--sanitized',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
fixture_source=ROOT/'tests/fast_wake_clock_test.c'
text=fixture_source.read_text()
changes=[
 ('static bool fast_diagnostic(const char*s){','static bool fast_diagnostic(const char*s){fprintf(stderr,"%s\\n",s);'),
 ('static const risc_runtime_api_v1 fast_runtime=','static risc_runtime_api_v1 fast_runtime='),
 ('int main(void){','int inherited_fast_main(void){'),
 (' if(fail_capability&&!strcmp(n,fail_capability))return false;',''),
 (' const void*p=NULL;',' if(fail_capability&&!strcmp(n,fail_capability))return false;\n const void*p=NULL;'),
]
for before,after in changes:
 assert text.count(before)==1,(before,text.count(before))
 text=text.replace(before,after)
text+='\n'+(ROOT/'tests/home_optional_controls.inc').read_text()
(a.output/'generated_fixture.c').write_text(text)
tree=ast.parse((ROOT/'scripts/build.py').read_text())
node=next(n.value for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='FLAGS' for t in n.targets))
selected=eval(compile(ast.Expression(node),'<build FLAGS>','eval'))
defined=set(re.findall(r'^#define ([A-Z_]+)',fixture_source.read_text(),re.M))
caps=['board.battery','rtc.clock','runtime.retained-wake','alarm.service','net.wifi','bluetooth.hci','telemetry.broadcast','contexts.service','power.sleep','input.power-key','display.power','storage.preferences','storage.files','motion.accel','software.installed-apps']
required=['display.output','input.touch.raw','runtime.realtime-control','runtime.provider-promotion']
commands=[];runs=[]
with tempfile.TemporaryDirectory(prefix='home-optional-') as tmp:
 w=Path(tmp);sdk=headers(w,a.system,a.utilities,ROOT);fixture=w/'fixture.c';fixture.write_text(text)
 flags=['-O1','-g1','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-misleading-indentation',
  '-DPORTABLE_LOW_BATTERY', f'-DTEST_WIDTH={a.width}u',f'-DTEST_HEIGHT={a.height}u',
  *[v for v in selected if v.startswith('-D') and v[2:].split('=')[0] not in defined],
  *['-I'+str(v) for v in (sdk,ROOT/'include',ROOT,ROOT/'tests',a.system/'lib/PortableApps/src')]]
 if a.sanitized:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 sources=[fixture,*[ROOT/'apps/clock'/n for n in ('nova/nova.c','points_projection.c','viewport.c','capability_clients.c','quick_radios.c')],
  *[a.system/'lib/PortableApps/src'/n for n in ('quick_actions.c','quick_render.c','quick_session.c','PortableAutomaticProvision.c','PortableNetworkSession.c')]]
 def run(cmd):
  commands.append(list(map(str,cmd)));subprocess.run(cmd,check=True)
 objects=[]
 for i,source in enumerate(sources):
  obj=w/f'{i}.o';run(['cc','-std=c11',*flags,*(['-DWATCH_CONTEXTS_CLIENT'] if i else []),'-c',str(source),'-o',str(obj)]);objects.append(str(obj))
 binary=a.output/'test';run(['c++','-std=c++11',*flags,str(ROOT/'apps/clock/effects/boot.cpp'),*objects,'-o',str(binary)])
 for mode,capabilities in [('preset',['alarm.service']),('absent',caps+required),('malformed',[c for c in caps+required if c not in ('storage.preferences','storage.files','motion.accel','software.installed-apps')])]:
  for cap in capabilities:
   result=subprocess.run([str(binary),mode,cap],capture_output=True,text=True,timeout=60,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
   log=mode+'-'+cap+'.log';(a.output/log).write_text(result.stdout+result.stderr)
   runs.append(dict(mode=mode,capability=cap,exit_code=result.returncode,log=log))
   print(result.stdout+result.stderr,end='',flush=True)
   (a.output/'runs.json').write_text(json.dumps(runs,indent=2)+'\n')
   result.check_returncode()
sources=[fixture_source,ROOT/'tests/home_optional_controls.inc',Path(__file__).resolve(),ROOT/'scripts/build.py',*ROOT.glob('apps/clock/*.c'),*ROOT.glob('apps/clock/*.inc')]
(a.output/'receipt.json').write_text(json.dumps(dict(schema=1,width=a.width,height=a.height,sanitized=a.sanitized,
 production_flags=True,hardware_tested=False,fixture_edits=changes,generated_fixture_sha256=hashlib.sha256(text.encode()).hexdigest(),
 commands=commands,runs=runs,source_sha256={str(v.relative_to(ROOT)):hashlib.sha256(v.read_bytes()).hexdigest() for v in sources}),indent=2)+'\n')
