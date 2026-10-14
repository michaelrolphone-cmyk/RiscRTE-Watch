#!/usr/bin/env python3
"""Qualify production Home's full-height render geometry and 240px golden parity."""
import os
import subprocess
import tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='home-height-') as temp:
    out=Path(temp)
    for sanitize in (False,True):
        flags=['-DWATCH_COMPRESSED_CALENDAR_ALPHA','-DWATCH_COMPRESSED_FACE_ALPHA','-O1','-g','-Wall','-Wextra','-Werror','-DWATCH_CONTEXTS_CLIENT']
        flags += ['-I'+str(root/p) for p in ('include','sdk/driver','sdk/app','.')]
        if sanitize:flags += ['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
        objects=[]
        for n,source in enumerate(('tests/home_native_height_test.c','apps/clock/nova/nova.c','apps/clock/effects/ripple.c')):
            obj=out/f'{n}.o'
            subprocess.run([os.environ.get('CC','cc'),'-std=c11',*flags,'-c',str(root/source),'-o',str(obj)],check=True)
            objects.append(str(obj))
        binary=out/'test'
        subprocess.run([os.environ.get('CXX','c++'),'-std=c++11',*flags,str(root/'apps/clock/effects/boot.cpp'),*objects,'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
        print(('ASan/UBSan' if sanitize else 'normal')+': PASS')
