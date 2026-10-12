"""One-time source migration; CI commits the result, ordinary builds do not run it."""
from pathlib import Path
import json,re,shutil,subprocess
R=Path(__file__).resolve().parents[1]
W=R/'.source/current-watch';B=R/'.source/original-watch'
marker=R/'provenance/fast-wake.json'
if marker.exists():raise SystemExit(0)
# Preserve the generic power clients while merging all current physical-Watch
# wake changes against the original extraction ancestor.
for name in ['crown.c','clock_alarm.inc','watch_motion_client.h']:
 p=R/'apps/clock'/name
 result=subprocess.run(['git','merge-file','-p',str(p),str(B/'apps/clock'/name),str(W/'apps/clock'/name)],capture_output=True,text=True)
 if result.returncode not in (0,1,2,3):raise RuntimeError(result.stderr)
 p.write_text(result.stdout)
p=R/'apps/clock/crown.c';s=p.read_text()
blocks=list(re.finditer(r'<<<<<<<[^\n]*\n(.*?)=======\n(.*?)>>>>>>>[^\n]*\n',s,re.S));assert len(blocks)==3
replacements=[]
for i,m in enumerate(blocks):
 a,b=m.group(1),m.group(2)
 if i==0:new=a+b
 elif i==1:new=b[:b.index('    risc_runtime_capability_v1')]+a
 else:new='    CLOCK_WAKE_MARK("display-end");\n    CLOCK_WAKE_MARK("power-services-begin");\n'+a+'    CLOCK_WAKE_MARK("power-services-end");\n    CLOCK_WAKE_MARK("rtc-begin");\n'+b[b.index('#if defined(WATCH_FAST_WAKE)'):]
 replacements.append((m.span(),new))
for (a,b),new in reversed(replacements):s=s[:a]+new+s[b:]
s=s.replace('if(!clock_broadcast.api && !portable_broadcast_open(&clock_broadcast,rt))return false;','if(!clock_broadcast.api && !portable_broadcast_open(&clock_broadcast,rt) && clock_broadcast.grant.api)return false;')
s=s.replace('if(!clock_contexts.api && (!portable_contexts_open(&clock_contexts,rt) ||\n           !clock_contexts_pause() || !clock_contexts_recover_export()))return false;','if(!clock_contexts.api && !portable_contexts_open(&clock_contexts,rt) && clock_contexts.grant.api)return false;\n        if(!clock_contexts_pause() || !clock_contexts_recover_export())return false;')
p.write_text(s)
p=R/'apps/clock/watch_motion_client.h';s=p.read_text();s=re.sub(r'<<<<<<<[^\n]*\n(.*?)=======\n(.*?)>>>>>>>[^\n]*\n',lambda m:m.group(1),s,flags=re.S)
s=s.replace('watch_sleep_stage=7;watch_sleep_detail=0;','watch_sleep_stage=11;watch_sleep_detail=0;').replace('    if(!rt->acquire(TWATCH_MOTION_CAPABILITY,1,0,&grant))','    watch_sleep_stage=10;\n    if(!rt->acquire(TWATCH_MOTION_CAPABILITY,1,0,&grant))');p.write_text(s)
p=R/'apps/clock/clock_alarm.inc';p.write_text(p.read_text().replace('pmu->key_events(pmu->base.context,','watch_capabilities_key_events(&device_clients,'))
p=R/'scripts/build.py';p.write_text(p.read_text().replace("FLAGS=['-DPORTABLE_LOW_BATTERY'","FLAGS=['-DWATCH_FAST_WAKE','-DPORTABLE_LOW_BATTERY'"))
for name in ['default','clock']:
 p=R/'apps/clock'/(name+'.json');v=json.loads(p.read_text());v['version']='0.14.0';p.write_text(json.dumps(v,indent=2)+'\n')
for name in ['quick_clock_test.c','fast_wake_clock_test.c']:shutil.copy2(W/'tests'/name,R/'tests'/name)
p=R/'tests/quick_clock_test.c';t=p.read_text().replace('#include "apps/clock/crown.c"','#include "twatch_power.h" /* Legacy test fixture only; Home uses independent APIs. */\n#include "apps/clock/crown.c"')
t=t.replace('.width=240,.height=240};return true;}','.width=240,.height=240,.supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565)};return true;}',1).replace('assert(id==15)','assert(id==0)').replace('assert(id==16)','assert(id==0)')
pos=t.index('static bool acquire_cap(');t=t[:pos]+'static risc_power_sleep_api_v1 test_power;\nstatic risc_power_key_api_v1 test_key;\nstatic risc_display_power_api_v1 test_display_power;\n'+t[pos:]
t=t.replace('static bool acquire_cap(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){(void)v;','''static bool acquire_cap(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){(void)v;
if(!strcmp(n,"power.sleep")){test_power=(risc_power_sleep_api_v1){1,sizeof(test_power),pa.base.context,pa.prepare_sleep,pa.resume,pa.light_sleep,pa.deep_sleep,pa.light_sleep_for,pa.sleep_wake_pending,pa.deep_sleep_for,pa.light_sleep_set,pa.deep_sleep_set};g->api=&test_power;grants++;return true;}
if(!strcmp(n,"input.power-key")){test_key=(risc_power_key_api_v1){1,sizeof(test_key),pa.base.context,pa.key_events};g->api=&test_key;grants++;return true;}
if(!strcmp(n,"display.power")){test_display_power=(risc_display_power_api_v1){1,sizeof(test_display_power),da.base.context,da.prepare_sleep,da.resume,da.prepare_deep_sleep,da.resume_status};g->api=&test_display_power;grants++;return true;}
''')
t=t.replace('static const risc_runtime_api_v1 runtime={1,sizeof(runtime),health,yield,diagnostic,launch,acquire_cap,release_cap};','static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.health=health,.yield_ms=yield,.diagnostic=diagnostic,.request_launch=launch,.acquire=acquire_cap,.release=release_cap};');p.write_text(t)
p=R/'tests/fast_wake_clock_test.c';t=p.read_text().replace('instance==7','instance==0').replace('instance=7','instance=0').replace('"battery-begin","battery-end"','"power-services-begin","power-services-end"')
t=t.replace('"display.output","board.battery",ALARM_SERVICE_CAPABILITY','"display.output",ALARM_SERVICE_CAPABILITY').replace('"runtime-grants","display","battery","alarm","touch"','"runtime-grants","display","alarm","touch"').replace('failure<=2','failure<=1')
t=t.replace('assert(first_seen&&service_acquires==fail&&!confirmations);','assert(first_seen&&service_acquires==4&&confirmations==1);')
t=t.replace('static const struct {risc_runtime_api_v1 prefix;bool(*confirm)(void);bool(*retain)(void);} fast_runtime={{1,sizeof(fast_runtime),fast_health,yield,fast_diagnostic,fast_launch,fast_acquire,fast_release},fast_confirm,fast_retain};','static const risc_runtime_api_v1 fast_runtime={.api_version=1,.struct_size=sizeof(fast_runtime),.health=fast_health,.yield_ms=yield,.diagnostic=fast_diagnostic,.request_launch=fast_launch,.acquire=fast_acquire,.release=fast_release,.confirm_boot=fast_confirm,.retain_invocation=fast_retain};').replace('return &fast_runtime.prefix;','return &fast_runtime;')
t=t.replace(' reset_case();fail_frame_acquire=true;',' reset_case();fail_capability="board.battery";app_main();check_clean();assert(first_seen&&!face.battery_valid&&confirmations==1);\n reset_case();fail_frame_acquire=true;',1)
t=t.replace('Both acquisition refusals','Settings acquisition refusal').replace('both acquire boundaries','settings acquisition');p.write_text(t)
p=R/'scripts/test_fast_wake_clock.py';t=(W/'scripts/test_fast_wake_clock.py').read_text();t=t.replace("root/'apps/clock/points_projection.c',","root/'apps/clock/points_projection.c',root/'apps/clock/viewport.c',root/'apps/clock/capability_clients.c',root/'apps/clock/quick_radios.c',").replace("'quick_session.c','quick_radios.c'","'quick_session.c'");p.write_text(t)
for name in ['crown.c','clock_alarm.inc','watch_motion_client.h']:assert '<<<<<<<' not in (R/'apps/clock'/name).read_text()
marker.write_text(json.dumps({'schema':1,'source_repository':'michaelrolphone-cmyk/RiscRTE-T-Watch-S3','source_commit':'fb01359770f457e410efe8cf7c7e0de5f3a43ebf','extraction_ancestor':'851b8998e804c57b2c9f42d9f8fd0a69e79f2b41','version':'0.14.0','generic_interfaces_preserved':True,'hardware_verified':False},indent=2)+'\n')
