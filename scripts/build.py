#!/usr/bin/env python3
"""Build the shared watch artifacts once, independent of board or resolution."""
import argparse,hashlib,json,os,re,shutil,subprocess,sys
from pathlib import Path
from compact_current_elf import compact
from normalize_xtensa_relocations import normalize
from build_inputs import file_manifest, verify_dependency
from time_defaults import local_time, time_default_flags
from sleep_defaults import add_sleep_defaults_arguments, sleep_default_flags, sleep_defaults_record
from raster_cache_options import add_raster_cache_arguments, raster_cache_flags, raster_cache_record
ROOT=Path(__file__).resolve().parents[1]
FLAGS=['-DWATCH_COMPACT_NOVA_ASSETS','-DWATCH_CRASH_REPORT','-DCRASH_REPORT_SESSION_FILES=1','-DWATCH_CRASH_REPORT_NAMESPACE=61','-DWATCH_SHARED_PREFERENCE_HEADERS','-DPORTABLE_PREFERENCE_STORE','-DWATCH_ATOMIC_PREFERENCES','-DPORTABLE_QUICK_ATOMIC_PREFERENCES','-DPORTABLE_QUICK_RADIOS','-DWATCH_COMPRESSED_CALENDAR_ALPHA','-DWATCH_COMPRESSED_FACE_ALPHA','-DWATCH_FAST_WAKE','-DWATCH_COLD_BOOT_ANIMATION','-DPORTABLE_WATCH_NATIVE_HEIGHT','-DPORTABLE_LOW_BATTERY','-DWATCH_RUNTIME_FEATURES','-DWATCH_BLE_BROADCAST',
 '-DWATCH_CLOCK_LAUNCHER','-DWATCH_CLOCK_ALARMS','-DWATCH_CLOCK_POINTS',
 '-DPORTABLE_RTC_UTC8_DENVER','-DWATCH_PAIRED_BOOT_CONFIRM','-DWATCH_QUICK_ACTIONS',
 '-DWATCH_QUICK_RADIOS','-DWATCH_MOTION_WAKE','-DWATCH_ALARM_SLEEP_RESUME','-DWATCH_CONTEXTS_CLIENT',
 '-flto',*['-Wl,--undefined='+s for s in ('__udivdi3','__umoddi3','__divdi3','__moddi3')]]
p=argparse.ArgumentParser(description=__doc__)
for name in ('system','utilities'):p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--output',type=Path,default=ROOT/'dist')
p.add_argument('--cc',default=os.environ.get('WATCH_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc'))
p.add_argument('--role',choices=['default','clock','both'],default='both')
p.add_argument('--time-authority',choices=['external-rtc','native'],default='external-rtc',help='Clock authority: legacy external RTC reconciliation or validated Runtime time (no implicit seed)')
p.add_argument('--unset-time-local',type=local_time,help='Explicit YYYY-MM-DDTHH:MM:SS fallback, native authority only; seed only when UNSET using saved time zone')
p.add_argument('--native-alarm-v2',action='store_true',help='Explicit native UTC tagged API2 alarm Home; exact separate source profile, no Points default')
p.add_argument('--clock-edit-authority',action='store_true',help='Explicit editor-aware tagged alarm descriptor acceptance; requires native-alarm-v2')
p.add_argument('--native-quick-actions',action='store_true',help='Provider safe-content Quick with independent radio availability; requires clock-edit authority')
p.add_argument('--native-performance-runtime',type=Path,help='Optional bounded RAM Home phases using exact Runtime273 ABI')
p.add_argument('--interaction-trace',action='store_true',help='Sparse safe-boundary Home interaction timing and RAM last-checkpoint diagnostics')
p.add_argument('--automatic-provision',action='store_true',help='Explicit generic foreground core-profile reconciliation')
p.add_argument('--serial-transfer-mode',action='store_true',help='Opt-in cooperative Home exit for a dedicated-transfer Runtime product')
add_sleep_defaults_arguments(p)
add_raster_cache_arguments(p)
a=p.parse_args()
if a.native_quick_actions and not a.clock_edit_authority:p.error('--native-quick-actions requires --clock-edit-authority')
if a.clock_edit_authority and not a.native_alarm_v2:p.error('--clock-edit-authority requires --native-alarm-v2')
if a.native_alarm_v2 and a.time_authority!='native':p.error('--native-alarm-v2 requires --time-authority native')
if a.unset_time_local is not None and a.time_authority!='native':p.error('--unset-time-local requires --time-authority native')
if not a.cc:p.error('Set WATCH_APP_CC or --cc to the pinned ESP32-S3 compiler')
S=a.system.resolve();U=a.utilities.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
lock=json.loads((ROOT/'dependencies.lock.json').read_text())
if a.native_alarm_v2:
 selected_profile='profiles/native-quick-actions.json' if a.native_quick_actions else 'profiles/native-clock-edit.json' if a.clock_edit_authority else 'profiles/native-alarm-v2.json'
 selected=json.loads((ROOT/selected_profile).read_text())
 for key in ('system','utilities'):lock[key]={**lock[key],'commit':selected[key]}
system_paths=['lib/PortableApps','lib/elf_loader','test/native_apps']
if a.native_performance_runtime:system_paths+=['scripts/runtime_sdk_identity.py']
utility_paths=['Apps/alarm_writer.h','lib/Alarm/include','lib/Contexts/include']
verified_dependencies={'system':verify_dependency(S,lock['system']['commit'],system_paths),
 'utilities':verify_dependency(U,lock['utilities']['commit'],utility_paths)}
home_paths=['dependencies.lock.json','scripts/build.py','scripts/raster_cache_options.py','scripts/sleep_defaults.py','scripts/time_defaults.py','scripts/build_inputs.py','scripts/merged_sdk.py','scripts/compact_current_elf.py',
 'scripts/normalize_xtensa_relocations.py','sdk/app','sdk/driver','include/WatchViewport.h','include/WatchLayout.h',
 'include/WatchHomeLayout.h','include/WatchCapabilityClients.h','include/WatchCapabilities.h',
 'include/WatchTapPolicy.h','include/PortableMotionTap.h','include/PortablePowerStatus.h','include/PortableTapSettings.h',
 'include/twatch_calendar.h','include/twatch_caps.h','include/twatch_power.h','include/twatch_util.h',
 'apps/runtime_features/clock_runtime.h','apps/runtime_features/sdk',
 'apps/clock/performance_trace.h','apps/clock/interaction_trace.h','apps/clock/clock_provision.inc','apps/clock/clock_crash.inc','apps/clock/crash_text.h','apps/clock/crown.c','apps/clock/viewport.c','apps/clock/capability_clients.c','apps/clock/quick_radios.c',
 'apps/clock/points_projection.c','apps/clock/points_projection.h','apps/clock/points_compat.h',
 'apps/clock/nova','apps/clock/faces','apps/clock/transitions','apps/clock/effects/boot.cpp',
 'apps/clock/effects/effects.h','apps/clock/effects/divdi3.c','apps/clock/clock_alarm.inc',
 'apps/clock/clock_contexts.inc','apps/clock/clock_atomic.inc','apps/clock/quick_pending.inc','apps/clock/launcher_touch.h','apps/clock/display_time.h',
 'apps/clock/WatchSleepDefaults.h','apps/clock/watch_quick_session.c','apps/clock/PortableSleepPolicy.h','apps/clock/PortableTimeFormat.h','apps/clock/watch_sleep.h',
 'apps/clock/watch_alarm_sleep.h','apps/clock/watch_motion_client.h','apps/clock/runtime_boot_confirm.h',
 'apps/clock/return_to_default.c','apps/clock/default.json','apps/clock/clock.json']
if a.native_alarm_v2:home_paths+=[selected_profile,'apps/clock/WatchRuntimeSleep.h']
home_inputs=file_manifest(ROOT,home_paths)
compiler=subprocess.check_output([a.cc,'--version'],text=True).splitlines()[0]
if '15.2.0' not in compiler or 'esp-15.2.0_20251204' not in compiler:raise RuntimeError('Expected the pinned native ESP32-S3 GCC 15.2.0 toolchain')
from merged_sdk import headers
includes=[headers(out,S,U,ROOT),ROOT/'include',S/'lib/PortableApps/src']
performance=None
if a.native_performance_runtime:
 runtime=a.native_performance_runtime.resolve()
 pinned='273bdbccf65647a803ab0586a418f8283732baec'
 sys.path.insert(0,str(S/'scripts'))
 from runtime_sdk_identity import verify_runtime_sdk
 sdk_identity=verify_runtime_sdk(runtime,pinned)
 def canonical_read(name):return subprocess.check_output(['git','-C',str(runtime),'show',pinned+':sdk/app/'+name])
 canonical=canonical_read('RiscRuntimeV1.h').decode(); staged=includes[0]/'RiscRuntimeV1.h'
 # Current Watch carries the same trace suffix; never replace its extended SDK.
 marker='#define RISC_RUNTIME_TRACE_V1_SIZE'
 def prefix(text):
  text=re.sub(r'/\*.*?\*/','',text,flags=re.S);text=re.sub(r'\s+','',text)
  end='uint32_t(*trace)(uint32_tinteraction_id,uint32_tphase,uint32_tvalue);'
  return text[text.index('uint32_tapi_version,struct_size;'):text.index(end)+len(end)]
 if marker not in staged.read_text() or prefix(staged.read_text())!=prefix(canonical):raise RuntimeError('Mismatched current performance ABI prefix')
 performance_header=canonical_read('RiscPerformanceV1.h');(includes[0]/'RiscPerformanceV1.h').write_bytes(performance_header)
 performance={'runtime':pinned,'runtime_sdk_identity':sdk_identity,'sdk_verifier_sha256':hashlib.sha256((S/'scripts/runtime_sdk_identity.py').read_bytes()).hexdigest(),'header_sha256':hashlib.sha256(performance_header).hexdigest(),'storage':'bounded native RAM','replay':'explicit perf command','callbacks':'frame/input milestones only; no pixel callback','physical_verified':False}

incs=['-I'+str(x) for x in includes]
common=['-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections']
effect=out/'boot-effect.o'
effect_command=[a.cc.removesuffix('gcc')+'g++','-std=c++11',*common,'-fno-exceptions','-fno-rtti','-fno-threadsafe-statics',*incs,'-c',str(ROOT/'apps/clock/effects/boot.cpp'),'-o',str(effect)]
subprocess.run(effect_command,check=True)
sources=[ROOT/'apps/clock/crown.c',ROOT/'apps/clock/viewport.c',ROOT/'apps/clock/capability_clients.c',ROOT/'apps/clock/quick_radios.c',ROOT/'apps/clock/watch_quick_session.c',ROOT/'apps/clock/nova/nova.c',ROOT/'apps/clock/points_projection.c',ROOT/'apps/clock/effects/divdi3.c',*[S/'lib/PortableApps/src'/n for n in ('quick_actions.c','quick_render.c')],effect]
if a.time_authority=='native':sources += [S/'lib/PortableApps/src'/n for n in ('PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c')]
if a.automatic_provision:sources += [S/'lib/PortableApps/src/PortableAutomaticProvision.c',S/'lib/PortableApps/src/PortableNetworkSession.c']
validator=out/'validate-elf'
subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(S/'test/native_apps/stubs'),'-I'+str(S/'lib/elf_loader/include'),str(S/'lib/elf_loader/src/esp_elf_validate.c'),str(S/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
records={'unset_time_default':{'local':a.unset_time_local,'scope':'only-when-unset','zone':'shared-saved-or-UTC','accuracy':'user-selected-placeholder-not-synchronized'},'time_authority':a.time_authority,'display_time_policy':'runtime-utc-saved-time-zone' if a.time_authority=='native' else 'external-rtc-utc8-denver','sleep_defaults':sleep_defaults_record(a),'automatic_provision':a.automatic_provision,'schema':1,'compiler':compiler,'dependency_lock':lock,'verified_dependencies':verified_dependencies,'home_files_sha256':home_inputs,'build_variant':'shared-no-board-defines','preferences':'runtime-provider','hardware_profile_inputs':False,'hardware_verified':False,'compiler_sha256':hashlib.sha256(Path(a.cc).read_bytes()).hexdigest(),'artifacts':{}}
if a.native_alarm_v2:records['native_alarm_v2']=selected
if a.clock_edit_authority:records['clock_edit_authority']={'enabled':True,'dispatches_editor_callbacks':False,'profile':selected_profile}
records['native_quick_actions']=a.native_quick_actions
records['interaction_trace']=a.interaction_trace
records['native_performance_trace']=performance
records['native_raster_caches']=raster_cache_record(a,home_inputs)
if a.serial_transfer_mode:records['serial_transfer_mode']=True
for role in ['default','clock'] if a.role=='both' else [a.role]:
 target=out/(role+'.elf');mapping=out/(role+'.map');mapping.write_text('{ global: app_main; local: *; };\n')
 flags=[*FLAGS,*sleep_default_flags(a)] if role=='default' else []
 flags+=raster_cache_flags(a,role)
 if role=='default' and a.time_authority=='native':
  flags.remove('-DPORTABLE_RTC_UTC8_DENVER');flags.append('-DWATCH_NATIVE_TIME_AUTHORITY');flags+=time_default_flags(a.unset_time_local)
 if role=='default' and a.native_alarm_v2:
  flags.remove('-DWATCH_CLOCK_POINTS')
  flags+=['-DWATCH_NATIVE_ALARM_V2','-DALARM_SERVICE_TAGGED_V2']
  if a.clock_edit_authority:flags.append('-DALARM_SERVICE_CLOCK_EDIT_V2')
 if role=='default' and a.native_quick_actions:flags+=['-DPORTABLE_QUICK_CAPABILITY_SAFE','-DPORTABLE_QUICK_SAFE_CONTENT']
 if role=='default' and performance:flags.append('-DWATCH_PERFORMANCE_TRACE')
 if role=='default' and a.interaction_trace:flags.append('-DWATCH_INTERACTION_TRACE')
 if role=='default' and a.automatic_provision:flags.append('-DWATCH_AUTOMATIC_PROVISION')
 if role=='default' and a.serial_transfer_mode:flags.append('-DWATCH_SERIAL_TRANSFER_MODE')
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
 manifest=json.loads((ROOT/'apps/clock'/(role+'.json')).read_text())
 if role=='default':
  if a.time_authority=='native':manifest['requires']=[r for r in manifest['requires'] if r['capability']!='rtc.clock']
  manifest['version']='0.20.6' if a.serial_transfer_mode else ('0.20.5' if a.automatic_provision else '0.19.4')
  if a.native_alarm_v2:
   manifest['optional']=[row for row in manifest['optional'] if row['capability']!='alarm.service']
   manifest['requires']=[row for row in manifest['requires'] if row['capability']!='alarm.service']
   manifest['requires'].append({'capability':'alarm.service','api':2})
  if a.automatic_provision:
   manifest['requires'].append({'capability':'software.installed-apps','api':1})
   manifest['optional'].append({'capability':'storage.volume','api':1})
 (out/(role+'.json')).write_text(json.dumps(manifest,indent=2)+'\n')
 records['artifacts'][role]={'role':'home' if role=='default' else 'return-to-default','file_name':target.name,'bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest(),'imports':sorted(imports),'exports':sorted(exports),'compaction':proof,'defines':flags}
 records['artifacts'][role]['compile_command']=command
 records['artifacts'][role]['compile_inputs']={'sources':list(map(str,role_sources)),'include_directories':list(map(str,includes)),'definitions':[x for x in flags if x.startswith('-D')],'source_sha256':{str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in role_sources}}
 if role=='default':records['artifacts'][role]['object_sources']={str(effect):{'source':str(ROOT/'apps/clock/effects/boot.cpp'),'language':'c++','definitions':[],'include_directories':list(map(str,includes)),'command':effect_command,'source_sha256':hashlib.sha256((ROOT/'apps/clock/effects/boot.cpp').read_bytes()).hexdigest()}}
 print(role+': target build and Runtime ELF validation PASS',flush=True)
# Detect changed input bytes before certifying the completed artifacts.
if home_inputs!=file_manifest(ROOT,home_paths):raise RuntimeError('Home inputs changed during the build')
if verified_dependencies!={'system':verify_dependency(S,lock['system']['commit'],system_paths),
 'utilities':verify_dependency(U,lock['utilities']['commit'],utility_paths)}:raise RuntimeError('Dependency inputs changed during the build')
(out/'build.json').write_text(json.dumps(records,indent=2)+'\n')
