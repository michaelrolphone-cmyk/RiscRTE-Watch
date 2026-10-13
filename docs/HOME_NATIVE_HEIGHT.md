# Home native-height reflow

This source checkpoint replaces the centered-square display/touch facade with
`WatchLayout.h`'s width-based geometry. Home consumes physical dimensions and
functional capabilities only. There are no brand/model/pin/resolution-selection
build defines. `PORTABLE_WATCH_NATIVE_HEIGHT` selects the same generic Quick
renderer in every shared Home build, not a device-specific ELF.

## Active layout

- 240x240 is the original direct path. 410x502 exposes a 240x294 logical surface.
- Every physical pixel is rendered with `floor(native_axis * 240 / native_width)`.
  Input uses that exact inverse. Physical row 501 maps to logical row 293; an
  invalid/out-of-surface contact cancels instead of clamping to a control.
- All existing catalog faces remain. Their original 240x240 artwork is centered
  vertically without stretching typography, circles, strokes or progress arcs.
  At 410x502, the added top area shows the selected face name. The bottom area
  shows the next real projected Point with its selected 12/24-hour clock and day
  suffix; if that projection is absent, empty, invalid or time is unset, the
  existing truthful date/battery labels are used. No synthetic event is added.
- Picker headers remain top-anchored; the original 132-unit focused card and its
  neighboring card sizes are unchanged. The card center moves from y92 to y119,
  and its name/action/dots move down 54 units. Collection pitch is the actual
  logical height, in both animation and gesture recognition. Saved face IDs and
  the six-square preview cache remain unchanged.
- Quick uses System Apps `076f8da38f7dfebdfde6dd5d46e06446470fb828` and
  `pqa_set_height()`. It accepts the full logical surface before uniform physical
  presentation. Sliders, controls and fonts retain their width-based proportions.
- Alarm dismiss/retry bands, sleep-error footer and Context dismissal hint anchor
  to the actual bottom. At 410x502, dismiss begins at physical row 349 and retry
  at row 431. The last physical row remains inside the appropriate control.
- Sleep blanks every logical/native row before panel preparation. Both historical
  retained-image transitions allocate/copy/blend the complete logical height;
  canvases above the transition helper's existing 320-unit bound retain the
  existing sharp-frame fallback. Boot artwork stays proportional and centered;
  ripple preserves its original square map and extends the effect through the
  surrounding rows, ending with the full canvas black.

This is a bounded portrait Home layout (240..2048 logical rows), not redesigned
artwork for each calendar/schedule face. The provider adapter also qualifies
landscape geometry, but Home's proportional face view requires a logical height
of at least 240. It does not crop those faces to admit a shorter viewport.

## Memory and custody

At 410x502 the logical canvas is 141,120 bytes, plus an 820-byte column map:
25,920 more canvas bytes than the old square adapter. There is no second
per-frame native buffer and no increase to the six-card picker cache. The native
provider retains its own frame; app canvas pointers are never submitted as native
provider storage. Dirty-rectangle bounds, acquire/release ownership, rejected
submit retry and stride padding remain checked.

The ordered raster-input journal, Contexts capture-only callbacks, settled-frame
handoff, alarm cancellation, RTC reconciliation/retry, radio ordering, and sleep
custody are unchanged. Added surrounding rows retain bounded capture points.

## Host qualification

Run with the exact dependency lock revisions:

```sh
python3 scripts/test_home_native_height.py
python3 scripts/test_viewport.py
python3 scripts/test_viewport.py --sanitize
python3 scripts/test_watch_layout.py
python3 scripts/test_fast_wake_clock.py --system-apps SYSTEM --utilities UTILITIES
python3 scripts/test_fast_wake_clock.py --native --system-apps SYSTEM --utilities UTILITIES
python3 scripts/test_clock_raster_gestures.py --system-apps SYSTEM --utilities UTILITIES
python3 scripts/test_clock_raster_gestures.py --native --output dist/clock-raster-gestures-native --system-apps SYSTEM --utilities UTILITIES
python3 scripts/check_capability_boundaries.py
python3 scripts/test_capabilities.py
python3 scripts/test_capabilities.py --sanitize
```

The new native render suite runs normal and ASan/UBSan checks:

- 362 exact RGB565 golden frames captured from preserved Home `16d15ab` before
  renderer changes. Covers every enabled face, 12/24-hour and invalid-time states,
  ready/empty/error Point projections, all collection positions, 64 alert states,
  Context/sleep overlays, boot ages and ripple scans.
- Every face's 240-square core remains byte-identical when placed inside the
  padded 240x294 canvas. Both added information areas contain visible content;
  changing an upcoming Point updates the added footer. Padding and outer guards
  are preserved throughout face/picker/modal/effect rendering.
- All 205,820 physical pixels of 410x502 are checked against visible-card hit
  geometry and both alarm bands. Final-row hold, category dragging, invalid
  height/coordinates and multitouch cancellation are covered.
- The existing complete fast-wake/RTC/alarms/Points/sleep-refusal suite now runs
  independently at both 240x240 and 410x502 against the real production Home.
- At each size, the raster suite checks 40/40 input timing combinations, 136/136
  launcher/Quick gestures across every enabled face and cold/light wake, plus
  15 interrupted/cancelled/boundary flows. Journaling cannot mutate UI during
  writes or launch/allocate before settled handling.
- The viewport suite covers direct, portrait, landscape, fractional enlargement,
  downsampling, final rows, retained-pixel recovery, invalid surfaces and lease
  ownership. The geometry foundation separately checks 5,033,052 physical pixels.

ASan and undefined-behavior checks are enabled; leak detection is disabled in
these host runners. Fake elapsed costs are not measurements of physical hardware.
Host-rendered 410x502 views of two circular faces, Points, picker, Quick and an
alert were also visually reviewed for clipping and proportions.

The shared default/return components pass the target Runtime ELF structural
validator with the pinned GCC 8.4.0 toolchain. `scripts/build.py` verifies actual
dependency HEADs, clean selected trees and every pinned git blob before compiling,
then checks again afterward. `build.json` records per-file SHA-256 hashes for
Home and those verified dependencies. Unit tests reject wrong revisions, dirty
files, untracked include shadows and changed bytes hidden by git index flags.

No firmware/device build, external publication, native RAM-capacity claim, boot
selection change or physical-device qualification is included in this checkpoint.
The shared component must still be assembled with each board's truthful providers
and qualified on the actual hardware.
