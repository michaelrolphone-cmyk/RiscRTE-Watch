#pragma once
/* Width-scaled watch layout, independent of device and provider identity.
 * Both axes use physical_width / 240. Extra height is layout space, never a
 * vertical stretch of the old square. This module owns no surface or memory.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define WATCH_LAYOUT_WIDTH 240u
#define WATCH_LAYOUT_MAX_SIDE 2048u

typedef struct { uint16_t top, right, bottom, left; } watch_layout_insets;
typedef struct { int32_t x, y; uint32_t width, height; } watch_layout_rect;
typedef struct {
    uint32_t physical_width, physical_height;
    uint32_t width, height;
    watch_layout_rect safe;
} watch_layout;
typedef struct {
    watch_layout_rect header, content, footer;
} watch_layout_stack;

static inline uint32_t watch_layout_ceil(uint32_t n, uint32_t d) {
    return n / d + (n % d != 0u);
}

static inline bool watch_layout_open(watch_layout *out, uint32_t width,
        uint32_t height, const watch_layout_insets *insets) {
    if (!out || !width || !height || width > WATCH_LAYOUT_MAX_SIDE ||
            height > WATCH_LAYOUT_MAX_SIDE) return false;
    const watch_layout_insets zero = {0, 0, 0, 0};
    const watch_layout_insets *s = insets ? insets : &zero;
    if ((uint32_t)s->left + s->right >= width ||
            (uint32_t)s->top + s->bottom >= height) return false;
    uint32_t logical_height = watch_layout_ceil(height * WATCH_LAYOUT_WIDTH, width);
    if (!logical_height || logical_height > WATCH_LAYOUT_MAX_SIDE) return false;
    uint32_t left = watch_layout_ceil(s->left * WATCH_LAYOUT_WIDTH, width);
    uint32_t top = watch_layout_ceil(s->top * WATCH_LAYOUT_WIDTH, width);
    uint32_t right = (width - s->right) * WATCH_LAYOUT_WIDTH / width;
    /* With no bottom inset, retain the last partial logical row. Rect-to-native
     * conversion clips that row at the physical edge instead of dropping it. */
    uint32_t bottom = s->bottom ? (height - s->bottom) * WATCH_LAYOUT_WIDTH / width : logical_height;
    if (left >= right || top >= bottom) return false;
    watch_layout value = {width, height, WATCH_LAYOUT_WIDTH, logical_height,
        {(int32_t)left, (int32_t)top, right - left, bottom - top}};
    *out = value;
    return true;
}

static inline bool watch_layout_valid(const watch_layout *l) {
    return l && l->physical_width && l->physical_width <= WATCH_LAYOUT_MAX_SIDE &&
        l->physical_height && l->physical_height <= WATCH_LAYOUT_MAX_SIDE &&
        l->width == WATCH_LAYOUT_WIDTH && l->height && l->height <= WATCH_LAYOUT_MAX_SIDE &&
        l->height == watch_layout_ceil(l->physical_height * WATCH_LAYOUT_WIDTH, l->physical_width);
}

/* The same inverse used by the renderer's physical-pixel lookup. Never clamp
 * an out-of-surface contact into a live control or swallow a physical gutter. */
static inline bool watch_layout_touch(const watch_layout *l, uint32_t x, uint32_t y,
        uint16_t *logical_x, uint16_t *logical_y) {
    if (!watch_layout_valid(l) || !logical_x || !logical_y ||
            x >= l->physical_width || y >= l->physical_height) return false;
    *logical_x = (uint16_t)(x * WATCH_LAYOUT_WIDTH / l->physical_width);
    *logical_y = (uint16_t)(y * WATCH_LAYOUT_WIDTH / l->physical_width);
    return true;
}

/* A logical half-open rectangle maps to exactly the native pixels whose inverse
 * touch coordinates lie inside it. Ceil at BOTH boundaries is important for
 * fractional scales. Clip negative animation coordinates and the last row. */
static inline bool watch_layout_native_rect(const watch_layout *l,
        const watch_layout_rect *r, watch_layout_rect *out) {
    if (!watch_layout_valid(l) || !r || !out || !r->width || !r->height) return false;
    int64_t left = r->x, top = r->y;
    int64_t right = left + r->width, bottom = top + r->height;
    if (left < 0) left = 0;
    if (top < 0) top = 0;
    if (right > l->width) right = l->width;
    if (bottom > l->height) bottom = l->height;
    if (left >= right || top >= bottom) return false;
    uint32_t x0 = watch_layout_ceil((uint32_t)left * l->physical_width, WATCH_LAYOUT_WIDTH);
    uint32_t y0 = watch_layout_ceil((uint32_t)top * l->physical_width, WATCH_LAYOUT_WIDTH);
    uint32_t x1 = watch_layout_ceil((uint32_t)right * l->physical_width, WATCH_LAYOUT_WIDTH);
    uint32_t y1 = watch_layout_ceil((uint32_t)bottom * l->physical_width, WATCH_LAYOUT_WIDTH);
    if (x1 > l->physical_width) x1 = l->physical_width;
    if (y1 > l->physical_height) y1 = l->physical_height;
    if (x0 >= x1 || y0 >= y1) return false;
    *out = (watch_layout_rect){(int32_t)x0, (int32_t)y0, x1 - x0, y1 - y0};
    return true;
}

static inline bool watch_layout_contains(const watch_layout_rect *r, uint32_t x, uint32_t y) {
    return r && (int64_t)x >= r->x && (int64_t)y >= r->y &&
        (int64_t)x < (int64_t)r->x + r->width && (int64_t)y < (int64_t)r->y + r->height;
}

/* Stable typography/control sizes with a flexible content viewport. Callers
 * derive their row count/scroll extent from content.height, not a fixed 240. */
static inline bool watch_layout_vertical(const watch_layout *l, uint32_t margin,
        uint32_t header_height, uint32_t footer_height, uint32_t gap,
        watch_layout_stack *out) {
    if (!watch_layout_valid(l) || !out || l->safe.x < 0 || l->safe.y < 0 ||
            (uint64_t)l->safe.x + l->safe.width > l->width ||
            (uint64_t)l->safe.y + l->safe.height > l->height ||
            (uint64_t)margin * 2u >= l->safe.width) return false;
    uint64_t fixed = (uint64_t)margin * 2u + header_height + footer_height;
    if (header_height) fixed += gap;
    if (footer_height) fixed += gap;
    if (fixed >= l->safe.height) return false;
    int32_t x = l->safe.x + (int32_t)margin, top = l->safe.y + (int32_t)margin;
    uint32_t width = l->safe.width - margin * 2u;
    int32_t footer_y = l->safe.y + (int32_t)(l->safe.height - margin - footer_height);
    int32_t content_y = top + (int32_t)(header_height + (header_height ? gap : 0u));
    *out = (watch_layout_stack){
        {x, top, width, header_height},
        {x, content_y, width, (uint32_t)(l->safe.height - fixed)},
        {x, footer_y, width, footer_height}};
    return true;
}
