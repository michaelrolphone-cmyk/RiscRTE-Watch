#!/usr/bin/env python3
import argparse,json,hashlib,subprocess,os
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();w=Path(__file__).resolve().parents[1];a.output.mkdir(parents=True,exist_ok=True);rows=[]
for sanitized in (False,True):
 binary=a.output/('sanitized' if sanitized else 'normal');cmd=['cc','-std=c11','-Wall','-Wextra','-Werror',*(['-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie'] if sanitized else []),'-I'+str(a.runtime/'sdk/app'),'-I'+str(a.runtime/'sdk/driver'),str(w/'tests/interaction_trace_test.c'),'-o',str(binary)];subprocess.run(cmd,check=True);r=subprocess.run([str(binary)],check=True,text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});rows.append({'sanitized':sanitized,'command':cmd,'stdout':r.stdout});print(r.stdout,end='')
files=[w/'tests/interaction_trace_test.c',w/'apps/clock/interaction_trace.h'];(a.output/'receipt.json').write_text(json.dumps({'passed':True,'tests':rows,'inputs':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},'physical_verified':False,'scope':'helper timing/wrap/budget/legacy and no-callback retained fences; not complete Home integration'},indent=2)+'\n')
