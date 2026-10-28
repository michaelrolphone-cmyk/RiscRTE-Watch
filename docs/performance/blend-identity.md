# Exact identity in RGB565 coverage blending

The existing nonzero/nonopaque `blend()` path already loads the destination RGB565 value. If it equals the source ink, return before channel arithmetic. Alpha-zero and opaque paths stay first, so opaque rendering does not acquire a destination read. Callers still evaluate `at()` before entering blend; checkpoint frequency/order and terminal sinks are unchanged. There is no allocation, cache, pixel-quality reduction or new retained state.

For each channel c and coverage a in0..255, the original rounded result is floor((c*(255-a)+c*a+127)/255) = floor((255*c+127)/255) = c. This applies to all5/6/5 channel ranges. No arithmetic shortcut for unequal pixels is introduced. The compiler already strength-reduces /255; this change avoids the whole channel sequence for equal ink, not a claim of replacing hardware division.

## Validation
`scripts/test_blend_identity.sh <clean-reference-checkout>` takes OUTPUT_DIR and optional SANITIZE=1. Reference for the original experiment is8ee04ce277e0304b0156aca285b436f3910363da; both modules use the same selected caches, compositor and Contexts options.

- Exhaustive65,536RGB565 values ×256alpha fixed-point cases, plus2,000,000 random old/color/alpha comparisons against independent channel quotient/remainder rounding oracle:18,777,216 cases per lane.
-4,040 complete picker framebuffer/callback pairs per lane: seven geometries, five categories, four motion speeds, category transitions, scale variants and checkpoint failures. Existing report text calls one module 'planned' and its reference 'original publication'; here both are the same current compositor except this identity branch, not a comparison against an older publication.
-1,020 full-face framebuffer/callback pairs per lane across all34 catalog faces, progressive time and five injected checkpoint failure positions.
- Normal and ASan/UBSan (leak detection disabled for this execution environment).
- Exact selected Home target recipe, only nova.c source changed, passed full link, normalization, structural ELF validator, identical import/export list and zero hardware CAS checks. This is not full product Runtime qualification or device execution.

## Host cost comparison
Five interleaved old/new CPU-time repeats, unchanged workload and pixels.466picker comparisons cover20category/speed workloads,100frames each; all-face comparisons have five warm-up frames then20timed frames per face. Mean candidate/reference picker ratios0.8477–0.8729, median0.8554 (12.7–15.2% lower host render CPU time). All-face aggregate ratios0.9876–1.0091, median1.0016 (essentially neutral, slight median regression). Individual faces need not improve. These host-O2 figures are not Xtensa-Os timings, FPS or proof of sufficient physical usability. Target confirmation remains required; current active integration and published images were not changed by this isolated experiment.
