"""One-time source migration from 8327c31; ordinary builds use committed results."""
from pathlib import Path
import json
r=Path(__file__).resolve().parents[1]
if json.loads((r/'apps/clock/default.json').read_text())['version']=='0.14.1':raise SystemExit(0)
p=r/'apps/clock/crown.c';s=p.read_text()
s=s.replace('static bool clock_alarm_modal,clock_display_settled;','static bool clock_alarm_modal,clock_display_settled;\nstatic bool clock_alarm_attempted;')
s=s.replace('clock_alarm=(portable_alarm_client){0};clock_display_settled=true;','clock_alarm=(portable_alarm_client){0};clock_display_settled=true;clock_alarm_attempted=false;')
s=s.replace('if(!portable_alarm_open(&clock_alarm,rt) || !launcher_touch_open(&touch)', 'if(!clock_alarm_open_optional() || !launcher_touch_open(&touch)')
s=s.replace('if(!clock_alarm.api) {\n            /* First Clock','if(!clock_alarm_attempted) {\n            /* First Clock')
s=s.replace('if(!portable_alarm_open(&clock_alarm,rt))break;','if(!clock_alarm_open_optional())break;')
p.write_text(s)
p=r/'apps/clock/clock_alarm.inc';s=p.read_text();old='static portable_alarm_client clock_alarm;'
new='''static portable_alarm_client clock_alarm;
/* An offline bring-up profile can omit alarm.service. Do not turn the missing
 * optional function into a default-app exit or retry it on every frame. An
 * acquired malformed interface is still an error and retains normal cleanup. */
static bool clock_alarm_open_optional(void) {
    clock_alarm_attempted=true;
    if(portable_alarm_open(&clock_alarm,rt))return true;
    if(clock_alarm.grant.api)return false;
    rt->diagnostic("WATCH_CLOCK alarm-service=unavailable alarms=disabled");
    return true;
}'''
assert old in s;s=s.replace(old,new)
s=s.replace('static bool clock_alarm_foreground(void) {','static bool clock_alarm_foreground(void) {\n    if(!clock_alarm.api)return true;')
p.write_text(s)
p=r/'apps/runtime_features/clock_runtime.h';s=p.read_text();old=' if(!rtc->read(rtc->context,&date))return false;';new=''' if(!rtc->read(rtc->context,&date)) {
  /* Fresh battery-backed RTCs can have an oscillator-stop flag or invalid
   * calendar. Keep TIME UNSET explicit; never invent compile time or gate the
   * first display frame on a clock the user has not set yet. */
  if(sample.validity!=RISC_REALTIME_UNSET)return false;
  c->runtime->diagnostic("WATCH_CLOCK rtc=unreadable-or-unset time=unset");
  return true;
 }'''
assert old in s;s=s.replace(old,new);p.write_text(s)
for name in ['default.json','clock.json']:
 p=r/'apps/clock'/name;m=json.loads(p.read_text());m['version']='0.14.1';p.write_text(json.dumps(m,indent=2)+'\n')
p=r/'tests/fast_wake_clock_test.c';s=p.read_text()
s=s.replace('static bool ended,early_launch,first_seen,fail_display,fail_release;','static bool ended,early_launch,first_seen,fail_display,fail_release;\nstatic bool unreadable_rtc;')
s=s.replace('static bool fast_rtc(void*c,twatch_rtc_time_v1*out){','static bool fast_rtc(void*c,twatch_rtc_time_v1*out){if(unreadable_rtc)return false;')
s=s.replace('ended=early_launch=first_seen=fail_display=fail_release=fail_context_pause=confirmed_early=unset_time=false;','ended=early_launch=first_seen=fail_display=fail_release=fail_context_pause=confirmed_early=unset_time=false;unreadable_rtc=false;')
s=s.replace('"display.output",ALARM_SERVICE_CAPABILITY,"input.touch.raw"','"display.output","input.touch.raw"')
s=s.replace('"runtime-grants","display","alarm","touch"','"runtime-grants","display","touch"')
needle=' reset_case();fail_capability="board.battery";'
insert=''' reset_case();fail_capability=ALARM_SERVICE_CAPABILITY;app_main();check_clean();
 assert(first_seen&&confirmations==1&&stage_count("alarm-begin")==1&&stage_count("alarm-end")==1&&!alarm_frames);
 reset_case();unreadable_rtc=true;native_time.validity=RISC_REALTIME_UNSET;native_time.epoch_seconds=0;
 app_main();check_clean();assert(first_seen&&!face.time_valid&&!seed_calls&&confirmations==1);
 reset_case();unreadable_rtc=true;app_main();check_clean();assert(!first_seen&&!confirmations);
'''
assert needle in s;s=s.replace(needle,insert+needle);p.write_text(s)
