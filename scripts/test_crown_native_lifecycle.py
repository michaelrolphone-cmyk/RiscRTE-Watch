#!/usr/bin/env python3
"""Qualify production Home boot/return ownership and physical snapshots."""
import argparse, os, subprocess, tempfile
from pathlib import Path
from merged_sdk import headers
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--utilities',type=Path,required=True)
p.add_argument('--performance-trace-runtime',type=Path)
p.add_argument('--monotonic-clock-test',action='store_true')
a=p.parse_args()
with tempfile.TemporaryDirectory(prefix='crown-native-lifecycle-') as temp:
 out=Path(temp);sdk=headers(out,a.system_apps,a.utilities,root)
 if a.performance_trace_runtime:
  import shutil
  shutil.copyfile(a.performance_trace_runtime/'sdk/app/RiscPerformanceV1.h',sdk/'RiscPerformanceV1.h')
 for sanitize in (False,True):
  for width,height in ((240,240),(410,502),(502,410),(120,150)):
   flags=['-O1','-g','-Wall','-Wextra','-Werror','-DWATCH_SHARED_PREFERENCE_HEADERS','-DWATCH_COMPACT_NOVA_ASSETS','-DWATCH_COMPRESSED_CALENDAR_ALPHA','-DWATCH_COMPRESSED_FACE_ALPHA','-DPORTABLE_WATCH_NATIVE_HEIGHT',f'-DTEST_WIDTH={width}u',f'-DTEST_HEIGHT={height}u',*['-I'+str(x) for x in (sdk,root/'include',root)]]
   if a.monotonic_clock_test:flags+=['-DTEST_HOME_MONOTONIC']
   if a.performance_trace_runtime:flags+=['-DWATCH_PERFORMANCE_TRACE']
   if sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
   objects=[]
   sources=[root/'tests/crown_native_lifecycle_test.c',*[root/'apps/clock'/n for n in ('nova/nova.c','viewport.c','capability_clients.c')],*[a.system_apps/'lib/PortableApps/src'/n for n in ('quick_actions.c','quick_render.c','quick_session.c')]]
   for i,source in enumerate(sources):
    obj=out/f'{i}.o';subprocess.run([os.environ.get('CC','cc'),'-std=c11',*flags,'-c',str(source),'-o',str(obj)],check=True);objects.append(str(obj))
   binary=out/'test'
   subprocess.run([os.environ.get('CXX','c++'),'-std=c++11',*flags,str(root/'apps/clock/effects/boot.cpp'),*objects,'-o',str(binary)],check=True)
   subprocess.run([str(binary)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
