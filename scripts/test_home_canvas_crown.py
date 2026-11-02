#!/usr/bin/env python3
"""Run production Home with the real capability-owned Canvas provider.
Both historical lifecycle fixtures stay unchanged. Controlled derivations and
all actual compiled source/header identities are recorded in hosted receipts.
"""
import argparse,ast,hashlib,json,os,re,subprocess,tempfile
from pathlib import Path
from merged_sdk import headers,sources as stage_sources
ROOT=Path(__file__).resolve().parents[1]
CANVAS_COMMIT='db53bf95040c35df459479a634a6fe1a610920ca'
p=argparse.ArgumentParser(description=__doc__)
for name in ('system','utilities','canvas-source','output'):p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--width',type=int,default=240);p.add_argument('--height',type=int,default=240)
p.add_argument('--sanitized',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
actual=subprocess.check_output(['git','-C',str(a.canvas_source),'rev-parse','HEAD'],text=True).strip()
assert actual==CANVAS_COMMIT,(actual,CANVAS_COMMIT)
fixture_source=ROOT/'tests/fast_wake_clock_test.c';quick_source=ROOT/'tests/quick_clock_test.c'
text=fixture_source.read_text()
changes=[
 ('static bool fast_diagnostic(const char*s){','static bool fast_diagnostic(const char*s){fprintf(stderr,"%s\\n",s);'),
 ('static const risc_runtime_api_v1 fast_runtime=','static risc_runtime_api_v1 fast_runtime='),
 ('int main(void){','int inherited_fast_main(void){'),
]
for before,after in changes:
 assert text.count(before)==1,(before,text.count(before));text=text.replace(before,after)
text+='\n'+(ROOT/'tests/home_canvas_crown_controls.inc').read_text()
(a.output/'generated_fixture.c').write_text(text)
tree=ast.parse((ROOT/'scripts/build.py').read_text())
node=next(n.value for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='FLAGS' for t in n.targets))
selected=eval(compile(ast.Expression(node),'<build FLAGS>','eval'))
assert '-DWATCH_HOME_CANVAS' in selected,'Production build must actually enable Canvas'
defined=set(re.findall(r'^#define ([A-Z_]+)',fixture_source.read_text(),re.M))
commands=[];runs=[];compiled={}
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
with tempfile.TemporaryDirectory(prefix='home-canvas-crown-') as tmp:
 w=Path(tmp);sdk=headers(w,a.system,a.utilities,ROOT);staged=stage_sources(w,a.system,ROOT)
 fixture=w/'fixture.c';fixture.write_text(text)
 flags=['-O1','-g1','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-misleading-indentation',
  '-DPORTABLE_LOW_BATTERY',f'-DTEST_WIDTH={a.width}u',f'-DTEST_HEIGHT={a.height}u',
  *[v for v in selected if v.startswith('-D') and v[2:].split('=')[0] not in defined],
  *['-I'+str(v) for v in (sdk,ROOT/'include',ROOT,ROOT/'tests',staged,a.canvas_source/'Services/canvas_host')]]
 if a.sanitized:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 sources=[fixture,*[ROOT/'apps/clock'/n for n in ('nova/nova.c','points_projection.c','viewport.c','capability_clients.c','quick_radios.c')],
  *[staged/n for n in ('quick_actions.c','quick_render.c','quick_session.c','PortableAutomaticProvision.c','PortableNetworkSession.c')]]
 def run(cmd):commands.append(list(map(str,cmd)));subprocess.run(cmd,check=True)
 objects=[]
 for i,source in enumerate(sources):
  obj=w/f'{i}.o';run(['cc','-std=c11',*flags,*(['-DWATCH_CONTEXTS_CLIENT'] if i else []),'-c',str(source),'-o',str(obj)]);objects.append(str(obj))
 binary=a.output/'test';run(['c++','-std=c++11',*flags,str(ROOT/'apps/clock/effects/boot.cpp'),*objects,'-o',str(binary)])
 modes=['normal','cold','deep','sleep','saved','no-raw-touch','nav-home','nav-picker','nav-quick','controls-button','multitouch','alarm-above','alarm-retry']
 cases=[(mode,encoding,rotation) for mode in modes for encoding in (5,1) for rotation in (0,1)]
 face_count=int(subprocess.check_output([str(binary),'face-count'],text=True))
 cases += [(f'face-{face}',encoding,0) for face in range(face_count) for encoding in (5,1)]
 cases+=[(mode,5,0) for mode in ('absent','malformed-api','open-retained','open-unknown','geometry-retained',
  'begin-retained','begin-unknown','malformed-frame','submit-retained','poll-retained','poll-unknown',
  'input-retained','quiesce-retained','close-retained','physical-retained')]
 for mode,encoding,rotation in cases:
  result=subprocess.run([str(binary),mode,str(encoding),str(rotation)],capture_output=True,text=True,timeout=90,
   env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
  log=f'{mode}-{encoding}-{rotation}.log';(a.output/log).write_text(result.stdout+result.stderr)
  runs.append(dict(mode=mode,physical_encoding=encoding,rotation=rotation,exit_code=result.returncode,log=log))
  print(result.stdout+result.stderr,end='',flush=True);(a.output/'runs.json').write_text(json.dumps(runs,indent=2)+'\n')
  result.check_returncode()
 compiled={str(f.relative_to(w)):digest(f) for folder in (sdk,staged) for f in folder.rglob('*') if f.is_file()}
sources=[fixture_source,quick_source,ROOT/'tests/home_canvas_crown_controls.inc',Path(__file__).resolve(),
 ROOT/'scripts/build.py',ROOT/'scripts/merged_sdk.py',ROOT/'dependencies.lock.json']
sources+=list((ROOT/'apps/clock').rglob('*.h'))+list((ROOT/'apps/clock').glob('*.c'))+list((ROOT/'apps/clock').glob('*.inc'))+list((ROOT/'include').glob('*.h'))
provider={str(f.relative_to(a.canvas_source)):digest(f) for f in (a.canvas_source/'Services/canvas_host').glob('*.h')}
(a.output/'receipt.json').write_text(json.dumps(dict(schema=1,width=a.width,height=a.height,sanitized=a.sanitized,
 production_flags=True,canvas_source_commit=actual,hardware_tested=False,fixture_edits=changes,
 generated_fixture_sha256=hashlib.sha256(text.encode()).hexdigest(),commands=commands,runs=runs,
 source_sha256={str(v.relative_to(ROOT)):digest(v) for v in sources},
 staged_source_header_sha256=compiled,canvas_provider_source_sha256=provider),indent=2)+'\n')
