"""One-time, readable migration of the pinned Home sources to generic services.
The resulting source files are committed. Ordinary builds never run this script.
"""
from pathlib import Path
import json
S=Path(__file__).resolve().parents[1]
marker=S/'provenance/generic-power.json'
if marker.exists():raise SystemExit(0)
for name in ['watch_sleep.h','watch_alarm_sleep.h','watch_motion_client.h']:
 p=S/'apps/clock'/name;s=p.read_text()
 s=s.replace('#include "twatch_power.h"','#include "RiscDisplayPowerV1.h"\n#include "RiscPowerSleepV1.h"')
 s=s.replace('twatch_panel_power_v1','risc_display_power_api_v1').replace('twatch_pmu_api_v1','risc_power_sleep_api_v1')
 s=s.replace('panel->base.','panel->').replace('pmu->base.','pmu->')
 s=s.replace('TWATCH_PANEL_','RISC_DISPLAY_POWER_').replace('TWATCH_PMU_','RISC_POWER_')
 s=s.replace('    uint32_t ignored=0;\n    if(!pmu->key_events(pmu->context,&ignored))return WATCH_SLEEP_FAILED;\n','')
 p.write_text(s)
p=S/'apps/clock/crown.c';s=p.read_text().replace('#include "twatch_power.h"','#include "WatchCapabilityClients.h"')
s=s.replace('static const twatch_pmu_api_v1 *pmu;','static watch_capability_clients device_clients;\nstatic const risc_battery_gauge_api_v1 *battery_api;\nstatic const risc_power_sleep_api_v1 *pmu;')
s=s.replace('static const twatch_panel_power_v1 *panel;','static const risc_display_power_api_v1 *panel;')
s=s.replace('pmu->base.read','battery_api->read').replace('pmu->base.context','battery_api->context')
s=s.replace('face.battery_valid=battery_api->read','face.battery_valid=battery_api && battery_api->read').replace('if(!battery_api->read','if(!battery_api || !battery_api->read')
s=s.replace('pmu->key_events(battery_api->context,','watch_capabilities_key_events(&device_clients,')
s=s.replace('held=0;rtc=NULL;display=NULL;pmu=NULL;panel=NULL;','held=0;rtc=NULL;display=NULL;pmu=NULL;panel=NULL;battery_api=NULL;\n    device_clients=(watch_capability_clients){0};')
s=s.replace(',pg={.struct_size=sizeof(pg)}','').replace('bool have_d=false,have_r=false,have_p=false;','bool have_d=false,have_r=false;')
s=s.replace('display->struct_size<TWATCH_PANEL_LIGHT_SLEEP_SIZE','display->struct_size<sizeof(*display)')
s=s.replace('    panel=dg.api;\n    if (!panel->prepare_sleep || !panel->resume) goto done;\n','')
a=s.index('    have_p=rt->acquire("board.battery"');b=s.index('    have_r=rt->acquire("rtc.clock"',a)
s=s[:a]+'    if(!watch_capabilities_open(&device_clients,rt))goto done;\n    battery_api=device_clients.battery;pmu=device_clients.sleep;panel=device_clients.display_power;\n'+s[b:]
s=s.replace('static int sleep_cycle(void) {','static int sleep_cycle(void) {\n    if(!watch_capabilities_can_sleep(&device_clients))return WATCH_SLEEP_REFUSED;')
s=s.replace('    if(rc<0)return false;\n','    if(rc<0)return false;\n    uint32_t cleared=0;\n    if(!watch_capabilities_key_events(&device_clients,&cleared))return false;\n',1)
s=s.replace('        if (manual || idle) {','        if ((manual || idle) && watch_capabilities_can_sleep(&device_clients)) {')
s=s.replace('    if (have_p) rt->release(&pg);','    if(!watch_capabilities_close(&device_clients)) {\n#ifdef WATCH_RUNTIME_FEATURES\n        watch_runtime_retain(rt);\n#endif\n        return;\n    }')
p.write_text(s)
p=S/'apps/clock/clock_alarm.inc';p.write_text(p.read_text().replace('pmu->key_events(pmu->base.context,','watch_capabilities_key_events(&device_clients,'))
p=S/'apps/clock/clock_contexts.inc';p.write_text(p.read_text().replace('if(pmu&&pmu->base.read&&pmu->base.read(pmu->base.context,','if(battery_api&&battery_api->read&&battery_api->read(battery_api->context,'))
p=S/'apps/clock/watch_motion_client.h';s=p.read_text().replace('#include "PortableTapSettings.h"','#include "WatchTapPolicy.h"')
old='    if(!motion->tap_info(motion->context,&info) || info.struct_size<sizeof(info) ||\n       (info.profile!=TWATCH_TAP_BMA423 && info.profile!=TWATCH_TAP_BMA456H) ||\n       !motion->tap_configure(motion->context,portable_tap_value(&preference,info.profile)))'
new='    uint16_t value=0;\n    if(!motion->tap_info(motion->context,&info) || !watch_tap_select(&preference,&info,&value) ||\n       !motion->tap_configure(motion->context,value))'
assert old in s;s=s.replace(old,new);p.write_text(s)
p=S/'scripts/build.py';p.write_text(p.read_text().replace("ROOT/'apps/clock/viewport.c',","ROOT/'apps/clock/viewport.c',ROOT/'apps/clock/capability_clients.c',"))
for name in ['default.json','clock.json']:
 p=S/'apps/clock'/name;m=json.loads(p.read_text());m['version']='0.13.0'
 for cap in ['power.sleep','input.power-key','display.power']:
  if not any(e['capability']==cap for e in m['requires']):m['requires'].append({'capability':cap,'api':1})
 p.write_text(json.dumps(m,indent=2)+'\n')
p=S/'include/WatchCapabilities.h';s=p.read_text().replace('typedef twatch_pmu_api_v1 risc_watch_power_v1;','').replace('typedef twatch_panel_power_v1 risc_watch_display_power_v1;','').replace('#include "twatch_power.h"','#include "RiscPowerSleepV1.h"\n#include "RiscPowerKeyV1.h"\n#include "RiscDisplayPowerV1.h"');p.write_text(s)
p=S/'.github/workflows/universal-watch.yml';s=p.read_text().replace('contents: write','contents: read').replace('ref: ${{ github.head_ref || github.ref_name }}','ref: ${{ github.event.pull_request.head.sha || github.sha }}')
a=s.index('      - name: Complete extraction migration');b=s.index('      - name: Test both display classes',a);s=s[:a]+s[b:]
s=s.replace('          python3 scripts/test_viewport.py\n','          python3 scripts/check_capability_boundaries.py\n          python3 scripts/test_capabilities.py\n          python3 scripts/test_capabilities.py --sanitize\n          python3 scripts/test_viewport.py\n');p.write_text(s)
marker.write_text(json.dumps({'schema':1,'version':'0.13.0','capabilities':['board.battery','input.power-key','power.sleep','display.output','display.power'],'legacy_suffixes_in_home':False},indent=2)+'\n')
print('Independent generic services connected to Home')
