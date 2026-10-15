#!/usr/bin/env python3
"""Qualify native boot/scrub/transition rasterization and memory boundaries."""
import os, subprocess, tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='native-effects-') as temp:
 for sanitize in (False,True):
  flags=['-O1','-g','-Wall','-Wextra','-Werror',*['-I'+str(root/p) for p in ('.','include','sdk/driver')]]
  if sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
  objects=[]
  for i,name in enumerate(('tests/native_effects_test.c','apps/clock/effects/ripple.c')):
   obj=Path(temp)/f'{i}.o';subprocess.run([os.environ.get('CC','cc'),'-std=c11',*flags,'-c',str(root/name),'-o',str(obj)],check=True);objects.append(str(obj))
  binary=Path(temp)/'test'
  subprocess.run([os.environ.get('CXX','c++'),'-std=c++11',*flags,str(root/'apps/clock/effects/boot.cpp'),*objects,'-o',str(binary)],check=True)
  subprocess.run([str(binary)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
