#pragma once
/* Settled alarm-only negotiation; no Runtime ABI/table change.
 * QUERY performs no provider operation. OK with zero reply flags advertises
 * an admitted validated alarm service; BUSY means unsupported/unsettled.
 * Never substitute POLL/POLICY. CHECKPOINT_ALARM (3) reconciles the existing
 * alarm UI without selecting idle sleep, controls or exit from manual keys.
 * Both require settled child UI/capture/background ownership and zero flags/
 * reserved. Existing alarm-modal dismissal remains permitted. APPLY may
 * REDRAW. Unknown/RETAINED forbids later I/O. Not capture-safe attention. */
#include <stdint.h>
#include "RiscResidentShellV1.h"
#define RISC_RESIDENT_QUERY_ALARM_ONLY UINT32_C(0x414c5201)

/* ALR01 support also guarantees this settled, cached outcome query.
 * Call immediately after successful IDLE_ONLY application, before reopening
 * child owners. OK/0 means no alarm input cancellation; OK/REDRAW means an
 * alarm/cue/error modal cancelled input during that application (including
 * final reconciliation). BUSY is retry, never a false outcome. Repeated
 * queries are idempotent. New idle attempts and child transitions invalidate
 * the prior result. No provider calls are made by the Home query itself. */
#define RISC_RESIDENT_QUERY_LAST_IDLE_ALARM UINT32_C(0x414c5202)
