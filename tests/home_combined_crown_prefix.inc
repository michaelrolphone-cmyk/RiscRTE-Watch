/* Real shared entry and installed provider implementations, controlled hardware only. */
#include "RiscProviderV2.h"
#include "RiscRuntimeV1.h"
#include "RiscFailureEvidenceV1.h"
#include "RiscCivilClockV1.h"
#include "RiscCanvasInputV1.h"
#include "RiscHomePresentationV1.h"
#include "PortableTimeZone.h"
extern const risc_driver_v2 *hc_input_driver_get(uint32_t),*hc_civil_driver_get(uint32_t);
extern int hc_entry_init(void);
extern void hc_entry_main(void),hc_entry_fini(void);
static const risc_driver_v2 *hc_input_driver,*hc_civil_driver;
static risc_canvas_input_v1 hc_input_api;
static risc_civil_clock_schedule_v1 hc_civil_api;
static risc_home_presentation_v1 hc_presentation;
static bool hc_shared_input_started;
static int hc_shared_acquire(const char*,uint32_t,uint64_t,risc_runtime_capability_v1*);
static void hc_shared_start(void),hc_shared_run(void),hc_shared_expected(void);
static unsigned hc_points_reads;
static unsigned hc_expected_points_loads(void);
static unsigned hc_crash_frames,hc_crash_fresh,hc_crash_started,hc_crash_last;
static bool hc_crash_inject,hc_crash_neutral,hc_crash_release;
static void hc_shared_diagnostic(const char*);
static bool hc_crash_navigation(risc_input_navigation_frame_v1*);
static bool hc_evidence(risc_failure_evidence_client_v1*);
static unsigned hc_live_peak,hc_capacity_refusals;
static bool hc_bounded_acquire(const char*,uint32_t,uint64_t,risc_runtime_capability_v1*);
static bool hc_runtime_lost,hc_view_seen;
static bool hc_shared_release(risc_runtime_capability_v1*);

static bool hc_query_pure;
static unsigned hc_query_dispatches,hc_wifi_disconnects;
static void hc_contexts_queries(void);
static bool hc_wifi_off(void*);
static int hc_alarm_fault(const char*);
static bool hc_lost_case(const char*);

static bool hc_brightness_supported;
static unsigned hc_brightness_calls;
static bool hc_display_info(void*,risc_display_info_v1*);
static bool hc_brightness(void*,uint16_t,uint16_t);
static unsigned hc_expected_brightness(void);

static unsigned hc_evidence_calls,hc_zero_namespace_warnings;
static bool hc_save_navigation(risc_input_navigation_frame_v1*);
static uint8_t hc_dnd_value;
static unsigned hc_dnd_writes;
static bool hc_dnd_save_case(void);
static void hc_radio_ready(void);

static const void*hc_battery_api;
static bool hc_battery_detached,hc_ble_refused;
static unsigned hc_battery_acquires,hc_battery_detaches,hc_rebind_refused;
static bool hc_radio_ble_fail(void);
