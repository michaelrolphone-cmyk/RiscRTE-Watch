/* Real shared entry and installed provider implementations, controlled hardware only. */
#include "RiscProviderV2.h"
#include "RiscRuntimeV1.h"
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
