# Home idle during a native serial session

Home uses its existing settled `service_checkpoint` result to qualify automatic
idle. BUSY leaves input, rendering and later service checkpoints running, before
closing touch or clearing the panel. The next OK result re-enables the existing
idle deadline; no new timeout or activity timestamp is added. An explicit crown
sleep request keeps its existing native preparation/refusal path.

This requires the Runtime companion change that reports BUSY when successful
native service dispatch leaves its existing `sleepServicesReady` gate false.
The shared Runtime SDK comment now documents that BUSY may follow a completed
command. There is no ABI change, cable-presence policy, new service, or file grant.
The native sleep preparation and CPU readiness checks remain authoritative.

## Host regression

Run `scripts/test_serial_session_idle.py` with `--system`, `--utilities`,
`--runtime` and `--output` pointing to exact source inputs and a fresh result
folder. Add `--home` and `--expect-baseline` to reproduce the original Home
behavior without modifying its source. The runner records commands, dependency
hashes, binary hashes and per-case outcomes. `--prepare-only` writes input hashes
without compiling. Builds are single-job and remove their temporary objects.

The fixture executes production Home and its renderer, with a display transfer
that takes 95 modeled milliseconds, and links the Runtime's real
`RiscSerial::Files` parser/session implementation. HELLO, NS and PATH replies are
fully drained; the real pending bit is false in between requests. A saved 10-second
Home idle preference exercises that gap before the real 30-second session expiry.

Cases cover the old OK-only checkpoint signal, expiry after inactivity, renewal
by a later PATH command, disconnect, no session, and an explicit crown request
while BUSY. Assertions check visible frames and input during BUSY, panel blanking,
SERVICE refusal labels, settled callback placement, clean resource release,
and eventual sleep after BUSY becomes OK. Both 240x240 and 410x502 run normally
and under ASan/UBSan; leak detection is explicitly disabled.

Runtime's service/sleep adapter and all peripherals remain modeled API
boundaries in this fixture. Native Runtime scheduling, target builds, physical
sleep/USB, and the browser installer's behavior across a confirmation pause
longer than the session timeout require separate qualification. A session may
arrive after Home's initial checkpoint; the authoritative later sleep
preparation can still refuse it. This change specifically defers idle while the
settled checkpoint already reports BUSY.

## Evidence snapshots

The separately archived six-case baseline runs exact Home commit
`416fa9f144fa5bd5471e6f4c9cefff21455c8361`. Its original fixture, bridge,
runner and generated C source are preserved under `baseline-fixture`; their
hashes were checked against that receipt before archival. This first bridge
had no namespace map, so NS returned a normal NOT_FOUND response. The extended
fixture uses one metadata-only namespace and requires successful HELLO/NS/PATH
responses; it performs no native storage operation.

The additional `blanking-arrival` case injects HELLO while the pre-sleep blank
frame is ACTIVE. The host queues NS immediately upon HELLO's reply, and PATH
immediately upon NS's reply. The actual Home call chain must restore visible
rendering after one SERVICE BUSY refusal, never enter physical sleep, and
answer all three requests within 5,000 modeled milliseconds individually.

## Measured results

All 28 extended cases passed at both sizes normally and under ASan/UBSan.
`final/receipt.json` records the exact source-byte inputs and command matrix.
`baseline/receipt.json` retains the six original 410x502 normal controls.

- Active drained session: baseline attempted sleep at 10,950 ms and displayed
  two SERVICE refusals. Fixed Home rendered 307 visible BUSY frames with 3,744
  input polls, no BUSY-time blank/refusal, and one sleep after session expiry.
- Renewed PATH session: fixed Home rendered 541 visible BUSY frames and slept
  after the renewed 30-second inactivity interval, without a false error.
- Disconnect: the first later OK checkpoint enables the already-expired Home
  idle deadline; one successful sleep, no BUSY-time blank/refusal.
- No session: baseline and fixed first sleep both occur at 10,895 modeled ms.
- Manual BUSY: baseline and fixed both attempt once at 4,775 modeled ms and
  restore the same SERVICE refusal, with zero actual sleep.
- Blanking arrival: HELLO, NS and PATH replies take 95, 95 and 1,495 modeled ms,
  respectively. One ordinary SERVICE refusal occurs; all responses arrive,
  Home returns to visible rendering, and no sleep or retention occurs.
- Old checkpoint signal: fixed Home with an always-OK service adapter still
  reproduces the refusal. The Runtime companion BUSY signal is required.

These are simulated device-time results, never ESP32 or browser measurements.
Existing broad Home checkpoint/sleep regression suites and target composition
remain separate checks; the new dedicated suite does not claim their results.
