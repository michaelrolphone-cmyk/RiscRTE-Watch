#!/usr/bin/env python3
"""Qualify shared Home presentation on source or exact generated manifests.

Uses unchanged production Runtime metadata normalization and catalog selection.
Policy admission, provider grants, target instructions and hardware are outside
this presentation-only probe; no product store or released artifact is changed.
"""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
RUNTIME_SOURCES = [
    'src/bootstrap/Json.cpp', 'src/bootstrap/Board.cpp', 'src/bootstrap/Runtime.cpp',
    'src/runtime/streams/AppStreamSessions.cpp', 'src/runtime/streams/ProviderQueueHost.cpp',
    'src/runtime/drivers/ProviderGraphV2.cpp', 'src/runtime/drivers/ProviderModuleV2.cpp',
]


def require(ok, message):
    if not ok:
        raise ValueError(message)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def home_contract(home):
    require(home['id'] == 'twatch-clock' and home['file_name'] == 'default.elf',
            'Persisted Home identity/path changed')
    require(home.get('display_name') == 'Home', 'Home display_name missing or changed')
    require(home.get('launcher_visible') is False, 'Home must explicitly be hidden')


def candidate_manifests(artifacts=None):
    source = ROOT / 'apps/clock'
    home = json.loads((source / 'default.json').read_text())
    clock = json.loads((source / 'clock.json').read_text())
    home_contract(home)
    require(clock['id'] == 'twatch-clock-return' and clock['file_name'] == 'clock.elf'
            and clock['requires'] == [], 'Return role identity/authority changed')
    if artifacts is None:
        return [source / 'default.json', source / 'clock.json']
    record = json.loads((artifacts / 'build.json').read_text())
    require(record['build_variant'] == 'shared-no-board-defines', 'Not a shared build')
    for role, expected in [('default', home), ('clock', clock)]:
        if role == 'default':
            # Versions are a separate source-owned build checkpoint. All other
            # fields must survive, with only the declared provisioning additions.
            expected = copy.deepcopy(expected)
            built = json.loads((artifacts / (role + '.json')).read_text())
            expected['version'] = ('0.20.6' if record.get('serial_transfer_mode', False) else
                                   '0.20.5' if record['automatic_provision'] else '0.19.4')
            if record['automatic_provision']:
                expected['requires'].append({'capability': 'software.installed-apps', 'api': 1})
                expected['optional'].append({'capability': 'storage.volume', 'api': 1})
        else:
            built = json.loads((artifacts / (role + '.json')).read_text())
        require(built == expected, 'Generated manifest lost or changed supported fields: ' + role)
        require(record['home_files_sha256']['apps/clock/' + role + '.json'] == sha(source / (role + '.json')),
                'Build did not consume this exact manifest template: ' + role)
        meta = record['artifacts'][role]
        require(meta['sha256'] == sha(artifacts / (role + '.elf')) and
                meta['bytes'] == (artifacts / (role + '.elf')).stat().st_size,
                'Exact candidate ELF does not match build receipt: ' + role)
    home_contract(json.loads((artifacts / 'default.json').read_text()))
    return [artifacts / 'default.json', artifacts / 'clock.json']


def build_probe(runtime, output, sanitize=False):
    includes = ['-I' + str(runtime / p) for p in
                ['src', 'sdk/app', 'sdk/driver', 'sdk/hardware', 'lib/ArduinoJson/src', 'test/drivers/stubs']]
    flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer'] if sanitize else []
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-Wno-missing-field-initializers', '-fno-access-control', '-no-pie',
                    *flags, *includes, *[str(runtime / p) for p in RUNTIME_SOURCES],
                    str(ROOT / 'tests/launcher_metadata_runtime.cpp'), '-ldl', '-o', str(output)], check=True)


def inventory(probe, paths):
    return json.loads(subprocess.check_output([str(probe), *map(str, paths)], text=True))


def staged_manifests(store, receipt, artifacts=None):
    require(receipt is not None, '--store requires its stage-derived receipt')
    staged = json.loads(receipt.read_text())
    for role in ('default', 'clock'):
        require(staged['manifest_sha256'][role + '.json'] == sha(store / (role + '.json')),
                'Staged manifest differs from the exact composition: ' + role)
    require(staged['home_sha256'] == sha(store / 'default.elf') and
            staged['return_sha256'] == sha(store / 'clock.elf'), 'Staged ELF differs from composition')
    # Device composition preserves product authority/optional fields. Verify
    # exact stage-derived bytes, not equality to the raw shared app manifest.
    home_contract(json.loads((store / 'default.json').read_text()))
    if artifacts:
        require((store / 'default.elf').read_bytes() == (artifacts / 'default.elf').read_bytes(),
                'Store does not contain the exact candidate Home ELF')
    boot = json.loads((store / 'boot.json').read_text())
    return [store / row['manifest'] for row in boot['app_capabilities']]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--artifacts', type=Path)
    parser.add_argument('--store', type=Path, help='Optional extracted, unsealed candidate inventory; read only')
    parser.add_argument('--stage-receipt', type=Path, help='Exact stage_product report for --store')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    runtime = args.runtime.resolve()
    require(not subprocess.check_output(['git', '-C', str(runtime), 'status', '--porcelain',
                                        '--untracked-files=all', '--', 'src', 'sdk', 'lib', 'test']),
            'Runtime source inputs are modified or untracked')
    require((args.store is None) == (args.stage_receipt is None), 'Pass --store and --stage-receipt together')
    paths = candidate_manifests(args.artifacts)
    args.output.mkdir(parents=True, exist_ok=True)
    probe = args.output / 'catalog-probe'
    build_probe(runtime, probe, args.sanitize)
    result = inventory(probe, paths)
    require(result['rows'][0]['display_name'] == 'Home' and result['rows'][0]['visible'] is False,
            'Production Runtime Home metadata differs')
    require('twatch-clock' not in result['selected'], 'Legacy Home ID leaked into launcher')
    require(result['selected'] == ['twatch-clock-return'], 'Return route default visibility changed')
    # A negative control must demonstrate the actual missing-field behavior,
    # not just compare two copies of the intended policy.
    with tempfile.TemporaryDirectory(prefix='watch-metadata-controls-') as tmp:
        old = json.loads(paths[0].read_text())
        old.pop('display_name'); old.pop('launcher_visible')
        control = Path(tmp) / 'default.json'
        control.write_text(json.dumps(old))
        negative = inventory(probe, [control, paths[1]])
        require(negative['rows'][0]['display_name'] == 'twatch-clock' and
                'twatch-clock' in negative['selected'], 'Legacy negative control not detected')
    result.update(schema=1, input_sha256={p.name: sha(p) for p in paths},
                  negative_control=negative, hardware_tested=False, target_instructions_executed=False,
                  native_policy_admission_tested=False,
                  runtime_commit=subprocess.check_output(['git', '-C', str(runtime), 'rev-parse', 'HEAD'], text=True).strip(),
                  runtime_catalog_sha256=sha(runtime / 'src/bootstrap/AppCatalogRuntime.inc'),
                  probe_source_sha256=sha(ROOT / 'tests/launcher_metadata_runtime.cpp'),
                  qualifier_source_sha256=sha(Path(__file__)),
                  sanitized=args.sanitize,
                  leak_detection_enabled=args.sanitize and 'detect_leaks=0' not in os.environ.get('ASAN_OPTIONS', ''))
    if args.store:
        manifests = staged_manifests(args.store, args.stage_receipt, args.artifacts)
        result['store_inventory'] = inventory(probe, manifests)
        result['store_input_sha256'] = {p.relative_to(args.store).as_posix(): sha(p) for p in manifests}
        require('twatch-clock' not in result['store_inventory']['selected'], 'Store exposes duplicate Home')
    (args.output / 'receipt.json').write_text(json.dumps(result, indent=2) + '\n')
    print('Actual Runtime metadata normalization/catalog selection: PASS')


if __name__ == '__main__':
    main()
