#pragma once
/* Native display lease validation. Home draws directly into the capability's
 * physical raster surface and consumes the same native touch coordinates.
 * The adapter owns no image buffer, scaling map, or retained framebuffer. */
#include "RiscDisplayOutputV1.h"
#include "RiscTouchV1.h"
#include "WatchLayout.h"
#include <stdbool.h>
#include <stdint.h>
#define WATCH_CANVAS_SIDE 240u
#define WATCH_VIEWPORT_MAX_SIDE 2048u

typedef struct {
    risc_display_output_api_v1 display;
    const risc_display_output_api_v1 *native;
    risc_display_surface_v1 lease;
    uint32_t width,height;
    watch_layout layout;
} watch_viewport;

typedef struct {
    risc_touch_api_v1 api;
    const risc_touch_api_v1 *native;
    const watch_viewport *viewport;
} watch_viewport_touch;

bool watch_viewport_open(watch_viewport *,const risc_display_output_api_v1 *);
void watch_viewport_close(watch_viewport *);
bool watch_viewport_map(const watch_viewport *,uint16_t *,uint16_t *);
bool watch_viewport_touch_open(watch_viewport_touch *,const watch_viewport *,const risc_touch_api_v1 *);
