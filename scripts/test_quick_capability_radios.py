#!/usr/bin/env python3
import argparse,json,os,pathlib,subprocess,hashlib
p=argparse.ArgumentParser();p.add_argument('--system',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args();w=pathlib.Path(__file__).resolve().parents[1];a.output.mkdir(parents=True,exist_ok=True)
inputs=[w/'tests/quick_capability_radios_test.c',w/'apps/clock/quick_radios.c'];rows=[]
for san in (False,True):
 out=a.output/('radio-san' if san else 'radio');flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
 cmd=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-DPORTABLE_QUICK_CAPABILITY_SAFE','-DPORTABLE_QUICK_ATOMIC_PREFERENCES','-I'+str(a.system/'lib/PortableApps/include'),'-I'+str(a.system/'lib/NativeApps/include'),*map(str,inputs),'-o',str(out)];subprocess.run(cmd,check=True);r=subprocess.run([out],check=True,capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});rows.append({'sanitized':san,'command':cmd,'stdout':r.stdout});print(r.stdout,end='')
(a.output/'evidence.json').write_text(json.dumps({'passed':True,'runs':rows,'source_sha256':{str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in inputs},'physical_tested':False},indent=2)+'\n')
