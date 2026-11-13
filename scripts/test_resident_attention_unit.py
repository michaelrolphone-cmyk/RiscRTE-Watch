#!/usr/bin/env python3
"""Narrow production Home observation logic with explicit endpoint doubles."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
from merged_sdk import headers
R=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser()
for n in ('system','utilities','output'):p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--sanitize',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
sdk=headers(a.output,a.system,a.utilities,R);source=R/'tests/attention/controller_test.c'
flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-I'+str(sdk),'-I'+str(R/'include'),'-I'+str(R)]
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
deps=subprocess.check_output(['cc',*flags,'-MM',str(source)],text=True).replace('\\\n',' ').split(':',1)[1].split();sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();before={p:sha(p) for p in deps}
binary=a.output/'test';cmd=['cc',*flags,str(source),'-o',str(binary)];subprocess.run(cmd,check=True)
x=subprocess.run([str(binary)],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},capture_output=True,text=True);(a.output/'test.log').write_text(x.stdout+x.stderr);print(x.stdout+x.stderr,end='');x.check_returncode();assert before=={p:sha(p) for p in deps}
(a.output/'receipt.json').write_text(json.dumps({'command':cmd,'sanitize':a.sanitize,'compiler_inputs':before,'binary_sha256':sha(binary),'scope':'Real Home attention include, endpoint/Runtime doubles; no physical/target execution'},indent=2)+'\n')
