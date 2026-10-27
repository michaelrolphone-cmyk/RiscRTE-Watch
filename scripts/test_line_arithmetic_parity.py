#!/usr/bin/env python3
"""Exact fline buffers, checkpoint state and signed-width/root equivalence."""
import argparse, pathlib, subprocess, tempfile, os, json, hashlib
p=argparse.ArgumentParser();p.add_argument('--baseline',required=True);p.add_argument('--source-root',type=pathlib.Path);p.add_argument('--output',type=pathlib.Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
r=pathlib.Path(__file__).resolve().parents[1];a.output.mkdir(parents=True,exist_ok=True);src=a.source_root or r
with tempfile.TemporaryDirectory(prefix='line-width-') as tmp:
 t=pathlib.Path(tmp);s=(src/'apps/clock/nova/nova.c').read_text().replace('#include "../faces/render.inc"','#include "'+str(r/'apps/clock/faces/render.inc')+'"');(t/'nova-candidate.c').write_text(s)
 old=subprocess.check_output(['git','-C',str(r),'show',a.baseline+':apps/clock/faces/render.inc'],text=True);start=old.index('static void fline(');end=old.index('#include "annulus_spans.h"',start);(t/'line_reference.inc').write_text(old[start:end].replace('static void fline(','static void fline_reference(',1))
 cmd=['cc','-std=c11','-O1','-Wall','-Wextra','-Werror','-DWATCH_CONTEXTS_CLIENT','-DWATCH_COMPACT_NOVA_ASSETS','-DWATCH_COMPRESSED_CALENDAR_ALPHA','-DWATCH_COMPRESSED_FACE_ALPHA','-I'+str(t),*['-I'+str(src/x) for x in ('.','include','sdk/app','sdk/driver','apps/clock/nova','apps/clock/faces')],str(r/'tests/line_arithmetic_parity_test.c'),'-o',str(t/'test')]
 if a.sanitize:cmd+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 subprocess.run(cmd,check=True)
 out=subprocess.check_output([str(t/'test')],text=True,env=os.environ|{'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1'});(a.output/'output.txt').write_text(out)
 (a.output/'receipt.json').write_text(json.dumps(dict(passed=True,sanitized=a.sanitize,baseline=a.baseline,command=cmd,source_sha256={str(x.relative_to(r)):hashlib.sha256(x.read_bytes()).hexdigest() for x in [r/'apps/clock/faces/render.inc',r/'tests/line_arithmetic_parity_test.c',pathlib.Path(__file__)]}),indent=2)+'\n');print(out)
