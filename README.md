# RiscRTE Watch

Canonical shared Home application for ESP32-S3 watches. Device repositories supply their hardware providers, board configuration, provisioning and firmware assembly.

`apps/clock/crown.c` is the maintained entry point, extracted with faces, effects and Points integration from T-Watch source `851b8998e804c57b2c9f42d9f8fd0a69e79f2b41`. Old `main.c` and portable variants are retained historical references, not active products.

## One build, independent capabilities

Home 0.18.0 builds `default.elf` and its `clock.elf` return-to-Home companion without board or resolution defines. These are roles, not board variants. Each board consumes the same bytes for each role.

Capabilities describe functions, never device models. Battery (`board.battery`), power key (`input.power-key`), system sleep (`power.sleep`), frame output (`display.output`) and display sleep (`display.power`) have independent interfaces. Home no longer casts a battery to a private T-Watch PMU table or a framebuffer to panel-specific power methods. Generic contracts and their compatibility adapters are pinned from RiscRTE-Drivers; the adapters, not the application, interpret legacy combined driver tables.

Absent optional services do not fabricate hardware support: no battery gives unknown telemetry, no key gives no key events, and missing sleep/display-power keeps Home awake. An acquired invalid API remains an error. Supported services resolve through capability selection, not fixed physical instance IDs. Existing application IDs and saved settings namespaces remain compatible.

See `docs/CAPABILITY_BOUNDARIES.md` for API and provisioning details.

## Settled native services

Home optionally calls Runtime's append-only service checkpoint once at a settled
foreground-loop boundary, after startup or the preceding presentation completes
and before acquiring another frame. Older Runtime tables remain compatible and
perform no service work. The callback is never added to presentation polling,
raster callbacks, pacing, or generic yield/BUSY paths. Both watches retain the
same source and build inputs. See `docs/RUNTIME_SERVICE_CHECKPOINT.md`.

## Display and input

Home renders and hit-tests directly in the dimensions reported by display and
touch capabilities. The 240x240 T-Watch and 410x502 Waveshare use the same Home
source and native geometry; there is no completed 240-wide viewport expansion.

All 34 face styles retain their artwork, RTC formatting and Points In Time
behavior. Geometry and glyph coverage are evaluated at destination resolution.
Circular faces use the smaller display axis, centered without distortion; tall
screens retain the face title and live next-Point summary. Picker previews are
rendered at their physical card dimensions and touch uses the same geometry.
Quick controls, alarms, boot effects, sleep blanking and return snapshots share
the native frame. Existing 240px golden images remain unchanged.

See `docs/NATIVE_FACES.md` for coverage and device-validation limits.

## Build and test

Use the compiler and System-Apps/Utilities source revisions in `dependencies.lock.json`:

```sh
python3 -m pip install -r scripts/requirements.txt
python3 scripts/test_capabilities.py
python3 scripts/test_capabilities.py --sanitize
python3 scripts/test_runtime_service_checkpoint.py --system ../RiscRTE-System-Apps --utilities ../RiscRTE-Utilities --output /tmp/home-checkpoint-proof
python3 scripts/test_viewport.py
python3 scripts/test_home_native_height.py
python3 scripts/test_native_faces.py
python3 scripts/test_native_effects.py
python3 scripts/check_capability_boundaries.py
python3 scripts/build.py --system ../RiscRTE-System-Apps --utilities ../RiscRTE-Utilities --cc /path/to/xtensa-esp32s3-elf-gcc
```

CI checks public header parity, all 16 independent service-presence combinations, base-only battery tables, generic sleep restoration, arbitrary motion profiles, malformed APIs and grant cleanup. It also exercises the viewport and compatibility providers with ASan/UBSan, then builds both app roles and all three adapters against the actual ELF loader validator. Builds do not modify repository sources.

## Integration status

This is component source/build integration, not a complete device firmware release. Both device packages must stage the app, supported capability subset, providers and grants before rebuilding paired metadata; replacing an ELF inside a sealed image is not sufficient. The new compatibility providers also need native provider capacity. Full Waveshare peripheral parity, complete device provisioning, firmware assembly and physical qualification remain unfinished. Existing device firmware/release selections have not been switched.
