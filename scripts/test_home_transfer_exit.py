#!/usr/bin/env python3
"""Exercise actual Home cooperative transfer exits with pinned host boundaries."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from merged_sdk import headers

ROOT = Path(__file__).resolve().parents[1]
BASE = '93c87d05d4da54df49240141bf9ca993bc30b462'
PINS = {'system': 'bb0cadf3cec3445a64e11513b47b2676f3fdd92d',
        'utilities': '4fdcad2495c33fe63f5f6faca9b11ced3c739a5b'}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--watch', type=Path, default=ROOT)
    for name in ('system', 'utilities', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--expect-baseline', action='store_true')
    parser.add_argument('--feature-off', action='store_true')
    parser.add_argument('--normal-only', action='store_true')
    parser.add_argument('--dimensions', choices=('both', '240x240', '410x502'), default='both')
    args = parser.parse_args()
    watch, system, utilities, output = (getattr(args, name).resolve()
                                        for name in ('watch', 'system', 'utilities', 'output'))
    output.mkdir(parents=True, exist_ok=True)
    for name, path in (('system', system), ('utilities', utilities)):
        assert subprocess.check_output(['git', '-C', str(path), 'rev-parse', 'HEAD'], text=True).strip() == PINS[name]
        assert not subprocess.check_output(['git', '-C', str(path), 'status', '--porcelain', '--untracked-files=no'], text=True).strip()
    if args.expect_baseline:
        original = subprocess.check_output(['git', '-C', str(ROOT), 'show', BASE + ':apps/clock/crown.c'])
        assert (watch / 'apps/clock/crown.c').read_bytes() == original
    report = {'status': 'running', 'hardware_tested': False, 'leak_detection': False,
              'scope': 'Production crown.c and renderer; Runtime admission, transport arrival and peripherals are modeled. No native Runtime parser, device or USB qualification.',
              'variant': 'baseline' if args.expect_baseline else 'feature-off' if args.feature_off else 'fixed',
              'watch_base': BASE, 'dependency_pins': PINS,
              'commands': [], 'runs': [], 'inputs': {}, 'frozen_inputs': {}}
    report_path = output / 'receipt.json'

    def save():
        report_path.write_text(json.dumps(report, indent=2) + '\n')

    def record(path):
        path = Path(path).resolve()
        key, sha = str(path), digest(path)
        if key in report['inputs']:
            assert report['inputs'][key] == sha, 'input changed during qualification: ' + key
            return
        report['inputs'][key] = sha
        destination = output / 'frozen-inputs' / (sha + '-' + path.name)
        destination.parent.mkdir(parents=True, exist_ok=True)
        if not destination.exists():
            shutil.copyfile(path, destination)
        report['frozen_inputs'][key] = str(destination.relative_to(output))

    def compile_command(command):
        command = list(map(str, command))
        report['commands'].append(command)
        save()
        result = subprocess.run(command, capture_output=True, text=True, timeout=120)
        if result.returncode:
            print(result.stdout + result.stderr, flush=True)
            result.check_returncode()

    try:
        with tempfile.TemporaryDirectory(prefix='home-transfer-exit-') as temp:
            work = Path(temp)
            sdk = headers(work, system, utilities, watch)
            inherited = watch / 'tests/fast_wake_clock_test.c'
            source = inherited.read_text()
            assert digest(inherited) == '85292c7c79aba23e97780c324fe50e3744e321d3ac3a63774556024de493b778'
            replacements = {
                'int main(void){': 'int inherited_fast_main(void){',
                'const risc_runtime_api_v1*risc_runtime_get_api(uint32_t version)':
                    'const risc_runtime_api_v1*inherited_fast_runtime_get_api(uint32_t version)',
            }
            for old, new in replacements.items():
                assert source.count(old) == 1
                source = source.replace(old, new)
            assert source.count('ended=true;') == 4
            source = '#include <stdbool.h>\nstatic bool probe_long;\n' + source.replace('ended=true;', 'if(!probe_long)ended=true;')
            source += (ROOT / 'tests/home_transfer_exit_controls.inc').read_text()
            fixture = output / 'generated_fixture.c'
            fixture.write_text(source)
            sources = [fixture,
                       *[watch / 'apps/clock' / name for name in
                         ('nova/nova.c', 'points_projection.c', 'viewport.c', 'capability_clients.c', 'quick_radios.c')],
                       *[system / 'lib/PortableApps/src' / name for name in
                         ('quick_actions.c', 'quick_render.c', 'quick_session.c', 'PortableAutomaticProvision.c', 'PortableNetworkSession.c')],
                       watch / 'apps/clock/effects/boot.cpp']
            for path in (inherited, fixture, Path(__file__), ROOT / 'tests/home_transfer_exit_controls.inc', ROOT / 'scripts/merged_sdk.py'):
                record(path)
            dimensions = [(240, 240), (410, 502)] if args.dimensions == 'both' else [tuple(map(int, args.dimensions.split('x')))]
            for width, height in dimensions:
                for mode in (('normal',) if args.normal_only else ('normal', 'address', 'undefined')):
                    label = f'{width}x{height}-{mode}'
                    flags = ['-O1', '-g', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', '-Wno-misleading-indentation',
                             '-DWATCH_COMPACT_NOVA_ASSETS', '-DWATCH_SHARED_PREFERENCE_HEADERS', '-DPORTABLE_LOW_BATTERY',
                             '-DWATCH_COMPRESSED_CALENDAR_ALPHA', '-DWATCH_COMPRESSED_FACE_ALPHA', '-DWATCH_FAST_WAKE',
                             '-DWATCH_COLD_BOOT_ANIMATION', '-DPORTABLE_WATCH_NATIVE_HEIGHT', '-DPORTABLE_RTC_UTC8_DENVER',
                             '-DPORTABLE_PREFERENCE_STORE', '-DWATCH_ATOMIC_PREFERENCES', '-DPORTABLE_QUICK_ATOMIC_PREFERENCES',
                             '-DPORTABLE_QUICK_RADIOS', '-DWATCH_AUTOMATIC_PROVISION', '-DWATCH_CRASH_REPORT',
                             '-DCRASH_REPORT_SESSION_FILES=1', '-DWATCH_CRASH_REPORT_NAMESPACE=61',
                             f'-DTEST_WIDTH={width}u', f'-DTEST_HEIGHT={height}u',
                             *['-I' + str(path) for path in (sdk, watch / 'include', watch, watch / 'tests', system / 'lib/PortableApps/src')]]
                    if mode != 'normal':
                        flags += [f'-fsanitize={mode}', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie']
                    if not args.feature_off:
                        flags.append('-DWATCH_SERIAL_TRANSFER_MODE')
                    objects = []
                    for index, path in enumerate(sources):
                        obj, dep = work / f'{index}.o', work / f'{index}.d'
                        cpp = path.suffix == '.cpp'
                        compile_command(['c++' if cpp else 'cc', '-std=c++17' if cpp else '-std=c11', *flags,
                                         *(['-DWATCH_CONTEXTS_CLIENT'] if index else []), '-MMD', '-MF', dep, '-c', path, '-o', obj])
                        objects.append(obj)
                        for included in dep.read_text().replace('\\\n', ' ').split(':', 1)[1].split():
                            record(Path(included))
                    binary = output / (label + '.test')
                    compile_command(['c++', *flags, *objects, '-o', binary])
                    cases = ('before-phase0', 'after-pacing', 'no-transfer', 'null-exit') if args.expect_baseline or args.feature_off else (
                        'helper', 'null-runtime', 'old', 'partial', 'null-exit', 'no-transfer', 'pending-pair',
                        'before-phase0', 'after-pacing', 'during-presentation', 'active-cleanup', 'release-refusal', 'pause-refusal',
                        'terminal-context', 'terminal-retained', 'terminal-unknown')
                    for case in cases:
                        command = [str(binary), case, report['variant']]
                        result = subprocess.run(command, capture_output=True, text=True, timeout=120,
                                                env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0:halt_on_error=1',
                                                     'UBSAN_OPTIONS': 'halt_on_error=1:print_stacktrace=1'})
                        (output / f'{label}-{case}.log').write_text(result.stdout + result.stderr)
                        print(result.stdout + result.stderr, end='', flush=True)
                        report['runs'].append({'command': command, 'dimensions': [width, height], 'mode': mode,
                                               'binary_sha256': digest(binary), 'case': case,
                                               'returncode': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr})
                        save()
                        result.check_returncode()
        report['status'] = 'passed'
    except Exception:
        report['status'] = 'failed'
        raise
    finally:
        save()


if __name__ == '__main__':
    main()
