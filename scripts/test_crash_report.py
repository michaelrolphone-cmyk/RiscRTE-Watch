"""Render exact copied report bytes with production RGB Home, normal + sanitizers."""
import os,subprocess,json,hashlib,argparse
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
r=Path(__file__).resolve().parents[1];receipts=[]
for sanitized in (False,True):
 flags=['-DWATCH_COMPACT_NOVA_ASSETS','-DWATCH_CRASH_REPORT','-DWATCH_CONTEXTS_CLIENT','-DWATCH_COMPRESSED_CALENDAR_ALPHA','-DWATCH_COMPRESSED_FACE_ALPHA','-O1','-g','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections',*['-I'+str(r/x) for x in ('include','sdk/driver','sdk/app','.')]]
 if sanitized:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 out=a.output/('sanitized' if sanitized else 'normal');cmd=['cc','-std=c11',*flags,str(r/'tests/crash_text_test.c'),str(r/'apps/clock/nova/nova.c'),'-Wl,--gc-sections','-o',str(out)]
 subprocess.run(cmd,check=True);run=subprocess.run([str(out)],capture_output=True,text=True,check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});print(run.stdout);(a.output/(out.name+'.log')).write_text(run.stdout+run.stderr);receipts.append({'command':cmd,'stdout':run.stdout})
(a.output/'receipt.json').write_text(json.dumps({'runs':receipts,'source_sha256':{str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [r/'tests/crash_text_test.c',r/'apps/clock/nova/nova.c',r/'apps/clock/nova/crash_font.inc',r/'apps/clock/crash_text.h']}},indent=2)+'\n')
