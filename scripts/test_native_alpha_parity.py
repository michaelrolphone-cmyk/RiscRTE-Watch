#!/usr/bin/env python3
"""Exact rendered-buffer AND callback-count parity; software-only timing."""
import argparse,hashlib,json,os,pathlib,subprocess,tempfile,time
p=argparse.ArgumentParser();p.add_argument('--baseline',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
r=pathlib.Path(__file__).resolve().parents[1];a.output.mkdir(parents=True,exist_ok=True);records=[]
inputs=[pathlib.Path(__file__),r/'tests/native_alpha_parity_test.c']
for root in (r,a.baseline):
 for folder in ('apps/clock/nova','apps/clock/faces','include','sdk'):
  inputs.extend(p for p in (root/folder).rglob('*') if p.is_file())
hashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
with tempfile.TemporaryDirectory(prefix='alpha-parity-') as tmp:
 for label,root in [('baseline',a.baseline),('candidate',r)]:
  exe=pathlib.Path(tmp)/label
  cmd=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DWATCH_CONTEXTS_CLIENT','-DWATCH_COMPACT_NOVA_ASSETS','-DWATCH_COMPRESSED_CALENDAR_ALPHA','-DWATCH_COMPRESSED_FACE_ALPHA',*['-I'+str(root/x) for x in ('.','include','sdk/app','sdk/driver')],str(r/'tests/native_alpha_parity_test.c'),str(root/'apps/clock/nova/nova.c'),'-o',str(exe)]
  if a.sanitize:cmd+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
  subprocess.run(cmd,check=True);began=time.monotonic();out=a.output/(label+'.txt')
  with out.open('w') as f:subprocess.run([str(exe)],stdout=f,check=True,env=os.environ|{'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1'})
  records.append(dict(label=label,host_elapsed_seconds=time.monotonic()-began,sha256=hashlib.sha256(out.read_bytes()).hexdigest(),frames=len(out.read_text().splitlines()),command=cmd))
 assert records[0]['sha256']==records[1]['sha256'],'Pixel/callback mismatch'
assert all(hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()==h for p,h in hashes.items())
(a.output/'receipt.json').write_text(json.dumps(dict(passed=True,sanitized=a.sanitize,source_sha256=hashes,records=records,physical_timing=False,scope='Exact full buffer hash incl padding/guards, renderer outcome, callback count and ordered17-pixel sampled-state signature per checkpoint; complete partial-buffer hashes at five forced failure boundaries; source-recompiled normal and forced callback failure'),indent=2)+'\n')
print('PASS native alpha exact pixel/callback parity',records)
