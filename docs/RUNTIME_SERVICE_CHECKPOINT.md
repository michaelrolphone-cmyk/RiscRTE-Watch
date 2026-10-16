# Home's settled native-service checkpoint

Home/default 0.18.0 consumes the optional append-only `service_checkpoint`
suffix from Runtime API v1 through the exact Runtime SDK helper
`RiscRuntimeCheckpointV1.h`. It does not request file grants, choose a physical
storage backend, expose an application serial interface, or change which native
namespaces a product admits. The native endpoint remains a separate optional
firmware feature. Both watch products build the same Home sources and inputs.
The tiny `clock.elf` return-to-default module is unchanged.

## Placement and results

The single call is at the top of the foreground `while(alive(&now))` loop in
`apps/clock/crown.c`. Startup has completed a visible frame before reaching this
loop. A preceding normal frame has completed `present()` before the next
iteration, and no new frame is acquired yet. Runtime independently rechecks its
resource and invocation custody before admitting service work.

There is no service checkpoint inside `present()`, `pace_frame()`, raster
callbacks, ordinary `yield_ms`, or a pending-frame/BUSY wait. The callback can
perform bounded synchronous I/O and must never be used to turn ordinary yielding
into provider dispatch.

- Missing suffix or a null optional pointer: no-op success, preserving older
  Runtime compatibility without reading beyond its reported table size.
- `RISC_RUNTIME_CHECKPOINT_OK`: continue the normal foreground iteration.
- `RISC_RUNTIME_CHECKPOINT_BUSY`: continue normal rendering/scheduling; another
  service attempt occurs only at a later settled loop boundary.
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

Target builds use the exact dependency revisions and GCC toolchain declared in
`dependencies.lock.json`. The build's source-byte manifest includes both copied
SDK headers and the common Home call site. Runtime ELF validation remains
required for both app roles.
