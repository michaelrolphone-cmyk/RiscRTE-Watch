#!/usr/bin/env python3
"""Test generic Home fallback policy and real pinned Quick loads, normal/sanitized."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
from merged_sdk import headers

ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('system','utilities'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args()
    subprocess.run(['python3',str(ROOT/'tests/test_sleep_defaults.py')],check=True)
    with tempfile.TemporaryDirectory(prefix='watch-sleep-defaults-') as temporary:
        out=Path(temporary);sdk=headers(out,a.system,a.utilities,ROOT)
        includes=['-I'+str(path) for path in (ROOT,sdk,a.system/'lib/PortableApps/src')]
        for sanitized in (False,True):
            for mode,seconds in ((None,None),('DEEP',15),('LIGHT',5),('HYBRID',3600)):
                for atomic in (False,True):
                    flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',
                           '-DWATCH_SHARED_PREFERENCE_HEADERS','-DPORTABLE_LOW_BATTERY',*includes]
                    if mode:flags += ['-DWATCH_DEFAULT_SLEEP_MODE=PORTABLE_SLEEP_'+mode,f'-DWATCH_DEFAULT_IDLE_SECONDS={seconds}u']
                    if atomic:flags += ['-DPORTABLE_QUICK_ATOMIC_PREFERENCES']
                    if sanitized:flags += ['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
                    binary=out/'sleep-defaults'
                    subprocess.run([os.environ.get('CC','cc'),*flags,str(ROOT/'tests/sleep_defaults_test.c'),
                        str(ROOT/'apps/clock/watch_quick_session.c'),str(a.system/'lib/PortableApps/src/quick_actions.c'),'-o',str(binary)],check=True)
                    subprocess.run([str(binary)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
            # Reject bypasses of the CLI constraints at C compilation too.
            for define in ('WATCH_DEFAULT_SLEEP_MODE=3','WATCH_DEFAULT_IDLE_SECONDS=4','WATCH_DEFAULT_IDLE_SECONDS=3601'):
                result=subprocess.run([os.environ.get('CC','cc'),'-std=c11',*includes,'-DWATCH_SHARED_PREFERENCE_HEADERS','-D'+define,
                    '-x','c','-fsyntax-only','-'],input='#include "apps/clock/WatchSleepDefaults.h"\n',text=True,capture_output=True)
                assert result.returncode and '#error' in result.stderr, result.stderr
        print('Normal and ASan/UBSan: 16 real Quick/policy configurations and compile-time bounds PASS')

if __name__ == '__main__':
    main()
