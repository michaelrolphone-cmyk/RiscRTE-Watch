# Visible RTC synchronization qualification

## Exact scope

This is a test-only qualification of the shared Home application source used
by T-Watch 1.1.43 and the Waveshare 0.1.52 candidate. No production application,
firmware binary, dependency pin, RTC policy or display geometry changes here.

Application source: `8132d73c828c0e4763f6c89a3d6a4a883314056a`, merged as
`aba55741d06d93a5c1fe9dc9bc9b337f11efd180` with identical tree
`bbb250bf17525c05057074a3bc0d2458fb21c446`.

Dependencies, read from `dependencies.lock.json`:

- System Apps `cb483470887b5198e2f3cd2457586ef6622142a9`
- Utilities `fc9f93402050f020a3422ee889620b2414c7ad5d`

The fixture compiles actual `apps/clock/crown.c`, native-time application
policy, renderer and Quick integration against checked provider doubles.
Runtime/provider behavior is modeled; this does not run native firmware or
an external RTC. Atomic preferences, Points, motion, alarms, crash reports and
both supported native display sizes are enabled in the new CI matrix.

## Measured contract

The existing suite checks cold boot, valid and unset native time, deep and
repeated light wake with +300/-300 seconds of native drift, RTC admission/read
refusal, bounded retry and warning clearance, and terminal custody faults.
A valid retained native sample is permitted for the first wake frame before
external RTC admission; the later reconciliation restores RTC authority.
Therefore this is an eventual post-reconciliation contract, not a claim that
the first provisional wake frame has already been checked against the RTC.

The new case changes a settled modeled external RTC and its accepted native
clock seed while Home remains active. It checks all 34 selectable faces over
eight independently specified raw-RTC/Denver calendar pairs:

- Forward and backward five-minute changes
- The last second before and first second after both 2026 DST transitions
- Leap-day and year boundaries

Each case requires a completed display transfer, expected civil-time fields,
and a full completed-frame pixel comparison against the renderer using that
expected time. Literal UTC epochs independently check conversion. The pixel
oracle reuses the selected-face renderer: it checks fresh render delivery, not
independent glyph/artwork correctness. Brightness must be nonzero and agree
with Home settings. It also checks selection preservation, no sleep, exactly two
native seeds (startup and update), and clean capability/display custody.
These are fixed boundary samples, not autonomous ticking across transitions.
The separate boot/wake tests check calendar/native reconciliation; they do not
repeat this new corrected-frame oracle across every face and date.
The first updated complete frame must appear within 500 ms of modeled fixture
time. This is not measured device latency. Eight cases × 34 faces × two sizes
× normal/ASan+UBSan builds gives 1,088 accepted-update cases.

The accepted-update fixture deliberately starts after the RTC write and
native seed have succeeded. It does not qualify the Settings write/verify/seed
transaction, an actual firmware update, or a live hardware clock correction.
The fixed UTC+08 external RTC policy and Denver display conversion are
unchanged. Home does not write the external RTC.

## Reproduction

Use clean checkouts of the exact dependencies above:

```sh
for size in 240x240 410x502; do
  python3 scripts/test_fast_wake_clock.py \
    --system-apps ../system --utilities ../utilities \
    --width "${size%x*}" --height "${size#*x}" --atomic --crash-report
done
```

Each invocation runs both normal and ASan/UBSan builds. The whole original
boot/wake/failure suite runs after the new cases. Set
`WATCH_TEST_RTC_UPDATES_ONLY=1` only for a focused local diagnostic; CI does not
set it. `.github/workflows/rtc-visible-sync.yml` runs the full matrix, reads
source-owned dependency pins, and publishes source identities and logs.

## X4 counterpart and limits

X4 uses a different delivered Home implementation. Its exact 0.1.122 target
ELF was reproduced, and actual-Home RTC/render, fresh timer-wake, and custody
qualification is recorded in the
[X4 desk-timer evidence](https://github.com/michaelrolphone-cmyk/RiscRTE-XTEINK-X4-PRO/blob/c9ecac4058ead7a82b0f74560ac5b19f0b858b3f/docs/desk-timer/README.md).
Those results must not be attributed to this Watch fixture.

Physical boot, RTC setting, firmware-update preservation, wake behavior and
visible display/RTC agreement remain **not tested**. No hardware result is
inferred from software tests or CI. An isolated mutation disabling periodic Home RTC resampling was rejected by
the new test at its completed-update assertion. No new firmware release is
needed for this test-only change.
