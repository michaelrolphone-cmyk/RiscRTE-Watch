#!/usr/bin/env python3
"""Qualify actual Bringup raster/input/lifecycle in normal and ASan/UBSan modes.

Provider doubles model time and hardware. This is source/host qualification,
not a physical-device, native scheduler, or target ELF execution claim.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--detect-leaks', action='store_true', help='Enable optional LSan; unavailable in ptrace-based executors.')
    parser.add_argument('--mode', choices=('normal', 'sanitized', 'both'), default='both')
    args = parser.parse_args()
    if args.output:
        args.output.mkdir(parents=True, exist_ok=True)
        (args.output / 'rasters').mkdir(exist_ok=True)
    runs = []
    with tempfile.TemporaryDirectory(prefix='bringup-native-') as temporary:
        work = Path(temporary)
        for mode in (('normal', 'sanitized') if args.mode == 'both' else (args.mode,)):
            binary = work / mode
            flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                     '-fno-omit-frame-pointer', '-no-pie'] if mode == 'sanitized' else []
            command = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g',
                       '-Wall', '-Wextra', '-Werror', '-Wno-misleading-indentation',
                       *flags, *['-I' + str(ROOT / p) for p in ('include', 'sdk/app', 'sdk/driver')],
                       str(ROOT / 'tests/bringup_test.c'), str(ROOT / 'apps/clock/viewport.c'),
                       '-o', str(binary)]
            subprocess.run(command, check=True)
            result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=120,
                                    env={**os.environ, 'ASAN_OPTIONS': f'detect_leaks={int(args.detect_leaks)}:halt_on_error=1',
                                         'UBSAN_OPTIONS': 'halt_on_error=1:print_stacktrace=1',
                                         'BRINGUP_RASTER_DIR': str((args.output / 'rasters').resolve()) if args.output else ''})
            print(result.stdout + result.stderr, end='', flush=True)
            runs.append({'mode': mode, 'command': command, 'returncode': result.returncode,
                         'stdout': result.stdout, 'stderr': result.stderr,
                         'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest()})
            if args.output:
                (args.output / (mode + '.log')).write_text(result.stdout + result.stderr)
            result.check_returncode()
    paths = [Path(__file__).resolve(), ROOT / 'tests/bringup_test.c',
             ROOT / 'apps/bringup/main.c', ROOT / 'apps/bringup/native_ui.h',
             ROOT / 'apps/clock/viewport.c', ROOT / 'include/WatchViewport.h',
             ROOT / 'include/WatchLayout.h', ROOT / 'include/PortableMotionTap.h', ROOT / 'include/twatch_caps.h',
             ROOT / 'include/twatch_calendar.h', *sorted((ROOT / 'sdk').rglob('*.h'))]
    receipt = {'hardware_tested': False, 'leak_detection': args.detect_leaks,
               'scope': 'Actual production Bringup renderer and app_main; independent modeled display, touch, optional providers and runtime.',
               'dimensions': [[240, 240], [410, 502], [128, 128], [502, 410], [320, 172]],
               'runs': runs, 'source_sha256': {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}}
    if args.output:
        (args.output / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')


if __name__ == '__main__':
    main()
