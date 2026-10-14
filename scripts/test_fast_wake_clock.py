#!/usr/bin/env python3
"""Exercise actual opt-in Watch Clock with modeled peripheral boundaries."""
from pathlib import Path
import argparse, os, subprocess
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--utilities',type=Path,required=True);p.add_argument('--native',action='store_true');p.add_argument('--atomic',action='store_true');a=p.parse_args()
out=root/(('dist/fast-wake-clock-native' if a.native else 'dist/fast-wake-clock')+('-atomic' if a.atomic else ''));out.mkdir(parents=True,exist_ok=True)
from merged_sdk import headers
sdk=headers(out,a.system_apps,a.utilities,root)
for sanitize in (False,True):
 flags=['-DWATCH_SHARED_PREFERENCE_HEADERS','-DPORTABLE_LOW_BATTERY','-DWATCH_COMPRESSED_CALENDAR_ALPHA','-DWATCH_COMPRESSED_FACE_ALPHA','-DWATCH_FAST_WAKE','-DWATCH_COLD_BOOT_ANIMATION','-DPORTABLE_WATCH_NATIVE_HEIGHT','-DPORTABLE_RTC_UTC8_DENVER','-O1','-g','-Wall','-Wextra','-Werror',*['-I'+str(x) for x in (sdk,root/'include',root)]]
 if a.atomic:flags+=['-DWATCH_ATOMIC_PREFERENCES','-DPORTABLE_QUICK_ATOMIC_PREFERENCES','-DPORTABLE_QUICK_RADIOS','-I'+str(a.system_apps/'lib/PortableApps/src')]
 if a.native:flags+=['-DTEST_WIDTH=410u','-DTEST_HEIGHT=502u']
 if sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 objects=[]
 for i,source in enumerate([root/'tests/fast_wake_clock_test.c',root/'apps/clock/nova/nova.c',root/'apps/clock/points_projection.c',root/'apps/clock/viewport.c',root/'apps/clock/capability_clients.c',root/'apps/clock/quick_radios.c',*[a.system_apps/'lib/PortableApps/src'/n for n in ('quick_actions.c','quick_render.c','quick_session.c')]]):
  obj=out/f'{int(sanitize)}-{i}.o';subprocess.run([os.environ.get('CC','cc'),'-std=c11',*flags,*(['-DWATCH_CONTEXTS_CLIENT'] if i else []),'-c',str(source),'-o',str(obj)],check=True);objects.append(str(obj))
 binary=out/f'test-{int(sanitize)}';subprocess.run([os.environ.get('CXX','c++'),'-std=c++11',*flags,str(root/'apps/clock/effects/boot.cpp'),*objects,'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True,timeout=60,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
