# Picker composition plan

The whole-picker audit of published Watch518e459 found composition, not face raster, dominant in the measured moving workload: about72–77% of host frame CPU at1/2/8/32px bounded steps. Every destination row recomputed the same column fade division/opacity and repeated rounded-corner and border classification per pixel. Existing six-slot face cache intentionally rerenders exact native card sizes during scaling; the prior stable-geometry caches cannot remove that changing work without changing imagery.

`--picker-compositor-plan` explicitly selects `WATCH_PICKER_COMPOSITOR_PLAN` for Home/default. Default is off; the return-to-default role does not receive it. This reusable card compositor is neither face- nor device-specific. Existing native/legacy sampling, geometry, animation, z-order and input model remain unchanged.

## What changes

One optional picker-owned column workspace stores each visible column's exact source-x and combined opacity/fade. Each row resolves the exact outer-circle interval and inner-border thresholds once, using floor/ceiling integer square roots matching the original squared comparisons. Original row-major blend calls and `at()` checkpoint ordering remain intact, including alpha-zero operations and the separate border blend. No RGB underlay, frame lease or output thumbnail is cached; no animation is frozen, throttled or resampled differently.

The plan has four bytes per existing cache-capacity column:1112bytes at466px, at most4884bytes under the2048px layout bound, plus a pointer in the selected picker cache. It is allocated once alongside picker creation, reused serially for every card, and freed on picker destruction. Allocation failure, insufficient workspace or unsupported helper geometry falls back to the original compositor. No per-frame allocation and no new provider callbacks occur. Current source supports serialized picker rendering with the existing capture-only/no-UI-mutation checkpoint contract.

## Evidence

- Exact helper-level complete-buffer/guard/callback/terminal parity across randomized clipping, corner/border thresholds, alpha, legacy sampling,2048px boundaries, faults and OOM fallback.
- Independent shared modules bind the untouched518e459 baseline and candidate, isolating static render/reuse caches. Execution order alternates.4040 complete picker framebuffer/callback pairs span7 display geometries,5 categories,1/2/8/32px bounded forward/reverse movement, changing time/battery/format, pulse scales, two-category transitions/wrap and terminal checkpoints.
-466px analog examples:1.089→.761,1.269→.966,1.266→.890,1.217→.883ms at1/2/8/32px. Across all20 native category/speed workloads, mean1.625→1.295ms (~20%); individual gains13–33%. These are CPU/zero-I/O host workloads, not deviceFPS. Some12-frame historical-size samples regress/noise; no universal speed claim.
- Matched bounded stage replay at1/2/8/32px: composition .788/.831/.824/.813ms before and .512/.539/.530/.522ms after; total1.026/1.122/1.154/1.133ms before and .727/.826/.834/.821ms after. This locates the gain in composition rather than cheaper/frozen face rendering.
- Stage-cost counters are test-only. No production timing/logging callbacks were added.

Target code/layout/import closure, full installed Runtime integration, actual allocation and physical40–60FPS remain unverified here. This removes measured redundant composition work; it does not establish the requested frame rate. The remaining streaming blend/transfer path remains a separate bottleneck candidate, not silently included in this change.

Reproduce normal and sanitizer lanes with `OUTPUT_DIR=<workspace-output> SANITIZE=0` (or1) `sh scripts/test_picker_compositor.sh <clean-518e459-checkout>`. LeakSanitizer is disabled in this ptrace-hosted environment; plan allocation/free and fallback are asserted explicitly. ASan and UBSan remain enabled for the sanitizer lane.
