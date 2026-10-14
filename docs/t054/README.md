# T054 shared Home atomic preferences

This builds on the deployed 0.15.2 source b62a0e5. All 133 Home input hashes match its recorded build and the baseline ELF is ecb9cdb78efed72d475b929255f50a96b3da4f8516c020a54fd9b6a30008c328. Compressed face/calendar assets, cold-boot intro, native-height layout, generic capability clients and fast-wake RTC reconciliation remain selected.

Quick saves and explicit face selection use copied USER transactions. Low-battery and consumed Contexts policy changes use the separate AUTOMATIC contract. Face saves capture their displayed base before editing; same-value intent, stale refusal, invalid selection and cancelled editor are covered. Native retention blocks every later atomic access, including cleanup, and does not double-retain an already halted invocation. The first input poll precedes low-battery transaction waits; following work samples current monotonic time.

Validation: the actual Quick/face tests pass ASan/UBSan. The full fast-wake fixture passes normal and ASan/UBSan builds at 240x240 and 410x502, including repeated refresh, +/-five-minute native drift reconciled against RTC after wake, transient RTC failure/retry, native custody fences, low battery, alarms and sleep refusal. All fixture translation units now use the same low-battery session layout. Five copied low-battery mutations add five explicitly modeled one-millisecond waits; physical timing is not claimed. GCC 8.4 target Home and return-module builds pass Runtime structural admission.

Product activation is still separate: exact native writer registration, cumulative cohort admission and actual-candidate preserving-update verification are required before firmware publication. No device was flashed or hardware-qualified.
