# Exact native glyph-coordinate recurrence

Isolated successor of Watch03aa63fb4830987c854e05a878c00c8cdde9577c.
Only production changes: `apps/clock/nova/nova.c` and new private
`native_alpha_coordinate.h`. No geometry/appearance/animation/input policy,
framebuffer scaling, cache size/lifetime, display token or provider change.

Original horizontal source coordinate divides the signed numerator
`((int64_t)x*256+128-left)*256` by positive sx for each pixel. Numerator advances
65536 per column. Store Euclidean quotient/remainder, add quotient/remainder of
65536 each step, and convert back to C truncation-toward-zero before original
-128/floor/fraction arithmetic. Positive denominator and current int sx bound
make the unsigned remainder addition safe. Row-major mask lookups, blending,
`at()` calls and checkpoints retain their original order. No new allocations.

Validation so far (software only):
- Actual C helper normal+ASan/UBSan:10,675,530 coordinates/15,390 cases crosscheck
  original C division, with independently generated Python big-integer endpoints.
- Independent Python recurrence oracle:2,603,520 comparisons.
- Baseline/candidate normal+ASan/UBSan raster differential:4,068 frames each,
  nine geometries including466/odd/portrait/landscape, all34 faces, picker sizes,
  complete buffer/padding/guards, outcomes, callback count and ordered17-pixel
  observed-state signatures at every checkpoint. Five callback failure cutoffs
  (1,2,17,257,1024) additionally compare complete resulting partial buffers.
  Exact per-frame output hash aa298659962f544f0cf2e7489e2222b3dbea8585aa752953a224ffc0c075d090.
- Exact existing target debug ELF's native_alpha body calls app-local fixed64-
  iteration __divdi3 in horizontal pixel loop. Isolated actual Xtensa compiler
  using selected definitions confirms candidate moves horizontal div/mod to
  glyph setup; row Y division remains. Target static stack112→128 bytes (+16).
  Existing PIC divdi3.c supplies __moddi3; no final link/admission claim yet.

Evidence directory `/tmp/badge-picker-evidence`: alpha-coordinate-* receipts,
alpha-parity-boundaries-{normal,sanitized}/receipt.json,
native-alpha-target-assembly.json, baseline-native-alpha-body-xtensa.asm and
native-alpha-{baseline,candidate}.s/.su. Hashes/commands identify source/compiler.
Host timings differ only slightly and are not meaningful ESP32 speed evidence.

Original picker baseline4normal+4sanitized passes and separate expected-retained
normal/sanitized typed-fault outcomes remain distinct. Actual Runtime picker
matrix with explicitly labeled candidate source override, final target build/
import/stack admission and affected product cohort remain pending. Existing
firmware/source artifacts retain their original identities. This is a bounded
arithmetic optimization, not a demonstrated repair of the reported physical
picker crash. No device or source-publication action performed.
