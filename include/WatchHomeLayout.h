#pragma once
/* Home geometry is expressed in the same width-scaled logical units as input.
 * A face remains a 240px square; surrounding UI uses the additional height. */
#include "WatchLayout.h"

static inline unsigned watch_home_height(unsigned height) {
    return height>=240u && height<=WATCH_LAYOUT_MAX_SIDE?height:240u;
}
static inline unsigned watch_home_extra(unsigned height) { return watch_home_height(height)-240u; }
static inline unsigned watch_home_face_top(unsigned height) { return watch_home_extra(height)/2u; }
static inline unsigned watch_home_picker_center(unsigned height) { return 92u+watch_home_extra(height)/2u; }
static inline unsigned watch_home_alarm_dismiss(unsigned height) { return watch_home_height(height)-90u; }
static inline unsigned watch_home_alarm_retry(unsigned height) { return watch_home_height(height)-42u; }
