#!/usr/bin/env python3
"""Actual deployed shared color Home with copied atomic preference transport."""
import argparse, hashlib, json, os, subprocess
from pathlib import Path
from merged_sdk import headers
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for n in ('system','utilities','output'):p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args();s=a.system.resolve();u=a.utilities.resolve();o=a.output.resolve();o.mkdir(parents=True,exist_ok=True)
sdk=headers(o,s,u,ROOT)
flags=['-O0','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-misleading-indentation','-DWATCH_COMPRESSED_CALENDAR_ALPHA','-DWATCH_COMPRESSED_FACE_ALPHA','-DWATCH_ATOMIC_PREFERENCES','-DPORTABLE_QUICK_ATOMIC_PREFERENCES','-DWATCH_QUICK_RADIOS','-DPORTABLE_QUICK_RADIOS','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie',*['-I'+str(p) for p in (sdk,s/'lib/PortableApps/src',ROOT/'include',ROOT)]]
common=[*[ROOT/'apps/clock'/n for n in ('nova/nova.c','viewport.c','capability_clients.c','quick_radios.c')],*[s/'lib/PortableApps/src'/n for n in ('quick_actions.c','quick_render.c','quick_session.c')]]
commands=[];sources=[]
def run(cmd,**kw):commands.append(list(map(str,cmd)));subprocess.run(cmd,check=True,**kw)
for name in ('quick_clock_test','atomic_face_test'):
 sources=[ROOT/'tests'/(name+'.c'),*common];objects=[]
 for i,src in enumerate(sources):
  obj=o/f'{name}-{i}.o';run(['cc','-std=c11',*flags,'-c',str(src),'-o',str(obj)]);objects.append(str(obj))
 run(['c++','-std=c++11',*flags,str(ROOT/'apps/clock/effects/boot.cpp'),*objects,'-o',str(o/name)])
 run([str(o/name)],timeout=60,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
inputs=[f for r in (ROOT/'apps',ROOT/'include',ROOT/'tests',sdk,s/'lib/PortableApps/src') for f in r.rglob('*') if f.is_file() and f.suffix in ('.c','.h','.inc','.cpp')]+sources+[Path(__file__).resolve()]
(o/'receipt.json').write_text(json.dumps({'commands':commands,'source_sha256':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(set(inputs))},'hardware_tested':False,'scope':'Actual shared Home controller and renderer; provider doubles for atomic storage and hardware'},indent=2)+'\n')
