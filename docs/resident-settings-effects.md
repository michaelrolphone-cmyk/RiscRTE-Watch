# Narrow resident settings effects

`RiscResidentSettingsEffectsV1.h` defines an additive reason namespace with no
Runtime table/status/reply-flag changes. `PortableResidentSettingsEffects.h` is
the shared checked host executor; `clock_resident_effects.inc` binds it to Home.

QUERY, APPLY, RESULT and PROGRESS requests have zero flags/reserved. QUERY and
RESULT do not call providers. APPLY attempts one effect and records a copied
outcome; ordinary OK transport is distinct from APPLIED/FAILED/PENDING. Pending
alarm work advances one `step` per PROGRESS without repeating `refresh`. A pending
result blocks another APPLY. Results clear at Home initialization and child
entry/exit. Root serializes requests and reads outcomes before another attempt.

Effects bypass keys, controls, sleep policy and attention-prime. Brightness,
Wi-Fi policy, Bluetooth policy and alarm refresh remain separate callbacks.
Wi-Fi permission does not connect. Malformed grants/status, identity loss,
reentry and uncertain cleanup retain custody; clean unavailable or reported
physical failure is a copied FAILED result. No preference writes occur here.

Strict real Home production-flag tests exercise the actual controller with
Runtime/hardware endpoints doubled. Separate tests compile the actual pinned
Runtime graph/ELF/checkpoint transport. These are not physical-device tests.
The old Home is separately tested and reports BUSY for the new queries.
