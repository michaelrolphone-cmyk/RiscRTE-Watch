#!/usr/bin/env python3
"""Test optional Runtime sleep transaction and production Home custody."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
from merged_sdk import headers

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('system', 'utilities', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--normal-only', action='store_true')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    runs, compilations = [], []
    with tempfile.TemporaryDirectory(prefix='home-native-sleep-') as temporary:
        work = Path(temporary)
        sdk = headers(work, args.system, args.utilities, ROOT)
        sources = [ROOT / 'tests/runtime_sleep_test.c',
                   *[ROOT / 'apps/clock' / name for name in
                     ('nova/nova.c', 'viewport.c', 'capability_clients.c')],
                   *[args.system / 'lib/PortableApps/src' / name for name in
                     ('quick_actions.c', 'quick_render.c', 'quick_session.c')]]
        for width, height in ((240, 240), (410, 502)):
            for sanitized in ([False] if args.normal_only else (False, True)):
                flags = ['-O1', '-g', '-Wall', '-Wextra', '-Werror',
                         '-DWATCH_SHARED_PREFERENCE_HEADERS', '-DWATCH_COMPACT_NOVA_ASSETS',
                         '-DWATCH_COMPRESSED_CALENDAR_ALPHA', '-DWATCH_COMPRESSED_FACE_ALPHA',
                         '-DPORTABLE_WATCH_NATIVE_HEIGHT',
                         f'-DTEST_WIDTH={width}u', f'-DTEST_HEIGHT={height}u',
                         *['-I' + str(path) for path in (sdk, ROOT / 'include', ROOT)]]
                if sanitized:
                    flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                              '-fno-omit-frame-pointer', '-no-pie']
                objects, commands = [], []
                def run(command):
                    commands.append(command)
                    subprocess.run(command, check=True)
                for index, path in enumerate(sources):
                    obj = work / f'{index}.o'
                    run(['cc', '-std=c11', *flags, '-c', str(path), '-o', str(obj)])
                    objects.append(str(obj))
                binary = work / 'test'
                run(['c++', '-std=c++11', *flags, str(ROOT / 'apps/clock/effects/boot.cpp'),
                     *objects, '-o', str(binary)])
                compilations.append({'dimensions': [width, height], 'sanitized': sanitized,
                                     'commands': commands,
                                     'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest()})
                for case in ('helper', 'old', 'missing', 'ok', 'busy', 'unsupported', 'context', 'prepare-retained', 'resume-retained', 'resume-busy', 'resume-context', 'resume-unknown', 'removed-card', 'power-retained'):
                    result = subprocess.run([str(binary), case], capture_output=True, text=True,
                                            timeout=60, env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0',
                                                            'UBSAN_OPTIONS': 'halt_on_error=1'})
                    print(result.stdout, end='', flush=True)
                    if result.returncode:
                        print(result.stderr, flush=True)
                        result.check_returncode()
                    runs.append({'dimensions': [width, height], 'sanitized': sanitized,
                                 'case': case, 'stdout': result.stdout.strip()})
        inputs = [*sources, ROOT / 'tests/quick_clock_test.c', ROOT / 'apps/clock/crown.c',
                  ROOT / 'sdk/app/RiscRuntimeV1.h', ROOT / 'sdk/app/RiscRuntimeSleepV1.h', ROOT / 'apps/clock/WatchRuntimeSleep.h',
                  Path(__file__).resolve()]
        (args.output / 'receipt.json').write_text(json.dumps({
            'hardware_tested': False, 'leak_detection': False,
            'scope': 'Production Home sleep boundary, refusal redraw, wake cleanup ordering and retained custody',
            'compilations': compilations, 'runs': runs,
            'source_sha256': {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in inputs}
        }, indent=2) + '\n')


if __name__ == '__main__':
    main()
