/* Hardware-neutral first-boot diagnostics. All physical access is through
 * generic capabilities. A failed optional test never prevents the first frame.
 * Home owns normal navigation; returning from this child re-enters Home. */
#include "RiscRuntimeV1.h"
#include "WatchViewport.h"
#include "RiscBatteryGaugeV1.h"
#include "twatch_calendar.h"
#include "native_ui.h"
#include <stdio.h>
#include <string.h>

#define VALID(a,v,extent) ((a) && (a)->api_version == (v) && (a)->struct_size >= (extent))
static bool get(const risc_runtime_api_v1 *rt, const char *cap, unsigned version,
        risc_runtime_capability_v1 *grant) {
    *grant = (risc_runtime_capability_v1){.struct_size = sizeof(*grant)};
    return rt->acquire(cap, version, 0, grant);
}
static bool terminal(uint8_t state) {
    return state == RISC_DISPLAY_PRESENT_COMPLETE || state == RISC_DISPLAY_PRESENT_FAILED ||
        state == RISC_DISPLAY_PRESENT_SUPERSEDED;
}
static void retain(const risc_runtime_api_v1 *rt, const char *message) {
    rt->diagnostic(message);
    for (;;) rt->yield_ms(100);
}
/* Snapshots are authoritative at startup, first visibility, and every event
 * gap. A held contact cannot become a new press when a page becomes visible. */
static bool resync(const risc_touch_api_v1 *touch, risc_touch_snapshot_v1 *snapshot,
        uint64_t *sequence, bool visible, bool *ready) {
    if (!touch->snapshot(touch->context, snapshot)) return false;
    *sequence = snapshot->sequence;
    *ready = visible && snapshot->contact_count == 0;
    return true;
}

__attribute__((visibility("default"))) void app_main(void) {
    const risc_runtime_api_v1 *rt = risc_runtime_get_api(1);
    if (!rt || rt->api_version != 1 || rt->struct_size < RISC_RUNTIME_CAPABILITIES_V1_SIZE ||
            !rt->health || !rt->yield_ms || !rt->diagnostic || !rt->acquire || !rt->release) return;
    risc_runtime_capability_v1 grants[6] = {{0}};
    bool live[6] = {0};
    watch_viewport viewport = {0};
    watch_viewport_touch touch_view = {0};
    bringup_layout layout = {0};
    const risc_touch_api_v1 *touch = NULL;
    const risc_battery_gauge_api_v1 *battery = NULL;
    const twatch_rtc_api_v1 *rtc = NULL;
    const twatch_motion_api_v1 *motion = NULL;
    const twatch_haptic_api_v1 *haptic = NULL;
    uint64_t subscription = 0, token = 0, sequence = 0;
    bool pending = false, dirty = true, leave = false, down = false, cancelled = false;
    bool visible = false, ready = false, motion_tried = false, haptic_tried = false;
    unsigned action = 0, pressed = 0, touches = 0;
    uint8_t contact = 0;
    uint16_t tx = 0, ty = 0, dx = 0, dy = 0;
    uint32_t last = 0, started = 0;
    risc_touch_snapshot_v1 snapshot = {0};
    char battery_text[40] = "BATTERY UNAVAILABLE", rtc_text[40] = "RTC UNSET";
    char motion_text[40] = "MOTION NOT STARTED", status_text[40] = "TAP BUTTONS TO TEST";

    live[0] = get(rt, "display.output", 1, &grants[0]);
    if (!live[0] || !watch_viewport_open(&viewport, grants[0].api) ||
            !bringup_layout_open(&layout, viewport.width, viewport.height)) goto cleanup;
    live[1] = get(rt, "input.touch.raw", 1, &grants[1]);
    if (!live[1] || !watch_viewport_touch_open(&touch_view, &viewport, grants[1].api)) goto cleanup;
    touch = &touch_view.api;
    subscription = touch->subscribe(touch->context);
    if (!subscription || !resync(touch, &snapshot, &sequence, false, &ready)) goto cleanup;
    live[2] = get(rt, "board.battery", 1, &grants[2]);
    battery = grants[2].api;
    if (!live[2] || !VALID(battery, 1, sizeof(*battery)) || !battery->read) battery = NULL;
    live[3] = get(rt, "rtc.clock", 2, &grants[3]);
    rtc = grants[3].api;
    if (!live[3] || !VALID(rtc, 2, sizeof(*rtc)) || !rtc->read) rtc = NULL;
    rt->diagnostic("WATCH_TEST opened display=ready touch=ready optional-tests=idle");

    for (;;) {
        risc_runtime_health_v1 health = {.struct_size = sizeof(health)};
        if (!rt->health(&health)) break;
        if (pending) {
            risc_display_present_status_v1 result = {0};
            if (!viewport.display.present_status(viewport.display.context, token, &result)) break;
            if (terminal(result.state)) {
                pending = false;
                if (result.state != RISC_DISPLAY_PRESENT_COMPLETE) break;
                if (!visible) {
                    visible = true;
                    down = false;
                    action = 0;
                    if (!resync(touch, &snapshot, &sequence, true, &ready)) break;
                }
            } else if (result.state != RISC_DISPLAY_PRESENT_ACTIVE && result.state != RISC_DISPLAY_PRESENT_QUEUED) {
                break;
            } else if ((uint32_t)(health.uptime_ms - started) > 10000) {
                rt->diagnostic("WATCH_TEST display=timeout");
                break;
            }
        }
        bool sync_after = false;
        if (!touch->poll(touch->context, 1)) {
            down = ready = false;
            action = 0;
            sync_after = true;
            strcpy(status_text, "TOUCH I/O ERROR");
            dirty = true;
        }
        for (unsigned i = 0; i < 8; i++) {
            risc_touch_event_v1 event = {0};
            int n = touch->next(touch->context, subscription, &event);
            if (!n) break;
            if (n < 0) {
                down = ready = false;
                action = 0;
                sync_after = true;
                break;
            }
            if (event.sequence <= sequence) continue;
            sequence = event.sequence;
            if (event.kind == RISC_TOUCH_EVENT_BUTTON_DOWN || event.kind == RISC_TOUCH_EVENT_BUTTON_UP) continue;
            if (event.kind != RISC_TOUCH_EVENT_DOWN && event.kind != RISC_TOUCH_EVENT_MOVE &&
                    event.kind != RISC_TOUCH_EVENT_UP) continue;
            tx = event.x;
            ty = event.y;
            if (tx >= layout.width || ty >= layout.height) {
                down = ready = false;
                sync_after = true;
                continue;
            }
            dirty = true;
            if (!visible || !ready) {
                if (event.kind == RISC_TOUCH_EVENT_UP) sync_after = true;
                continue;
            }
            if (event.kind == RISC_TOUCH_EVENT_DOWN) {
                if (down) {
                    /* A second contact cancels, rather than stealing, a press. */
                    down = ready = false;
                    continue;
                }
                down = true;
                cancelled = false;
                contact = event.id;
                dx = tx;
                dy = ty;
                pressed = bringup_hit(&layout, tx, ty);
                touches++;
            } else if (down && contact == event.id) {
                int ax = (int)tx - dx, ay = (int)ty - dy;
                if (ax <= -(int)layout.slop || ax >= (int)layout.slop ||
                        ay <= -(int)layout.slop || ay >= (int)layout.slop ||
                        bringup_hit(&layout, tx, ty) != pressed) {
                    cancelled = true;
                }
                if (event.kind == RISC_TOUCH_EVENT_UP) {
                    down = false;
                    if (!cancelled) {
                        if (pressed == 3) leave = true;
                        else if (pressed && !action) action = pressed;
                    }
                }
            }
        }
        if (sync_after) {
            down = false;
            action = 0;
            leave = false;
            if (!resync(touch, &snapshot, &sequence, visible, &ready)) leave = true;
        }
        if (!pending && leave) break;
        if (!pending && visible && action) {
            if (action == 1) {
                if (!motion_tried) {
                    motion_tried = true;
                    live[4] = get(rt, "motion.accel", 1, &grants[4]);
                    motion = grants[4].api;
                    if (!live[4] || !VALID(motion, 1, TWATCH_MOTION_SAMPLE_SIZE) || !motion->read) motion = NULL;
                }
                strcpy(status_text, motion ? "MOTION SAMPLING ON" : "MOTION UNAVAILABLE");
                rt->diagnostic(motion ? "WATCH_TEST motion=started" : "WATCH_TEST motion=unavailable");
            } else {
                if (!haptic_tried) {
                    haptic_tried = true;
                    live[5] = get(rt, "haptic.effect", 1, &grants[5]);
                    haptic = grants[5].api;
                    if (!live[5] || !VALID(haptic, 1, sizeof(*haptic)) || !haptic->effect || !haptic->stop) haptic = NULL;
                }
                bool ok = haptic && haptic->effect(haptic->context, 14);
                strcpy(status_text, ok ? "VIBRATION REQUESTED" : "VIBRATE UNAVAILABLE");
                rt->diagnostic(ok ? "WATCH_TEST haptic=pulse-requested" : "WATCH_TEST haptic=unavailable");
            }
            action = 0;
            dirty = true;
        }
        if (!pending && (dirty || (uint32_t)(health.uptime_ms - last) >= 1000)) {
            risc_battery_sample_v1 b = {0};
            if (battery) {
                if (!battery->read(battery->context, &b)) strcpy(battery_text, "BATTERY READ ERROR");
                else if (b.percent <= 100) snprintf(battery_text, sizeof(battery_text), "BATTERY %u%% %u MV", b.percent, b.millivolts);
                else snprintf(battery_text, sizeof(battery_text), "BATTERY --%% %u MV", b.millivolts);
            }
            twatch_rtc_time_v1 date = {0};
            if (rtc && rtc->read(rtc->context, &date) && tw_valid_time(&date))
                snprintf(rtc_text, sizeof(rtc_text), "RTC RAW %02u:%02u:%02u", date.hour, date.minute, date.second);
            else strcpy(rtc_text, "RTC UNSET");
            if (motion) {
                twatch_accel_sample_v1 sample = {0};
                if (motion->read(motion->context, &sample))
                    snprintf(motion_text, sizeof(motion_text), "XYZ %d %d %d MG", sample.x, sample.y, sample.z);
                else strcpy(motion_text, "MOTION READ ERROR");
            }
            risc_display_surface_v1 surface = {0};
            if (!viewport.display.acquire(viewport.display.context, RISC_DISPLAY_FORMAT_RGB565, &surface)) break;
            if (!render(&surface, &layout, health.uptime_ms / 1000, battery_text, rtc_text,
                    motion_text, status_text, tx, ty, touches)) break;
            if (!viewport.display.submit(viewport.display.context, surface.frame, NULL, 0, NULL, &token)) break;
            pending = true;
            dirty = false;
            started = last = health.uptime_ms;
        }
        rt->yield_ms(pending ? 1 : 10);
    }
cleanup:
    /* An accepted QUEUED frame owns the provider just as an ACTIVE frame does.
     * Never return to Home while completion is unknown, even after a timeout. */
    if (pending && viewport.native) {
        for (unsigned n = 0; n < 10000; n++) {
            risc_display_present_status_v1 result = {0};
            if (viewport.display.present_status(viewport.display.context, token, &result) && terminal(result.state)) {
                pending = false;
                break;
            }
            rt->yield_ms(1);
        }
        if (pending) retain(rt, "WATCH_TEST retained=display");
    }
    if (haptic && !haptic->stop(haptic->context)) retain(rt, "WATCH_TEST retained=haptic-stop");
    if (subscription && !touch->unsubscribe(touch->context, subscription)) retain(rt, "WATCH_TEST retained=touch");
    watch_viewport_close(&viewport);
    for (unsigned n = 6; n > 0; n--)
        if (live[n - 1] && !rt->release(&grants[n - 1])) retain(rt, "WATCH_TEST retained=release");
    rt->diagnostic("WATCH_TEST return=home");
}
