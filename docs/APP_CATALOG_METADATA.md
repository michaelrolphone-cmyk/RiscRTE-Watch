# Installed application presentation metadata

`scripts/stage_app_catalog_metadata.py` is a generic, source-only provisioning
step for `app.catalog@1`. A product supplies presentation data for existing
application manifests plus an explicit list of catalog consumers. The helper
never compiles or generates a catalog, discovers application directories, adds
applications, chooses storage media, or changes an ELF. Native Runtime admission
owns the installed directory inventory, launch identities and availability.
Settings and Springboard can therefore reuse the same ELF bytes across products.

## Schema 1

```json
{
  "schema": 1,
  "entries": [
    {"manifest": "default.json", "id": "existing-home-id", "launcher_visible": false},
    {"manifest": "launcher.json", "id": "existing-launcher-id", "launcher_visible": false},
    {"manifest": "settings.json", "id": "existing-settings-id", "display_name": "Settings", "icon": "solid:f013"},
    {"manifest": "calculator.json", "id": "existing-calculator-id", "display_name": "Calculator", "icon": "solid:f1ec"}
  ],
  "consumers": ["settings.json", "launcher.json"]
}
```

The example's IDs and manifest paths must be replaced with the exact existing
product identities. `entries` and `consumers` are required arrays, each with at
most 128 rows; empty arrays are valid. Unknown schema/entry fields are rejected.

Each entry requires a unique `manifest` path and unique `id`. Optional fields:

- `display_name`: nonempty UTF-8 string, at most 95 encoded bytes
- `icon`: nonempty UTF-8 string, at most 23 encoded bytes
- `launcher_visible`: JSON boolean; absent values remain untouched and Runtime
  defaults an absent manifest value to true

Strings cannot contain ASCII control characters, DEL or invalid Unicode.
Byte bounds are checked before modification; strings are never truncated.
Preexisting descriptive values remain untouched unless explicitly replaced,
including legacy values handled by the Runtime's display fallback. Existing
`launcher_visible` values must be boolean because they control membership.
Hiding an app does not revoke or change its admission, launch route or authority.

Inventory comes only from the existing `boot.json` `app_capabilities` policies.
A presentation entry absent from that inventory is reported and skipped. This
allows a shared product configuration with apps absent from a smaller product.
It cannot create a manifest, ELF, inventory row or availability claim. A matching
ID at a different path, or an unadmitted file occupying the requested path, is a
conflict. Consumers must already exist in the inventory; unknown consumers fail.

Paths are normalized relative POSIX paths, at most 192 UTF-8 bytes, with no
traversal, absolute paths, control characters, backslashes or colons. IDs follow
Runtime package identity syntax (1–63 lowercase ASCII letters/digits, hyphens
and underscores; first and last characters are alphanumeric). Executable names
remain the existing safe `.elf` basenames, at most 127 bytes. Directory-qualified
launch identities are never shortened; final Runtime admission checks native
route limits. Duplicate manifest paths, IDs and resolved ELF paths are rejected.

## Run before final device assembly

```sh
python3 scripts/stage_app_catalog_metadata.py \
  --store ../product/build/unsealed-store \
  --profile ../product/profiles/app-presentation.json \
  --requirement-capacity 16 --grant-capacity 16 \
  --output ../product/build/catalog-staged-store
```

Capacity arguments must describe the selected native build. Supported native
requirement capacities are 16, 17, 24 and 32; grant capacities are 16, 17, 18, 24
and 32. These are supported build-selected bounds, not permission to raise a
product's actual native capacity. Both arguments must match its existing build.
The helper checks capacity before appending one `app.catalog@1` requirement and
one instance-0 grant to each selected consumer. Existing identical entries are
reused; conflicting catalog APIs/instances and duplicate declarations fail.
It preserves all existing grants, positive namespace selectors, shared-file
paths, opaque policy selections and unrelated manifest fields. It does not
revoke catalog grants already present on consumers outside this selection.

The input remains unchanged. Output must be a new directory outside the input;
validation completes before output creation. Symlinks, nonregular files and
file/directory collisions fail. Stores are bounded to 4,096 files / 64 MiB and
metadata documents to the Runtime's 64 KiB read bound. Existing JSON bytes are
retained exactly when their contents need no change. A repeat run is byte-for-byte
idempotent. No source/destination file is added to the store's inventory.

Only unsigned metadata is supported. Signature fields in boot/app metadata and
recognized detached `.sig`, `.signature`, `signature.json`, `signatures.json`
or `signed.json` files are refused, even if the requested operation is a no-op.
This is not a signature verifier or resealing tool. Signed formats must first be
regenerated through their signing owner. The output receipt always declares
`flashable: false` and `requires_device_assembly: true`; existing cohort metadata
is preserved and reported as requiring rebind. Final native admission, any
signatures/cohort/bank bindings, and image construction belong to device assembly.

The JSON receipt reports changed files, absent selections, consumer paths,
inventory count, capacity values and SHA-256 hashes of every unchanged ELF.
It does not claim hardware qualification or runtime availability.

## Source-only regression tests

```sh
python3 tests/test_stage_app_catalog_metadata.py --system ../RiscRTE-System-Apps -v
python3 tests/test_stage_product.py
```

The System checkout must contain
`test/native_apps/fixtures/watch_springboard_catalog_23.c` (original fixture from
System commit `2c3f44dd773e27e3a142462088a96328119a79e7`). The test reads all 23
original names/icons into declarative provisioning data, preserves them in the
resulting manifests, and hides synthetic Home/launcher entries by configuration.
It compares identical synthetic Settings/launcher ELF hashes across inventories
of 25 and 3 applications. No ELF is executed or compiled, no hardware image is
built, and the fixture is never used to generate product C sources. Without
`--system`, the independent tests run and the external-fixture test is skipped.
