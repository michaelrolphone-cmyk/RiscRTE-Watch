# Native Bringup diagnostics

## Scope and interfaces

`apps/bringup/main.c` is the hardware-neutral, hidden diagnostic application.
`apps/bringup/native_ui.h` owns its geometry, glyph rasterization and control hit
regions. The production build continues to compile `apps/bringup/main.c` together
with `apps/clock/viewport.c`. No additional translation unit, runtime capability,
board define, framebuffer allocation or viewport scaling path is needed.

- `display.output` v1: native RGB565 dimensions, stride and frame lease;
  asynchronous QUEUED/ACTIVE/COMPLETE/FAILED/SUPERSEDED presentation states.
- `input.touch.raw` v1: native-coordinate events and authoritative snapshots,
  checked against the acquired display dimensions by the shared touch wrapper.
- `board.battery` v1 and `rtc.clock` v2: optional read-only telemetry.
- `motion.accel` v1 sample prefix and `haptic.effect` v1: lazily acquired on a
  visible, completed control tap. Only effect 14 is requested; cleanup stops it.

Shared `WatchViewport`, Home, launcher, capability headers, product dependency
pins, firmware build recipes and released firmware bytes are unchanged.
Consumers must explicitly adopt and qualify this source revision before it can
change a product diagnostic. This change does not upgrade the separately pinned
Waveshare diagnostic or the .64 release.

## Native layout and raster

The same application renders directly into the provider's native lease at
240x240, 410x502, 128x128, 502x410 and 320x172 in host qualification. Back stays
bottom-anchored; Motion and Vibrate occupy the adjacent row above it. The seven
telemetry/status rows divide the available height, without a 240-pixel logical
canvas or completed-frame resampling. The compact layout omits the redundant
version subtitle when needed and retains all telemetry rows and controls.

Glyph size is selected from native dimensions. Each label fits complete glyphs
inside its own rectangle, reducing font size before adding an ellipsis. The
renderer touches visible RGB565 pixels only, including the last row and column;
row padding and memory beyond the declared extent remain untouched. Accepted
layout bounds are 64..2048 pixels wide and 96..2048 pixels high; smaller displays
return safely without a frame. Very narrow displays necessarily ellipsize labels.

## Input and custody

No action is accepted before the first presentation completes. A fresh snapshot
at that boundary suppresses stale events and contacts held while the application
opened. Startup, queue gaps, poll errors and invalid native coordinates reset
press state. Held contacts must clear before a new DOWN can arm a control.

A tap requires a matching contact ID, DOWN and UP inside the same half-open
painted rectangle, and movement smaller than the native slop. Leaving a control
or moving too far cancels it even if the finger later returns. Gutters, margins,
telemetry rows and out-of-display coordinates activate nothing. A second contact
cancels a press. One diagnostic action may await a pending frame; Back takes
priority. Repeated completed taps remain supported.

Accepted frames are drained before return, including QUEUED frames and terminal
failure/supersession. A refused submit or invalid acquired surface is released
through the unchanged viewport lease owner. If completion cannot be established
after the cleanup bound, the app reports `WATCH_TEST retained=display` and retains
custody instead of racing Home. Existing retained-custody behavior for failed
haptic stop, touch unsubscribe and grant release remains intact. Grants release
in reverse order after native frame custody is settled.

Optional missing or malformed APIs never block the first frame. Battery and
motion read failures and unset/invalid RTC data remain explicit. Motion and
haptic lookups occur only after requested actions; absent optional grants are
not fabricated or released. Each entry has fresh local state.

## Verification

Run the production-source raster and lifecycle harness in normal and ASan/UBSan
modes:

```sh
python3 scripts/test_bringup_native.py --output /tmp/bringup-native
python3 scripts/test_viewport.py
python3 scripts/test_viewport.py --sanitize
python3 scripts/test_watch_layout.py
python3 scripts/test_home_native_height.py
python3 scripts/test_capabilities.py
ASAN_OPTIONS=detect_leaks=0 python3 scripts/test_capabilities.py --sanitize
python3 scripts/check_capability_boundaries.py
```

The harness includes `apps/bringup/main.c` and links the production viewport.
Provider doubles exercise actual `app_main`, renderer output, guard bytes, native
hit rectangles, contact continuity, optional capabilities, asynchronous frames,
repeated entry and cleanup/failure paths. Its receipt records exact commands,
source hashes and logs, and the output directory contains native raster images.
The universal-watch CI runs this test in both modes.

Local target qualification also compiles the production source pair with pinned
ESP32-S3 GCC 15.2.0 (`esp-15.2.0_20251204`), normalizes and compacts the ELF using
the existing Watch scripts, and validates it with the Runtime ELF validator.
`app_main` is the only exported symbol. This is source, raster, host lifecycle
and target-build qualification; physical display/touch/haptic operation and a
released device firmware upgrade are not claimed.
