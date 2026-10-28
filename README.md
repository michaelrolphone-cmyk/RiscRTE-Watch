# RiscRTE Watch

Canonical shared Home application for ESP32-S3 watches. Device repositories supply their hardware providers, board configuration, provisioning and firmware assembly.

`apps/clock/crown.c` is the maintained entry point, extracted with faces, effects and Points integration from T-Watch source `851b8998e804c57b2c9f42d9f8fd0a69e79f2b41`. Old `main.c` and portable variants are retained historical references, not active products.

## One build, independent capabilities

Home 0.18.0 builds `default.elf` and its `clock.elf` return-to-Home companion without board or resolution defines. These are roles, not board variants. Each board consumes the same bytes for each role.

Capabilities describe functions, never device models. Battery (`board.battery`), power key (`input.power-key`), system sleep (`power.sleep`), frame output (`display.output`) and display sleep (`display.power`) have independent interfaces. Home no longer casts a battery to a private T-Watch PMU table or a framebuffer to panel-specific power methods. Generic contracts and their compatibility adapters are pinned from RiscRTE-Drivers; the adapters, not the application, interpret legacy combined driver tables.

Absent optional services do not fabricate hardware support: no battery gives unknown telemetry, no key gives no key events, and missing sleep/display-power keeps Home awake. An acquired invalid API remains an error. Supported services resolve through capability selection, not fixed physical instance IDs. Existing application IDs and saved settings namespaces remain compatible.

See `docs/CAPABILITY_BOUNDARIES.md` for API and provisioning details.

The persisted `twatch-clock` identity is presented as hidden Home. Device-owned
Clock shortcut visibility stays unchanged. See `docs/LAUNCHER_METADATA.md`.

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
the native frame. The earlier native migration preserved 240px golden artwork. The later shared
AM/PM footer repair deliberately moves only that overlay/background inside
the circular aperture; historical golden files are not rewritten.

See `docs/NATIVE_FACES.md` for coverage and device-validation limits.

## Bringup diagnostics

The hidden Bringup child uses diagnostic-local native geometry and RGB565 raster
coverage at the display capability's dimensions. Its Motion, Vibrate and Back
controls share their painted rectangles with raw-touch hit testing. Optional
diagnostic actions remain lazy and capability-selected. See
[`docs/BRINGUP_NATIVE.md`](docs/BRINGUP_NATIVE.md) for lifecycle guarantees, host
qualification and the unchanged product integration boundary.

## Build and test

Use the compiler and System-Apps/Utilities source revisions in `dependencies.lock.json`:

```sh
python3 -m pip install -r scripts/requirements.txt
python3 scripts/test_capabilities.py
python3 scripts/test_capabilities.py --sanitize
python3 scripts/test_runtime_service_checkpoint.py --system ../RiscRTE-System-Apps --utilities ../RiscRTE-Utilities --output /tmp/home-checkpoint-proof
python3 scripts/test_viewport.py
python3 scripts/test_bringup_native.py --output /tmp/bringup-native
python3 scripts/test_home_native_height.py
python3 scripts/test_native_faces.py
python3 scripts/test_native_effects.py
python3 scripts/check_capability_boundaries.py
python3 scripts/build.py --system ../RiscRTE-System-Apps --utilities ../RiscRTE-Utilities --cc /path/to/xtensa-esp32s3-elf-gcc
```

Home builds can opt into generic fallback defaults without a board-specific app:

```sh
python3 scripts/build.py --system ../RiscRTE-System-Apps --utilities ../RiscRTE-Utilities --cc /path/to/xtensa-esp32s3-elf-gcc --default-sleep-mode deep --default-idle-seconds 15
python3 scripts/test_sleep_defaults.py --system ../RiscRTE-System-Apps --utilities ../RiscRTE-Utilities
```

`--default-sleep-mode` accepts `deep`, `light` or `hybrid`;
`--default-idle-seconds` accepts integer seconds from 5 to 3600. Omit either flag
to retain its Hybrid/60-second default. Valid saved preferences always win,
including an explicitly saved 60-second idle value. Missing, malformed or
unreadable records select the configured fallback without writing storage.
The five-minute Hybrid deep-transition fallback is unchanged. The effective
Home fallbacks are recorded in `build.json`; sleep diagnostics report the
actual fallback and load status. These options do not change hardware wake
support or guarantee physical sleep/wake operation.

The app-local Quick translation unit includes the pinned shared controller and
substitutes only the timer reader, so reloads after Contexts or low-battery work
use the same policy without a second storage read. Dependency sources and
persistence formats are unchanged. These are Home defaults, not a rewrite of
System Settings: its missing-record display may retain its own default until a
user saves an explicit preference.

For a deployment whose clock authority is Runtime's native UTC service rather
than an external RTC, add `--time-authority native`. The default remains
`external-rtc` with the existing UTC+08 RTC reconciliation and Denver display
policy. Native mode:

- Reads and validates Runtime snapshots at startup and after light sleep;
  preserves explicit UNSET and the Runtime owner/custody fence.
- Never acquires/reads an external RTC. Without an explicit unset-time default
  it never calls native seed. Its generated Home
  manifest omits `rtc.clock`; other independently declared requirements remain.
- Converts UTC directly using the shared Settings `time_zone` preference in
  namespace 1, loaded before the first face and again on light wake. Missing
  means the shared virtual UTC default. Invalid/unavailable zone records are
  visibly unconfirmed rather than silently applying Denver.
- Does not compile the legacy UTC+08/Denver policy. Display and Points civil
  dates use the selected local zone. Valid time is not a claim of clock accuracy
  or battery-backed cold-start persistence.

Run the focused native-authority qualification:

```sh
python3 scripts/test_native_time_authority.py --system ../RiscRTE-System-Apps --utilities ../RiscRTE-Utilities
```

It exercises real Home startup/wake paths, snapshot malformation/custody checks
and exact UTC, Denver, Tokyo and DST expectations.

CI checks public header parity, all 16 independent service-presence combinations, base-only battery tables, generic sleep restoration, arbitrary motion profiles, malformed APIs and grant cleanup. It also exercises the viewport and compatibility providers with ASan/UBSan, then builds both app roles and all three adapters against the actual ELF loader validator. Builds do not modify repository sources.

## Integration status

This is component source/build integration, not a complete device firmware release. Both device packages must stage the app, supported capability subset, providers and grants before rebuilding paired metadata; replacing an ELF inside a sealed image is not sufficient. The new compatibility providers also need native provider capacity. Full Waveshare peripheral parity, complete device provisioning, firmware assembly and physical qualification remain unfinished. Existing device firmware/release selections have not been switched.

### Explicit initial time for native-only products

`--time-authority native --unset-time-local 2026-01-01T00:00:00` explicitly
selects midnight January 1, 2026 as an initial display value, not synchronized
current time. Home seeds only a validated UNSET Runtime clock, after loading
the saved shared time zone (missing means UTC), and checks native readback.
A valid Settings update or retained sleep clock is never overwritten on Home
reentry or wake. Invalid/unavailable zone data is not silently replaced.
Other products retain their prior no-seed default unless they opt in.

To set actual time, open Settings, choose Time Zone, then Set Time, edit the
calendar fields, and Save. Settings verifies Runtime UTC readback; returning
Home uses the same zone. The ESP32 RTC-backed timer advances through admitted
deep sleep. A cold power cycle or ordinary software reset currently discards
native validity and this selected default is used again. No flash timestamp
can recover elapsed time while the RTC loses power. Wi-Fi time synchronization
is not implemented by this option.
