# Settled alarm-only Home checkpoints

This additive contract separates alarm reconciliation from manual key policy.
It does not change the Runtime table, existing resident client prefix, ordinary
Controls, or the established POLL/POLICY handshake. Home packages are 0.25.0
(default), 0.25.1 (automatic provisioning), and 0.25.2 (automatic provisioning
plus serial-transfer mode). Existing static manifest templates are unchanged.

## Negotiation and ownership

RiscResidentAlarmV1.h defines QUERY_ALARM_ONLY. At a settled child boundary,
OK with zero reply flags advertises an admitted validated alarm service.
BUSY means unsupported or unsettled. Never substitute POLL or POLICY.
CHECKPOINT_ALARM (existing reason 3) reconciles the shared alarm UI without
selecting sleep, controls, or exit from manual keys. Alarm-modal key dismissal
and exact occurrence acknowledgement remain legitimate modal behavior.

All requests require zero flags/reserved and settled child UI, capture and
background ownership. These are full native custody checkpoints, not the
capture-safe attention observation interface. Old hosts return BUSY.
Malformed or RETAINED results prohibit further provider calls.

## Idle and editor cancellation

The existing idle-only QUERY/APPLY contract remains configured, alarm-aware
idle policy without consuming manual intent. Alarm-only negotiation also
advertises QUERY_LAST_IDLE_ALARM. Immediately after successful idle APPLY,
before reopening child owners, query its cached outcome: OK/0 means no alarm
input cancellation; OK/REDRAW means an alarm, cue, or error modal cancelled
input, including final reconciliation. BUSY must be retried rather than
treated as a negative outcome. Repeated queries are idempotent; new idle
attempts and child transitions invalidate the prior result.

A text client must fully pause for a focus change before these checkpoints.
Resume after a clean quiet outcome; an alarm cancellation must instead close
the editor without saving or inventing a Home-exit request. Do not use a
POLL continuity optimization to negotiate these non-POLL reasons.

## Qualification and scope

The resident-alarm workflow runs the new minimal and production-profile Home
cases, retained idle cases, and actual Runtime boundary fixtures in normal
and ASan/UBSan modes. The Runtime fixture uses deterministic host/native
safety doubles; production Home tests use endpoint doubles. Neither is a
physical-device result.

Target builds preserve System bb0cadf3cec3445a64e11513b47b2676f3fdd92d
and Utilities 4fdcad2495c33fe63f5f6faca9b11ced3c739a5b with the existing
locked GCC 15.2.0 compiler. Watch automatic and Wave joint targets each build
twice, pass the unchanged ELF validator, and undergo shared-default repeat
verification. Runtime tests pin 0052d1fc42e9cfc82d626c5a247d460919c87935.
The artifact retains source archives, commands, dependency receipts, logs and
generated targets. This document describes the qualification recipe, not a
claim that an unexecuted workflow has passed.
