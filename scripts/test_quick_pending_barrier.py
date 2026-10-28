#!/usr/bin/env python3
import argparse,hashlib,json,os,pathlib,subprocess
p=argparse.ArgumentParser();p.add_argument('--system',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args();w=pathlib.Path(__file__).resolve().parents[1];a.output.mkdir(parents=True,exist_ok=True)
files=[w/'tests/quick_pending_barrier_test.c',a.system/'lib/PortableApps/src/quick_actions.c'];inputs=[*files,w/'apps/clock/quick_pending.inc',a.system/'lib/PortableApps/include/PortableQuickActions.h'];rows=[]
for san in (False,True):
 binary=a.output/('sanitized' if san else 'normal');flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
 cmd=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-DPORTABLE_QUICK_CAPABILITY_SAFE','-I'+str(a.system/'lib/PortableApps/include'),*map(str,files),'-o',str(binary)];subprocess.run(cmd,check=True);r=subprocess.run([binary],check=True,text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});print(r.stdout,end='');rows.append({'sanitized':san,'command':cmd,'stdout':r.stdout})
(a.output/'evidence.json').write_text(json.dumps({'passed':True,'tests':rows,'inputs':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in inputs},'modeled':'display acquisition/drawing/completion, input/key/alarm and preference restoration callbacks; actual production helper and System controller','raster_verified':False,'physical_verified':False},indent=2)+'\n')
