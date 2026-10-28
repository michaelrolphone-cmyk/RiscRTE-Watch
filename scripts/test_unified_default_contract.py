#!/usr/bin/env python3
"""Assert a fixed app build and qualify generic-file and legacy crash sessions."""
import argparse
import ast
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
from merged_sdk import headers

ROOT=Path(__file__).resolve().parents[1]
def contract():
    tree=ast.parse((ROOT/'scripts/build.py').read_text())
    arguments=[]
    defines=[]
    for node in ast.walk(tree):
        if isinstance(node,ast.Call) and isinstance(node.func,ast.Attribute) and node.func.attr=='add_argument':
            arguments.extend(x.value for x in node.args if isinstance(x,ast.Constant) and isinstance(x.value,str))
        if isinstance(node,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='FLAGS' for t in node.targets):
            defines=[n.value for n in ast.walk(node.value) if isinstance(n,ast.Constant) and isinstance(n.value,str)]
    assert set(arguments)-{'--'}=={'--output','--cc','--role','--automatic-provision','--serial-transfer-mode',
        '--time-authority','--unset-time-local','--native-alarm-v2','--clock-edit-authority',
        '--native-quick-actions','--native-performance-runtime','--interaction-trace'}, arguments
    options={node.args[0].value:node for node in ast.walk(tree)
             if isinstance(node,ast.Call) and isinstance(node.func,ast.Attribute) and node.func.attr=='add_argument'
             and node.args and isinstance(node.args[0],ast.Constant)}
    assert {kw.arg:ast.literal_eval(kw.value) for kw in options['--time-authority'].keywords if kw.arg in ('choices','default')} == {'choices':['external-rtc','native'],'default':'external-rtc'}
    for option in ('--native-alarm-v2','--clock-edit-authority','--native-quick-actions','--interaction-trace'):
        assert any(kw.arg=='action' and ast.literal_eval(kw.value)=='store_true' for kw in options[option].keywords)
        assert not any(kw.arg=='default' and ast.literal_eval(kw.value) for kw in options[option].keywords)
    assert not any(value in defines for value in ('-DWATCH_NATIVE_TIME_AUTHORITY','-DWATCH_NATIVE_ALARM_V2','-DALARM_SERVICE_CLOCK_EDIT_V2','-DPORTABLE_QUICK_CAPABILITY_SAFE','-DWATCH_PERFORMANCE_TRACE','-DWATCH_INTERACTION_TRACE'))
    transfer=[node for node in ast.walk(tree) if isinstance(node,ast.If) and ast.unparse(node.test)=="role == 'default' and a.serial_transfer_mode"]
    assert len(transfer)==1 and ast.unparse(transfer[0].body[0])=="flags.append('-DWATCH_SERIAL_TRANSFER_MODE')"
    assert not any('WATCH_SERIAL_TRANSFER_MODE' in value for value in defines)
    assert '-DCRASH_REPORT_SESSION_FILES=1' in defines
    assert '-DPORTABLE_PREFERENCE_STORE' in defines
    assert '-DWATCH_ATOMIC_PREFERENCES' in defines
    assert '-DPORTABLE_QUICK_ATOMIC_PREFERENCES' in defines
    assert not any(word in value.lower() for value in defines for word in ('waveshare','twatch','410','502','240','sd_volume_instance'))
    provision=(ROOT/'apps/clock/clock_provision.inc').read_text()
    assert 'hooks.sd_enabled=true;hooks.volume_instance=0;' in provision
    assert 'WATCH_PROVISION_VOLUME' not in provision
    source=(ROOT/'apps/clock/clock_crash.inc').read_text()
    assert 'hooks.files_enabled=true;hooks.files_instance=0;' in source
    assert 'WATCH_CRASH_REPORT_SD_VOLUME_INSTANCE' not in source
    print('Unified default contract: fixed common preference route; no board-specific build switches; explicit Home-only transfer/native options; legacy time default retained; optional generic files instance0 PASS')

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('system','utilities','output'):p.add_argument('--'+name,type=Path,required=True)
    args=p.parse_args();args.output.mkdir(parents=True,exist_ok=True);contract();runs=[]
    with tempfile.TemporaryDirectory(prefix='unified-default-contract-') as temporary:
        out=Path(temporary);sdk=headers(out,args.system,args.utilities,ROOT)
        for scenario in ('legacy', 'optional-files', 'lifecycle-files'):
            generic=scenario!='legacy'
            for sanitized in (False,True):
                flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
                source=ROOT/'tests'/({'legacy':'crash_optional_volume_test.c','optional-files':'crash_optional_files_test.c','lifecycle-files':'crash_files_session_lifecycle_test.c'}[scenario])
                if generic:flags+=['-DCRASH_REPORT_SESSION_FILES=1']
                command=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-I'+str(sdk),'-I'+str(args.system/'test/native_apps'),str(source),'-o',str(out/'test')]
                dependencies=subprocess.check_output(command[:-2]+['-MM'],text=True)
                if generic:
                    assert not any(name in dependencies for name in ('CrashReportSd.h','RiscStorageVolume','RiscStorageExport')), dependencies
                subprocess.run(command,check=True)
                result=subprocess.run([str(out/'test')],check=True,timeout=30,capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
                print(result.stdout,flush=True);runs.append({'scenario':scenario,'generic_files':generic,'sanitized':sanitized,'command':command,'dependencies':dependencies,'stdout':result.stdout})
        files=[ROOT/'scripts/build.py',ROOT/'apps/clock/clock_crash.inc',ROOT/'tests/crash_optional_volume_test.c',ROOT/'tests/crash_optional_files_test.c',ROOT/'tests/crash_files_session_lifecycle_test.c',args.system/'test/native_apps/crash_report_files_test.c',Path(__file__).resolve(),args.system/'test/native_apps/crash_report_spool_test.c',*sdk.glob('*.h')]
        (args.output/'receipt.json').write_text(json.dumps({'hardware_tested':False,'runs':runs,'sha256':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files}},indent=2)+'\n')
if __name__=='__main__':main()
