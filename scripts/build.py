#!/usr/bin/env python3
"""Build the shared watch artifacts once, independent of board or resolution."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
from compact_current_elf import compact
from normalize_xtensa_relocations import normalize
ROOT=Path(__file__).resolve().parents[1]
FLAGS=['-DWATCH_FAST_WAKE','-DPORTABLE_LOW_BATTERY','-DWATCH_RUNTIME_FEATURES','-DWATCH_BLE_BROADCAST',
 '-DWATCH_CLOCK_LAUNCHER','-DWATCH_CLOCK_ALARMS','-DWATCH_CLOCK_POINTS',
 '-DPORTABLE_RTC_UTC8_DENVER','-DWATCH_PAIRED_BOOT_CONFIRM','-DWATCH_QUICK_ACTIONS',
 '-DWATCH_QUICK_RADIOS','-DWATCH_MOTION_WAKE','-DWATCH_ALARM_SLEEP_RESUME','-DWATCH_CONTEXTS_CLIENT',
 '-flto',*['-Wl,--undefined='+s for s in ('__udivdi3','__umoddi3','__divdi3','__moddi3')]]
p=argparse.ArgumentParser(description=__doc__)
for name in ('system','utilities'):p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--output',type=Path,default=ROOT/'dist')
p.add_argument('--cc',default=os.environ.get('WATCH_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc'))
p.add_argument('--role',choices=['default','clock','both'],default='both')
a=p.parse_args()
if not a.cc:p.error('Set WATCH_APP_CC or --cc to the pinned ESP32-S3 compiler')
S=a.system.resolve();U=a.utilities.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
compiler=subprocess.check_output([a.cc,'--version'],text=True).splitlines()[0]
if '8.4.0' not in compiler:raise RuntimeError('Expected the deployed ESP32-S3 GCC 8.4.0 toolchain')
includes=[S/'lib/PortableApps/include',U/'lib/Alarm/include',U/'lib/Contexts/include',ROOT/'sdk/app',ROOT/'sdk/driver',ROOT/'include']
incs=['-I'+str(x) for x in includes]
common=['-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections']
effect=out/'boot-effect.o'
subprocess.run([a.cc.removesuffix('gcc')+'g++','-std=c++11',*common,'-fno-exceptions','-fno-rtti','-fno-threadsafe-statics',*incs,'-c',str(ROOT/'apps/clock/effects/boot.cpp'),'-o',str(effect)],check=True)
sources=[ROOT/'apps/clock/crown.c',ROOT/'apps/clock/viewport.c',ROOT/'apps/clock/capability_clients.c',ROOT/'apps/clock/quick_radios.c',ROOT/'apps/clock/nova/nova.c',ROOT/'apps/clock/points_projection.c',ROOT/'apps/clock/effects/divdi3.c',*[S/'lib/PortableApps/src'/n for n in ('quick_actions.c','quick_render.c','quick_session.c')],effect]
validator=out/'validate-elf'
subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(S/'test/native_apps/stubs'),'-I'+str(S/'lib/elf_loader/include'),str(S/'lib/elf_loader/src/esp_elf_validate.c'),str(S/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
records={'schema':1,'compiler':compiler,'dependency_lock':json.loads((ROOT/'dependencies.lock.json').read_text()),'build_variant':'shared-no-board-defines','hardware_verified':False,'artifacts':{}}
for role in ['default','clock'] if a.role=='both' else [a.role]:
 target=out/(role+'.elf');mapping=out/(role+'.map');mapping.write_text('{ global: app_main; local: *; };\n')
 flags=FLAGS if role=='default' else []
 role_sources=sources if role=='default' else [ROOT/'apps/clock/return_to_default.c']
 command=[a.cc,'-std=c11',*common,'-nostdlib','-nostartfiles','-shared','-Wl,--gc-sections','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wl,--version-script='+str(mapping),*flags,*incs,*map(str,role_sources),'-lgcc','-o',str(target)]
 subprocess.run(command,check=True);normalize(target)
 proof=compact(target,a.cc,debug_path=out/'debug'/(role+'.elf'))
 subprocess.run([str(validator),str(target)],check=True)
 symbols=subprocess.check_output([a.cc.removesuffix('gcc')+'nm','-D',str(target)],text=True)
 imports={r.split()[-1] for r in symbols.splitlines() if ' U ' in ' '+r}
 exports={r.split()[-1] for r in symbols.splitlines() if len(r.split())>=3 and r.split()[-2] in ('T','D','B','R')}
 allowed={'risc_runtime_get_api','memcpy','memset','memcmp','memmove','strcmp','strncmp','strlen','snprintf','malloc','calloc','free','strcpy','memchr','strrchr'}
 if not imports<=allowed or exports!={'app_main'}:raise RuntimeError(f'Unexpected application ABI: {imports=}, {exports=}')
 raw=target.read_bytes()
 if raw[:7]!=b'\x7fELF\x01\x01\x01' or raw[16:20]!=b'\x03\x00\x5e\x00':raise RuntimeError('Not an ESP32-S3 shared ELF')
 if role=='clock' and (imports or len(raw)>4096):raise RuntimeError('Return module unexpectedly contains services or Home assets')
 shutil.copy2(ROOT/'apps/clock'/(role+'.json'),out/(role+'.json'))
 records['artifacts'][role]={'role':'home' if role=='default' else 'return-to-default','file_name':target.name,'bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest(),'imports':sorted(imports),'exports':sorted(exports),'compaction':proof,'defines':flags}
 print(role+': target build and Runtime ELF validation PASS',flush=True)
(out/'build.json').write_text(json.dumps(records,indent=2)+'\n')
