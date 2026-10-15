#include "WatchLayout.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static unsigned long long checked_pixels;

static void full_surface(unsigned width, unsigned height) {
    watch_layout l = {0};
    assert(watch_layout_open(&l, width, height, NULL));
    assert(l.width == 240 && l.height == (height * 240 + width - 1) / width);
    watch_layout_rect all = {0, 0, l.width, l.height}, native = {0};
    assert(watch_layout_native_rect(&l, &all, &native));
    assert(native.x == 0 && native.y == 0 && native.width == width && native.height == height);
    uint64_t area = 0;
    for (unsigned y = 0; y < l.height; ++y) {
        for (unsigned x = 0; x < l.width; ++x) {
            watch_layout_rect cell = {(int32_t)x, (int32_t)y, 1, 1};
            if (watch_layout_native_rect(&l, &cell, &native)) {
                area += (uint64_t)native.width * native.height;
                for (unsigned py = (unsigned)native.y; py < (unsigned)native.y + native.height; ++py)
                    for (unsigned px = (unsigned)native.x; px < (unsigned)native.x + native.width; ++px) {
                        uint16_t lx = UINT16_MAX, ly = UINT16_MAX;
                        assert(watch_layout_touch(&l, px, py, &lx, &ly));
                        assert(lx == x && ly == y);
                        ++checked_pixels;
                    }
            }
        }
    }
    assert(area == (uint64_t)width * height); /* no gaps or overlap, including final row */
    uint16_t lx = 777, ly = 888;
    assert(!watch_layout_touch(&l, width, height - 1, &lx, &ly) && lx == 777 && ly == 888);
    assert(!watch_layout_touch(&l, width - 1, height, &lx, &ly) && lx == 777 && ly == 888);
    assert(watch_layout_touch(&l, width - 1, height - 1, &lx, &ly));
    assert(lx < l.width && ly < l.height);
}

static void identity(void) {
    watch_layout l;
    assert(watch_layout_open(&l, 240, 240, NULL));
    for (unsigned y = 0; y < 240; ++y)
        for (unsigned x = 0; x < 240; ++x) {
            uint16_t lx, ly;
            assert(watch_layout_touch(&l, x, y, &lx, &ly));
            assert(lx == x && ly == y);
        }
    watch_layout_rect r = {10, 30, 82, 16}, n;
    assert(watch_layout_native_rect(&l, &r, &n));
    assert(n.x == r.x && n.y == r.y && n.width == r.width && n.height == r.height);
}

static void tall_reflow(void) {
    watch_layout square, tall;
    assert(watch_layout_open(&square, 240, 240, NULL));
    assert(watch_layout_open(&tall, 410, 502, NULL));
    assert(tall.height == 294 && tall.safe.height == 294);
    watch_layout_stack a, b;
    assert(watch_layout_vertical(&square, 8, 28, 32, 8, &a));
    assert(watch_layout_vertical(&tall, 8, 28, 32, 8, &b));
    assert(a.header.y == b.header.y && a.header.height == b.header.height);
    assert(a.footer.height == b.footer.height && b.footer.y == a.footer.y + 54);
    assert(b.content.height == a.content.height + 54);
    assert(a.content.height / 28 == 5 && b.content.height / 28 == 7);
    assert(a.content.width == b.content.width); /* no stretched typography/controls */
    watch_layout_rect last = {0, 293, 240, 1}, native;
    assert(watch_layout_native_rect(&tall, &last, &native));
    assert(native.y == 501 && native.height == 1 && native.width == 410);
    uint16_t x, y;
    assert(watch_layout_touch(&tall, 409, 501, &x, &y) && x == 239 && y == 293);
    /* The former gutters are usable content coordinates, not clamped edges. */
    assert(watch_layout_touch(&tall, 205, 45, &x, &y) && y == 26);
    assert(watch_layout_touch(&tall, 205, 456, &x, &y) && y == 266);
    assert(watch_layout_contains(&last, 239, 293));
    assert(!watch_layout_contains(&last, 240, 293));
    assert(!watch_layout_contains(&last, 239, 294));
}

static void rect_hit_agreement(void) {
    watch_layout l;
    assert(watch_layout_open(&l, 410, 502, NULL));
    const watch_layout_rect buttons[] = {{12, 250, 102, 36}, {126, 250, 102, 36},
        {-10, -5, 22, 20}, {239, 293, 99, 99}, {0, 0, 240, 294}};
    for (unsigned n = 0; n < sizeof(buttons) / sizeof(*buttons); ++n) {
        watch_layout_rect physical;
        assert(watch_layout_native_rect(&l, buttons + n, &physical));
        for (unsigned py = 0; py < 502; ++py)
            for (unsigned px = 0; px < 410; ++px) {
                uint16_t x, y;
                assert(watch_layout_touch(&l, px, py, &x, &y));
                assert(watch_layout_contains(&physical, px, py) == watch_layout_contains(buttons + n, x, y));
            }
    }
}

static void safe_insets(void) {
    watch_layout l;
    watch_layout_insets insets = {3, 9, 7, 5};
    assert(watch_layout_open(&l, 410, 502, &insets));
    watch_layout_rect native;
    assert(watch_layout_native_rect(&l, &l.safe, &native));
    assert(native.x >= 5 && native.y >= 3);
    assert((uint32_t)native.x + native.width <= 401);
    assert((uint32_t)native.y + native.height <= 495);
    watch_layout_stack stack;
    assert(watch_layout_vertical(&l, 0, 24, 28, 4, &stack));
    assert(stack.header.y == l.safe.y);
    assert((uint32_t)stack.footer.y + stack.footer.height == (uint32_t)l.safe.y + l.safe.height);
}

static void refusals(void) {
    watch_layout l = {0}, before = {0};
    assert(!watch_layout_open(NULL, 240, 240, NULL));
    const unsigned bad[][2] = {{0,240},{240,0},{2049,240},{240,2049},{1,2048}};
    for (unsigned n = 0; n < sizeof(bad) / sizeof(*bad); ++n) {
        assert(!watch_layout_open(&l, bad[n][0], bad[n][1], NULL));
        assert(!memcmp(&l, &before, sizeof(l)));
    }
    watch_layout_insets bad_safe = {0, 120, 0, 120};
    assert(!watch_layout_open(&l, 240, 240, &bad_safe));
    assert(watch_layout_open(&l, 410, 502, NULL));
    watch_layout_stack stack;
    assert(!watch_layout_vertical(&l, UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX, &stack));
    assert(!watch_layout_vertical(&l, 0, 294, 0, 0, &stack));
    assert(!watch_layout_vertical(&l, 120, 0, 0, 0, &stack));
    assert(watch_layout_vertical(&l, 0, 0, 0, 0, &stack) && stack.content.height == 294);
    watch_layout_rect output = {7,8,9,10};
    const watch_layout_rect bad_rects[] = {{0,0,0,1},{0,0,1,0},{240,0,1,1},
        {0,294,1,1},{INT_MIN,INT_MIN,1,1},{INT_MAX,INT_MAX,UINT_MAX,UINT_MAX}};
    for (unsigned n = 0; n < sizeof(bad_rects) / sizeof(*bad_rects); ++n) {
        assert(!watch_layout_native_rect(&l, bad_rects + n, &output));
        assert(output.x == 7 && output.y == 8 && output.width == 9 && output.height == 10);
    }
}

static void native_layout(void) {
    const unsigned sizes[][2]={{240,240},{410,502},{502,410},{120,150},{1,2048},{2048,1}};
    for(unsigned n=0;n<sizeof(sizes)/sizeof(*sizes);n++) {
        unsigned w=sizes[n][0],h=sizes[n][1];watch_layout l;
        assert(watch_layout_native_open(&l,w,h,NULL)&&l.native&&watch_layout_valid(&l));
        assert(l.width==w&&l.height==h);
        for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++) {
            uint16_t xx,yy;assert(watch_layout_touch(&l,x,y,&xx,&yy)&&xx==x&&yy==y);
        }
        watch_layout_rect r={-1,-2,w+1,h+2},out;
        assert(watch_layout_native_rect(&l,&r,&out));
        assert(!out.x&&!out.y&&out.width==w&&out.height==h);
    }
    watch_layout l;watch_layout_insets insets={3,9,7,5};
    assert(watch_layout_native_open(&l,410,502,&insets));
    assert(l.safe.x==5&&l.safe.y==3&&l.safe.width==396&&l.safe.height==492);
}

int main(void) {
    native_layout(); identity(); tall_reflow(); rect_hit_agreement(); safe_insets(); refusals();
    const unsigned sizes[][2] = {{240,240},{410,502},{502,410},{239,501},
        {240,294},{320,480},{160,160},{1,1},{1,8},{2048,2048}};
    for (unsigned n = 0; n < sizeof(sizes) / sizeof(*sizes); ++n) full_surface(sizes[n][0], sizes[n][1]);
    printf("PASS width-based layout: %llu native pixels, final rows, inverse hits, 240 identity and tall reflow\n", checked_pixels);
    return 0;
}
