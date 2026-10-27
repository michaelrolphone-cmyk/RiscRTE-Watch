#!/usr/bin/env python3
"""Exact circle coverage/custody equivalence; no hardware performance claim."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--baseline-commit',default='e76a1d2ca88ce7bb3fd2f4766a4c436d3d28e75d');p.add_argument('--output',type=Path,required=True);a=p.parse_args();r=Path(__file__).resolve().parents[1];a.output.mkdir(parents=True,exist_ok=True)
s=subprocess.check_output(['git','-C',str(r),'show',a.baseline_commit+':apps/clock/faces/render.inc'],text=True);start=s.index('static void fcirc(');end=s.index('static void fpoint',start);ref=a.output/'circle_reference.inc';ref.write_text(s[start:end].replace('static void fcirc(','static void fcirc_reference(',1));records=[]
for sanitize in (False,True):
 for name in ('annulus_spans','circle_raster_parity'):
  label=name+('-sanitized' if sanitize else '-normal');exe=a.output/label
  flags=['-std=c11','-O1' if sanitize else '-O2','-g','-Wall','-Wextra','-Werror','-DWATCH_CONTEXTS_CLIENT','-DWATCH_COMPACT_NOVA_ASSETS','-DWATCH_COMPRESSED_CALENDAR_ALPHA','-DWATCH_COMPRESSED_FACE_ALPHA',*['-I'+str(r/x) for x in ('.','include','sdk/app','sdk/driver')],'-I'+str(a.output)]
  if sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
  cmd=['cc',*flags,str(r/'tests'/(name+'_test.c')),'-o',str(exe)];subprocess.run(cmd,check=True);out=subprocess.check_output([str(exe)],text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});(a.output/(label+'.log')).write_text(out);print(out,end='');records.append({'label':label,'command':cmd,'stdout':out})
inputs=[r/'apps/clock/faces/annulus_spans.h',r/'apps/clock/faces/render.inc',r/'tests/annulus_spans_test.c',r/'tests/circle_raster_parity_test.c',Path(__file__),ref]
(a.output/'receipt.json').write_text(json.dumps({'passed':True,'baseline_commit':a.baseline_commit,'physical_timing':False,'records':records,'sha256':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}},indent=2)+'\n')
