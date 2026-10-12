#!/usr/bin/env python3
"""Run real generic capability clients/sleep policy with independent providers."""
import argparse,os,subprocess,tempfile,sys
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
 flags=['-std=c11','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation']
 if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
 includes=['-I'+str(r/d) for d in ['include','sdk/driver','sdk/app','apps/clock','apps/runtime_features/sdk']]
 for test,extra in [('test_capability_clients.c',['apps/clock/capability_clients.c']),('test_generic_sleep.c',[]),('test_tap_policy.c',[])]:
  out=Path(tmp)/test.removesuffix('.c')
  subprocess.run([os.environ.get('CC','cc'),*flags,*includes,str(r/'tests'/test),*[str(r/f) for f in extra],'-o',str(out)],check=True)
  subprocess.run([str(out)],check=True)

if not a.sanitize:
 subprocess.run([sys.executable,str(r/'tests/test_stage_product.py')],check=True)
