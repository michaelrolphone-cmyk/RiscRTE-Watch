# Shared Bringup diagnostics

One target ELF provides shared semantic diagnostics on X4, T-Watch and Waveshare. Hardware capabilities are optional; native dimensions and presentation remain provider-owned. The original RGB565 color bars/raw-touch diagnostic is retained as an explicit native test when compatible display/touch providers are authorized. Battery, raw RTC, motion and haptic tests preserve their native behavior. No physical execution is claimed by hosted tests.

## Cumulative installation boundary

Use the published Home/Contexts baseline:
https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/releases/tag/home-shared-83afd19e5184eb511cc205299c83eb9cb5c05430

This release changes only Bringup canonical application/manifest/policy content. Other application/provider/data canonical bytes are retained, including the baseline USB application. The unpublished USB candidate is NOT included.

The three product stores and SD archives are companion application payloads, **not offset-zero full flash images**. Do not apply them to a different native firmware or product geometry. Retain the exact native firmware qualified with the baseline and follow its UPGRADE.md decoder-first, data-preserving procedure before installing an encoded store. In particular Watch must already run the published Runtime 0.4.20 decoder; older firmware cannot read its ordinary-app envelopes.

Watch reserve is enforced without enlarging the partition. Bringup and, if needed, existing ordinary application images can use the baseline's lossless RTEIMG1 envelope. assembly.json records each representation change and exact canonical/stored hashes; actual Runtime admission decodes and validates these bytes. Providers and authority are never changed to make space.

Existing hidden watch-bringup identity and Watch internal/Wave SD routes are preserved. X4 receives the same hidden identity and ELF using its existing authorized ui.scene provider. This does not silently expose a launcher entry or assign new hardware capabilities.

## Evidence

build.json records exact compiler, sources, imports/exports and target hash. qualification.tar.gz contains normal/sanitized preserved native and shared-controller tests plus exact-source actual Runtime admission. assembly.json binds three fixed geometry store/SD roundtrips and canonical byte preservation. release-manifest.json records asset hashes. Physical hardware acceptance remains separate.
