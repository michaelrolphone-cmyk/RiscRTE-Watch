#!/usr/bin/env python3
import pathlib,tempfile,subprocess,os
r=pathlib.Path(__file__).resolve().parents[1]
s=(r/'apps/clock/nova/nova.c').read_text();start=s.index('static int native_q(');end=s.index('static int floor_q8',start)
with tempfile.TemporaryDirectory(prefix='native-q-exact-') as d:
 p=pathlib.Path(d);(p/'native_q_under_test.inc').write_text(s[start:end])
 for sanitizer in (False,True):
  command=['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+d,str(r/'tests/native_q_exact_test.c'),'-o',str(p/'test')]
  if sanitizer:command+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie']
  subprocess.run(command,check=True);subprocess.run([str(p/'test')],check=True,env=os.environ|{'ASAN_OPTIONS':'detect_leaks=0'})
