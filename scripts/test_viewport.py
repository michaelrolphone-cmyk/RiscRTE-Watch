#!/usr/bin/env python3
"""Exercise production viewport rendering and touch transforms with fake providers."""
import argparse,os,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
    out=Path(tmp)/'viewport'
    flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if a.sanitize else []
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',*flags,
        '-I'+str(r/'include'),'-I'+str(r/'sdk/driver'),str(r/'tests/test_viewport.c'),str(r/'apps/clock/viewport.c'),'-o',str(out)],check=True)
    subprocess.run([str(out)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
