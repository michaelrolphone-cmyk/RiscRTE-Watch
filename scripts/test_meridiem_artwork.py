#!/usr/bin/env python3
"""Compare every face with pinned predecessor outside the two changed overlays."""
from pathlib import Path
import subprocess,tempfile,os,tarfile,io
root=Path(__file__).resolve().parents[1];baseline='447b94f13ad963701414d2f17223d0218ee85b5f'
with tempfile.TemporaryDirectory(prefix='watch-artwork-') as d:
 d=Path(d);old=d/'old';old.mkdir();archive=subprocess.check_output(['git','-C',str(root),'archive',baseline]);tarfile.open(fileobj=io.BytesIO(archive)).extractall(old,filter='data')
 def render(source,name):
  exe=d/name;flags=['-std=c11','-O1','-Wall','-Wextra','-Werror','-DWATCH_CONTEXTS_CLIENT','-DWATCH_COMPACT_NOVA_ASSETS','-DWATCH_COMPRESSED_CALENDAR_ALPHA','-DWATCH_COMPRESSED_FACE_ALPHA']
  subprocess.run(['cc',*flags,*['-I'+str(source/x) for x in ['.','include','sdk/app','sdk/driver']],str(root/'tests/meridiem_artwork_test.c'),str(source/'apps/clock/nova/nova.c'),'-o',str(exe)],check=True)
  return subprocess.check_output([str(exe)],text=True).splitlines()
 before=render(old,'before');after=render(root,'after');assert len(before)==len(after);changed=0
 for a,b in zip(before,after):
  aa=a.split();bb=b.split();assert aa[:4]==bb[:4];assert aa[5]==bb[5],('unrelated artwork changed',a,b)
  if aa[4]!=bb[4]:assert int(aa[2])<2 and int(aa[3])<24;changed+=1
  elif int(aa[2])<2 and int(aa[3])<24:raise AssertionError(('missing overlay correction',a))
 print('PASS',len(after),'native face/state images; only',changed,'12-hour overlays changed; all pixels outside old/new overlay bounds exact; other modes/faces byte-identical')
