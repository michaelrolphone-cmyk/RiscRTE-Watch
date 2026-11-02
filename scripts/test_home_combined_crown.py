#!/usr/bin/env python3
"""Run production Home with the real capability-owned Canvas provider.
Both historical lifecycle fixtures stay unchanged. Controlled derivations and
all actual compiled source/header identities are recorded in hosted receipts.
"""
import argparse,ast,hashlib,json,os,re,shutil,subprocess,tempfile
from pathlib import Path
from merged_sdk import headers,sources as stage_sources
from home_alarm_fixture import tagged_alarm
ROOT=Path(__file__).resolve().parents[1]
CANVAS_COMMIT='cf483dfb592269aa232d7ba95d4f7459d644f9fe'
p=argparse.ArgumentParser(description=__doc__)
for name in ('system','utilities','canvas-source','output'):p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--width',type=int,default=240);p.add_argument('--height',type=int,default=240)
p.add_argument('--sanitized',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
actual=subprocess.check_output(['git','-C',str(a.canvas_source),'rev-parse','HEAD'],text=True).strip()
assert actual==CANVAS_COMMIT,(actual,CANVAS_COMMIT)
fixture_source=ROOT/'tests/fast_wake_clock_test.c';quick_source=ROOT/'tests/quick_clock_test.c'
quick_text=quick_source.read_text()
text,tagged_changes=tagged_alarm(fixture_source.read_text())
changes = [
 ('static bool fast_diagnostic(const char*s){','static bool fast_diagnostic(const char*s){fprintf(stderr,"%s\\n",s);'),
 ('static const risc_runtime_api_v1 fast_runtime=','static risc_runtime_api_v1 fast_runtime='),
 ('int main(void){','int inherited_fast_main(void){'),
]
for before,after in changes:
 assert text.count(before)==1,(before,text.count(before));text=text.replace(before,after)
changes=tagged_changes+changes
def replace_once(value,before,after):
 assert value.count(before)==1,(before,value.count(before))
 changes.append((before,after));return value.replace(before,after)
quick_text=replace_once(quick_text,'static int32_t face_get(void*c,const char*k,void*b,uint32_t n,uint32_t*z){(void)c;*z=0;','static int32_t face_get(void*c,const char*k,void*b,uint32_t n,uint32_t*z){(void)c;*z=0;if(hc_dnd_save_case()&&!strcmp(k,PQA_DND_KEY)){assert(n==1);*(uint8_t*)b=hc_dnd_value;*z=1;return RISC_KEY_VALUE_OK;}')
quick_text=replace_once(quick_text,'assert(atomic_publishing);','assert(atomic_publishing);if(hc_dnd_save_case()&&!strcmp(k,PQA_DND_KEY)){assert(n==1);hc_dnd_value=*(const uint8_t*)b;++hc_dnd_writes;return RISC_KEY_VALUE_OK;}')
text=replace_once(text,'#include "quick_clock_test.c"','#include "derived_quick.c"')
before='if(face.time_valid){twatch_rtc_time_v1 raw={0},expected={0};assert(watch_runtime_calendar(native_time.epoch_seconds,&raw));assert(watch_display_time(&raw,&expected));assert(!memcmp(&face.time,&expected,sizeof(expected)));}'
text=replace_once(text,before,'if(face.time_valid)hc_shared_expected();')
text=replace_once(text,'static void check_io(void){assert(!retained_fence);}','static void check_io(void){assert(!retained_fence&&!hc_query_pure);}')
for op in ('status','step','refresh','ack'):
 before={'status':'static int32_t alarm_status(void*c,alarm_status_v1*out){check_io();','step':'static int32_t alarm_step(void*c){check_io();','refresh':'static int32_t alarm_refresh(void*c){check_io();','ack':'static int32_t alarm_ack(void*c,const alarm_token_v1*token){check_io();'}[op]
 text=replace_once(text,before,before+'int fault=hc_alarm_fault("'+op+'");if(fault)return fault;')
text=replace_once(text,'check_io();(void)c;*cause=boot_cause;','check_io();if(hc_lost_case("boot-read-lost-retained"))return RISC_RETAINED_WAKE_ABSENT;(void)c;*cause=boot_cause;')
text=replace_once(text,'static int32_t fast_get(void*c,const char*k,void*b,uint32_t n,uint32_t*z){check_io();','static int32_t fast_get(void*c,const char*k,void*b,uint32_t n,uint32_t*z){check_io();if(!strcmp(k,"home_view")&&hc_lost_case("view-read-lost-retained"))return RISC_KEY_VALUE_NOT_FOUND;')
text=replace_once(text,'assert(hardware_brightness==clock_quick.brightness&&!pending&&!owned&&touch.subscription&&last_complete==submitted);','assert(hardware_brightness==hc_expected_brightness()&&!pending&&!owned&&touch.subscription&&last_complete==submitted);')
text=replace_once(text,'return &fast_runtime;}','return hc_runtime_lost?NULL:&fast_runtime;}')

text=replace_once(text,'assert(points_loads==1&&face.points==&clock_points_view','assert(points_loads==hc_expected_points_loads()&&face.points==&clock_points_view')
controls=(ROOT/'tests/home_canvas_crown_controls.inc').read_text()
controls=replace_once(controls,'check_io();\n if(!strcmp(name,"ui.canvas"))','check_io();\n int shared=hc_shared_acquire(name,version,instance,g);if(shared>=0)return shared!=0;\n if(!strcmp(name,"ui.canvas"))')
controls=replace_once(controls,'if(!strcmp(name,"ui.canvas"))','if(!strcmp(name,RISC_CANVAS_RASTER_CAPABILITY))')
controls=replace_once(controls,'canvas_provider_init(&fixture_canvas,&fixture_physical,&fixture_touch,rotation);','canvas_provider_init(&fixture_canvas,&fixture_physical,NULL,rotation);')
controls=replace_once(controls,' fixture_canvas.navigation=&hc_navigation;','')
controls=replace_once(controls,'if(!strncmp(canvas_case,"alarm-",6)){alarm_touch_test=true;','if(!strcmp(canvas_case,"alarm-above")||!strcmp(canvas_case,"alarm-retry")){alarm_touch_test=true;')
controls=replace_once(controls,'if(!strncmp(canvas_case,"alarm-",6))assert(alarm_touch_phase','if(!strcmp(canvas_case,"alarm-above")||!strcmp(canvas_case,"alarm-retry"))assert(alarm_touch_phase')
controls=replace_once(controls,'if(!strcmp(canvas_case,"controls-button")&&first_seen','if((!strcmp(canvas_case,"controls-button")||(!strncmp(canvas_case,"brightness-absent-",18)&&strncmp(canvas_case,"brightness-absent-save",22)))&&first_seen')
controls=replace_once(controls,'  check_clean();','  check_clean();if(!strcmp(canvas_case,"brightness-refuse")||!strcmp(canvas_case,"brightness-null-advertised")){assert(!first_seen&&!confirmations);free(physical_pixels);puts("Advertised malformed/refused brightness safely refuses startup PASS");return 0;}')
controls=replace_once(controls,'.supported_formats=RISC_DISPLAY_FORMAT_BIT(physical_format)};return true;','.supported_formats=RISC_DISPLAY_FORMAT_BIT(physical_format),.flags=hc_brightness_supported?RISC_DISPLAY_INFO_BRIGHTNESS:0};return true;')
controls=replace_once(controls,' hc_reset(format,rotation);',' hc_reset(format,rotation);hc_shared_start();')
controls=replace_once(controls,' app_main();',' hc_shared_run();')
controls=replace_once(controls,'retained_fence&&retains&&native_clock.halted','retained_fence&&(retains||hc_runtime_lost)&&(native_clock.halted||portable_home_bindings_retained())')

controls=replace_once(controls,'confirmations==1+wake_count','confirmations==1+wake_count+(fixture_provision_mode?1u:0u)')
controls=replace_once(controls,'if(!strcmp(canvas_case,"malformed-api")){assert(!first_seen&&!canvas_submits&&!confirmations);',
 'if(!strcmp(canvas_case,"malformed-api")||!strcmp(canvas_case,"absent")||!strcmp(canvas_case,"companion-absent")||!strcmp(canvas_case,"companion-malformed")||!strcmp(canvas_case,"policy-absent")||!strcmp(canvas_case,"policy-null")||!strcmp(canvas_case,"boot-null")){assert(!first_seen&&!canvas_submits&&!confirmations);')

controls=replace_once(controls,'if(strncmp(canvas_case,"nav-",4)||!first_seen)return true;','if(hc_save_navigation(out)||hc_crash_navigation(out))return true;\n if(strncmp(canvas_case,"nav-",4)||!first_seen)return true;')
controls=replace_once(controls,'++canvas_submits;return canvas_provider_submit(c,s,lease,out);','++canvas_submits;if(clock_crash_modal)++hc_crash_frames;return canvas_provider_submit(c,s,lease,out);')
controls=replace_once(controls,'static int32_t hc_next(void*c,uint64_t s,risc_canvas_event_v1*out){','static int32_t hc_next(void*c,uint64_t s,risc_canvas_event_v1*out){\n if(!strcmp(canvas_case,"crash-retained")&&clock_crash_modal&&hc_crash_frames){check_io();canvas_faulted=true;return RISC_CANVAS_RETAINED;}')
controls=replace_once(controls,'ended=false;return ok;}','ended=false;hc_shared_diagnostic(s);return ok;}')

text=(ROOT/'tests/home_combined_crown_prefix.inc').read_text()+'\n'+text+'\n'+controls+'\n'+(ROOT/'tests/home_combined_crown_services.inc').read_text()

(a.output/'generated_fixture.c').write_text(text)
(a.output/'derived_quick.c').write_text(quick_text)
tree=ast.parse((ROOT/'scripts/build.py').read_text())
node=next(n.value for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='FLAGS' for t in n.targets))
selected=eval(compile(ast.Expression(node),'<build FLAGS>','eval'))
assert '-DWATCH_HOME_CANVAS' in selected,'Production build must actually enable Canvas'
defined=set(re.findall(r'^#define ([A-Z_]+)',fixture_source.read_text(),re.M))
commands=[];runs=[];compiled={}
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
with tempfile.TemporaryDirectory(prefix='home-canvas-crown-') as tmp:
 w=Path(tmp);sdk=headers(w,a.system,a.utilities,ROOT);staged=stage_sources(w,a.system,ROOT)
 # Match the production combined-view canonical header replacement order.
 shared=a.canvas_source
 for name in ('PortableHomeBoot.h','PortableHomeBindings.h','PortableDeskClock.h','PortableTimeZone.h'):
  shutil.copyfile(shared/'lib/PortableApps/include'/name,sdk/name)
 for name in ('RiscCanvasV1.h','RiscCanvasInputV1.h','RiscCivilClockV1.h','RiscHomePresentationV1.h','RiscResidentContextsV1.h'):
  shutil.copyfile(shared/'sdk/app'/name,sdk/name)
 for name in ('RiscRetainedWakeV1.h','RiscRealtimeV1.h','RiscProviderPromotionV1.h'):
  shutil.copyfile(ROOT/'apps/runtime_features/sdk'/name,sdk/name)
 branch=next(n for n in tree.body if isinstance(n,ast.If) and isinstance(n.test,ast.Attribute) and n.test.attr=='shared_view_output')
 env={'FLAGS':selected,'includes':[sdk],'str':str}
 for statement in branch.body:
  if isinstance(statement,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='flags' for t in statement.targets) or isinstance(statement,ast.AugAssign) and isinstance(statement.target,ast.Name) and statement.target.id=='flags':
   exec(compile(ast.Module(body=[statement],type_ignores=[]),'<production shared flags>','exec'),env)
 selected=env['flags']
 assert all('-D'+name in selected for name in ('WATCH_SHARED_HOME_BOOT','WATCH_SHARED_HOME_CIVIL','WATCH_HOME_CANVAS_INPUT','WATCH_SHARED_HOME_RETAINED'))
 fixture=w/'fixture.c';fixture.write_text(text);(w/'derived_quick.c').write_text(quick_text)
 flags=['-O1','-g1','-Wall','-Wextra','-Werror','-Wno-unused-function',
  '-include',str(sdk/'PortableHomeBindings.h'),'-DPORTABLE_LOW_BATTERY',f'-DTEST_WIDTH={a.width}u',f'-DTEST_HEIGHT={a.height}u',
  *[v for v in selected if v.startswith('-D') and v[2:].split('=')[0] not in defined],
  *['-I'+str(v) for v in (sdk,ROOT/'include',ROOT,ROOT/'tests',staged,a.canvas_source/'Services/canvas_host',a.canvas_source/'lib/PortableApps/include')]]
 if a.sanitized:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 sources=[fixture,*[ROOT/'apps/clock'/n for n in ('nova/nova.c','points_projection.c','viewport.c','capability_clients.c','quick_radios.c')],
  *[staged/n for n in ('quick_actions.c','quick_render.c','quick_session.c','PortableAutomaticProvision.c','PortableNetworkSession.c')]]
 sources += [a.canvas_source/p for p in ('Apps/shared_home.c','lib/PortableApps/src/PortableHomeBoot.c',
  'lib/PortableApps/src/PortableTimeZone.c','lib/PortableApps/src/PortableTimeZoneCatalog.c',
  'Services/canvas_input/input.c','Services/civil_clock/clock.c')]
 def source_flags(source):
  if source.name=='shared_home.c':return ['-Dapp_main=hc_entry_main','-Dapp_module_init=hc_entry_init','-Dapp_module_fini=hc_entry_fini','-Drisc_resident_app_descriptor_v1=hc_entry_descriptor']
  if source.name=='input.c':return ['-Dt5_driver_get=hc_input_driver_get']
  if source.name=='clock.c':return ['-Dt5_driver_get=hc_civil_driver_get','-DCIVIL_CLOCK_ID="home-civil-fixture"','-DPORTABLE_RTC_UTC8_DENVER']
  return []
 def run(cmd):commands.append(list(map(str,cmd)));subprocess.run(cmd,check=True)
 objects=[]
 for i,source in enumerate(sources):
  obj=w/f'{i}.o';run(['cc','-std=c11',*flags,*(['-DWATCH_CONTEXTS_CLIENT'] if i else []),'-MMD','-MF',str(obj)+'.d',*source_flags(source),'-c',str(source),'-o',str(obj)]);objects.append(str(obj))
 binary=a.output/'test';run(['c++','-std=c++11',*flags,'-MMD','-MF',str(w/'boot.d'),str(ROOT/'apps/clock/effects/boot.cpp'),*objects,'-o',str(binary)])
 modes=['normal','cold','deep','sleep','saved','no-raw-touch','nav-home','nav-picker','nav-repeat','nav-quick','controls-button','multitouch','alarm-above','alarm-retry']
 cases=[(mode,encoding,rotation) for mode in modes for encoding in (5,1) for rotation in (0,1)]
 face_count=int(subprocess.check_output([str(binary),'face-count'],text=True))
 cases += [(f'face-{face}',encoding,0) for face in range(face_count) for encoding in (5,1)]
 cases+=[(mode,5,0) for mode in ('absent','malformed-api','open-retained','open-unknown','geometry-retained',
  'begin-retained','begin-unknown','malformed-frame','submit-retained','poll-retained','poll-unknown',
  'input-retained','quiesce-retained','close-retained','physical-retained',
  'companion-absent','companion-malformed','companion-retained','companion-close-retained','civil-absent','civil-malformed','civil-retained','zero-crash-namespace','brightness-absent-save-dnd-0','brightness-absent-save-dnd-1','brightness-absent-save-dnd-2','brightness-absent-save-dnd-3','brightness-absent-save-dnd-absent','brightness-absent-save','brightness-absent-refuse','brightness-absent-null','brightness-refuse','brightness-null-advertised','civil-lost-retained','companion-lost-retained','boot-read-lost-retained','view-read-lost-retained','alarm-status-lost-retained','alarm-step-lost-retained','alarm-refresh-lost-retained','alarm-ack-lost-retained','alarm-status-retained','alarm-status-unknown','alarm-step-retained','alarm-step-unknown','alarm-refresh-retained','alarm-refresh-unknown','alarm-ack-retained','alarm-ack-unknown','output-0','output-1','output-2','output-3','contexts-query','schema-native','schema-unspecified','schema-catalog','schema-malformed','fraction','crash-navigation','crash-retained','provision','provision-volume','boot-acquire-retained','policy-acquire-retained','view-acquire-retained',
  'boot-absent','policy-absent','view-absent','boot-null','policy-null','view-null',
  'boot-null-release-retained','policy-null-release-retained','view-null-release-retained')]
 for mode,encoding,rotation in cases:
  command=[str(binary),mode,str(encoding),str(rotation)]
  try:
   result=subprocess.run(command,capture_output=True,text=True,timeout=90,
    env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
  except subprocess.TimeoutExpired as exc:
   def decoded(value):return value.decode(errors='replace') if isinstance(value,bytes) else value or ''
   result=subprocess.CompletedProcess(command,124,decoded(exc.stdout),decoded(exc.stderr)+'\nCase timed out after 90 seconds\n')
  log=f'{mode}-{encoding}-{rotation}.log';(a.output/log).write_text(result.stdout+result.stderr)
  peaks=re.findall(r'Home actual acquisition boundary peak=(\d+) overflows=(\d+) live=(\d+)',result.stderr)
  runs.append(dict(mode=mode,physical_encoding=encoding,rotation=rotation,exit_code=result.returncode,log=log,app_grant_peak=int(peaks[-1][0]) if peaks else None))
  print(result.stdout+result.stderr,end='',flush=True);(a.output/'runs.json').write_text(json.dumps(runs,indent=2)+'\n')
 compiled={str(f.relative_to(w)):digest(f) for folder in (sdk,staged) for f in folder.rglob('*') if f.is_file()}
 dependency_files=set()
 for dep in w.glob('*.d'):
  for name in dep.read_text().replace(chr(92)+chr(10),' ').split(':',1)[1].split():
   path=Path(name);assert path.is_file(),name;dependency_files.add(path)
 compiled_dependencies={str(path):digest(path) for path in sorted(dependency_files)}
sources=[fixture_source,quick_source,ROOT/'tests/home_canvas_crown_controls.inc',ROOT/'tests/home_combined_crown_prefix.inc',ROOT/'tests/home_combined_crown_services.inc',Path(__file__).resolve(),
 ROOT/'scripts/build.py',ROOT/'scripts/merged_sdk.py',ROOT/'scripts/home_alarm_fixture.py',ROOT/'dependencies.lock.json']
sources+=list((ROOT/'apps/clock').rglob('*.h'))+list((ROOT/'apps/clock').glob('*.c'))+list((ROOT/'apps/clock').glob('*.inc'))+list((ROOT/'include').glob('*.h'))
provider={str(f.relative_to(a.canvas_source)):digest(f) for f in (a.canvas_source/'Services/canvas_host').glob('*.h')}
(a.output/'receipt.json').write_text(json.dumps(dict(schema=1,width=a.width,height=a.height,sanitized=a.sanitized,
 production_flags=True,canvas_source_commit=actual,hardware_tested=False,fixture_edits=changes,
 generated_fixture_sha256=hashlib.sha256(text.encode()).hexdigest(),generated_quick_sha256=hashlib.sha256(quick_text.encode()).hexdigest(),commands=commands,runs=runs,
 source_sha256={str(v.relative_to(ROOT)):digest(v) for v in sources},
 staged_source_header_sha256=compiled,compiled_dependency_sha256=compiled_dependencies,canvas_provider_source_sha256=provider),indent=2)+'\n')

failures=[case for case in runs if case['exit_code']]
if failures:
 print(json.dumps({'failed_cases':failures},indent=2),flush=True)
 raise SystemExit(1)
