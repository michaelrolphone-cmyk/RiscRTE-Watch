"""Source-only metadata provisioning tests; synthetic ELFs are never executed.

Pass --system PATH to additionally verify all names/icons against the canonical
System test/native_apps/fixtures/watch_springboard_catalog_23.c fixture.
"""
from pathlib import Path
import argparse
import copy
import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from stage_app_catalog_metadata import (GRANT, REQUIREMENT, MAX_APPS, MAX_DOCUMENT_BYTES,
    document, encoded, read_store, safe_path, stage, write_new_store)

SYSTEM = None


def fake_elf(name):
    # A stable, small byte sentinel: this test does not claim ELF admission.
    return b"\x7fELF\x01\x01\x01" + b"\0" * 45 + name.encode()


def manifest(app_id, filename):
    return dict(type="application", id=app_id, version="1.2.3", architecture="xtensa-esp32s3",
                file_name=filename, entry="app_main", requires=[])


def fixture(names=("home", "launcher", "settings", "calculator")):
    store = {"user.bin": b"untouched user bytes\x00\xff", "driver.elf": fake_elf("provider"),
             "cohort.json": b'{"existing":"unsealed assembly input"}\n'}
    policies = []
    for name in names:
        path = name + ".json"
        value = manifest(name.replace("_", "-"), name + ".elf")
        value["description"] = "Preserve my original description"
        value["supported_file_types"] = [".txt"]
        value["min_firmware_version"] = "0.8.0"
        value["runtime_lifecycle"] = "legacy"
        value["requires"] = [dict(capability="storage.key-value", api=1),
                             dict(capability="storage.shared-data", api=1)]
        grants = [dict(capability="storage.key-value", api=1, instance_id=n) for n in (1, 5)]
        grants += [dict(capability="storage.shared-data", api=1, instance_id=n, file="context-rules.ctx") for n in (2, 3)]
        policies.append(dict(manifest=path, grants=grants,
                             app_data_export={"namespace": name, "files": ["prefs.bin"]}))
        # Deliberately noncanonical whitespace: an unchanged file must not be reserialized.
        store[path] = json.dumps(value, separators=(", ", " : ")).encode()
        store[name + ".elf"] = fake_elf(name)
    store["boot.json"] = json.dumps(dict(default_app="home.elf", drivers=[{"manifest": "provider.json", "instance_id": 9}],
                                        app_capabilities=policies, installation={"existing": "policy"}), indent=3).encode()
    profile = dict(schema=1, entries=[
        dict(manifest="home.json", id="home", launcher_visible=False),
        dict(manifest="launcher.json", id="launcher", launcher_visible=False),
        dict(manifest="settings.json", id="settings", display_name="Settings", icon="solid:f013"),
        dict(manifest="calculator.json", id="calculator", display_name="Calculator", icon="solid:f1ec")],
        consumers=[name + ".json" for name in ("settings", "launcher")])
    return store, profile


def replace(store, name, edit):
    value = document(store[name])
    edit(value)
    store[name] = encoded(value)


def compose(store, profile, requirement_capacity=16, grant_capacity=16):
    return stage(store, profile, requirement_capacity=requirement_capacity, grant_capacity=grant_capacity)


class MetadataTest(unittest.TestCase):
    def setUp(self):
        self.store, self.profile = fixture()

    def test_selected_consumers_only_and_original_policy_preserved(self):
        out, report = compose(self.store, self.profile)
        old_boot, new_boot = document(self.store["boot.json"]), document(out["boot.json"])
        for old, new in zip(old_boot["app_capabilities"], new_boot["app_capabilities"]):
            expected = copy.deepcopy(old)
            if old["manifest"] in self.profile["consumers"]:
                expected["grants"].append(GRANT)
            self.assertEqual(new, expected)
        old_boot["app_capabilities"] = new_boot["app_capabilities"]
        self.assertEqual(old_boot, new_boot)
        for name in ("home", "launcher", "settings", "calculator"):
            old, new = document(self.store[name + ".json"]), document(out[name + ".json"])
            for key in ("id", "version", "entry", "file_name", "architecture", "description", "supported_file_types", "min_firmware_version", "runtime_lifecycle"):
                self.assertEqual(old[key], new[key])
            self.assertEqual(new["requires"], old["requires"] + ([REQUIREMENT] if name in ("launcher", "settings") else []))
        self.assertFalse(report["flashable"])
        self.assertTrue(report["requires_device_assembly"])
        self.assertTrue(report["requires_cohort_rebind"])

    def test_every_elf_and_unselected_file_is_exactly_unchanged(self):
        out, report = compose(self.store, self.profile)
        self.assertEqual(set(out), set(self.store))
        for name, raw in self.store.items():
            if name.endswith(".elf") or name not in report["changed_files"]:
                self.assertEqual(out[name], raw, name)
            if name.endswith(".elf"):
                self.assertEqual(report["elf_sha256"][name], hashlib.sha256(raw).hexdigest())
        self.assertEqual(report["added_files"], [])

    def test_same_consumer_hashes_across_different_inventories(self):
        other, _ = fixture(("home", "launcher", "settings"))
        a, first = compose(self.store, self.profile)
        b, second = compose(other, self.profile)
        self.assertEqual((first["applications"], second["applications"]), (4, 3))
        self.assertEqual(second["skipped_absent_entries"], ["calculator.json"])
        self.assertNotIn("calculator.json", b)
        for name in ("settings.elf", "launcher.elf"):
            self.assertEqual(first["elf_sha256"][name], second["elf_sha256"][name])
            self.assertEqual(a[name], b[name])

    def test_input_immutable_on_success_and_late_failure(self):
        before = copy.deepcopy((self.store, self.profile))
        compose(self.store, self.profile)
        self.assertEqual((self.store, self.profile), before)
        def full(value):
            value["requires"] = [dict(capability="cap" + str(n), api=1) for n in range(16)]
        replace(self.store, "settings.json", full)
        before = copy.deepcopy((self.store, self.profile))
        with self.assertRaisesRegex(ValueError, "capacity"):
            compose(self.store, self.profile)
        self.assertEqual((self.store, self.profile), before)

    def test_idempotence_preserves_second_pass_bytes(self):
        once, _ = compose(self.store, self.profile)
        twice, report = compose(once, self.profile)
        self.assertEqual(once, twice)
        self.assertEqual(report["changed_files"], [])

    def test_noop_keeps_noncanonical_json_bytes(self):
        out, report = compose(self.store, dict(schema=1, entries=[], consumers=[]))
        self.assertEqual(self.store, out)
        self.assertEqual(report["changed_files"], [])

    def test_metadata_already_equal_keeps_original_json_bytes(self):
        for item in self.profile["entries"]:
            replace(self.store, item["manifest"], lambda value, item=item: value.update({k: v for k, v in item.items() if k not in ("manifest", "id")}))
        self.profile["consumers"] = []
        self.store["boot.json"] += b" \n"
        out, report = compose(self.store, self.profile)
        self.assertEqual(self.store, out)
        self.assertEqual(report["changed_files"], [])

    def test_existing_legacy_descriptive_values_are_preserved_until_selected(self):
        replace(self.store, "calculator.json", lambda m: m.update(display_name=42, icon=None))
        self.profile = dict(schema=1, entries=[], consumers=[])
        out, report = compose(self.store, self.profile)
        self.assertEqual(out, self.store)
        self.assertEqual(report["changed_files"], [])
        self.profile["entries"] = [dict(manifest="calculator.json", id="calculator", display_name="Calculator", icon="solid:f1ec")]
        out, _ = compose(self.store, self.profile)
        self.assertEqual(document(out["calculator.json"])["display_name"], "Calculator")
        replace(self.store, "calculator.json", lambda m: m.update(launcher_visible="false"))
        with self.assertRaisesRegex(ValueError, "boolean"):
            compose(self.store, self.profile)

    def test_hidden_home_launcher_and_default_visible(self):
        out, _ = compose(self.store, self.profile)
        for name in ("home", "launcher"):
            self.assertIs(document(out[name + ".json"])["launcher_visible"], False)
        for name in ("settings", "calculator"):
            self.assertNotIn("launcher_visible", document(out[name + ".json"]))
        self.profile["entries"][2]["launcher_visible"] = True
        out, _ = compose(self.store, self.profile)
        self.assertIs(document(out["settings.json"])["launcher_visible"], True)

    def test_missing_elf_does_not_fabricate_availability_or_payload(self):
        del self.store["calculator.elf"]
        replace(self.store, "boot.json", lambda b: b["app_capabilities"][-1].update(image_source={"volume_instance": 42, "path": "packages/calc.elf"}))
        out, _ = compose(self.store, self.profile)
        self.assertNotIn("calculator.elf", out)
        self.assertNotIn("available", document(out["calculator.json"]))
        self.assertEqual(document(out["boot.json"])["app_capabilities"][-1]["image_source"], {"volume_instance": 42, "path": "packages/calc.elf"})

    def test_unknown_consumer_rejected_even_with_absent_presentation_entry(self):
        self.profile["entries"].append(dict(manifest="absent.json", id="absent", display_name="Absent"))
        self.profile["consumers"].append("absent.json")
        with self.assertRaisesRegex(ValueError, "Unknown catalog consumer"):
            compose(self.store, self.profile)

    def test_absent_profile_identity_cannot_alias_installed_app(self):
        self.profile["entries"] = [dict(manifest="other.json", id="settings", icon="solid:f013")]
        with self.assertRaisesRegex(ValueError, "conflicts"):
            compose(self.store, self.profile)

    def test_unadmitted_existing_manifest_is_not_silently_selected(self):
        self.store["orphan.json"] = encoded(manifest("orphan", "orphan.elf"))
        self.profile["entries"].append(dict(manifest="orphan.json", id="orphan", display_name="Orphan"))
        with self.assertRaisesRegex(ValueError, "conflicts"):
            compose(self.store, self.profile)

    def test_identity_mismatch_rejected(self):
        self.profile["entries"][2]["id"] = "other"
        with self.assertRaisesRegex(ValueError, "identity mismatch"):
            compose(self.store, self.profile)

    def test_duplicate_profile_id_path_and_consumer_rejected(self):
        for change in (lambda p: p["entries"].append(p["entries"][0]),
                       lambda p: p["entries"].append(dict(manifest="other.json", id="home")),
                       lambda p: p["consumers"].append(p["consumers"][0])):
            with self.subTest(change=change):
                profile = copy.deepcopy(self.profile)
                change(profile)
                with self.assertRaisesRegex(ValueError, "Duplicate"):
                    compose(self.store, profile)

    def test_duplicate_inventory_manifest_identity_and_elf_rejected(self):
        for edit in (lambda s: replace(s, "boot.json", lambda b: b["app_capabilities"].append(b["app_capabilities"][0])),
                     lambda s: replace(s, "calculator.json", lambda m: m.update(id="settings")),
                     lambda s: replace(s, "calculator.json", lambda m: m.update(file_name="settings.elf"))):
            with self.subTest(edit=edit):
                store = copy.deepcopy(self.store)
                edit(store)
                with self.assertRaisesRegex(ValueError, "Duplicate"):
                    compose(store, self.profile)

    def test_malformed_identity_and_artifact_rejected(self):
        for field, values in {"id": [None, 1, "", "UPPER", "-prefix", "trailing-", "a" * 64, "a/b"],
                              "file_name": [None, "x", "a..elf", "../x.elf", "a/x.elf", "a\\x.elf", "a" * 124 + ".elf"],
                              "version": [None, "1", "1.2.3.4", "1.2.4294967296"]}.items():
            for value in values:
                with self.subTest(field=field, value=value):
                    store = copy.deepcopy(self.store)
                    replace(store, "settings.json", lambda m: m.update({field: value}))
                    with self.assertRaises(ValueError):
                        compose(store, self.profile)

    def test_runtime_package_identity_legal_boundary_values_preserved(self):
        # PackageIdentity.h::safeId accepts lowercase ASCII alphanumeric ends,
        # arbitrary internal hyphens/underscores, and at most 63 bytes.
        for app_id in ("a", "0", "1settings", "a-b", "a_b", "a__--b", "a" * 63, "9" * 63):
            with self.subTest(app_id=app_id):
                store, profile = copy.deepcopy((self.store, self.profile))
                replace(store, "settings.json", lambda m: m.update(id=app_id))
                profile["entries"][2]["id"] = app_id
                out, _ = compose(store, profile)
                self.assertEqual(document(out["settings.json"])["id"], app_id)

    def test_paths_reject_traversal_aliases_controls_and_overflow(self):
        for value in (None, 1, "", ".", "..", "../a", "/a", "a/../b", "a/./b", "a//b", "a/", "a\\b", "a:b", "a\0b", "a\nb", "a" * 193):
            with self.subTest(value=value), self.assertRaises(ValueError):
                safe_path(value)
        self.assertEqual(safe_path("apps/nested/settings.json"), "apps/nested/settings.json")

    def test_nested_and_long_paths_preserved_without_truncation(self):
        prefix = "nested/" + "p" * 115 + "/"
        for suffix in ("json", "elf"):
            self.store[prefix + "settings." + suffix] = self.store.pop("settings." + suffix)
        replace(self.store, "boot.json", lambda b: b["app_capabilities"][2].update(manifest=prefix + "settings.json"))
        self.profile["entries"][2]["manifest"] = prefix + "settings.json"
        self.profile["consumers"][0] = prefix + "settings.json"
        out, _ = compose(self.store, self.profile)
        self.assertIn(prefix + "settings.elf", out)
        self.assertEqual(document(out[prefix + "settings.json"])["file_name"], "settings.elf")
        self.assertEqual(document(out["boot.json"])["app_capabilities"][2]["manifest"], prefix + "settings.json")

    def test_typed_bounded_utf8_presentation(self):
        for field, values in {"display_name": [None, True, 4, "", "a" * 96, "é" * 48, "x\ny", "\ud800"],
                              "icon": [None, False, 3, "", "a" * 24, "é" * 12, "bad\x7f"],
                              "launcher_visible": [None, 0, 1, "false", [], {}]}.items():
            for value in values:
                with self.subTest(field=field, value=repr(value)):
                    profile = copy.deepcopy(self.profile)
                    profile["entries"][2][field] = value
                    with self.assertRaises(ValueError):
                        compose(self.store, profile)
        self.profile["entries"][2].update(display_name="é" * 47 + "a", icon="é" * 11 + "a", launcher_visible=False)
        out, _ = compose(self.store, self.profile)
        self.assertEqual(document(out["settings.json"])["display_name"], "é" * 47 + "a")

    def test_absent_entries_are_still_fully_validated(self):
        self.profile["entries"].append(dict(manifest="absent.json", id="absent", launcher_visible="false"))
        with self.assertRaisesRegex(ValueError, "boolean"):
            compose(self.store, self.profile)

    def test_profile_cannot_change_identity_authority_or_preserved_fields(self):
        for field in ("file_name", "version", "requires", "grants", "namespace", "available", "min_firmware_version"):
            with self.subTest(field=field):
                profile = copy.deepcopy(self.profile)
                profile["entries"][2][field] = "forbidden"
                with self.assertRaisesRegex(ValueError, "fields"):
                    compose(self.store, profile)

    def test_signed_metadata_and_detached_signatures_refused(self):
        for name in ("boot.json", "settings.json"):
            for key in ("signed", "signature", "signatures", "signed_metadata", "signature_algorithm", "signing_key_id"):
                with self.subTest(name=name, key=key):
                    store = copy.deepcopy(self.store)
                    replace(store, name, lambda m: m.update({key: None}))
                    with self.assertRaisesRegex(ValueError, "Signed metadata"):
                        compose(store, self.profile)
        for name in ("settings.json.sig", "boot.signature", "signature.json", "nested/signatures.json"):
            store = {**self.store, name: b"signed"}
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, "Signed metadata"):
                compose(store, self.profile)

    def test_duplicate_json_nested_fields_and_invalid_documents_refused(self):
        for raw in (b'{"id":1,"id":2}', b'{"nested":{"id":1,"id":2}}', b'{"v":NaN}', b'{"v":Infinity}', b'[]', b'\xff'):
            with self.subTest(raw=raw), self.assertRaises(ValueError):
                document(raw)
        with self.assertRaisesRegex(ValueError, "too large"):
            document(b" " * (MAX_DOCUMENT_BYTES + 1))

    def test_duplicate_requirements_grants_and_bad_catalog_authority(self):
        for target in ("requires", "grants"):
            store = copy.deepcopy(self.store)
            if target == "requires":
                replace(store, "settings.json", lambda m: m["requires"].append(m["requires"][0]))
            else:
                replace(store, "boot.json", lambda b: b["app_capabilities"][2]["grants"].append(b["app_capabilities"][2]["grants"][0]))
            with self.subTest(target=target), self.assertRaisesRegex(ValueError, "Duplicate"):
                compose(store, self.profile)
        for value in ({"capability": "app.catalog", "api": 2}, {"capability": "app.catalog", "api": True}):
            store = copy.deepcopy(self.store)
            replace(store, "settings.json", lambda m: m["requires"].append(value))
            with self.subTest(value=value), self.assertRaises(ValueError):
                compose(store, self.profile)
        for instance in (1, True, -1):
            store = copy.deepcopy(self.store)
            replace(store, "boot.json", lambda b: b["app_capabilities"][2]["grants"].append({**GRANT, "instance_id": instance}))
            with self.subTest(instance=instance), self.assertRaises(ValueError):
                compose(store, self.profile)

    def test_preserves_existing_catalog_grants_on_unselected_apps(self):
        replace(self.store, "home.json", lambda m: m["requires"].append(REQUIREMENT))
        replace(self.store, "boot.json", lambda b: b["app_capabilities"][0]["grants"].append(GRANT))
        self.profile = dict(schema=1, entries=[], consumers=[])
        out, report = compose(self.store, self.profile)
        self.assertEqual(out, self.store)
        self.assertEqual(report["changed_files"], [])

    def test_native_capacities_and_append_only_limits(self):
        for kwargs in ({"requirement_capacity": 15}, {"requirement_capacity": True}, {"grant_capacity": 19}, {"grant_capacity": None}):
            with self.subTest(kwargs=kwargs), self.assertRaises(ValueError):
                compose(self.store, self.profile, **kwargs)
        replace(self.store, "settings.json", lambda m: m.update(requires=[{"capability": "cap" + str(n), "api": 1} for n in range(16)]))
        with self.assertRaisesRegex(ValueError, "requirement capacity"):
            compose(self.store, self.profile)
        out, _ = compose(self.store, self.profile, requirement_capacity=17)
        self.assertEqual(len(document(out["settings.json"])["requires"]), 17)
        replace(self.store, "boot.json", lambda b: b["app_capabilities"][2].update(grants=[{"capability": "cap" + str(n), "api": 1, "instance_id": n} for n in range(16)]))
        with self.assertRaisesRegex(ValueError, "grant capacity"):
            compose(self.store, self.profile, requirement_capacity=17)
        compose(self.store, self.profile, requirement_capacity=17, grant_capacity=17)

    def test_explicit_32_row_native_capacities_without_expanding_smaller_profiles(self):
        requests = [{"capability": "cap" + str(n), "api": 1} for n in range(31)]
        grants = [{**request, "instance_id": n} for n, request in enumerate(requests)]
        replace(self.store, "settings.json", lambda m: m.update(requires=requests))
        replace(self.store, "boot.json", lambda b: b["app_capabilities"][2].update(grants=grants))
        for requirement_capacity, grant_capacity in ((16, 16), (24, 24), (24, 32), (32, 24)):
            with self.subTest(requirement_capacity=requirement_capacity, grant_capacity=grant_capacity):
                with self.assertRaisesRegex(ValueError, "capacity"):
                    compose(self.store, self.profile, requirement_capacity, grant_capacity)
        out, report = compose(self.store, self.profile, 32, 32)
        self.assertEqual(document(out["settings.json"])["requires"], requests + [REQUIREMENT])
        self.assertEqual(document(out["boot.json"])["app_capabilities"][2]["grants"], grants + [GRANT])
        self.assertEqual((report["requirement_capacity"], report["grant_capacity"]), (32, 32))
        again, report = compose(out, self.profile, 32, 32)
        self.assertEqual(again, out)
        self.assertEqual(report["changed_files"], [])
        for filename in ("settings.elf", "launcher.elf"):
            self.assertEqual(out[filename], self.store[filename])
        replace(self.store, "settings.json", lambda m: m["requires"].append({"capability": "cap31", "api": 1}))
        with self.assertRaisesRegex(ValueError, "requirement capacity"):
            compose(self.store, self.profile, 32, 32)
        replace(self.store, "settings.json", lambda m: m["requires"].pop())
        replace(self.store, "boot.json", lambda b: b["app_capabilities"][2]["grants"].append({"capability": "cap31", "api": 1, "instance_id": 31}))
        with self.assertRaisesRegex(ValueError, "grant capacity"):
            compose(self.store, self.profile, 32, 32)

    def test_staged_document_size_bound_checked_before_output(self):
        value = document(self.store["settings.json"])
        value["description"] = "x" * 65100
        self.store["settings.json"] = json.dumps(value, separators=(",", ":")).encode()
        self.assertLessEqual(len(self.store["settings.json"]), MAX_DOCUMENT_BYTES)
        with self.assertRaisesRegex(ValueError, "Staged metadata document is too large"):
            compose(self.store, self.profile)

    def test_existing_catalog_authority_at_capacity_needs_no_extra_slot(self):
        replace(self.store, "settings.json", lambda m: m.update(requires=[{"capability": "cap" + str(n), "api": 1} for n in range(15)] + [REQUIREMENT]))
        replace(self.store, "boot.json", lambda b: b["app_capabilities"][2].update(grants=[{"capability": "cap" + str(n), "api": 1, "instance_id": n} for n in range(15)] + [GRANT]))
        out, _ = compose(self.store, self.profile)
        self.assertEqual(document(out["settings.json"])["requires"], document(self.store["settings.json"])["requires"])
        self.assertEqual(document(out["boot.json"])["app_capabilities"][2]["grants"], document(self.store["boot.json"])["app_capabilities"][2]["grants"])

    def test_empty_inventory_valid_and_oversized_inputs_rejected(self):
        empty = {"boot.json": b'{"app_capabilities":[]}'}
        out, report = compose(empty, dict(schema=1, entries=[dict(manifest="absent.json", id="absent")], consumers=[]))
        self.assertEqual(out, empty)
        self.assertEqual(report["applications"], 0)
        self.assertEqual(report["skipped_absent_entries"], ["absent.json"])
        for field, value in (("schema", True), ("schema", 2), ("entries", {}), ("consumers", {}),
                             ("entries", [{}] * (MAX_APPS + 1)), ("consumers", ["x"] * (MAX_APPS + 1))):
            profile = {**self.profile, field: value}
            with self.subTest(field=field, value=str(value)[:20]), self.assertRaises(ValueError):
                compose(self.store, profile)

    def test_missing_malformed_manifest_and_path_collision(self):
        for edit in (lambda s: s.pop("settings.json"), lambda s: s.update({"settings.json": b"null"}),
                     lambda s: s.update({"user.bin/nested": b"invalid"}),
                     lambda s: s.update({"unsafe/../alias": b"invalid"})):
            store = copy.deepcopy(self.store)
            edit(store)
            with self.subTest(edit=edit), self.assertRaises(ValueError):
                compose(store, self.profile)

    def test_new_output_only_and_symlinks_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "input"
            write_new_store(root, self.store)
            self.assertEqual(read_store(root), self.store)
            with self.assertRaisesRegex(ValueError, "already exists"):
                write_new_store(root, self.store)
            link = Path(tmp) / "link"
            link.symlink_to(root, target_is_directory=True)
            with self.assertRaisesRegex(ValueError, "linked"):
                read_store(link)
            (root / "alias").symlink_to(root / "settings.json")
            with self.assertRaisesRegex(ValueError, "Linked"):
                read_store(root)
            dangling = Path(tmp) / "dangling"
            dangling.symlink_to(Path(tmp) / "missing")
            with self.assertRaisesRegex(ValueError, "already exists"):
                write_new_store(dangling, self.store)

    def test_cli_atomic_output_report_idempotence_and_refusal(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source, profile, output = root / "source", root / "profile.json", root / "output"
            write_new_store(source, self.store)
            profile.write_bytes(encoded(self.profile))
            def run(destination, input_store=source, hash_seed="1", requirement_capacity=16, grant_capacity=16):
                return subprocess.run([sys.executable, str(ROOT / "scripts/stage_app_catalog_metadata.py"),
                    "--store", str(input_store), "--profile", str(profile), "--output", str(destination),
                    "--requirement-capacity", str(requirement_capacity), "--grant-capacity", str(grant_capacity)], capture_output=True, text=True, env={**os.environ, "PYTHONHASHSEED": hash_seed})
            result = run(output)
            self.assertEqual(result.returncode, 0, result.stderr)
            report = json.loads(result.stdout)
            self.assertEqual(report["applications"], 4)
            self.assertEqual(read_store(source), self.store)
            result = run(root / "different-seed", hash_seed="2")
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(read_store(output), read_store(root / "different-seed"))
            result = run(root / "capacity-32", requirement_capacity=32, grant_capacity=32)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(read_store(output), read_store(root / "capacity-32"))
            self.assertEqual(json.loads(result.stdout)["grant_capacity"], 32)
            result = run(root / "second", output)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(json.loads(result.stdout)["changed_files"], [])
            result = run(output)
            self.assertNotEqual(result.returncode, 0)
            result = run(source / "nested")
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse((source / "nested").exists())
            profile.write_bytes(encoded({**self.profile, "consumers": ["missing.json"]}))
            result = run(root / "refused")
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse((root / "refused").exists())
            self.assertEqual(list(root.glob(".catalog-stage-*")), [])

    def test_original_23_entry_presentation_from_system_fixture(self):
        if SYSTEM is None:
            self.skipTest("Pass --system PATH to verify the canonical System 23-entry fixture")
        source = SYSTEM / "test/native_apps/fixtures/watch_springboard_catalog_23.c"
        rows = re.findall(r'\.display_name="([^"]+)",\.file_name="([^"]+)",\.icon="([^"]+)"', source.read_text())
        self.assertEqual(len(rows), 23)
        names = [filename[:-4] for _, filename, _ in rows]
        store, _ = fixture(("home", "launcher", *names))
        profile = dict(schema=1, entries=[dict(manifest="home.json", id="home", launcher_visible=False),
                                        dict(manifest="launcher.json", id="launcher", launcher_visible=False)],
                       consumers=["settings.json", "launcher.json"])
        for display_name, filename, icon in rows:
            stem = filename[:-4]
            profile["entries"].append(dict(manifest=stem + ".json", id=stem.replace("_", "-"), display_name=display_name, icon=icon))
        out, report = compose(store, profile)
        actual = []
        for policy in document(out["boot.json"])["app_capabilities"]:
            value = document(out[policy["manifest"]])
            if value.get("launcher_visible", True):
                actual.append((value["display_name"], value["file_name"], value["icon"]))
        self.assertEqual(actual, rows)
        self.assertEqual(report["applications"], 25)
        subset, _ = fixture(("home", "launcher", "settings"))
        _, other = compose(subset, profile)
        self.assertEqual(len(other["skipped_absent_entries"]), 22)
        for filename in ("settings.elf", "launcher.elf"):
            self.assertEqual(report["elf_sha256"][filename], other["elf_sha256"][filename])
        print("\nCatalog provisioning receipt: " + json.dumps({"original_entries": len(rows), "fixture_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
              "inventories": [report["applications"], other["applications"]],
              "settings_sha256": report["elf_sha256"]["settings.elf"],
              "launcher_sha256": report["elf_sha256"]["launcher.elf"], "synthetic_elf_bytes_only": True}, sort_keys=True))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--system", type=Path)
    args, remaining = parser.parse_known_args()
    SYSTEM = args.system
    unittest.main(argv=[sys.argv[0], *remaining])
