#!/usr/bin/env python3
"""Compare vertical coordinate recurrence with the prior literal division."""
import argparse,hashlib,json,os,pathlib,subprocess
p=argparse.ArgumentParser();p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args();r=pathlib.Path(__file__).resolve().parents[1];a.output.mkdir(parents=True,exist_ok=True)
base='06cf061b6fbe93a1d2f2f8a197ba773a54c98b18'
s=subprocess.check_output(['git','-C',str(r),'show',base+':apps/clock/nova/nova.c'],text=True);start=s.index('static void native_alpha(');end=s.index('static const nova_glyph *glyph',start)
(a.output/'alpha_reference.inc').write_text(s[start:end].replace('static void native_alpha(','static void native_alpha_reference(',1))
records=[]
for sanitized in (False,True):
 exe=a.output/('test-sanitized' if sanitized else 'test-normal')
 cmd=['cc','-std=c11','-O1','-Wall','-Wextra','-Werror','-DWATCH_CONTEXTS_CLIENT','-DWATCH_COMPACT_NOVA_ASSETS','-DWATCH_COMPRESSED_CALENDAR_ALPHA','-DWATCH_COMPRESSED_FACE_ALPHA',*['-I'+str(r/x) for x in ('.','include','sdk/app','sdk/driver')],'-I'+str(a.output),str(r/'tests/native_alpha_vertical_test.c'),'-o',str(exe)]
 if sanitized:cmd+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 subprocess.run(cmd,check=True);out=subprocess.check_output([str(exe)],text=True,env=os.environ|{'ASAN_OPTIONS':'detect_leaks=0'});print(out,end='');records.append({'command':cmd,'stdout':out,'binary_sha256':hashlib.sha256(exe.read_bytes()).hexdigest()});exe.unlink()
(a.output/'receipt.json').write_text(json.dumps({'passed':True,'baseline':base,'records':records},indent=2)+'\n')
