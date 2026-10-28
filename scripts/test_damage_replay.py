#!/usr/bin/env python3
"""Independent original-renderer oracle; selected owner pixels and authority faults."""
import argparse, os, subprocess, json, hashlib
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--reference',type=Path,required=True);p.add_argument('--system',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');p.add_argument('--retry-interval',type=int,choices=range(1,65));a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
env={**os.environ,'TMPDIR':str(a.output.resolve()),'ASAN_OPTIONS':'detect_leaks=0'}
flags=['-std=c11','-O2','-g1','-Wall','-Wextra','-Werror','-DWATCH_CONTEXTS_CLIENT','-DWATCH_NATIVE_STROKE_CACHE','-DWATCH_NATIVE_COVERAGE_CACHE','-DWATCH_PICKER_COMPOSITOR_PLAN','-DWATCH_COMPACT_NOVA_ASSETS','-DWATCH_COMPRESSED_CALENDAR_ALPHA','-DWATCH_COMPRESSED_FACE_ALPHA','-I'+str(root),'-I'+str(root/'include'),'-I'+str(a.system/'lib/PortableApps/include'),'-I'+str(a.system/'lib/PortableApps/time/denver')]
flags += ['-I'+str(root/'sdk'/part) for part in ('app','driver','hardware')]
if a.retry_interval:flags += ['-DWATCH_DAMAGE_UNSUPPORTED_RETRY='+str(a.retry_interval)]
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
commands=[]
def run(cmd):
 commands.append(cmd);subprocess.run(cmd,check=True,env=env)
for name,source,extra in [('reference',a.reference,[]),('selected',root,['-DWATCH_NATIVE_DAMAGE_REPLAY'])]:
 run([os.environ.get('CC','cc'),*flags,'-fPIC','-shared','-fvisibility=hidden',*extra,'-DNOVA_SOURCE="'+str(source.resolve()/'apps/clock/nova/nova.c')+'"',str(root/'tests/damage_replay_module.c'),'-o',str(a.output/(name+'.so'))])
run([os.environ.get('CC','cc'),*flags,'-no-pie',str(root/'tests/damage_replay_execution.c'),'-ldl','-o',str(a.output/'execution')])
run([str(a.output/'execution'),str(a.output/'reference.so'),str(a.output/'selected.so')])
(a.output/'commands.json').write_text(json.dumps({'commands':commands,'sanitized':a.sanitize,'leak_detection':False,'reference_commit':subprocess.check_output(['git','-C',str(a.reference),'rev-parse','HEAD'],text=True).strip()},indent=2)+'\n')
