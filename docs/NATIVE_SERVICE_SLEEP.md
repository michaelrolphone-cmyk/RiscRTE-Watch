# Shared Home native sleep boundary

Home calls the optional Runtime sleep transaction only after its black frame has fully completed and touch/frame leases are settled. It performs the existing shared physical sleep preparation next. Ordinary wake/refusal restores physical state before the same invocation resumes the token. Refused Runtime preparation redraws the complete clock and restores its chosen brightness. Retained preparation, retained physical sleep or uncertain resume stops cleanup and lets Runtime preserve custody.

The SDK uses the exact Runtime source pin in sdk/app/SOURCES.json. Short legacy tables return a zero-token no-op without reading absent fields; this preserves historical behavior and does not claim to fix old firmware. Full tables missing either callback refuse preparation. The new Runtime maintains its actual native/provider safety guards and the CPU Deep/reset resident-stack gate.

56 cases passed across 240x240 and 410x502 normally and under ASan/UBSan: exact short allocations, missing callbacks, success, busy/unsupported/context refusal, retained prepare, retained/busy/context/unknown resume, removed-card resume and retained physical sleep. All 36 existing service-checkpoint cases also passed. Fixtures implementing the old Runtime prefix now declare that exact prefix; the dedicated transaction fixture declares and implements the full suffix.

Home versions advance to 0.19.0 (ordinary) / 0.20.0 (automatic provisioning). The independent return-to-default module is unchanged. The previously qualified .56 Home hash remains immutable. Target build, linked product/store memory and hardware sleep/wake remain separate qualification steps.
