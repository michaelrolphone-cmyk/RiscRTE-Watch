#pragma once
#include "points_compat.h"
#include "faces/points_state.h"
/* Keep the pure Utilities time converter in its own translation unit; the
 * Watch and shared client expose ABI-identical RTC typedefs under different
 * header paths and must not both declare them inside crown.c. */
bool watch_points_projection(const points_config *,const points_meta *,uint32_t,nova_points_state *);

#ifdef WATCH_SHARED_HOME_CIVIL
#include "../runtime_features/sdk/RiscCivilClockV1.h"
/* Basis is record provenance, never device identity. No records are rewritten.
 * The result uses UTC seconds since 2000 for all historical *_rtc field names.
 * Fence nonzero policy_status before any further provider call. */
typedef enum { WATCH_POINTS_RECORD_RTC, WATCH_POINTS_RECORD_UTC } watch_points_record_basis;
bool watch_points_projection_utc(const points_config *,const points_meta *,
    watch_points_record_basis,int64_t,const risc_civil_calendar_v1 *,
    const risc_civil_clock_policy_v1 *,nova_points_state *,int32_t *policy_status);
#endif
