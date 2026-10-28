#pragma once
/* Diagnostic-local native geometry. No completed canvas, framebuffer allocation,
 * board identity, or changes to Home's shared viewport are involved. */
#include "WatchLayout.h"
#include "RiscDisplayOutputV1.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    uint32_t width, height, body_scale, title_scale, slop;
    watch_layout_rect title, subtitle, bars, rows[7], motion, haptic, back;
} bringup_layout;

static uint32_t bringup_min(uint32_t a, uint32_t b) { return a < b ? a : b; }
static uint32_t bringup_max(uint32_t a, uint32_t b) { return a > b ? a : b; }

static bool bringup_layout_open(bringup_layout *out, uint32_t width, uint32_t height) {
    if (!out || width < 64 || height < 96 || width > WATCH_LAYOUT_MAX_SIDE ||
            height > WATCH_LAYOUT_MAX_SIDE) return false;
    uint32_t side = bringup_min(width, height);
    uint32_t scale = bringup_max(1, bringup_min(4, side / 200));
    uint32_t margin = bringup_max(3, bringup_min(24, side / 20));
    uint32_t gap = bringup_max(1, bringup_min(12, side / 40));
    uint32_t button = bringup_max(16, 28 * scale);
    uint32_t content_width = width - 2 * margin;
    uint32_t title_scale = bringup_max(1, bringup_min(2 * scale, (content_width + 1) / 72));
    uint32_t subtitle = 7 * scale, bar = bringup_max(2, 4 * scale);
    /* On short displays retain every live telemetry row and all controls; the
     * redundant version subtitle is the first item to go. */
    uint32_t fixed = 2 * margin + 7 * title_scale + subtitle + bar + 2 * button + 5 * gap;
    if (fixed + 7 * 9 * scale > height) {
        fixed -= subtitle + gap;
        subtitle = 0;
    }
    if (fixed + 7 * 9 * scale > height) {
        button = 16;
        margin = 3;
        gap = 1;
        content_width = width - 2 * margin;
        title_scale = 1;
        scale = 1;
        bar = 2;
    }
    if (height < 112) button = 11;
    uint32_t y = margin;
    bringup_layout l = {.width = width, .height = height, .body_scale = scale,
        .title_scale = title_scale, .slop = bringup_max(8, 12 * scale)};
    l.title = (watch_layout_rect){(int32_t)margin, (int32_t)y, content_width, 7 * title_scale};
    y += l.title.height + gap;
    l.subtitle = (watch_layout_rect){(int32_t)margin, (int32_t)y, content_width, subtitle};
    if (subtitle) y += subtitle + gap;
    l.bars = (watch_layout_rect){(int32_t)margin, (int32_t)y, content_width, bar};
    y += bar + gap;
    l.back = (watch_layout_rect){(int32_t)margin, (int32_t)(height - margin - button), content_width, button};
    uint32_t controls_y = (uint32_t)l.back.y - gap - button;
    uint32_t half = (content_width - gap) / 2;
    l.motion = (watch_layout_rect){(int32_t)margin, (int32_t)controls_y, half, button};
    l.haptic = (watch_layout_rect){(int32_t)(margin + half + gap), (int32_t)controls_y,
        content_width - half - gap, button};
    uint32_t content_bottom = controls_y - gap;
    if (content_bottom < y || content_bottom - y < 7 * 7 * scale) return false;
    uint32_t available = content_bottom - y;
    for (unsigned n = 0; n < 7; n++) {
        uint32_t top = y + available * n / 7, bottom = y + available * (n + 1) / 7;
        l.rows[n] = (watch_layout_rect){(int32_t)margin, (int32_t)top, content_width, bottom - top};
    }
    *out = l;
    return true;
}

/* Position live controls inside provider-owned content bounds while preserving
 * the full raster and raw touch coordinate system. Zero insets are identical. */
static bool bringup_layout_open_safe(bringup_layout *out, const watch_layout *v) {
    if (!out || !watch_layout_valid(v)) return false;
    bringup_layout l;
    if (!bringup_layout_open(&l,v->safe.width,v->safe.height)) return false;
    watch_layout_rect *fixed[]={&l.title,&l.subtitle,&l.bars,&l.motion,&l.haptic,&l.back};
    for(unsigned n=0;n<sizeof(fixed)/sizeof(*fixed);n++) {fixed[n]->x+=v->safe.x;fixed[n]->y+=v->safe.y;}
    for(unsigned n=0;n<7;n++) {l.rows[n].x+=v->safe.x;l.rows[n].y+=v->safe.y;}
    l.width=v->physical_width;l.height=v->physical_height;*out=l;return true;
}

static unsigned bringup_hit(const bringup_layout *l, uint16_t x, uint16_t y) {
    if (!l || x >= l->width || y >= l->height) return 0;
    if (watch_layout_contains(&l->motion, x, y)) return 1;
    if (watch_layout_contains(&l->haptic, x, y)) return 2;
    if (watch_layout_contains(&l->back, x, y)) return 3;
    return 0;
}

static const uint8_t numbers[10][5] = {
 {62,81,73,69,62},{0,66,127,64,0},{98,81,73,73,70},{34,65,73,73,54},{24,20,18,127,16},
 {39,69,69,69,57},{60,74,73,73,48},{1,113,9,5,3},{54,73,73,73,54},{6,73,73,41,30}};
static const uint8_t letters[26][5] = {
 {126,9,9,9,126},{127,73,73,73,54},{62,65,65,65,34},{127,65,65,34,28},
 {127,73,73,73,65},{127,9,9,9,1},{62,65,73,73,122},{127,8,8,8,127},
 {0,65,127,65,0},{32,64,65,63,1},{127,8,20,34,65},{127,64,64,64,64},
 {127,2,12,2,127},{127,4,8,16,127},{62,65,65,65,62},{127,9,9,9,6},
 {62,65,81,33,94},{127,9,25,41,70},{70,73,73,73,49},{1,1,127,1,1},
 {63,64,64,64,63},{31,32,64,32,31},{63,64,56,64,63},{99,20,8,20,99},
 {3,4,120,4,3},{97,81,73,69,67}};
static uint8_t glyph(char c, unsigned x) {
    if (c >= 'A' && c <= 'Z') return letters[c - 'A'][x];
    if (c >= '0' && c <= '9') return numbers[c - '0'][x];
    if (c == '-') return 8;
    if (c == ':') return x == 2 ? 36 : 0;
    if (c == '.') return x == 2 ? 64 : 0;
    if (c == '/') return (uint8_t)(64u >> x);
    if (c == '%') { const uint8_t a[] = {99,19,8,100,99}; return a[x]; }
    return 0;
}
static void box(risc_display_surface_v1 *s, unsigned x, unsigned y,
        unsigned width, unsigned height, uint16_t color) {
    if (x >= s->width || y >= s->height) return;
    width = bringup_min(width, s->width - x);
    height = bringup_min(height, s->height - y);
    for (unsigned yy = y; yy < y + height; yy++) {
        uint8_t *p = (uint8_t *)s->pixels + (size_t)yy * s->stride_bytes;
        for (unsigned xx = x; xx < x + width; xx++) {
            p[xx * 2] = (uint8_t)color;
            p[xx * 2 + 1] = (uint8_t)(color >> 8);
        }
    }
}
/* Fit complete glyphs inside their own row/control, using a smaller native font
 * before ellipsizing. Never rasterize text into a neighboring control. */
static void text(risc_display_surface_v1 *s, const watch_layout_rect *r,
        const char *value, unsigned scale, uint16_t color, bool centered) {
    if (!r->width || !r->height || r->x < 0 || r->y < 0) return;
    size_t length = strlen(value);
    while (scale > 1 && (length * 6 - (length != 0)) * scale > r->width) scale--;
    while (scale > 1 && 7 * scale > r->height) scale--;
    if (7 * scale > r->height) return;
    unsigned count = (r->width + scale) / (6 * scale);
    if (length < count) count = (unsigned)length;
    if (!count) return;
    unsigned width = (count * 6 - 1) * scale;
    unsigned x = (unsigned)r->x + (centered ? (r->width - width) / 2 : 0);
    unsigned y = (unsigned)r->y + (r->height - 7 * scale) / 2;
    for (unsigned k = 0; k < count; k++) {
        char c = length > count && count >= 3 && k >= count - 3 ? '.' : value[k];
        for (unsigned col = 0; col < 5; col++) for (unsigned row = 0; row < 7; row++)
            if (glyph(c, col) & (1u << row))
                box(s, x + (k * 6 + col) * scale, y + row * scale, scale, scale, color);
    }
}
static void button(risc_display_surface_v1 *s, const watch_layout_rect *r,
        const char *label, unsigned scale, uint16_t color) {
    box(s, (unsigned)r->x, (unsigned)r->y, r->width, r->height, color);
    watch_layout_rect inside = *r;
    if (inside.width > 4) { inside.x += 2; inside.width -= 4; }
    text(s, &inside, label, scale, 0xffff, true);
}
static bool render(risc_display_surface_v1 *s, const bringup_layout *l, uint32_t uptime,
        const char *battery, const char *rtc, const char *motion, const char *status,
        uint16_t tx, uint16_t ty, unsigned touches) {
    if (!s || !l || !s->pixels || !s->frame || s->width != l->width || s->height != l->height ||
            !s->width || !s->height || s->pixel_format != RISC_DISPLAY_FORMAT_RGB565 ||
            (s->stride_bytes & 1u) || s->stride_bytes < s->width * 2u ||
            s->stride_bytes > SIZE_MAX / s->height ||
            s->size_bytes < (size_t)s->stride_bytes * (s->height - 1) + s->width * 2u) return false;
    box(s, 0, 0, s->width, s->height, 0x0841);
    text(s, &l->title, "HARDWARE TEST", l->title_scale, 0xffff, false);
    text(s, &l->subtitle, "RISC RTE  BOOT 0.1.0", l->body_scale, 0x7bef, false);
    const uint16_t colors[] = {0xf800, 0x07e0, 0x001f};
    for (unsigned n = 0; n < 3; n++) {
        unsigned left = l->bars.width * n / 3, right = l->bars.width * (n + 1) / 3;
        box(s, (unsigned)l->bars.x + left, (unsigned)l->bars.y, right - left, l->bars.height, colors[n]);
    }
    char uptime_text[40], touch_text[48];
    snprintf(uptime_text, sizeof(uptime_text), "UPTIME %lu S", (unsigned long)uptime);
    snprintf(touch_text, sizeof(touch_text), "TOUCH %u  X%u Y%u", touches, tx, ty);
    const char *idle = l->rows[6].width < 143 ? "RAD AUD SD SLEEP OFF" : "RADIO AUDIO SD SLEEP OFF";
    const char *values[] = {uptime_text, battery, rtc, touch_text, motion, status, idle};
    for (unsigned n = 0; n < 7; n++)
        text(s, &l->rows[n], values[n], l->body_scale, n == 3 ? 0x07ff : n == 5 ? 0xffe0 : n == 6 ? 0x7bef : 0xffff, false);
    button(s, &l->motion, "MOTION", l->body_scale, 0x2124);
    button(s, &l->haptic, "VIBRATE", l->body_scale, 0x2124);
    button(s, &l->back, "BACK TO HOME", l->body_scale, 0x032c);
    return true;
}
