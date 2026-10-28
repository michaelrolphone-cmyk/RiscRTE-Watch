# Home uses the existing inexpensive Runtime clock

Home `alive()` consumes only the boot-local milliseconds and callback success.
Previously every call requested full Runtime health. In particular the raster
checkpoint did so before deciding whether its eight-millisecond touch sampling
interval had elapsed. The renderer invokes its checkpoint every 256 pixel
accesses and every eight clear rows; overdraw adds accesses. A 466-square
single-write pass alone accounts for about 848 pixel checkpoints, plus 59
clear-row checkpoints. Actual work depends on the face, clipping and overdraw;
these counts are not elapsed-time measurements.

Runtime273 (`273bdbccf65647a803ab0586a418f8283732baec`) already defines
`monotonic_ms`: an inexpensive, active-owner-only, wrapping boot-local clock,
explicitly without heap/identity queries, provider polling or I/O. Home now
uses this suffix when both its table-size and pointer checks pass. Older or
null-suffix tables retain the original full-health fallback. A present clock
callback failure returns failure immediately, never hiding a lost authority by
trying another callback. The touch-retained fence still precedes all callbacks.
No checkpoints, touch sampling, yields, deadlines or rendering are removed.

## Exact native accounting

Runtime `src/bootstrap/Runtime.cpp:1438` health and `monotonicMs` both check
`active()` (invocation, owner, stream/sleep/checkpoint state). Native full health
at `src/main.cpp:127` queries uptime and free heap, gets the running OTA partition,
reads eFuse MAC, and formats target identity; it returns true after those calls.
The native monotonic backend calls only `RiscPlatform::millis()`, implemented
using `esp_timer_get_time()/1000`. It is bound in the selected native Port.

Pinned IDF implementation details: free-heap query traverses registered heaps
and sums cached `free_bytes` (not a full heap-block walk); running OTA partition
is cached after first lookup; default MAC reads eFuse fields. Do not inflate
those operations into unmeasured multi-second delays. No provider polling occurs
inside native health itself. Its extra work is unnecessary here, but the actual
share of reported device slowness remains unmeasured.

Current System adapter `millis_now()` already prefers this suffix. Its raster
budget reads therefore do not imply full health queries with the selected273
SDK. Health fallback remains for historical ABI compatibility. The repair is a
Home clock consumer correction, not a kernel timer or per-app clock invention.

## Qualification scope

`test_home_monotonic_clock.py` compiles the exact production function, current
and suffix-disabled headers, normal and ASan/UBSan. It checks 100,000 successful
current-clock calls without a health query, wrap, null/small tables, callback
failure without fallback, legacy health failure, and no callback after retained.
Existing lifecycle and real raster-gesture fixtures can select
`--monotonic-clock-test`; their default null-suffix path stays unchanged.
These are software tests, not device timing or usability acceptance.

## Current callback refusal: invocation authority boundary

The native monotonic backend always succeeds; Runtime rejects the call when
`active()` is false. Home therefore latches a distinct
`clock_runtime_unavailable` on a present current callback's false return. It is
not classified as touch retention, and Home must not request retention after
losing that authority. Historical health-fallback false behavior is unchanged.

The latch resets at the next `app_main` entry, not during the failed invocation.
It prevents sparse exit diagnostics, timing/performance callbacks, resource
cleanup callbacks, display polling/yield after an accepted present, and boot/
wake diagnostic helper continuation. Boot/return image cleanup does not free
invocation-owned buffers through unavailable Runtime imports. Quick's pending
frame failure also skips its otherwise appropriate retain request when the
failure is specifically lost Runtime clock authority. Other presentation faults
retain their existing behavior. The selected Home ELF exports only `app_main`;
there is no Home module-fini callback to perform a second cleanup.

The actual production Home fault fixture exercises 13 injection positions per
normal/sanitized lane: startup clock samples, owned raster, accepted pending
present and after first usable frame. It rejects subsequent Runtime/provider,
checkpoint, diagnostic and yield callbacks; tests also re-enter a new invocation
with an old latch set, and directly revisit exit/present/frame/pacing boundaries
after failure. The shared Quick pending helper separately injects first/second
clock failure, requires no retain, write, restore or discard callback, and keeps
all existing presentation-failure/cancellation cases. Native healthy lifecycle
and legacy gesture regressions remain distinct from these fault cases.

### Selected crash-report build correction

The first unavailable-fence revision incorrectly embedded `goto done` inside
`CLOCK_WAKE_MARK`; the selected crash-report bool helper has no such label.
That full target build failure is preserved and is not qualification evidence.
The marker now returns a boolean expression. All 21 Home callsites explicitly
choose `goto done`; both crash-report callsites explicitly `return false`.
Disabled markers evaluate true. No caller silently continues after unavailable.

The exact failed Xtensa flags/SDK were replayed as a crown object compilation
with crash reports, interaction trace and performance trace selected; it passed.
`test_crash_clock_marker.py` executes the exact production crash-step body and
actual enabled macro with modeled callbacks. Normal and sanitizer lanes cover
healthy report, failure at begin, and failure at end, with every subsequent
operation forbidden after refusal. Full linked product qualification remains
with the integrator.
