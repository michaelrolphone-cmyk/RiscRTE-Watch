#!/usr/bin/env python3
"""Qualify one Home controller binary with synchronous and atomic providers."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
from merged_sdk import headers

ROOT = Path(__file__).resolve().parents[1]
FAULTS = 'missing bad-api bad-grant bad-flags state-io read-io submit-io collect-io bad-receipt dirty-missing'.split()

def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('system', 'utilities', 'output'):
        p.add_argument('--'+name, type=Path, required=True)
    p.add_argument('--width', type=int)
    p.add_argument('--height', type=int)
    p.add_argument('--normal-only', action='store_true')
    p.add_argument('--native-quick-actions', action='store_true')
    args = p.parse_args()
    assert (args.width is None) == (args.height is None)
    results = []
    compilations = []
    args.output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='generic-home-preferences-') as temporary:
        work = Path(temporary)
        sdk = headers(work, args.system, args.utilities, ROOT)
        sources = [ROOT/'tests/generic_preferences_test.c', *[ROOT/'apps/clock'/n for n in
            ('nova/nova.c', 'viewport.c', 'capability_clients.c', 'quick_radios.c')],
            *[args.system/'lib/PortableApps/src'/n for n in ('quick_actions.c', 'quick_render.c', 'quick_session.c')]]
        for width, height in ([(args.width, args.height)] if args.width else [(240, 240), (410, 502)]):
            for sanitized in ([False] if args.normal_only else [False, True]):
                flags = ['-O1', '-g', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function',
                    '-Wno-misleading-indentation', '-DWATCH_COMPACT_NOVA_ASSETS',
                    f'-DTEST_WIDTH={width}u', f'-DTEST_HEIGHT={height}u',
                    '-DWATCH_COMPRESSED_CALENDAR_ALPHA', '-DWATCH_COMPRESSED_FACE_ALPHA',
                    '-DWATCH_SHARED_PREFERENCE_HEADERS', '-DWATCH_ATOMIC_PREFERENCES', '-DPORTABLE_WATCH_NATIVE_HEIGHT',
                    '-DPORTABLE_PREFERENCE_STORE', '-DPORTABLE_QUICK_ATOMIC_PREFERENCES',
                    '-DWATCH_QUICK_RADIOS', '-DPORTABLE_QUICK_RADIOS',
                    *['-I'+str(x) for x in (sdk, args.system/'lib/PortableApps/src', ROOT/'include', ROOT)]]
                if args.native_quick_actions:
                    flags += ['-DPORTABLE_QUICK_CAPABILITY_SAFE', '-DPORTABLE_QUICK_SAFE_CONTENT']
                if sanitized:
                    flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie']
                objects = []
                commands = []
                def run(command):
                    commands.append(list(map(str, command)))
                    subprocess.run(command, check=True)
                for index, source in enumerate(sources):
                    obj = work/f'{index}.o'
                    run(['cc', '-std=c11', *flags, '-c', str(source), '-o', str(obj)])
                    objects.append(str(obj))
                binary = work/'generic-preferences'
                run(['c++', '-std=c++11', *flags, str(ROOT/'apps/clock/effects/boot.cpp'), *objects, '-o', str(binary)])
                binary_sha = hashlib.sha256(binary.read_bytes()).hexdigest()
                compilations.append({'dimensions':[width,height], 'sanitized':sanitized, 'binary_sha256':binary_sha, 'commands':commands})
                for provider_flags in (4, 15):
                    cases = ['quick', 'face', *[f'{controller}-{fault}' for controller in ('face', 'quick') for fault in FAULTS]]
                    if provider_flags == 4:
                        cases.append('partial')
                    for case in cases:
                        result = subprocess.run([str(binary), str(provider_flags), case], check=False, timeout=30,
                            capture_output=True, text=True, env={**os.environ, 'ASAN_OPTIONS':'detect_leaks=0', 'UBSAN_OPTIONS':'halt_on_error=1'})
                        print(result.stdout.strip(), flush=True)
                        if result.returncode:
                            print(result.stderr, flush=True)
                            result.check_returncode()
                        results.append({'dimensions':[width,height], 'sanitized':sanitized, 'provider_flags':provider_flags, 'case':case,
                            'same_binary_sha256':binary_sha, 'stdout':result.stdout.strip()})
        inputs = [*sources, ROOT/'tests/quick_clock_test.c', Path(__file__).resolve(),
            *sdk.glob('*.h'), *[args.system/'lib/PortableApps/src'/n for n in ('quick_atomic.inc', 'home_automatic.inc')],
            ROOT/'apps/clock/crown.c', ROOT/'apps/clock/clock_atomic.inc']
        receipt = {'native_quick_actions':args.native_quick_actions, 'hardware_tested':False, 'scope':'Real Home and Quick controllers with provider doubles; same compiled binary receives flags4/15 at runtime',
            'compilations':compilations, 'runs':results, 'source_sha256':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in inputs}}
        (args.output/'receipt.json').write_text(json.dumps(receipt, indent=2)+'\n')
if __name__ == '__main__':
    main()
