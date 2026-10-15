# Capability-native Home faces

Home 0.16.0 uses the display provider's physical RGB565 frame directly. The
viewport validates the dimensions and forwards native touches without inversely
mapping through a completed 240-wide framebuffer. There are no model switches.

## Preserved behavior

- All 34 face IDs, names, collections and stored selection remain stable.
- Face geometry, strokes, circles, gradients and glyph coverage are evaluated at
  their physical destination. Static glyph masks remain licensed source assets.
- RTC validation, 12/24-hour formatting, battery validity, Points In Time and
  Contexts state are unchanged. Missing services retain truthful empty/error UI.
- Proportional face cores use min(width,height), centered in the display.
  Tall displays retain title and live next-Point/date/battery content.
- Native picker previews are rendered at the destination card size. Cache slots
  are bounded by visible cards; allocation and release follow picker lifetime.
  The 240-wide compatibility preview path preserves established pixels.
- Quick controls, picker gestures and alarm hit regions share native geometry.
- App-return captures, boot transitions, sleep blanking and first wake frames use
  complete provider dimensions and stride. Clock remains a return-to-Home ELF.

## Verification

`test_native_faces.py` renders 34 faces across six RTC/Points/battery states at
240x240, 410x502, 160x160, 200x200, 320x240, 640x240, 240x320 and odd-origin 241x160. It verifies 1,632
face frames per run, padding/outer guards, picker collection transitions, alert
variants, context/sleep messages and every pixel's centered-card hit result.
It runs normally and under ASan/UBSan. LeakSanitizer is disabled because this
execution environment uses ptrace; explicit cache lifetime tests still run.

`test_home_native_height.py` retains all 362 established 240px image hashes and
full-height legacy coverage. `test_native_effects.py`, viewport/layout tests,
real-Home RTC/wake/gesture/atomic suites and target ELF loader checks cover the
lifecycle around the faces. Device release evidence records exact final source
pins, hashes and packaging checks separately.

The face and calendar atlases use independently decodable 256-byte RLE/LZ
blocks. They retain every original coverage byte and the existing bounded
stack scratch, with no heap or cross-block history. The packed-atlas tests
compare every original byte and boundary-crossing range, and exercise malformed
streams and overlapping backreferences under ASan/UBSan.

Host timing is not a hardware frame-rate claim. Physical display, sleep/wake,
face selection and gesture acceptance remain device tests until reported.
