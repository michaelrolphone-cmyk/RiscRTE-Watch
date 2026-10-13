# Shared Home input and RTC parity

## Sources and scope

The raster journal and deep/light-wake RTC reconciliation are ported from
RiscRTE-T-Watch-S3 source `b7a6cf0d2505ecf9b1c625bbfcc6d4048c13b471` into the
shared Home implementation. Shared dependencies remain pinned by
`dependencies.lock.json`:

- System Apps: `14762601b3fa7bbeeac5ed565a6825534bf74dca`
- Utilities: `fc0bdbbbc600ce891e60c44d091fce0586ade7d8`

The port preserves independent optional capability clients, viewport touch
mapping, an absent retained-wake service's cold-boot path, the zero-service
`clock.elf` return module, all face IDs/assets and Points integration. It does
not alter renderer geometry, System Apps, provisioning or device firmware.

## Ordered raster input

Raster callbacks journal raw touch observations only. The Home/Quick recognizers
are replayed in order after frame writes stop. Capture never allocates, changes
UI state, changes a face, or launches another application. Existing completed
presentation handling remains the handoff boundary.

The journal holds 128 observations. Overflow discards the entire journal,
cancels the current contact and requires fresh neutral before a new gesture.
This is intentionally bounded, not unlimited gesture retention. A stress
experiment injecting 3 ms per renderer checkpoint on face 2 produced a modeled
1,170 ms raster and 130 polls; its gesture was canceled by this overflow policy.
The all-face acceptance matrix therefore uses 1 ms per checkpoint and tests
non-overflow trajectories. The separate overflow test proves cancellation and
neutral recovery. Neither injected timing is measured device performance.

## RTC authority and recovery

External RTC time remains the authoritative civil-time source. The existing
fixed UTC+08 RTC policy and Denver display conversion are unchanged. Home never
writes the RTC, uses compiler/host time, or treats retained native time as an
accuracy check.

A valid deep-wake native snapshot permits the first frame and input poll before
RTC admission. Light wake likewise presents first, then reconciles. Ordinary
RTC refusal preserves a freshly validated native clock. If that clock is
UNSET, Home remains visible with TIME UNSET. The status strip shows TIME CHECK
ERR while reconciliation is pending.

The shared implementation additionally repairs transient initial failures:

1. Try the existing initial seed when RTC admission is not deferred.
2. Retry pending reconciliation after the first frame/input.
3. If it still fails, retry at most once per 5 seconds while Home is active,
   using unsigned elapsed-time arithmetic and measuring from completion of
   the preceding attempt. Wake supplies a fresh immediate opportunity.
4. A successful RTC read/seed clears only TIME CHECK ERR and replaces the
   provisional native sample. Other status messages are preserved.

An unavailable, unreadable or invalid/unset RTC remains eligible for retry.
Runtime CONTEXT, retained custody or malformed native snapshots stop the
invocation rather than using cached state to authorize more I/O. The wider
accepted RTC calendar fallback remains intact.

## Reproduction

With clean checkouts of the pinned dependencies:

```sh
python3 scripts/test_fast_wake_clock.py --system-apps ../System --utilities ../Utilities
python3 scripts/test_clock_raster_gestures.py --system-apps ../System --utilities ../Utilities
python3 scripts/check_capability_boundaries.py
python3 scripts/test_capabilities.py
ASAN_OPTIONS=detect_leaks=0 python3 scripts/test_capabilities.py --sanitize
python3 scripts/test_viewport.py
ASAN_OPTIONS=detect_leaks=0 python3 scripts/test_viewport.py --sanitize
```

Both Home suites run normal and AddressSanitizer/UndefinedBehaviorSanitizer
builds. The renderer translation units explicitly receive
`WATCH_CONTEXTS_CLIENT`, matching the production callback/state layout.

The same 40-trajectory matrix against shared predecessor
`76aa47728192531ca9812f3e026057e0b174cb00` recognizes only 14/40 gestures
(`--expect-loss --mode normal`). The repaired source recognizes 40/40 in both
normal and sanitized runs. This is a modeled regression comparison.

Coverage:

- 40 raster trajectories: launcher/Quick, 80/160 ms gestures, five modeled
  checkpoint costs, cold/light wake.
- 136 bounded all-face cases: both gestures on all 34 faces, before/after light
  wake, with the production Points path.
- 15 boundary cases: tap, hold, poll/snapshot/queue errors, geometry rejection,
  multitouch, contact replacement, overflow/neutral recovery, consecutive
  Quick open/close/reopen, top-edge routing and both directions across raster
  boundaries.
- Deep and repeated light-wake native drift of +300 and -300 seconds, ordinary
  late RTC acquire/read refusal, successful next-wake recovery, and Runtime
  CONTEXT fencing.
- Actual Home cold-UNSET and normal-invocation VALID native clocks with two
  transient RTC failures, same-invocation hardware recovery after the bounded
  retry, continuing frame/input handling and warning clearance.
- Cold RTC failure followed by Runtime CONTEXT in both native-validity states,
  with no subsequent settings/Points/optional-service admission.
- Existing first-frame/input order, due alarms, sleep refusal pixel preservation
  for every face, optional service absence, startup failures and cleanup.

The fixtures model provider timing. Their 95 ms frame/input assertions are not
hardware timing claims. Device assembly, flashing and physical testing are
outside this source qualification.
