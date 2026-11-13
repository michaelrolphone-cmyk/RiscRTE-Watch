#!/usr/bin/env python3
"""Production Home resident adapter and native rendering; hardware/Runtime doubles."""
import argparse, hashlib, json, os, subprocess
from pathlib import Path
from merged_sdk import headers
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for n in ('system','utilities','output'):p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--sanitized',action='store_true')
p.add_argument('--native',action='store_true')
p.add_argument('--atomic',action='store_true')
a=p.parse_args();s=a.system.resolve();u=a.utilities.resolve();o=a.output.resolve();o.mkdir(parents=True,exist_ok=True)
sdk=headers(o,s,u,ROOT)
flags=['-DWATCH_SHARED_PREFERENCE_HEADERS','-O0','-g1','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-misleading-indentation','-DWATCH_COMPRESSED_CALENDAR_ALPHA','-DWATCH_COMPRESSED_FACE_ALPHA',*['-I'+str(p) for p in (sdk,s/'lib/PortableApps/src',ROOT/'include',ROOT)]]
if a.atomic:flags+=['-DWATCH_ATOMIC_PREFERENCES','-DPORTABLE_QUICK_ATOMIC_PREFERENCES']
if a.sanitized:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
if a.native:flags+=['-DTEST_WIDTH=410u','-DTEST_HEIGHT=502u','-DPORTABLE_WATCH_NATIVE_HEIGHT']
common=[*[ROOT/'apps/clock'/n for n in ('nova/nova.c','viewport.c','capability_clients.c','quick_radios.c')],*[s/'lib/PortableApps/src'/n for n in ('quick_actions.c','quick_render.c','quick_session.c')]]
commands=[];sources=[]
def run(cmd,**kw):commands.append(list(map(str,cmd)));subprocess.run(cmd,check=True,**kw)
for name in ('resident_home_test',):
 sources=[ROOT/'tests'/(name+'.c'),*common];objects=[]
 for i,src in enumerate(sources):
  obj=o/f'{name}-{i}.o';run(['cc','-std=c11',*flags,'-c',str(src),'-o',str(obj)]);objects.append(str(obj))
 run(['c++','-std=c++11',*flags,str(ROOT/'apps/clock/effects/boot.cpp'),*objects,'-o',str(o/name)])
 for case in ('controls','no-pending','failed','retained','unknown','replaced','short-key','long-key'):
  run([str(o/name),case],timeout=60,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
inputs=[f for r in (ROOT/'apps',ROOT/'include',ROOT/'tests',sdk,s/'lib/PortableApps/src') for f in r.rglob('*') if f.is_file() and f.suffix in ('.c','.h','.inc','.cpp')]+sources+[Path(__file__).resolve()]
(o/'receipt.json').write_text(json.dumps({'commands':commands,'source_sha256':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(set(inputs))},'hardware_tested':False,'scope':'Actual shared Home resident controller and renderer; Runtime admission and hardware doubles','sanitized':a.sanitized,'native':a.native},indent=2)+'\n')
