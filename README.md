# RiscRTE Watch

Canonical shared Home application for ESP32-S3 watches. Device repositories supply their own hardware providers, board configuration, provisioning and complete firmware assembly.

`apps/clock/crown.c` is the maintained application entry point. The source and all original faces, effects and Points integration were extracted from RiscRTE-T-Watch-S3 commit `851b8998e804c57b2c9f42d9f8fd0a69e79f2b41` (Watch 1.1.10 integration). The older `main.c` and portable/legacy variants are retained historical references, not the current product.

## Build once, deploy the same bytes

Install `scripts/requirements.txt`, use the compiler in `dependencies.lock.json`, and check out the pinned System-Apps and Utilities dependencies:

```sh
python3 scripts/build.py --system ../RiscRTE-System-Apps --utilities ../RiscRTE-Utilities --cc /path/to/xtensa-esp32s3-elf-gcc
python3 scripts/test_viewport.py
python3 scripts/test_viewport.py --sanitize
```

The output is `dist/default.elf` plus the companion `clock.elf` used for return-to-Home. These are application roles, not board variants. No board or resolution define is passed to either compilation. `build.json` records their byte identities and loader validation. Both device packages consume identical bytes for each role; device-specific manifests bind capabilities and retain their existing app IDs/settings namespaces.

## Resolution and hardware behavior

The shared app queries the actual display dimensions and uses one matching rendering/touch transform. The 240x240 LilyGO path uses its native surface directly. On Waveshare's 410x502 display, the preserved square faces occupy a centered 410x410 area, with 46-pixel top and bottom gesture gutters. This preserves circles and existing controls; it is not yet a redesigned full-height rectangular layout.

Panel brightness and power, RTC, battery, touch, motion and radios are capability clients. Hardware selection does not depend on T-Watch instance IDs. Legacy `twatch_*` type names remain ABI-compatible aliases, not board-pin dependencies. Optional motion/BLE/context providers may be absent without stopping the clock. Admitted providers that fail still follow the existing cleanup paths.

## Status

Development integration, not a hardware-qualified release. Host tests exercise every physical pixel and inverse touch coordinate on both display sizes, landscape, padded stride, retained frames and failure cleanup. The target build checks ESP32-S3 ELF architecture, imports/exports and the deployed Runtime loader validator. Physical bring-up and complete Waveshare peripheral/application parity remain separate device-integration work. Do not replace files inside an already sealed paired firmware image; stage the shared application before the device assembler generates its paired metadata.
