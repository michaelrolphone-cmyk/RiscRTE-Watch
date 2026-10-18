# Home navigation during admitted native service work

Home consumes Springboard swipes while the existing settled service checkpoint
reports BUSY, before closing touch or automatic provisioning. It renders one
`SERVICE BUSY - RETRY` status and keeps frame completion, input polling, capture,
and later service checkpoints progressing. The swipe latch is always cleared;
keeping it set would suppress sampling, rendering and pacing. The touch
recognizer already requires neutral followed by a fresh contact. The rejected
swipe never becomes a launch when the session expires or disconnects.

The existing WiFi application fallback consumes its navigation action before
navigation-related closure in the same way. Ordinary inline radio controls are
unchanged. A later authoritative Runtime refusal reopens touch; Springboard
refusal also displays `NAVIGATION UNAVAILABLE`, since the boolean launch API does
not distinguish busy service work from other launch failures.

Contexts pending export admission waits before pause/begin-export. Its automatic
workflow ABI has no non-destructive peek: claim advances the cursor before the
step is known. Home therefore defers workflow claims while BUSY, including timer
and message steps. Capture, status, rule evaluation and rule settings continue.
The original pending export/workflow remains available after OK, without a false
unsupported/failed result. This is automatic work, unlike a consumed user swipe.
No new service, ABI, timeout, file grant, cable-presence rule or feature is added.

The Runtime companion must freshly test its existing native service readiness
hook immediately before committing a launch. This closes admission after Home's
sample; the hook also covers native Settings work, so the UI says service busy.
Home cannot revoke a launch already committed before a new serial HELLO.
There is a separate Contexts late-admission limitation: if a session starts after
Home sampled OK but before its launch request, Runtime safely refuses the handoff,
but the existing Contexts caller still reports the already-claimed workflow as
failed or the already-begun export as unsupported. The checkpoint BUSY precheck
preserves these outcomes only when BUSY was observed before claim/begin. This
change deliberately adds no pending-result state or retry semantics for that
race. The late-admission regressions verify a single generic failed outcome,
continued same-session PATH service and rendering, and a new explicitly requested
export/workflow succeeding after release. Production Utilities source at
`4fdcad2495c33fe63f5f6faca9b11ced3c739a5b` maps unsupported export results to
`MODEL_FAILED`, not `MODEL_UNAVAILABLE`, and accepts a later request_export;
workflow failure consumes/logs that step without disabling future workflow work.
Thus the limitation is generic failure feedback and an explicit retry, not a
permanent capability disable. The Home fixture models these Contexts callbacks;
this statement about provider state also rests on source inspection. Native
shutdown, failed-close retention, and explicit crown sleep remain authoritative.

## Reproduction and qualification

Run `scripts/test_serial_session_idle.py --navigation-only --system SYSTEM
--utilities UTILITIES --runtime RUNTIME --output RESULTS`. Add `--home RELEASED
--expect-baseline --normal-only` to reproduce the released behavior without
editing the released source. Without `--navigation-only`, the existing idle
suite still runs unchanged. Each receipt records exact source-byte/dependency
hashes, generated-fixture and binary hashes, commands, and per-case outcomes.
The fixture source is preserved beside each receipt. A baseline `PASS` means
that the test reproduced its expected unsafe behavior, not that it was safe.

The fixture executes production Home, touch recognition, rendering, and the
Runtime's real `RiscSerial::Files` parser/session. It drives a real left swipe
while PATH is drained, queued, or has an undrained response. The rejected contact
is held across expiry/disconnect before neutral and a fresh gesture. The WiFi
fallback injects its existing action bit at the input boundary with optional
radios unavailable; Contexts provider callbacks are modeled and count destructive
claim/begin/report calls. A separate message case checks the deliberate claim
compatibility tradeoff. A late-admission case admits real HELLO/NS/PATH at the
modeled launch boundary after the settled checkpoint returned OK.

All peripherals, Runtime API adapters and time remain modeled. Display transfers
take 95 modeled milliseconds. These are host tests, not ESP32, USB, browser,
physical sleep, target build, or complete native Runtime scheduling measurements.
Leak detection is disabled explicitly; address and undefined behavior sanitizers
are enabled for the sanitized matrix.

## Evidence

- `baseline-first`: five 410x502 normal cases recorded before production edits,
  exact released Home `5ea25dda1c6c7b254e35ac649c91f357f7fcc6ee` and Files at
  Runtime `7bcac96868bf37c33022e0d1f00997e91f187b02`.
- `baseline`: 14 extended cases at both dimensions, 28 normal runs.
- `final`: the same 14 cases at 240x240 and 410x502, normal and ASan/UBSan,
  56 runs. Source-byte hashes identify the tested changes on the recorded base
  HEAD; `source-manifest.json` verifies their correspondence to this checkpoint.
- `idle-regression`: all seven existing idle cases at both dimensions, normal
  and ASan/UBSan, 28 runs.

At 410x502, released Home closed touch and accepted Springboard at approximately
6.3 modeled seconds with PATH active, pending or undrained, before expiry or
disconnect. Fixed Home recorded zero busy-time closes/launch calls, one bounded
status, 317 visible frames and 4,221 input polls for the drained case, and 333
frames/4,413 polls for pending/undrained PATH. The command/reply drained normally.
After expiry only a fresh gesture launched. Disconnect behaved the same way;
the no-session control kept its original behavior. The late-admission control
refused once at Runtime's modeled boundary, restored visible Home and input,
and accepted only the later fresh gesture. Contexts claimed/began only after OK,
with no failed/unsupported report, while capture and evaluation continued.

## Additional unchanged safety controls

The full fast-wake suite passed at both dimensions normally and sanitized,
including failed-release retention, cleanup, manual sleep and provisioning.
The service-checkpoint suite passed both dimensions normally and sanitized.
Crown native lifecycle passed normal/sanitized at 240x240, 410x502, 502x410 and
120x150. Home first-exit checkpoint tests passed normal, address-sanitized and
undefined-sanitized at 240x240 and 410x502. Their logs and checkpoint receipts
are preserved here. Runtime companion source is separately frozen at
`d172878cec0431e98ddcd4bf3651a227cbc23c6d`; this Home harness links the exact
released Files source, whose bytes the companion does not change.

A supplemental attempt to run the complete production Contexts provider was
not executable with the exact release Drivers pin: `5890721073ebabd781d5d278e447c761947a5c58`
provides `sdk/driver/RiscRadioIqV1.h` without the temporal/envelope types required
by the selected Utilities source. `contexts-provider-check` records the exact
fetched header/blob, blocked compiler log, generated test and source-review
hashes. No replacement header was invented. This does not affect the passing
Home/Files boundary matrix; the full provider retry statement remains a source
inspection result, not an executed provider test.

## Versioned successor

The prior qualified source checkpoint `c7fd39e9e45cd821eaeb7560ef010d4679c9ba5e`
is retained. The shared builder now assigns 0.20.3 to automatically provisioned
Home and 0.19.3 to ordinary Home; the ordinary source manifest matches 0.19.3.
Return-to-default stays 0.15.0 and its rebuilt ELF is byte-identical to both
released return artifacts. The legacy 0.8.0 manifest is unchanged.

`versioned` preserves both pinned Xtensa build receipts, selected manifests,
independent replay proofs, version-selection controls, and a fresh 56-run
normal/ASan/UBSan navigation matrix. Existing release files were not replaced.
The new pull-request CI runs exact-head Home with pinned production Files and
the idle compatibility matrix; native Runtime admission remains separately
qualified by the companion change.

### Confirmation fixture compatibility

PR CI run `39107863994` exposed the older confirmation `long-busy` fixture's
expectation that a swipe launches while the checkpoint remains BUSY. The same
assertion failure was reproduced locally before changing the fixture. That
expectation contradicts the new navigation admission contract.

The fixture now submits one action, verifies it is consumed with one busy
diagnostic and zero launches, and checks visible frames and confirmation wait
progress through its 30-second modeled observation window. Confirmation success,
quiescent refusal and retained-state assertions are unchanged. All five cases
passed at both dimensions normally and under ASan/UBSan (20 runs). Evidence and
exact tested source hashes are in `confirmation-compatibility`. This is a
test-only correction; Home production bytes, versions and qualified ELF hashes
are unchanged from the versioned checkpoint.
