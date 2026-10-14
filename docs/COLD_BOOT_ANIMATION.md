# Shared cold-boot animation

The shared default build selects WATCH_COLD_BOOT_ANIMATION together with WATCH_FAST_WAKE. On a Runtime exposing the optional first-default callback, cold/reset entry displays the existing RiscRTE intro, then transitions to the clock. Deep wake, later child returns, and older Runtime prefixes retain the fast path. The application checks the capability-reported retained-wake cause; there is no board-name test.

The build and fast-wake fixture stage one merged SDK include directory, with the application's current SDK headers overriding the locked dependency copies. This avoids mixing pragma-once headers from two physical paths while preserving dependency revision checks.

Validation: normal and ASan/UBSan cold/deep/return cases at 240×240 and 410×502, full-height Home raster/input checks, 362 square-display golden frames, and both shared target ELFs built with GCC 8.4 and accepted by the Runtime ELF validator. Host modeled timing is not physical device timing.
