# Sparse Home interaction boundary trace

Explicit `--interaction-trace` adds diagnostic-only Home markers. Historical builds leave the selector off. The selected operation, key policy, preferences, display pixels, timers and return values are unchanged.

Quick records modal opening/state-read completion and its first ensuing completed frame (not proof that an already-cancelled panel remained visible). Sleep records touch close, copied scope settlement, black frame completion, native preparation, policy entry/return and native restoration. `elapsed_ms` is actual wrapping uint32 Runtime uptime since the interaction began. `last` names the last completed boundary; a pending phase is not proof that the operation completed. `custody=prechecked` describes only the instant before the marker; no claim is made about a subsequent call.

Markers use the optional RAM-only checkpoint and the existing diagnostic queue, at most24 per interaction and only on boundaries, never per pixel/poll/frame. Missing optional checkpoint support is tolerated. Diagnostic rejection does not retry or change operation results. Health failure and retained touch/clock/preference/alarm/context/broadcast state suppress further callbacks. A retained operation therefore leaves its preceding checkpoint; it is not followed by a speculative cleanup or log. Ordinary outer exit appends its line to the preserved last interaction record.

RAM checkpoints do not survive reset/power loss. Queue output may be lost or truncated. This is instrumentation, not a physical freeze fix. The latest installed6b image has reported lock/Quick freezes without corresponding post-reset serial capture. Source-only reasoning and host tests cannot establish device success.
