#!/usr/bin/env python3
"""Prove canonical Home bytes and capability-only build inputs.

Pass two independent build outputs. Product packaging additionally passes both
extracted boot stores; their exact default.elf/default.json must match the same
canonical artifacts. This check never accesses a device.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys


def require(ok, message):
    if not ok:
        raise ValueError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def check_build(path):
    record = json.loads((path / 'build.json').read_text())
    require(record['build_variant'] == 'shared-no-board-defines', 'Unexpected build profile')
    require(record['preferences'] == 'runtime-provider', 'Application selected a preference backend')
    require(record['hardware_profile_inputs'] is False, 'Hardware build inputs were admitted')
    require(record['hardware_verified'] is False, 'Software build cannot assert hardware acceptance')
    raw = (path / 'default.elf').read_bytes()
    manifest = (path / 'default.json').read_bytes()
    app = json.loads(manifest)
    require(record['artifacts']['default']['sha256'] == digest(raw), 'ELF receipt digest differs')
    require(record['artifacts']['default']['bytes'] == len(raw), 'ELF receipt size differs')
    flags = record['artifacts']['default']['defines']
    require('-DPORTABLE_PREFERENCE_STORE' in flags and '-DCRASH_REPORT_SESSION_FILES=1' in flags,
            'Canonical storage interfaces missing')
    require(not any('WATCH_CRASH_REPORT_VOLUME' in flag for flag in flags), 'Physical storage identity in build')
    required = {(x['capability'], x['api']) for x in app['requires']}
    optional = {(x['capability'], x['api']) for x in app.get('optional', [])}
    require(('storage.preferences', 1) in required and ('storage.files', 1) in optional,
            'Generic storage declarations missing')
    automatic = record.get('automatic_provision', False)
    require(('storage.volume', 1) not in required, 'Removable volume must stay optional')
    require((('storage.volume', 1) in optional) == automatic,
            'Optional removable-volume declaration differs from selected provisioning feature')
    require((('-DWATCH_AUTOMATIC_PROVISION' in flags)) == automatic,
            'Provisioning selection differs from compiler receipt')
    require(not any('WATCH_PROVISION_VOLUME' in flag for flag in flags),
            'Physical provisioning volume identity in build')
    require(('storage.key-value.atomic', 1) not in required | optional,
            'Application declares a preference backend')
    forbidden = (b'storage.key-value.atomic\0',) if automatic else (b'storage.volume\0', b'storage.key-value.atomic\0')
    for literal in forbidden:
        require(literal not in raw, 'Backend-specific capability compiled into Home: ' + repr(literal))
    return raw, manifest, record


def verify(first, second, stores=(), runtime=None):
    left, manifest, a = check_build(first)
    right, second_manifest, b = check_build(second)
    require(left == right, 'Independent canonical default ELF builds differ')
    require(manifest == second_manifest, 'Independent canonical default manifests differ')
    for field in ('compiler_sha256', 'verified_dependencies', 'home_files_sha256'):
        require(a[field] == b[field], 'Canonical source/toolchain receipt differs: ' + field)
    require(not stores or len(stores) == 2, 'Provide exactly both product stores')
    extracted = []
    if stores:
        require(runtime is not None, 'Runtime source required to extract stores')
        sys.path.insert(0, str(runtime / 'scripts'))
        from read_only_spiffs import read_image
        for path in stores:
            packed = path.read_bytes()
            files = read_image(packed, len(packed))
            require(files['default.elf'] == left, 'Product default differs: ' + str(path))
            require(files['default.json'] == manifest, 'Product default manifest differs: ' + str(path))
            extracted.append({'store': str(path), 'store_sha256': digest(packed),
                              'default_sha256': digest(files['default.elf'])})
    return {'schema': 1, 'default_sha256': digest(left), 'default_bytes': len(left),
            'manifest_sha256': digest(manifest), 'independent_builds_identical': True,
            'product_extractions': extracted, 'hardware_tested': False}


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--first', type=Path, required=True)
    p.add_argument('--second', type=Path, required=True)
    p.add_argument('--store', type=Path, action='append', default=[])
    p.add_argument('--runtime', type=Path)
    p.add_argument('--receipt', type=Path)
    args = p.parse_args()
    receipt = verify(args.first, args.second, args.store, args.runtime)
    text = json.dumps(receipt, indent=2) + '\n'
    if args.receipt:
        args.receipt.parent.mkdir(parents=True, exist_ok=True)
        args.receipt.write_text(text)
    print(text, end='')
