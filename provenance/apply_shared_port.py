"""One-time readable extraction migration. Final sources are committed by CI."""
from pathlib import Path
import json,shutil
R=Path(__file__).resolve().parents[1];W=R/'.source/twatch';S=R/'.source/system';U=R/'.source/utilities';N=R/'.source/runtime'
marker=R/'provenance/universal-migration.json'
if marker.exists():raise SystemExit(0)
def edit(path,old,new):
 p=R/path;t=p.read_text();assert old in t,(path,old[:80]);p.write_text(t.replace(old,new,1))
edit('apps/clock/crown.c','#include "RiscRuntimeV1.h"','#include "RiscRuntimeV1.h"\n#include "WatchViewport.h"\nstatic watch_viewport clock_viewport;')
edit('apps/clock/crown.c','    if (!panel->prepare_sleep || !panel->resume) goto done;','    if (!panel->prepare_sleep || !panel->resume) goto done;\n    if (!watch_viewport_open(&clock_viewport,display)) {\n        rt->diagnostic("WATCH_CLOCK error=display-viewport");goto done;\n    }\n    display=&clock_viewport.display;')
edit('apps/clock/crown.c','    if (have_d) rt->release(&dg);','    watch_viewport_close(&clock_viewport);\n    if (have_d) rt->release(&dg);')
edit('apps/clock/launcher_touch.h','    const risc_touch_api_v1 *api;','    const risc_touch_api_v1 *api;\n    watch_viewport_touch viewport;')
edit('apps/clock/launcher_touch.h','    t->subscription=t->api->subscribe(t->api->context);','    if (!watch_viewport_touch_open(&t->viewport,&clock_viewport,t->api)) return false;\n    t->api=&t->viewport.api;\n    t->subscription=t->api->subscribe(t->api->context);')
edit('apps/clock/watch_motion_client.h','TWATCH_MOTION_CAPABILITY,1,7,','TWATCH_MOTION_CAPABILITY,1,0,')
edit('apps/clock/watch_motion_client.h','if(!rt->acquire || !rt->release || !rt->acquire(TWATCH_MOTION_CAPABILITY,1,0,&grant))return WATCH_SLEEP_REFUSED;','if(!rt->acquire(TWATCH_MOTION_CAPABILITY,1,0,&grant))return alarms?\n        watch_alarm_sleep_prepared(panel,pmu,mode,alarms,rt->diagnostic):\n        watch_sleep_prepared(panel,pmu,mode,rt->diagnostic);')
edit('apps/clock/watch_motion_client.h','watch_sleep_stage=8;return rt->release(&grant)?WATCH_SLEEP_REFUSED:WATCH_SLEEP_RETAINED;','watch_sleep_stage=8;\n        if(!rt->release(&grant))return WATCH_SLEEP_RETAINED;\n        return alarms?watch_alarm_sleep_prepared(panel,pmu,mode,alarms,rt->diagnostic):\n            watch_sleep_prepared(panel,pmu,mode,rt->diagnostic);')
edit('apps/clock/watch_motion_client.h','if(!portable_motion_tap_valid(motion) || !motion->tap_info(motion->context,&info)','if(!portable_motion_tap_valid(motion)) {\n        if(!rt->release(&grant))return WATCH_SLEEP_RETAINED;\n        return alarms?watch_alarm_sleep_prepared(panel,pmu,mode,alarms,rt->diagnostic):\n            watch_sleep_prepared(panel,pmu,mode,rt->diagnostic);\n    }\n    if(!motion->tap_info(motion->context,&info)')
edit('apps/clock/crown.c','if(!portable_broadcast_open(&clock_broadcast,rt))goto done;','if(!portable_broadcast_open(&clock_broadcast,rt) && clock_broadcast.grant.api)goto done;')
edit('apps/clock/crown.c','if(!portable_contexts_open(&clock_contexts,rt)||!clock_contexts_pause())goto done;','if(!portable_contexts_open(&clock_contexts,rt) && clock_contexts.grant.api)goto done;\n    if(!clock_contexts_pause())goto done;')
for func in ('clock_contexts_recover_export','clock_contexts_tick'):
 edit('apps/clock/clock_contexts.inc',f'static bool {func}(void) {{',f'static bool {func}(void) {{\n    if(!clock_contexts.api)return true;')
t=(S/'lib/PortableApps/src/quick_radios.c').read_text().replace('"net.wifi",15','"net.wifi",0').replace('"bluetooth.hci",16','"bluetooth.hci",0')
t=t.replace('if(!bind(rt,&g,"bluetooth.hci",0))return false;','if(!bind(rt,&g,"bluetooth.hci",0))return true;')
(R/'apps/clock/quick_radios.c').write_text('/* Extracted from RiscRTE-System-Apps 14762601b3fa7bbeeac5ed565a6825534bf74dca.\n * Hardware providers resolve by capability; storage instance 1 is the existing\n * settings namespace, not a device identity. */\n'+t)
edit('include/twatch_caps.h','/* Board-local capabilities. Runtime treats the id as opaque. These are not\n * upstream RiscRTE capabilities until a later ABI promotion. */','/* Shared watch capability contracts. Historical twatch_* C names preserve the\n * deployed ABI; neither these interfaces nor app callers contain board pins. */')
edit('include/twatch_caps.h','/* frames are s16le. Mono is written to both MAX98357A slots. */','/* Frames are signed16 PCM; providers handle their physical slot mapping. */')
edit('sdk/driver/GardenPlatformV1.h','    bool (*release)(void *, uint64_t token);\n} garden_spi_v1;','    bool (*release)(void *, uint64_t token);\n    /* Append-only shared data pin mode; no change to legacy SPI prefix. */\n    bool (*claim_three_wire)(void *, uint8_t sclk, uint8_t mosi, uint8_t cs, uint64_t *token);\n} garden_spi_v1;\n#define GARDEN_SPI_THREE_WIRE_V1_SIZE (offsetof(garden_spi_v1, claim_three_wire) + sizeof(((garden_spi_v1*)0)->claim_three_wire))')
for src,dst in [(W/'scripts/compact_current_elf.py',R/'scripts/compact_current_elf.py'),(U/'scripts/normalize_xtensa_relocations.py',R/'scripts/normalize_xtensa_relocations.py'),(N/'sdk/driver/RiscSpiQuadV1.h',R/'sdk/driver/RiscSpiQuadV1.h'),(N/'sdk/hardware/RiscDisplaySpiV2.h',R/'sdk/driver/RiscDisplaySpiV2.h')]:shutil.copy2(src,dst)
for role,ident in [('default','twatch-clock'),('clock','twatch-clock-return')]:
 m=json.loads((W/'apps/clock/manifest.json').read_text());m.update(id=ident,version='0.12.0',file_name=role+'.elf')
 m['requires']=[{'capability':cap,'api':api} for cap,api in [('display.output',1),('board.battery',1),('rtc.clock',2),('input.touch.raw',1),('storage.key-value',1),('runtime.realtime-control',1),('runtime.retained-wake',1),('runtime.provider-promotion',1),('alarm.service',1),('motion.accel',1),('net.wifi',1),('bluetooth.hci',1),('telemetry.broadcast',1),('contexts.service',1)]]
 (R/'apps/clock'/(role+'.json')).write_text(json.dumps(m,indent=2)+'\n')
marker.write_text(json.dumps({'schema':1,'source':'851b8998e804c57b2c9f42d9f8fd0a69e79f2b41','entrypoint':'apps/clock/crown.c','display_layout':'uniform-fit240','physical_verification':False},indent=2)+'\n')
