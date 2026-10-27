# Exact-width line coverage arithmetic

The four original Q4 samples, endpoint circles, alpha/blend operations, pixel order,
and render checkpoints remain unchanged. This is not frame skipping or a new face.

For eligible geometry, a conservative triangle bound proves every sample dot/cross
and recurrence update fits signed32. The additional L1 bound extent+delta<=46340
proves both endpoint squared sums also fit signed32, even for degenerate segments. The bound includes the final unused update;
empty clipped rows are skipped before initializing a recurrence. Larger geometry
runs the original signed64 sample loop. Radius magnitude is bounded to46340 and
length toINT32_MAX on the fast path, so its newly eager r²*length computation cannot
overflow signed64 even when no interior sample exists. This does not enlarge the
original renderer's defined input domain (its coordinate and endpoint expressions
remain unchanged).

For integer cross, cross² <= r²*length is exactly equivalent to
-root <= cross <= root, where root=floor(sqrt(r²*length)). The existing exact integer
root routine runs once per eligible line, not per pixel. A root aboveINT32_MAX is
clamped only because the proven sample magnitude cannot exceedINT32_MAX. The helper
uses at most16 bit-pair iterations for32-bit inputs or32 for64-bit inputs. This and
the bounds/row initialization add setup cost; sparse/short lines need not get faster.

## Qualification

`python scripts/test_line_arithmetic_parity.py --baseline 7ef1fdd --output OUT`
(and `--sanitize`) compares original and current primitive buffers, padding, callback
counts, ordered checkpoint-state signatures and forced failure prefixes across
3,200 generalized shapes /244,328,000 active pixels, including clipped/negative,
degenerate, fractional Q4, thick and explicit wide-fallback geometry. It also checks
200,000 root thresholds and adjacent signed sample values. `--source-root` supports
a sparse implementation checkout using unchanged supporting renderer inputs.
The existing native-alpha face/picker lane additionally compares4,068 full frames
at nine dimensions including all faces and five forced failure boundaries.

## Target cost evidence, not measured FPS

Xtensa GCC15 `-Os -mlongcalls` generated-code paths show these representative first
x-sample costs including the next-sample update:

| Coverage branch | Original instructions | Narrow instructions |
| --- | ---: | ---: |
| Interior |48|27|
| Before start |40|22|
| After end |50|28|

Interior multiplication instructions fall4→0, loads13→6, stores3→0. These counts
exclude line/row setup, root, blend/checkpoints and memory wait time. Exact paths
and branch assumptions are retained in qualification evidence. The compiler already
strength-reduces some original64-bit products into multiword recurrences; it is
incorrect to count every source multiplication as a dynamic target multiplication.

At466px, default face0 performs20,004–404,340 line samples over sampled seconds;
face2 approximately376–379k, face12 418,104. Faces8/24/33 do not call this primitive.
Thus this targets one real cost, not every face or the display transfer. Final normal host
full-suite elapsed2.070s→2.153s and sanitized3.691s→3.647s are single observations,
not a robust speed benchmark or an Xtensa prediction. Actual device raster/FPS
improvement remains unmeasured. No change to transport, polling or animation cadence.
