#!/usr/bin/env python3
"""Actual shared face output must keep the twelve-hour indicator inside a circle."""
import argparse,os,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--source',type=Path,default=root);a=p.parse_args();source=a.source.resolve()
for sanitize in [False,True]:
 with tempfile.TemporaryDirectory(prefix='watch-aperture-') as d:
  binary=Path(d)/'test';flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DWATCH_COMPACT_NOVA_ASSETS','-DWATCH_COMPRESSED_CALENDAR_ALPHA','-DWATCH_COMPRESSED_FACE_ALPHA']
  if sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
  subprocess.run(['cc',*flags,*['-I'+str(source/x) for x in ['.','include','sdk/app','sdk/driver']],str(root/'tests/meridiem_aperture_test.c'),'-o',str(binary)],check=True)
  subprocess.run([str(binary)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
