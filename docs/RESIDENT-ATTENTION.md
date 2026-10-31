# Capture-safe attention (candidate)

Based on published Home e462545c20f32014f013cfcaa3840cfcb4d75d6e, not the unpublished Files policy extension. SOURCE-BASE.json joins all 149 prior Home build inputs and selected Watch 0.21.1 / Wave joint 0.21.2 targets. New versions are 0.23.1 / 0.23.2.

The optional Runtime attention client preserves the original resident-client prefix/sizeof. Old Runtime continues the original Home behavior, but does not provide capture-safe foreground attention. X4 Home is not changed.

Before child entry, and after successful settled policy/controls, Home reconciles alarm state and copies the existing prepare_sleep deadline (including Points). This is only an observation hint; it is not sleep authorization or an acknowledgement. RTC acquisition, alarm steps, touch opens/closes, and output modal work happen only at this settled boundary.

During foreground capture the Runtime calls a narrow observer with its native monotonic timestamp. Home reads only already-owned alarm.status, key.events and (at most once a second) rtc.read. Alarm status is a memory-only copy. Watch key events read/ack classified PMIC key IRQ bits; Wave key events read the already-claimed GPIO and consume debounced events. Those reads only latch intent, never invoke sleep, acquire focus, open a display/input session, acknowledge an alarm token, refresh/step a service, write settings, or yield. Runtime identity is checked after each observation. Uncertain output, malformed/unknown status, key failure, and identity replacement fence further calls.

A changed/active alarm snapshot, a due copied deadline, or invalid/backward/discontinuous RTC requests ordinary settled reconciliation. The foreground stops capture/background and closes presentation before POLL(INHIBIT_IDLE) and POLICY. The policy alone reconciles and acknowledges exact current tokens through the original modal. Quiet capture causes no checkpoint or DSP restart.

Manual bits preserve the selected common Home meanings: short is sleep intent, long is Home intent. A foreground may reject copied manual bits locally, without rejecting ALARM. Spectrum projects these observations through its own original short-Back/long-ignore semantics; this is not a new hardware branch or a change to common Home policy. A rejection does not call any provider and cannot cancel an already-requested Runtime exit.

Focused tests include the production observer with endpoint doubles and full production-flag Home with Runtime/hardware doubles. They are not physical-device or complete firmware proof. Actual Runtime/application integration is separately owned.
