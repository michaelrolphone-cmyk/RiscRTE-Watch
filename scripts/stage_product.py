#!/usr/bin/env python3
"""Stage one shared Home build into a device-owned, unsealed firmware store.

Physical providers, capability bindings and optional services come from the
selected device profile. No app is compiled here and no live device is modified.
The device assembler subsequently binds native/cohort metadata and builds flash.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import tempfile
from typing import Any

Store = dict[str, bytes]
ROLES = ("default", "clock")
DEVICE_CAP = re.compile(r"(^|[._-])(x4|x4pro|xteink|twatch|t-watch|lilygo|waveshare|wavshare)([._-]|$)", re.I)


def require(ok: bool, message: str) -> None:
    if not ok:
        raise ValueError(message)


def sha(raw: bytes) -> str:
    return hashlib.sha256(raw).hexdigest()


def document(raw: bytes) -> Any:
    def unique(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
        out: dict[str, Any] = {}
        for key, value in pairs:
            require(key not in out, "Duplicate JSON field: " + key)
            out[key] = value
        return out
    return json.loads(raw, object_pairs_hook=unique)


def encoded(value: Any) -> bytes:
    return (json.dumps(value, indent=2, sort_keys=True) + "\n").encode()


def safe_path(name: str) -> str:
    require(isinstance(name, str) and bool(name) and name.isascii(), "Invalid store path")
    path = PurePosixPath(name)
    require(not path.is_absolute() and str(path) == name and ".." not in path.parts
            and "\\" not in name and "\0" not in name
            and len(("/" + name).encode()) < 32, "Invalid SPIFFS path: " + name)
    return name


def read_store(root: Path) -> Store:
    require(root.is_dir() and not root.is_symlink(), "Store directory is missing or linked")
    out: Store = {}
    for path in root.rglob("*"):
        require(not path.is_symlink(), "Linked store input: " + str(path))
        if path.is_file():
            out[safe_path(path.relative_to(root).as_posix())] = path.read_bytes()
    return out


def elf(raw: bytes, name: str) -> None:
    # Detailed relocations/import checks remain the real Runtime validator's job.
    require(len(raw) >= 52 and raw[:7] == b"\x7fELF\x01\x01\x01"
            and raw[16:20] == b"\x03\x00\x5e\x00", "Not an ESP32-S3 shared ELF: " + name)


def app_bundle(root: Path) -> tuple[Store, dict[str, Any]]:
    record = document((root / "build.json").read_bytes())
    require(record.get("build_variant") == "shared-no-board-defines", "Not a shared Home build")
    out: Store = {}
    for role in ROLES:
        name = role + ".elf"
        raw = (root / name).read_bytes()
        elf(raw, name)
        meta = record["artifacts"][role]
        require(meta["file_name"] == name and meta["bytes"] == len(raw)
                and meta["sha256"] == sha(raw), "App build receipt mismatch: " + role)
        manifest = (root / (role + ".json")).read_bytes()
        value = document(manifest)
        require(value["file_name"] == name and value["entry"] == "app_main"
                and value["type"] == "application", "Invalid app manifest: " + role)
        out[name], out[role + ".json"] = raw, manifest
    require(document(out["clock.json"])["requires"] == []
            and record["artifacts"]["clock"].get("role") == "return-to-default",
            "clock.elf must be the shared return module, not a second Home implementation")
    return out, record


def request_key(value: dict[str, Any]) -> tuple[str, int]:
    cap, api = value["capability"], value["api"]
    require(isinstance(cap, str) and not DEVICE_CAP.search(cap), "Device-specific capability: " + str(cap))
    require(type(api) is int and api > 0, "Invalid capability API")
    return cap, api


def stage(store: Store, apps: Store, services: Store, profile: dict[str, Any],
          *, provider_capacity: int) -> tuple[Store, dict[str, Any]]:
    """Pure composition. The source store and unrelated application data stay intact."""
    require(profile.get("schema") == 1, "Unsupported shared-Home profile schema")
    out = dict(store)
    boot = document(store["boot.json"])
    before = copy.deepcopy(boot)
    board = document(store[safe_path(boot["board"])])
    require(board["board_id"] == profile["board_id"], "Profile does not match the device board")
    require(boot["default_app"] == "default.elf", "Expected the existing default.elf entry point")
    rows = {row["manifest"]: row for row in boot["app_capabilities"]}
    require(len(rows) == len(boot["app_capabilities"]), "Duplicate application policy")
    require(all(role + ".json" in rows for role in ROLES), "Both Home role policies are required")
    for role in ROLES:
        elf(apps[role + ".elf"], role)
    require(document(apps["clock.json"])["requires"] == [], "Return module must need no services")

    provided: dict[tuple[str, int], list[str]] = {}
    for selection in boot["drivers"]:
        path = safe_path(selection["manifest"])
        for capability in document(store[path])["provides"]:
            provided.setdefault(request_key(capability), []).append(path)
    extra: list[dict[str, Any]] = []
    for selection in profile.get("services", []):
        directory, artifact = safe_path(selection["directory"]), safe_path(selection["artifact"])
        source_manifest = safe_path(artifact + "/manifest.json")
        source_elf = safe_path(artifact + "/driver.elf")
        manifest = document(services[source_manifest])
        require(manifest["type"] == "driver" and manifest["driver_abi"] == 2
                and manifest["architecture"] == "xtensa-esp32s3" and manifest["file_name"] == "driver.elf"
                and len(manifest["provides"]) == 1, "Invalid selected service artifact")
        cap, api = request_key(manifest["provides"][0])
        require((cap, api) == request_key(selection), "Service provides a different function")
        require((cap, api) in {request_key(r) for r in document(apps["default.json"])["requires"]},
                "Home does not declare selected service: " + cap)
        target_manifest, target_elf = safe_path(directory + "/manifest.json"), safe_path(directory + "/driver.elf")
        raw = services[source_elf]
        elf(raw, source_elf)
        existing = provided.get((cap, api), [])
        if existing:
            require(existing == [target_manifest]
                    and store.get(target_manifest) == services[source_manifest]
                    and store.get(target_elf) == raw, "Conflicting service implementation: " + cap)
        else:
            require(target_manifest not in out and target_elf not in out, "Service path already occupied")
            out[target_manifest], out[target_elf] = services[source_manifest], raw
            boot["drivers"].append({"manifest": target_manifest})
            provided[(cap, api)] = [target_manifest]
        extra.append({"capability": cap, "api": api, "instance_id": 0})

    require(type(provider_capacity) is int and len(boot["drivers"]) <= provider_capacity,
            f"Native needs {len(boot['drivers'])} provider slots; only {provider_capacity} supplied")
    default_row = rows["default.json"]
    for grant in extra:
        if grant not in default_row["grants"]:
            default_row["grants"].append(grant)
    keys: set[tuple[str, int, int]] = set()
    declared: list[dict[str, Any]] = []
    vocabulary: set[tuple[str, int]] = set()
    for grant in default_row["grants"]:
        cap, api = request_key(grant)
        instance = grant["instance_id"]
        require(type(instance) is int and 0 <= instance <= 0x7fffffff, "Invalid instance selector")
        identity = cap, api, instance
        require(identity not in keys, "Duplicate Home grant")
        keys.add(identity)
        if (cap, api) not in vocabulary:
            declared.append({"capability": cap, "api": api})
            vocabulary.add((cap, api))
    require({("display.output", 1), ("input.touch.raw", 1), ("runtime.realtime-control", 1)} <= vocabulary,
            "Missing required Home display/input/time service")
    # Legacy stores have additional generic Contexts data grants; preserve those
    # declarations as well as all original namespace/file selections.
    for role in ROLES:
        name = role + ".json"
        old, new = document(store[name]), document(apps[name])
        require(old["id"] == new["id"] and old["file_name"] == new["file_name"], "Application identity/path changed")
        old.update(version=new["version"], requires=declared if role == "default" else [])
        out[name], out[role + ".elf"] = encoded(old), apps[role + ".elf"]
    # The Runtime restores configured Home after this zero-service child returns.
    # No second copy of Home, duplicate background clients or extra grants exist.
    rows["clock.json"]["grants"] = []
    out["boot.json"] = encoded(boot)
    allowed = {"boot.json", "default.json", "default.elf", "clock.json", "clock.elf"}
    changed = {n for n in store if out[n] != store[n]}
    require(changed <= allowed, "Unrelated store content changed")
    require(len(boot["app_capabilities"]) == len(before["app_capabilities"]), "Application inventory changed")
    require(boot["drivers"][:len(before["drivers"])] == before["drivers"], "Existing provider selection changed")
    report = {
        "schema": 1, "profile": profile["name"], "flashable": False,
        "requires_device_assembly": True, "requires_cohort_rebind": "cohort.json" in store,
        "applications": len(boot["app_capabilities"]),
        "providers_before": len(before["drivers"]), "providers_after": len(boot["drivers"]),
        "provider_capacity": provider_capacity,
        "changed_files": sorted(changed), "added_files": sorted(set(out) - set(store)),
        "unchanged_files": len(store) - len(changed),
        "home_sha256": sha(out["default.elf"]), "return_sha256": sha(out["clock.elf"]),
        "old_return_bytes": len(store["clock.elf"]), "return_bytes": len(out["clock.elf"]),
    }
    return out, report


def write_new_store(path: Path, store: Store) -> None:
    require(not path.exists(), "Output already exists; input is never overwritten")
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix=".watch-stage-", dir=path.parent))
    try:
        for name, raw in store.items():
            target = temporary / safe_path(name)
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(raw)
        temporary.rename(path)
    finally:
        if temporary.exists():
            shutil.rmtree(temporary)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("store", "apps", "profile", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--services", type=Path)
    parser.add_argument("--provider-capacity", type=int, required=True)
    args = parser.parse_args()
    apps, _ = app_bundle(args.apps)
    profile = document(args.profile.read_bytes())
    services = {}
    for selection in profile.get("services", []):
        require(args.services is not None, "Selected services need --services")
        for filename in ("driver.elf", "manifest.json"):
            name = safe_path(selection["artifact"] + "/" + filename)
            path = args.services / name
            require(path.is_file() and not path.is_symlink(), "Service file missing or linked")
            services[name] = path.read_bytes()
    staged, report = stage(read_store(args.store), apps, services, profile,
                           provider_capacity=args.provider_capacity)
    write_new_store(args.output, staged)
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
