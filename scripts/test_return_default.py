#!/usr/bin/env python3
"""Execute the real Runtime handoff using the production clock return module."""
import argparse,os,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
r=Path(__file__).resolve().parents[1];n=a.runtime.resolve()
with tempfile.TemporaryDirectory(prefix='shared-watch-return-') as tmp:
 out=Path(tmp);san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'] if a.sanitize else []
 includes=['-I'+str(n/x) for x in ['src','sdk/app','sdk/driver','sdk/hardware','lib/ArduinoJson/src','test','test/drivers/stubs']]
 for name in ['default','child','clock']:
  src=r/('apps/clock/return_to_default.c' if name=='clock' else 'tests/return_default_fixture.c')
  defines=['-DRETURN_TEST_CHILD'] if name=='child' else []
  subprocess.run([os.getenv('CC','cc'),'-std=c11','-g','-Wall','-Wextra','-Werror','-fPIC','-fvisibility=hidden','-shared',*san,*defines,*includes,str(src),'-o',str(out/(name+'.elf'))],check=True)
 sources=[n/x for x in ['src/bootstrap/Json.cpp','src/bootstrap/Board.cpp','src/bootstrap/Runtime.cpp','src/runtime/streams/AppStreamSessions.cpp','src/runtime/streams/ProviderQueueHost.cpp','src/runtime/drivers/ProviderGraphV2.cpp','src/runtime/drivers/ProviderModuleV2.cpp']]
 subprocess.run([os.getenv('CXX','c++'),'-std=c++17','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers','-rdynamic','-g',*san,'-no-pie',*includes,*map(str,sources),str(r/'tests/return_default_runtime.cpp'),'-ldl','-o',str(out/'test')],check=True)
 subprocess.run([str(out/'test'),str(out)],check=True)
