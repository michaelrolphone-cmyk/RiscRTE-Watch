# Generic app-local Watch bridges

`apps/clock/portable_navigation.c` implements the existing PortableNavigation
hook using the declared `input.power-key@1`, instance 0. `portable_sleep.c`
implements the existing app-local sleep hook using independently acquired
`power.sleep@1` and `display.power@1`, both instance 0. The battery argument is
kept for ABI compatibility and is never interpreted as a sleep/key table.
There are no board/model names, pins, or private battery/display suffix casts
in these clients. Emergency reboot remains a native/provider responsibility.

## Build contract

Keep existing full-feature app definitions, including alarm, quick actions,
low-battery policy, motion wake, and alarm sleep-resume where selected. Enable
`PORTABLE_NAVIGATION_RETENTION` with `PORTABLE_INPUT_NAVIGATION_LOCAL` for the
new bridge and the qualified System adapter. This opt-in requires the
`portable_input_navigation_retained()` declaration and callback-boundary checks
in the paired System source. Historical recipes without this opt-in retain
their old link contract; replacing only their bridge path is insufficient.

The selected Runtime should expose its size-checked `retain_invocation` suffix.
The bridge latches terminal custody and invokes that fence once when available.
The adapter may confirm it idempotently. A retained bridge never resets its
fence within the invocation; returning to ordinary operation requires a fresh
runtime invocation. Older tables are never read beyond their reported size.

## Preserved behavior

- Completed short-key events become Back only after neutral input. Long,
  combined, unknown, and boundary events disarm; they never synthesize Back.
- Foreground claims for `input.navigation` or its underlying `input.power-key`
  suppress polling and reset consumption. Leaving the claim drains old events
  and waits for neutral before rearming.
- Sleep retains app stack/state and the full runtime physical RGB565 image.
  Width, height, stride, byte span, format, and overflow are validated for each
  borrowed frame. Copies cover every visible pixel while preserving padding.
- The deployment's retained RGB565 acquire contract still applies: acquisition
  must expose the last completed app image. This is not a new guarantee for
  arbitrary `display.output@1` providers.
- Black presentation must complete asynchronously before power preparation.
  Wake and ordinary refusal restore the complete prior frame before restoring
  invocation-local quick brightness (or the existing default without quick
  controls). An unobservable/pending submission cannot authorize cleanup.
- Existing watch sleep/alarm/motion policy helpers are reused unchanged. Their
  timer/deep progression, alarm deadline reconciliation and light-resume
  reanchoring, tap preference encoding, and generic motion default selection
  remain in effect. Clean unavailable power services refuse sleep honestly.
  A missing alarm service uses the existing non-alarm policy; unreadable tap
  preferences retain the existing crown/timer-only behavior.

## Terminal boundaries

Native RETAINED and CONTEXT results, a partial failed grant acquisition, a
failed release, or failed boolean service operation with failed fresh owner
health latch terminal custody. CONTEXT is translated before entering the old
policy helpers' result handling. Alarm errors use their own code domain and
are never confused with identically numbered native errors. Failed motion
unarm is retained under the existing policy.

Guarded policy callbacks prevent later provider calls even if an unchanged
helper continues its local rollback branches. No later runtime polling,
diagnostics, allocation/free, display restore, or second grant release occurs.
The framebuffer snapshot remains reachable on retained return. Runtime release
is passed a copy, preserving the bridge's original descriptor if a refusing
provider mutates its output. A normal clean refusal is not reclassified merely
because a service returned false. Failed diagnostic callbacks are health-checked
too; void cooperative yields are followed by an owner-health check before the
next display callback. A healthy pre-policy display failure rolls back the saved
image and brightness once after confirmed frame release/retirement.

## Host qualification

Run `scripts/test_portable_watch_bridges.py` with `--system`, `--utilities`, and
`--output`. Dependency headers are extracted by git object from System
`269440c1a6b72407cc57f02f99dc4ec2c47402c0` and Utilities
`fc0bdbbbc600ce891e60c44d091fce0586ade7d8`; ongoing worktree edits cannot alter
those inputs. Basic, alarm, and full-feature profiles run separately under
normal execution, AddressSanitizer, and UndefinedBehaviorSanitizer at 240×240
and 410×502. Cases include repeated open/close and sleep/wake, ordinary
refusals, API rejection, row/padding guards, async completion, timer/deep entry,
alarm and motion policy, ownership overlap, and retained fault injection.

Run `scripts/test_portable_watch_adapter_bridge.py` against the reviewed paired
System checkpoint using `--system-ref` to execute the actual Settings app/adapter and generic
navigation bridge together. The receipt records its exact System HEAD and
transitive source/header hashes. The paired hook checkpoint is
`c088e3cca918618d65a81a4d39e21f81bd67a80a`. It covers normal repeated lifecycle and failures at open,
foreground, reset, poll and close, including API rejection, partial grants,
release refusal and post-retention reentry. Use an immutable checkout of the
final paired System checkpoint for reproduction.

The test environment may reject LeakSanitizer because of ptrace. Scripts detect
that specific error and disclose the limitation in receipts before rerunning
with leak scanning disabled; AddressSanitizer remains enabled. Explicit
allocation/free counters and no-free-after-fence assertions remain active.
These are host/source qualifications. They do not establish physical wake,
actual provider availability, panel behavior, battery readings or a delivered
firmware image.
