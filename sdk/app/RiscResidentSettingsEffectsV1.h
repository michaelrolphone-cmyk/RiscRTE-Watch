#pragma once
#include "RiscResidentShellV1.h"
/* Invocation-scoped, settled settings reconciliation. No new Runtime ABI.
 * All requests/replies have zero flags/reserved. QUERY is side-effect free;
 * old hosts reply BUSY. Query each effect before APPLY. An advertised effect
 * may still have unavailable hardware or unreadable preferences at execution.
 * APPLY returns OK after one attempt, including clean failure or pending alarm
 * reconciliation. It never reads keys, opens UI, selects sleep, connects Wi-Fi,
 * or writes preferences. Read the copied result before another APPLY; matching
 * result queries return OK, all other queries BUSY without changing state.
 * No result means no inferred success. Serialize one caller per invocation.
 * A pending alarm is advanced with PROGRESS(ALARM), never another APPLY.
 * PROGRESS calls step once and updates the copied outcome; result queries
 * remain read-only. BUSY progression means no matching pending operation.
 * Results clear on child entry/exit and before each admitted APPLY. A result is
 * evidence of an attempt, never permission to replay a committed action.
 * Root must close scene/focus and pause capture, storage, network and BLE before
 * APPLY. Malformed output, identity loss and uncertain custody are RETAINED;
 * clean failed physical effects are recorded, not RISC_RESIDENT_FAILED.
 */
enum { RISC_SETTINGS_BRIGHTNESS=1, RISC_SETTINGS_WIFI=2,
 RISC_SETTINGS_BLUETOOTH=3, RISC_SETTINGS_ALARM=4 };
enum { RISC_SETTINGS_APPLIED=1, RISC_SETTINGS_FAILED=2,
 RISC_SETTINGS_PENDING=3 };
#define RISC_SETTINGS_QUERY(effect) (UINT32_C(0x53450100)+(effect))
#define RISC_SETTINGS_APPLY(effect) (UINT32_C(0x53450200)+(effect))
#define RISC_SETTINGS_RESULT(effect,outcome) (UINT32_C(0x53451000)+((effect)<<4)+(outcome))

#define RISC_SETTINGS_PROGRESS(effect) (UINT32_C(0x53450300)+(effect))
