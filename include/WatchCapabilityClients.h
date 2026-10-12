#pragma once
#include "RiscRuntimeV1.h"
#include "RiscBatteryGaugeV1.h"
#include "RiscPowerSleepV1.h"
#include "RiscPowerKeyV1.h"
#include "RiscDisplayPowerV1.h"
typedef struct {
    const risc_runtime_api_v1 *runtime;
    risc_runtime_capability_v1 grants[4];
    bool live[4];
    const risc_battery_gauge_api_v1 *battery;
    const risc_power_sleep_api_v1 *sleep;
    const risc_power_key_api_v1 *key;
    const risc_display_power_api_v1 *display_power;
} watch_capability_clients;
bool watch_capabilities_open(watch_capability_clients *,const risc_runtime_api_v1 *);
bool watch_capabilities_close(watch_capability_clients *);
bool watch_capabilities_can_sleep(const watch_capability_clients *);
bool watch_capabilities_key_events(const watch_capability_clients *,uint32_t *);
