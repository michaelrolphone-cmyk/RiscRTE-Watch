/* Compile the pinned shared Quick controller unchanged. Only its read fallback
 * is selected here; saved timer validation, grants, writes and UI stay shared.
 * Include the policy before substitution so saves keep their original reader.
 * Every load, including Contexts/low-battery reloads, sees the same fallback. */
#include "WatchSleepDefaults.h"
#define portable_sleep_timer_load watch_sleep_timer_load
#include <quick_session.c>
#undef portable_sleep_timer_load
