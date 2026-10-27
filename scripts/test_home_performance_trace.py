#!/usr/bin/env python3
"""Exercise optional Home RAM milestones and custody guards; no device I/O."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();root=Path(__file__).resolve().parents[1];a.output.mkdir(parents=True,exist_ok=True);rows=[]
for sanitize in (False,True):
 binary=a.output/('sanitized' if sanitize else 'normal');cmd=['cc','-std=c11','-Wall','-Wextra','-Werror',*(['-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie'] if sanitize else []),'-I'+str(a.runtime/'sdk/app'),str(root/'tests/home_performance_trace_test.c'),'-o',str(binary)];subprocess.run(cmd,check=True);r=subprocess.run([str(binary)],check=True,text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});rows.append({'sanitized':sanitize,'command':cmd,'stdout':r.stdout});print(r.stdout,end='')
files=[root/'tests/home_performance_trace_test.c',root/'apps/clock/performance_trace.h'];(a.output/'receipt.json').write_text(json.dumps({'passed':True,'tests':rows,'inputs':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},'physical_verified':False,'scope':'helper phase ordering/local counters/legacy ABI and retained callback fences; product qualification separate'},indent=2)+'\n')
