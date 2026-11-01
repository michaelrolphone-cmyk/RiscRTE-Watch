#ifndef RISC_RESIDENT_IDLE_V1_H
#define RISC_RESIDENT_IDLE_V1_H
#include "RiscResidentShellV1.h"
/* Additive guarded checkpoint reasons; no Runtime ABI/table change.
 * Both require settled application stream/background/UI ownership and zero
 * request flags/reserved. QUERY: OK with zero reply flags advertises support;
 * BUSY means unsupported or temporarily unsettled, with no idle effect.
 * APPLY: existing configured, alarm-aware idle policy only. Never reads or
 * consumes manual intent to select sleep/exit. Normal alarm handling and sleep
 * wake-discard remain owned lifecycle work. REDRAW/CONFIGURATION_CHANGED and
 * genuine deferred Deep EXIT retain their existing meanings. No forced sleep.
 * Old hosts return BUSY. RETAINED or malformed replies forbid later I/O.
 */
#define RISC_RESIDENT_QUERY_IDLE_ONLY UINT32_C(0x49444c01)
#define RISC_RESIDENT_APPLY_IDLE_ONLY UINT32_C(0x49444c02)
#endif
