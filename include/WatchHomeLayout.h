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

/* Native geometry. Resolve the design dimensions before rasterization and use
 * these same edges for input. The square core is never stretched in landscape. */
static inline unsigned watch_home_side(unsigned width,unsigned height) {
    return width < height ? width : height;
}
static inline unsigned watch_home_scale(unsigned width,unsigned height,unsigned value) {
    return (unsigned)((uint64_t)value * watch_home_side(width,height) / 240u);
}
static inline unsigned watch_home_left(unsigned width,unsigned height) {
    return (width-watch_home_side(width,height))/2u;
}
static inline unsigned watch_home_top(unsigned width,unsigned height) {
    return (height-watch_home_side(width,height))/2u;
}
static inline unsigned watch_home_picker_center_at(unsigned width,unsigned height) {
    return height/2u-watch_home_scale(width,height,28u);
}
static inline unsigned watch_home_alarm_dismiss_at(unsigned width,unsigned height) {
    return height-watch_home_scale(width,height,90u);
}
static inline unsigned watch_home_alarm_retry_at(unsigned width,unsigned height) {
    return height-watch_home_scale(width,height,42u);
}
