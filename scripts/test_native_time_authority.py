#!/usr/bin/env python3
"""Exercise actual Home with explicit native time, normal and ASan/UBSan."""
import argparse
import ast
import os
from pathlib import Path
import subprocess
import tempfile
from merged_sdk import headers

ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for name in ('system','utilities'):p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args()
# Verify the public CLI keeps the external-clock choice unless requested.
tree=ast.parse((ROOT/'scripts/build.py').read_text())
option=next(n for n in ast.walk(tree) if isinstance(n,ast.Call) and n.args and
            isinstance(n.args[0],ast.Constant) and n.args[0].value=='--time-authority')
kwargs={k.arg:ast.literal_eval(k.value) for k in option.keywords}
assert kwargs['choices']==['external-rtc','native'] and kwargs['default']=='external-rtc'
with tempfile.TemporaryDirectory(prefix='watch-native-time-') as temporary:
 out=Path(temporary);sdk=headers(out,a.system,a.utilities,ROOT)
 sources=[ROOT/'tests/native_time_authority_test.c',*[ROOT/'apps/clock'/name for name in
    ('nova/nova.c','points_projection.c','viewport.c','capability_clients.c','quick_radios.c','watch_quick_session.c')],
    *[a.system/'lib/PortableApps/src'/name for name in ('quick_actions.c','quick_render.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c')]]
 for sanitized in (False,True):
  flags=['-O1','-g','-Wall','-Wextra','-Werror','-DWATCH_COMPACT_NOVA_ASSETS','-DWATCH_SHARED_PREFERENCE_HEADERS',
    '-DPORTABLE_LOW_BATTERY','-DWATCH_COMPRESSED_CALENDAR_ALPHA','-DWATCH_COMPRESSED_FACE_ALPHA',
    '-DWATCH_FAST_WAKE','-DWATCH_COLD_BOOT_ANIMATION','-DPORTABLE_WATCH_NATIVE_HEIGHT',
    '-DWATCH_ATOMIC_PREFERENCES','-DPORTABLE_QUICK_ATOMIC_PREFERENCES',
    '-DPORTABLE_QUICK_RADIOS',*['-I'+str(path) for path in (sdk,a.system/'lib/PortableApps/src',ROOT/'include',ROOT)]]
  if sanitized:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
  objects=[]
  for i,source in enumerate(sources):
   obj=out/f'{i}.o';objects.append(str(obj))
   subprocess.run([os.environ.get('CC','cc'),'-std=c11',*flags,*(['-DWATCH_CONTEXTS_CLIENT'] if i else []),
       '-c',str(source),'-o',str(obj)],check=True)
  binary=out/'native-time'
  subprocess.run([os.environ.get('CXX','c++'),'-std=c++11',*flags,str(ROOT/'apps/clock/effects/boot.cpp'),*objects,'-o',str(binary)],check=True)
  subprocess.run([str(binary)],check=True,timeout=90,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
