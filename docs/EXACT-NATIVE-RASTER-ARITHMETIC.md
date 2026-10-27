# Exact native raster arithmetic

This successor changes only shared Home geometry/glyph arithmetic, not frame
cadence, animation, pixel coverage, blending order, input checkpoints or transport.
Baseline: Watch06cf061b6fbe93a1d2f2f8a197ba773a54c98b18.

`native_q` previously always divided a signed64-bit product by240. The native
Xtensa GCC15 size-optimized object emits five `__divdi3` calls in each `fline`
geometry setup. Compute the same64-bit product, then use a signed32-bit quotient
only when the product fits; otherwise retain the original wide division. Both
paths truncate toward zero. This preserves negative/offscreen and large geometry.
The actual target fast branch is a reciprocal multiply/add/shift sequence; the
wide branch still calls `__divdi3`.

Native glyph interpolation already steps horizontal sample coordinates with an
exact quotient/remainder recurrence. Use that same existing recurrence vertically
instead of a fresh64-bit division on every glyph row. The original four source
samples, rounding, opacity, pixel order and callback points are unchanged.

## Evidence and limits

Counter-instrumented baseline466 face0 at second0/29/58 respectively performs
440/2180/3920 `native_q` calls, all with products fitting signed32, and713 native
glyph rows. Face1 performs431 fitting geometry calls and400 glyph rows. These
are source-model operation counts, not measured hardware cycles. The additional
vertical recurrence setup still costs one-time division/remainder per glyph.

The physical report measured Home rendering431ms and transfer115ms. This change
removes a concrete target arithmetic cost but does not demonstrate that either
physical number, device FPS or overall usability has improved. Host x86-64 has
hardware64-bit division; whole-scene host times are not a target speed prediction.
All-face normal baseline/candidate elapsed times were2.155/2.159s in one parity
run; sanitized3.432/3.445s. No host throughput gain is claimed.

Validation:
- `scripts/test_native_q_exact.py`:1,852,286 exact cases, including negative
  rounding, signed32 product edges, zero, odd dimensions and wide fallback;
  normal and ASan/UBSan.
- `scripts/test_native_alpha_vertical.py --output DIR`:3200 randomized glyph
  rasters,244,328,000 active pixels; complete buffer/padding,0/180 transformed
  pixels, ordered callback states and failure-prefix equivalence; normal and
  ASan/UBSan.
- Existing `scripts/test_native_alpha_parity.py --baseline BASE --output DIR`
  (also `--sanitize`):4068 frames per source, all faces/picker and failure cases,
  identical signature aa298659962f544f0cf2e7489e2222b3dbea8585aa752953a224ffc0c075d090.
- Exact final `nova.c` compiles with Xtensa GCC15 `-Os -Wall -Wextra -Werror`;
  undefined imports unchanged. This is target object validation, not a firmware
  link or physical acceptance.

A separate line cross-product/threshold recurrence experiment was not adopted:
its host benefit was inconsistent and it expanded the hot primitive. Its patch
and results remain external investigation evidence. Likewise transport's extra
PSRAM pairing copy is separate work, not part of this change.
