#!/usr/bin/env python3
"""Check that capability identity and Home service bindings are hardware-neutral."""
import argparse,json,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
DEVICE=re.compile(r'(^|[._-])(x4|x4pro|xteink|twatch|t-watch|lilygo|waveshare|wavshare|esp32s3)([._-]|$)',re.I)
def capabilities(value):
    if isinstance(value,dict):
        if isinstance(value.get('capability'),str):yield value['capability']
        for child in value.values():yield from capabilities(child)
    elif isinstance(value,list):
        for child in value:yield from capabilities(child)
def check(roots):
    count=0
    for root in roots:
        for path in root.rglob('*.json'):
            if any(part in {'.git','.source','dist','provenance'} for part in path.relative_to(root).parts):continue
            try:value=json.loads(path.read_text())
            except (UnicodeError,json.JSONDecodeError):continue
            for cap in capabilities(value):
                if DEVICE.search(cap):raise ValueError(f'Device identity in capability contract: {path}: {cap}')
                count+=1
    for name in ['crown.c','capability_clients.c','watch_sleep.h','watch_alarm_sleep.h','watch_motion_client.h','clock_alarm.inc','clock_contexts.inc','clock_resident.inc']:
        path=ROOT/'apps/clock'/name;source=path.read_text()
        for private in ['twatch_pmu_api_v1','twatch_panel_power_v1','legacy_battery_power_v1','legacy_display_power_v1']:
            if private in source:raise ValueError(f'Private driver ABI leaked into Home: {name}: {private}')
        for cap,instance in re.findall(r'->acquire\("([^\"]+)",\s*\d+,\s*(\d+)',source):
            if DEVICE.search(cap):raise ValueError(f'Device capability lookup in {name}: {cap}')
            if int(instance) and cap!='storage.key-value':raise ValueError(f'Hard-coded hardware provider instance in {name}: {cap}/{instance}')
    assert DEVICE.search('x4.battery') and DEVICE.search('battery.waveshare') and not DEVICE.search('board.battery')
    print(f'Capability boundaries: {count} generic declarations; Home has no private battery/display suffix or fixed hardware instance')
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--provider-root',type=Path,action='append',default=[]);a=p.parse_args()
    check([ROOT,*a.provider_root])
