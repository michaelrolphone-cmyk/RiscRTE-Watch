#!/usr/bin/env python3
"""Bounded actual-Home RAM checkpoint regression, normal/ASan/UBSan at both sizes."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shlex
import subprocess
import tempfile

from merged_sdk import headers

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def git_commit(path):
    return subprocess.check_output(['git', '-C', str(path), 'rev-parse', 'HEAD'], text=True).strip()


def fixture_files(directory, crash_report):
    source = (ROOT / 'tests/fast_wake_clock_test.c').read_text()
    replacements = {
        'int main(void){': 'int inherited_fast_fixture_main(void){',
        'const risc_runtime_api_v1*risc_runtime_get_api(uint32_t version)':
            'const risc_runtime_api_v1*fixture_base_runtime_get_api(uint32_t version)',
    }
    for old, new in replacements.items():
        assert source.count(old) == 1, f'inherited fixture changed: {old}'
        source = source.replace(old, new)
    (directory / 'home_exit_fixture.inc').write_text(source)
    anchors = {
        'INVALID_RUNTIME': '!rt->health || !rt->yield_ms ||',
        'INITIAL_HEALTH': 'if(!alive(&clock_wake_started))',
        'DISPLAY_ACQUIRE': 'if (!have_d)',
        'RENDER_FAILURE': 'if (!frame(&s) || !draw_clock(now,&s) || !present() || !pace_frame(now))',
        'NORMAL_END': 'while(alive) ended without an earlier reason',
        'RTC_CUSTODY': 'if(!watch_runtime_snapshot(&native_clock,&remaining)){clock_exit_checkpoint(__LINE__);goto done;}',
    }
    if crash_report:
        # This optional hook observes the fixture's shutdown before the next
        # while-condition check; preserve that earlier production exit route.
        anchors['NORMAL_END'] = 'if(!clock_crash_step())'
    lines = (ROOT / 'apps/clock/crown.c').read_text().splitlines()
    expected = {}
    for name, anchor in anchors.items():
        matches = [i for i, text in enumerate(lines, 1) if anchor in text and 'clock_exit_checkpoint(__LINE__)' in text]
        assert len(matches) == 1, f'expected unique production checkpoint route: {name}'
        line = matches[0]
        assert 'clock_exit_checkpoint(__LINE__)' in lines[line - 1]
        assert line < 100000, 'checkpoint source-line encoding overflow'
        expected[name] = line
    (directory / 'home_exit_expected_lines.h').write_text(''.join(
        f'#define HOME_EXIT_{name} {line}u\n' for name, line in expected.items()))
    return expected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--system', '--system-apps', dest='system', type=Path, required=True)
    parser.add_argument('--utilities', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=ROOT / 'dist/home-exit-checkpoint')
    parser.add_argument('--width', type=int)
    parser.add_argument('--height', type=int)
    parser.add_argument('--atomic', action='store_true')
    parser.add_argument('--crash-report', action='store_true')
    parser.add_argument('--automatic-provision', action='store_true')
    args = parser.parse_args()
    assert (args.width is None) == (args.height is None), 'specify both width and height'
    dimensions = [(args.width, args.height)] if args.width else [(410, 502), (240, 240)]
    assert all(0 < width <= 512 and 0 < height <= 512 for width, height in dimensions)
    args.system = args.system.resolve()
    args.utilities = args.utilities.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    source_names = ['quick_actions.c', 'quick_render.c', 'quick_session.c']
    if args.automatic_provision:
        source_names += ['PortableAutomaticProvision.c', 'PortableNetworkSession.c']
    sources = [ROOT / 'tests/home_exit_checkpoint_test.c', ROOT / 'apps/clock/nova/nova.c',
               ROOT / 'apps/clock/points_projection.c', ROOT / 'apps/clock/viewport.c',
               ROOT / 'apps/clock/capability_clients.c', ROOT / 'apps/clock/quick_radios.c']
    sources += [args.system / 'lib/PortableApps/src' / name for name in source_names]
    boot = ROOT / 'apps/clock/effects/boot.cpp'
    inputs = sources + [boot, Path(__file__).resolve(), ROOT / 'scripts/merged_sdk.py',
                       ROOT / 'tests/fast_wake_clock_test.c', ROOT / 'tests/quick_clock_test.c',
                       ROOT / 'apps/clock/crown.c', args.system / 'lib/PortableApps/include/RiscDiagnosticCheckpointV1.h',
                       ROOT / 'sdk/app/RiscRuntimeV1.h']
    report = {'status': 'running', 'commits': {str(path): git_commit(path) for path in
              (ROOT, args.system, args.utilities)}, 'source_sha256': {str(path): digest(path) for path in inputs},
              'dimensions': dimensions, 'configuration': {'atomic': args.atomic,
              'crash_report': args.crash_report, 'automatic_provision': args.automatic_provision},
              'commands': [], 'cases': [], 'dependency_sha256': {}}
    report_path = args.output / 'report.json'

    def run(command, log, timeout):
        report['commands'].append(command)
        log.write('$ ' + ' '.join(command) + '\n')
        log.flush()
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True, timeout=timeout, env={**os.environ,
                                'ASAN_OPTIONS': 'detect_leaks=0:halt_on_error=1',
                                'UBSAN_OPTIONS': 'halt_on_error=1:print_stacktrace=1'})
        assert len(result.stdout) <= 262144, 'unexpectedly unbounded compiler/test output'
        log.write(result.stdout)
        log.flush()
        if result.returncode:
            raise RuntimeError(f'command failed ({result.returncode}): {command[0]}\n{result.stdout[-12000:]}')
        return result.stdout

    try:
        for width, height in dimensions:
            for mode in ('normal', 'address', 'undefined'):
                label = f'{width}x{height}-{mode}'
                with tempfile.TemporaryDirectory(prefix='home-exit-', dir=args.output) as tmp:
                    out = Path(tmp)
                    report['expected_source_lines'] = fixture_files(out, args.crash_report)
                    sdk = headers(out, args.system, args.utilities, ROOT)
                    report.setdefault('merged_sdk_sha256', {path.name: digest(path) for path in sorted(sdk.glob('*.h'))})
                    flags = ['-DWATCH_COMPACT_NOVA_ASSETS', '-DWATCH_SHARED_PREFERENCE_HEADERS',
                             '-DPORTABLE_LOW_BATTERY', '-DWATCH_COMPRESSED_CALENDAR_ALPHA',
                             '-DWATCH_COMPRESSED_FACE_ALPHA', '-DWATCH_FAST_WAKE', '-DWATCH_COLD_BOOT_ANIMATION',
                             '-DPORTABLE_WATCH_NATIVE_HEIGHT', '-DPORTABLE_RTC_UTC8_DENVER',
                             f'-DTEST_WIDTH={width}u', f'-DTEST_HEIGHT={height}u',
                             '-O1', '-g1', '-Wall', '-Wextra', '-Werror']
                    flags += ['-I' + str(path) for path in (out, sdk, ROOT / 'include', ROOT / 'tests', ROOT)]
                    if args.atomic:
                        flags += ['-DWATCH_ATOMIC_PREFERENCES', '-DPORTABLE_QUICK_ATOMIC_PREFERENCES',
                                  '-DPORTABLE_QUICK_RADIOS', '-I' + str(args.system / 'lib/PortableApps/src')]
                    if args.crash_report:
                        flags += ['-DWATCH_CRASH_REPORT', '-DCRASH_REPORT_SESSION_FILES=1', '-DWATCH_CRASH_REPORT_NAMESPACE=62']
                    if args.automatic_provision:
                        flags += ['-DWATCH_AUTOMATIC_PROVISION']
                    if mode != 'normal':
                        flags += [f'-fsanitize={mode}', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie']
                    with (args.output / f'{label}.log').open('w') as log:
                        objects = []
                        for index, source in enumerate(sources):
                            obj = out / f'{index}.o'
                            dep = out / f'{index}.d'
                            run([os.environ.get('CC', 'cc'), '-std=c11', *flags,
                                 *(['-DWATCH_CONTEXTS_CLIENT'] if index else []),
                                 '-MMD', '-MF', str(dep), '-c', str(source), '-o', str(obj)], log, 90)
                            objects.append(obj)
                        binary = out / 'home-exit-checkpoint'
                        run([os.environ.get('CXX', 'c++'), '-std=c++11', *flags,
                             '-MMD', '-MF', str(out / 'boot.d'), str(boot),
                             *map(str, objects), '-o', str(binary)], log, 90)
                        for dep in out.glob('*.d'):
                            dependencies = dep.read_text().replace('\\\n', ' ').split(':', 1)[1]
                            for name in shlex.split(dependencies):
                                path = Path(name)
                                key = str(path).replace(str(out), '<generated>')
                                report['dependency_sha256'][key] = digest(path)
                        for obj in objects:
                            obj.unlink()
                        output = run([str(binary)], log, 120)
                        report['cases'].append({'label': label, 'status': 'pass',
                                                'results': output.splitlines()})
                        print(f'{label}: PASS', flush=True)
                report_path.write_text(json.dumps(report, indent=2) + '\n')
        report['status'] = 'pass'
    except BaseException as error:
        report['status'] = 'failed'
        report['error'] = str(error)
        raise
    finally:
        report_path.write_text(json.dumps(report, indent=2) + '\n')
    print(f'Actual Home checkpoint tests PASS; source hashes and bounded logs: {args.output}')


if __name__ == '__main__':
    main()
