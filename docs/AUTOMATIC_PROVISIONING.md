# Optional shared Home provisioning

The explicit `scripts/build.py --automatic-provision` selection produces Home0.19.0 with the same source and ELF for both watch products. No board, resolution, preference backend or SD instance is compiled into Home. Unselected builds retain the existing Home0.18.0 feature selection.

The foreground controller starts only after the completed first frame, deferred service admission and input. It pauses Contexts/broadcast during catalog/network work and completes cancellation before modal or handoff transitions. Cached local discovery precedes network acquisition. Saved radio policy remains authoritative.

A selected Home requires `software.installed-apps@1` and optionally requests `storage.volume@1` at instance0. Runtime resolves only the one volume explicitly authorized to the app. Missing optional authority falls back to internal discovery; ambiguous authority fails admission. A product supplies its actual SD grant in its boot policy. Preference and crash storage remain generic provider-backed contracts.

Private first-fetch time accepts only a strictly validated fresh native UNSET snapshot as its zero token. Valid native time is usable only after pending RTC reconciliation clears. Malformed, I/O and retained-context snapshots fail closed. This read-only check does not seed native time, read/write RTC, or alter face/reconciliation state.

## Qualification

`test_fast_wake_clock.py --automatic-provision --atomic --crash-report` passes normal and ASan/UBSan at240x240 and410x502, including actual Home foreground admission, absent/authorized optional volumes, saved-radio fence, service pause/resume, cleanup and strict UTC snapshots. The previous visible-RTC, native-frame, sleep-refusal and fast-wake cases remain in the same runs. Leak detection is disabled under the executor's ptrace limitation.

The selected SDK6/GCC15 target link and Runtime structural validator pass: default.elf625648 bytes, SHA256db23f172746ab32c0ba9c94d4c7a93d99be5f16247b6f267b48af8d7ca252d6f. The return module remains1236 bytes, SHA256f29065b5a57b88f8fd6c2a328c0865dd2628cc484da30f6c23470a82ae8c87bc. Exact System/Utilities inputs are in dependencies.lock.json. Existing universal CI builds both selections and verifies independent byte-identical selected builds.

This is application-source/target qualification. Product stores, feed publication, full firmware link and physical provisioning remain separate gates. No released serial package is changed by this source checkpoint.
