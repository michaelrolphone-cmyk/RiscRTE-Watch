# Exact picker destination-row reuse

The selected optional compositor already plans column alpha/source-index and rounded row bounds. This change keeps that plan, original source sampling and blends, but derives destination row address once per row rather than multiplying stride at every pixel access.

`picker_row_at` preserves every original access, including alpha-zero and border accesses. The operation counter and each 256th checkpoint are unchanged; checkpoint accesses delegate to `at`, and the derived row is refreshed after a successful callback. A failed callback uses the original sink with no pointer arithmetic on a lost/null destination. The callback can rebind pixels or change stride without leaving a stale row. This does not coalesce input polls, replace clocks, add allocation, change animation, or introduce any opaque memcpy/resampling path.

The optimization is under the existing `WATCH_PICKER_COMPOSITOR_PLAN` selection. Missing optional workspace and unsupported plan geometry still use the unchanged reference path. It is a Home picker implementation, not the separate System Nova row writer.

## Validation

`OUTPUT_DIR=... scripts/test_picker_row_writer.sh /path/to/unchanged-0ed6624` builds independent original/current renderers. `SANITIZE=1` retains -Os with ASan/UBSan; leak detection defaults off for this host environment. Tests include:

- 1,717 full-buffer/guard/callback/terminal and optional-plan OOM/fallback cases.
- 168 writer cases: pointer rebind, same-pointer stride/dimension changes, combined changes, terminal/null pixel loss, operation wrap and a checkpoint exactly between source and border blends. Entire alternate images, guards, sink bytes, callback counts, terminal status and operation counts match.
- 600 non-context full-buffer/guard/scaled-source/fallback cases.
- 4,040 complete frame/callback pairs over seven square/rectangular geometries, all five categories, horizontal speeds, category transitions, pulse, changing telemetry/time and injected failure points.

Current-workload host -Os timing uses per-frame interleaved original/new calls, one complete warmup pass and three measured repetitions. At 466x466 the aggregate raster time was 1.361→1.273ms, 1.417→1.339ms, and 1.350→1.265ms (5.5–6.5% lower). Individual workloads range from improvement to a noisy 1.2% regression; this is not universal or physical FPS evidence. Real provider/transfer costs are excluded.

Selected Xtensa compilation uses current build-record flags/includes, adding `-ffat-lto-objects` solely to expose diagnostic machine code while retaining LTO. That object comparison adds 164 text bytes, changes no data/BSS, and retains identical imports. These are not final linked-size/cycle results; whole-product integration and actual device performance remain separate.
