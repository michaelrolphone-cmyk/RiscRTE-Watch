# Optional Home performance phases

Selection: `scripts/build.py --native-performance-runtime <exact clean Runtime 273bdbccf65647a803ab0586a418f8283732baec>`.
The native firmware must independently enable its existing RAM recorder. This stages only canonical `RiscPerformanceV1.h`, verifies the trace table prefix against the retained Watch SDK, and adds `WATCH_PERFORMANCE_TRACE` to default/Home only. It does not switch legacy profiles or replace the extended Watch ABI. Omission leaves instrumentation compiled out.

Milestones are RAM-only Runtime trace calls: first accepted single contact starts an interaction; accepted launcher/picker actions and taken Quick actions record dispatch; each draw records first-draw and raster span begin/end; presentation records submit attempt and confirmed transfer completion. A new contact starts a new diagnostic correlation, not a new application gesture. Raw coordinates, strings, preferences, or other user data are never recorded. Tag 0x1001 denotes raster; 0x400 denotes Home; dispatch 0x400+action identifies launcher/picker action and 0x410 identifies a taken Quick action.

Every canvas checkpoint increments only a saturating local counter; touch capture increments only a local count except the first-contact milestone. No per-pixel Runtime/provider/serial callback is added. Completion emits counts tagged 0x41 (raster checkpoints) and 0x42 (samples since raster begin), saturated to 65535; counters reset afterward. Counter values do not claim raw device-event rate or physical motion.

The existing shared guard checks invocation-local touch, clock, atomic, Quick, alarm, Contexts and BLE custody before every optional callback. No health poll, allocation, provider acquisition, touch poll, formatting or serial write is added by the helper. Missing/disabled/legacy trace API is harmless. Runtime owns bounded ring storage and explicit `perf` command replay; normal capture has no unsolicited transport.

Boundaries: this diagnoses the existing synchronous Home raster and presentation; it does not make Home cooperative, alter animation cadence, or prove device FPS. Completion means provider transfer completion, not measured visible pixels. The ring can overwrite old milestones; Runtime reports lost records. Existing sparse sleep/Quick interaction logs remain a separate mechanism.
