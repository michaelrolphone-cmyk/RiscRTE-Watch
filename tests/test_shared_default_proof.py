#!/usr/bin/env python3
"""Negative gates for the exact canonical/default extraction proof."""
import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('proof', ROOT / 'scripts/verify_shared_default.py')
proof = importlib.util.module_from_spec(spec)
spec.loader.exec_module(proof)


class ProofTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.data = b'\x7fELF canonical fixture'
        self.manifest = {'requires': [{'capability': 'storage.preferences', 'api': 1}],
                         'optional': [{'capability': 'storage.files', 'api': 1}]}
        self.record = {'build_variant': 'shared-no-board-defines', 'preferences': 'runtime-provider',
                       'hardware_profile_inputs': False, 'hardware_verified': False,
                       'compiler_sha256': 'compiler', 'verified_dependencies': {'system': 'same'},
                       'home_files_sha256': {'clock': 'same'},
                       'artifacts': {'default': {'sha256': proof.digest(self.data), 'bytes': len(self.data),
                           'defines': ['-DPORTABLE_PREFERENCE_STORE', '-DCRASH_REPORT_SESSION_FILES=1']}}}
        self.write('a')
        self.write('b')

    def tearDown(self):
        self.temp.cleanup()

    def write(self, name, data=None, record=None, manifest=None):
        path = self.root / name
        path.mkdir(exist_ok=True)
        (path / 'default.elf').write_bytes(self.data if data is None else data)
        (path / 'build.json').write_text(json.dumps(self.record if record is None else record))
        (path / 'default.json').write_text(json.dumps(self.manifest if manifest is None else manifest))
        return path

    def verify(self):
        return proof.verify(self.root / 'a', self.root / 'b')

    def test_equal(self):
        self.assertTrue(self.verify()['independent_builds_identical'])
        self.assertFalse(self.verify()['hardware_tested'])

    def test_modified_elf(self):
        self.write('b', data=self.data + b'!')
        with self.assertRaisesRegex(ValueError, 'digest'):
            self.verify()

    def test_rebuilt_different_elf(self):
        other = self.data + b'!'
        r = copy.deepcopy(self.record)
        r['artifacts']['default'].update(sha256=proof.digest(other), bytes=len(other))
        self.write('b', data=other, record=r)
        with self.assertRaisesRegex(ValueError, 'ELF builds differ'):
            self.verify()

    def test_hardware_profile(self):
        r = copy.deepcopy(self.record)
        r['hardware_profile_inputs'] = True
        self.write('b', record=r)
        with self.assertRaisesRegex(ValueError, 'Hardware build inputs'):
            self.verify()

    def test_backend_literal(self):
        other = self.data + b'storage.volume\0'
        r = copy.deepcopy(self.record)
        r['artifacts']['default'].update(sha256=proof.digest(other), bytes=len(other))
        self.write('b', data=other, record=r)
        with self.assertRaisesRegex(ValueError, 'compiled into Home'):
            self.verify()

    def test_backend_manifest(self):
        m = copy.deepcopy(self.manifest)
        m['optional'].append({'capability': 'storage.volume', 'api': 1})
        self.write('b', manifest=m)
        with self.assertRaisesRegex(ValueError, 'backend-specific contract'):
            self.verify()

    def test_dependency_drift(self):
        r = copy.deepcopy(self.record)
        r['verified_dependencies'] = {'system': 'different'}
        self.write('b', record=r)
        with self.assertRaisesRegex(ValueError, 'receipt differs'):
            self.verify()

    def test_both_stores_required(self):
        with self.assertRaisesRegex(ValueError, 'exactly both'):
            proof.verify(self.root / 'a', self.root / 'b', [self.root / 'store'])


if __name__ == '__main__':
    unittest.main()
