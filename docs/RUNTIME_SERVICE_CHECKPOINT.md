# Home's settled native-service checkpoint

Home/default 0.18.0 consumes the optional append-only `service_checkpoint`
suffix from Runtime API v1 through the exact Runtime SDK helper
`RiscRuntimeCheckpointV1.h`. It does not request file grants, choose a physical
storage backend, expose an application serial interface, or change which native
namespaces a product admits. The native endpoint remains a separate optional
firmware feature. Both watch products build the same Home sources and inputs.
The tiny `clock.elf` return-to-default module is unchanged.

## Placement and results

The first call is at the top of the foreground `while(alive(&now))` loop in
`apps/clock/crown.c`. Startup has completed a visible frame before reaching this
loop. A preceding normal frame has completed `present()` before the next
iteration, and no new frame is acquired yet. Runtime independently rechecks its
resource and invocation custody before admitting service work.

The default-off `WATCH_SERIAL_TRANSFER_MODE` build option adds cooperative exit
handling for a selected dedicated-transfer product. After a healthy OK/BUSY
result, Home checks the existing size-checked `exit_requested` suffix before any
broadcast, alarm, context, navigation or sleep work. An exit goes through `done`
and normal checked cleanup; terminal custody still takes its original return.

The selected clock-first build also calls the checkpoint after rendering,
completed presentation and pacing, before deferred or other optional providers.
This second call requires the existing cancellation suffix and a non-null
callback. The read-only exit query cannot parse a HELLO arriving during a frame,
so the settled checkpoint must run first. Missing, truncated or null cancellation
suffixes keep their original cadence. Feature-off products also retain one call
per turn. Runtime owns admission and pending-pair refusal; Home does not change
the paired-boot confirmation sequence or manufacture confirmation.

There is no service checkpoint inside `present()`, `pace_frame()`, raster
callbacks, ordinary `yield_ms`, or a pending-frame/BUSY wait. The callback can
perform bounded synchronous I/O and must never be used to turn ordinary yielding
into provider dispatch.

- Missing suffix or a null optional pointer: no-op success, preserving older
  Runtime compatibility without reading beyond its reported table size.
- `RISC_RUNTIME_CHECKPOINT_OK`: continue the normal foreground iteration.
- `RISC_RUNTIME_CHECKPOINT_BUSY`: continue normal input/rendering/scheduling;
  defer automatic idle sleep before closing input or blanking the panel. Another
  service attempt occurs only at a later settled loop boundary. The next OK
  result re-enables the existing idle deadline without resetting it. Explicit
  crown sleep requests retain their normal preparation/refusal behavior.
- In the selected transfer build, `exit_requested` after OK/BUSY: stop admitting
  work and run checked cleanup. A cleanup refusal preserves its existing
  retention fence.
- `RISC_RUNTIME_CHECKPOINT_CONTEXT`, `RISC_RUNTIME_CHECKPOINT_RETAINED`, or an
  unknown result: return promptly from `app_main`, with no subsequent provider
  calls, speculative cleanup, or callback retry. Runtime owns retained custody.

No timing qualification, hardware safety claim, physical serial transfer, or
native namespace admission is implied by this source integration.

## Verification

```sh
python3 scripts/test_runtime_service_checkpoint.py \
  --system /path/to/pinned/RiscRTE-System-Apps \
  --utilities /path/to/pinned/RiscRTE-Utilities \
  --output /tmp/home-checkpoint-proof
```

The host fixture executes the production Home controller and renderer against a
display that reports ACTIVE for 95 modeled milliseconds per frame. It verifies
that service callbacks run only after COMPLETE with no held/provider-owned
frame, and that direct presentation polling, frame pacing and ordinary yields
never dispatch the callback. Real truncated allocations exercise both older API
boundaries and a partially present suffix, including under ASan/UBSan.

The same compiled test binary exercises old, null, OK, BUSY, CONTEXT, RETAINED and
unknown-status paths. Both 240×240 and 410×502 display dimensions are tested in
normal and sanitized modes. Terminal callbacks fence subsequent runtime/provider
operations and verify that Home does not release grants after the fence. The
runner records source hashes, compiler commands and results in `receipt.json`.
Leak detection is disabled in these traced-executor host tests; this is not an
LSan result. Hardware remains untested.

`scripts/test_home_transfer_exit.py` executes real `crown.c` regressions for a
transfer before phase 0 and during frame pacing, active-background cleanup,
cleanup retention, unchanged pending-pair confirmation, and absent/truncated/null
Runtime cancellation suffixes. It runs both display sizes in normal, ASan and
UBSan modes and freezes compiler-reported source/header inputs. `--feature-off`
verifies unchanged one-checkpoint cadence, including when the cancellation
suffix is available. Its Runtime callbacks are modeled; native parser and
admission qualification belongs to Runtime's separate suite.

Select this Home variant explicitly with `scripts/build.py --serial-transfer-mode`
alongside the existing pinned dependency, role and automatic-provision options.
The flag applies only to `default.elf`; `clock.elf` remains the return module.
A dedicated-transfer firmware must package the newly built Home artifact rather
than retaining a previous product's Home. Omitting the option leaves default
compiler flags unchanged.

Target builds use the exact dependency revisions and GCC toolchain declared in
`dependencies.lock.json`. The build's source-byte manifest includes both copied
SDK headers and the common Home call site. Runtime ELF validation remains
required for both app roles.
