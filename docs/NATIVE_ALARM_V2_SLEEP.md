# Explicit native tagged alarm Home sleep

Development opt-in `--native-alarm-v2 --time-authority native`. Legacy API1,
optional alarm behavior, default source pins and golden assets are unchanged.
Exact separate dependency selection lives in `profiles/native-alarm-v2.json`:
System6e4cda9, Utilities4cca8b3, Runtime interface273bdbc. This profile makes
`alarm.service@2` mandatory, disables Home's Points projection selection and
consumes the service's existing tagged descriptor/ticket API. It does not add
alarm scheduling, a new time epoch, output hardware or a second service.

## Composition and custody

- Reuses Utilities `AlarmServiceV2.h`, System `PortableAlarmClient.h`, Home's
  existing owned native sleep helpers and append-only GPIO+timer APIs.
- Native service tickets express UTC seconds since2000. Duration is the copied
  deadline-minus-current value, clamped to1ms for due/past tickets and24hours for
  farther deadlines. No alarm deadline means no alarm-imposed limit. Explicit Light remains
  untimed GPIO sleep; explicit Deep enters owned Deep without an alarm timer.
  Hybrid retains the established Light-first configured transition timer even
  with no scheduled alarm, then enters Deep after timer-only completion and
  fresh alarm reconciliation. This is one application-owned staged transition,
  not an invented recurring alarm or periodic wake schedule.
- Fresh reconciliation occurs after ordinary panel preparation's delay and
  before Deep pad hold. No service/KV call is issued after hold. Held-key/entry
  refusal does not consume a ticket. Successful Light consumes the unchanged
  ticket exactly once before another service mutation. Hybrid with a farther
  deadline replans after timer return and rechecks the key before Deep.
- All service results use the shared tagged client's custody taxonomy.
  RETAINED, OUTPUT and unknown results stop immediately. In particular, step
  failure cannot be followed by status, and resume failure cannot be followed
  by power/display restoration. A terminal refresh after ordinary restoration
  permits no subsequent call. Failed power restoration conservatively retains.
- Home is not a PortableApps adapter. Its explicit bridge raises
  `watch_runtime_halt`/Runtime invocation retention for a retained shared client,
  including modal refresh/ack, failure-stop and uncertain grant release. Exit
  cleanup and the diagnostic checkpoint stop at that fence. Do not add an
  unresolved `portable_adapter_retain` alias to the shared client.
- A newly entered Home, including native Deep wake, acquires and reconciles the
  service before the ordinary startup/wake introduction. Durable occurrence
  recovery remains the service's job; Home never resumes an old Light ticket
  across a fresh invocation.

## Build and test

```
python scripts/test_native_alarm.py --system SYSTEM --utilities UTILITIES \
  --output EXTERNAL_TEST_OUTPUT
python scripts/build.py --system SYSTEM --utilities UTILITIES \
  --time-authority native --native-alarm-v2 --cc XTENSA_GCC_15_2 \
  --output EXTERNAL_BUILD_OUTPUT
```

The builder verifies exact clean selected dependency bytes and records the
profile, source/header inputs and artifact definitions. Python needs the existing
`pyelftools` dependency. Both default Home and tiny Clock-return ELF were compiled
and structurally/import validated with the pinned Xtensa15.2 compiler.

Owning tests compile production sleep code plus a separate test including actual
Home `crown.c`/shared client. Normal and ASan/UBSan cover negotiated API2, malformed
and legacy rejection, UTC delta/clamp, no-schedule untimed, timer versus key,
32 successive Light plans, one consume per unchanged ticket, Hybrid fresh delta,
Deep hold ordering, and retained/output/unknown faults at every service dispatch.
The Home test verifies retained step/refresh/ack/stop/release raises Runtime's
fence and forbids later client cleanup/checkpoint calls. Generic legacy sleep and
56 existing Home native-transaction cases also pass normal/sanitized at two
geometries. LeakSanitizer is disabled in the ptraced host; ASan/UBSan remain on.

## Completion gate still open

These are owning source/model tests and isolated Home ELF checks, not a complete
product. Integrate the exact service/app/runtime/dependency graph and prove
native KV/transaction custody while sleep is prepared, due occurrence delivery
on successful timer wake, fresh Deep recovery, active/inactive application
handoff, dismissal/cancellation and clock changes under normal/sanitized Runtime
execution. All foreground apps need the separately planned deadline-aware sleep
composition; this Home increment alone cannot ensure Alarms/Countdown wake while
another app is sleeping. Physical timer/key wake, visible alerts and power draw
remain unverified. No device, sealed cohort, install catalog or publication was
changed.

## Conservative native UTC duration

Only the native API2 profile subtracts one second from positive UTC deltas
above one second; delta one uses a 1 ms poll. Deltas above 86400 seconds cap
at 86400000 ms; absent deadlines remain untimed. Due/malformed tickets retain
the checked reconciliation/refusal contract. API1 preprocessing is unchanged.
The whole-second sample has an unknown fractional part. This early-wake margin
reduces that uncertainty, but post-ticket preparation, scheduling and physical
wake latency can consume it: this is not an exact physical deadline guarantee.
Ordinary panel/power preparation precedes ticket preparation. Tests vary that
latency across second boundaries and enumerate millisecond sample fractions.

## Hybrid preservation regression

`watch_alarm_sleep.h` previously treated no alarm deadline as untimed Light in
native API2 Hybrid and consequently never progressed to Deep. The tagged
service's absence of an alarm is independent of the user's staged sleep timer.
Both service profiles now use the same Hybrid timer-selection predicate; exact
tagged ticket resume and fresh-plan custody fences are unchanged. Earlier or
equal effective alarm duration wins and returns normally rather than going Deep.
The existing conservative whole-second alarm margin remains intact.

Owning tests enumerate every supported deep-timer second (60 through 3600),
no deadline and before/equal/after alarm boundaries, timer/GPIO wake, fresh-plan
BUSY/storage/terminal results, key arrival at both transition checks, unsupported
timed Light and explicit Light/Deep behavior. Malformed saved deep=0 is rejected
by the established shared loader with the five-minute fallback, not reinterpreted
as a new disable setting. Physical radio/touch/key timing and actual CPU sleep
are separate integration/device qualification, not proven by these owning tests.
