"""Contract negatives bind to the real shared template and builder receipts."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from test_launcher_metadata import candidate_manifests, home_contract, sha, staged_manifests


class MetadataTest(unittest.TestCase):
    def test_actual_source_templates(self):
        home, clock = candidate_manifests()
        home_contract(json.loads(home.read_text()))
        self.assertEqual(json.loads(clock.read_text())['id'], 'twatch-clock-return')

    def test_required_presentation_and_identity_negative_controls(self):
        home = json.loads(candidate_manifests()[0].read_text())
        mutations = [('display_name', None), ('display_name', 'twatch-clock'),
                     ('launcher_visible', None), ('launcher_visible', True),
                     ('launcher_visible', 'false'), ('launcher_visible', 0),
                     ('id', 'Home'), ('file_name', 'clock.elf')]
        for field, value in mutations:
            with self.subTest(field=field, value=value):
                candidate = copy.deepcopy(home)
                if value is None:
                    candidate.pop(field)
                else:
                    candidate[field] = value
                with self.assertRaises(ValueError):
                    home_contract(candidate)

    def test_generated_candidate_keeps_every_supported_template_field(self):
        sources = candidate_manifests()
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            record = {'build_variant': 'shared-no-board-defines', 'automatic_provision': False,
                      'home_files_sha256': {}, 'artifacts': {}}
            for source in sources:
                role = source.stem
                (root / source.name).write_bytes(source.read_bytes())
                image = root / (role + '.elf')
                image.write_bytes(b'fixture-not-target-execution-' + role.encode())
                record['home_files_sha256']['apps/clock/' + source.name] = sha(source)
                record['artifacts'][role] = {'bytes': image.stat().st_size, 'sha256': sha(image)}
            (root / 'build.json').write_text(json.dumps(record))
            candidate_manifests(root)
            home = json.loads(sources[0].read_text())
            for field in home:
                if field == 'version':
                    continue  # Malformed value is checked separately below.
                with self.subTest(field=field):
                    changed = copy.deepcopy(home)
                    changed.pop(field)
                    (root / 'default.json').write_text(json.dumps(changed))
                    with self.assertRaises((ValueError, KeyError)):
                        candidate_manifests(root)
            changed = copy.deepcopy(home)
            changed['version'] = '0.0.0'
            (root / 'default.json').write_text(json.dumps(changed))
            with self.assertRaisesRegex(ValueError, 'Generated manifest'):
                candidate_manifests(root)
            (root / 'default.json').write_bytes(sources[0].read_bytes())
            (root / 'default.elf').write_bytes(b'wrong')
            with self.assertRaisesRegex(ValueError, 'ELF'):
                candidate_manifests(root)

    def test_staged_manifest_digest_rejects_metadata_drift(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            sources = candidate_manifests()
            for source in sources:
                (root / source.name).write_bytes(source.read_bytes())
                (root / (source.stem + '.elf')).write_bytes(source.stem.encode())
            (root / 'boot.json').write_text(json.dumps({'app_capabilities':
                [{'manifest': source.name} for source in sources]}))
            record = {'manifest_sha256': {s.name: sha(root / s.name) for s in sources},
                      'home_sha256': sha(root / 'default.elf'), 'return_sha256': sha(root / 'clock.elf')}
            receipt = root / 'receipt.json'; receipt.write_text(json.dumps(record))
            self.assertEqual(staged_manifests(root, receipt), [root / s.name for s in sources])
            for source in sources:
                changed = json.loads(source.read_text())
                changed['launcher_visible'] = True
                (root / source.name).write_text(json.dumps(changed))
                with self.assertRaisesRegex(ValueError, 'Staged manifest'):
                    staged_manifests(root, receipt)
                (root / source.name).write_bytes(source.read_bytes())
            (root / 'default.elf').write_bytes(b'wrong')
            with self.assertRaisesRegex(ValueError, 'Staged ELF'):
                staged_manifests(root, receipt)


if __name__ == '__main__':
    unittest.main()
