# Exact row clipping for circle rasterization

The shared `fcirc` raster still uses its original four Q4 coverage samples, alpha, blend, and ordered `at()`/custody callbacks. Only centers which the existing outer/inner rejection would discard are skipped. There is no animation/frame skipping, dirty-scene assumption, complete-frame cache, floating-point rounding, or per-device special case.

For each row, exact floor integer square root bounds the outer center interval. For the strict inner predicate (`dist < inner²`), the rejected interval uses `floor_sqrt(inner² − dy² − 1)`. Signed exact ceil/floor division by16 maps center bounds to pixel indices; intervals intersect the original clipped bounding rectangle. Empty, filled (`width=0`), thick (`inner<=0`), and offscreen circles retain their existing behavior. When `inner<=12` there is no rejected inner disk, so the original full-row candidate path avoids unnecessary integer roots; this selection follows the actual geometry rather than a device/face identifier. Helpers use widened arithmetic for valid native canvas coordinates, with32-bit roots on the usual path and64-bit roots for larger squared distances.

The circle0 static thin ring on466px previously examined150,544 centers and rejected143,092; the new intervals examine7,452 centers while preserving the original final predicate. Target GCC15 emits no integer divide instruction in either circle implementation; the new function adds per-row integer-root work and increases code size. Static code size is not timing evidence.

Owning validation:
-40,000 generalized rows,22,780,000 exact center classifications.
-202,752 exhaustive fractional-center/tiny-radius/strict-inner tangent rows.
-640 generalized primitive rasters,48,865,600 active pixels; complete padding/guard buffers, ordered callback/fault prefixes, and0/180-degree transfer parity.
-4,068 all-face/picker frames each baseline/candidate, normal and ASan/UBSan, exact full-buffer hashes and checkpoint signatures.

Host measurements are scoped: a200-render466px thin-ring batch dropped from~36ms to~10ms; the initial broad4,068-frame normal batch improved only~5%; subsequent no-hole fallback measurements are recorded separately. Representative Home faces vary; unchanged/non-circle-heavy scenes are not claimed faster. These results do not establish device FPS or fully resolve synchronous input-dispatch latency. Native phase instrumentation and product qualification remain separate.
