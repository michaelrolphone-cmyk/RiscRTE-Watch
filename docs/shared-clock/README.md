# Shared boot-local clock reader

Watch vendors the exact header-only Runtime clock reader from the commit recorded in `sdk/app/RiscRuntimeClockV1.provenance.json`. `merged_sdk.headers` checks that digest and stages the same bytes; `test_shared_clock_sdk.py` compares them with the canonical owning Runtime source. Historical Runtime profile pins are not moved to obtain this additive helper.

Adopted callers are Home `alive`, sparse interaction timestamps, the basic Clock loop, and Bringup's timing loop. A present monotonic callback is used once; its failure never falls back to health. Older prefixes and null suffixes retain health-based uptime. Zero and uint32 wrap remain valid. No native ABI, wall-clock authority, sleep policy or capability grant changes.

Home preserves its invocation-local retained/unavailable fences and present-clock failure latch. Other callers retain their existing failure/unwind behavior. Genuine health metrics remain unchanged. This is common source adoption, not evidence that every historical immutable firmware has been rebuilt or that physical performance is qualified.

Owning tests cover canonical staging, current/legacy Home reads and failure boundaries, actual Home authority-loss continuations, interaction trace callback count/budget, and the full Bringup lifecycle suite with both legacy health and current cheap-clock tables. The basic Clock test executes its actual renderer and app_main in21 normal and21 sanitized scenarios, including exact redraw/timeout boundaries and unchanged cleanup ordering. Source/host tests are separate from target composition and device acceptance.
