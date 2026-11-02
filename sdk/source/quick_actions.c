#include "PortableQuickActions.h"
#include "PortableQuickLayout.h"
#include <string.h>

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static int absi(int v) { return v < 0 ? -v : v; }
static void emit_brightness(pqa_state *s, bool commit) {
    if (commit) {
        s->pending &= ~(uint32_t)PQA_BRIGHTNESS_PREVIEW;
        s->pending |= PQA_BRIGHTNESS_COMMIT;
        s->action_brightness = s->brightness;
    } else if (!(s->pending & PQA_BRIGHTNESS_COMMIT)) {
        s->pending |= PQA_BRIGHTNESS_PREVIEW;
        s->action_brightness = s->brightness;
    }
}
#ifdef PORTABLE_FRONTLIGHT_TONE
static void emit_tone(pqa_state *s,bool commit) {
    if(commit){s->pending&=~(uint32_t)PQA_TONE_PREVIEW;s->pending|=PQA_TONE_COMMIT;s->action_tone=s->tone;}
    else if(!(s->pending&PQA_TONE_COMMIT)){s->pending|=PQA_TONE_PREVIEW;s->action_tone=s->tone;}
}
#endif
static void emit_volume(pqa_state *s, uint32_t action) {
    s->action_volume = s->volume;
    s->pending |= action;
}
static void emit_torch(pqa_state *s) {
    s->action_torch = s->torch;
    s->pending |= PQA_TORCH;
}
void pqa_init(pqa_state *s) {
    if (!s) return;
    memset(s, 0, sizeof(*s));
    s->torch_valid = true;
#ifdef PORTABLE_RESIDENT_SHELL_HOST
    s->audio_controls = true;
#endif
    s->brightness = 40;
#ifdef PORTABLE_FRONTLIGHT_TONE
    s->tone=50;
#endif
#ifdef PORTABLE_PAPER_TRANSITIONS
    s->last_nonzero_brightness = 40;
#endif
    s->volume = 50;
    s->last_nonzero_volume = 50;
    s->pressed_tile = -1;
}
bool pqa_set_height(pqa_state *s,unsigned logical_height) {
    if(!s || logical_height<240 || logical_height>2048 || pqa_visible(s) ||
       s->gesture!=PQA_IDLE)return false;
    s->logical_height=(uint16_t)logical_height;s->native_width=0;
#ifdef PORTABLE_QUICK_SAFE_CONTENT
    s->safe_left=s->safe_top=s->safe_width=s->safe_height=0;
#endif
    return true;
}
bool pqa_set_dimensions(pqa_state *s,unsigned width,unsigned height) {
    if(!s || !width || !height || width>2048 || height>2048 || s->paper ||
       pqa_visible(s) || s->gesture!=PQA_IDLE)return false;
    s->native_width=(uint16_t)width;s->logical_height=(uint16_t)height;
#ifdef PORTABLE_QUICK_SAFE_CONTENT
    s->safe_left=s->safe_top=s->safe_width=s->safe_height=0;
#endif
    return true;
}
#ifdef PORTABLE_QUICK_SAFE_CONTENT
bool pqa_set_safe_content(pqa_state *s,unsigned left,unsigned top,unsigned width,unsigned height) {
#ifndef PORTABLE_QUICK_MIN_CONTENT
#define PORTABLE_QUICK_MIN_CONTENT 240
#endif
#if PORTABLE_QUICK_MIN_CONTENT < 160 || PORTABLE_QUICK_MIN_CONTENT > 240
#error Quick minimum content must remain within its native 160 to 240 pixel range
#endif
    if(!s || !s->native_width || s->paper || pqa_visible(s) || s->gesture!=PQA_IDLE ||
       width<PORTABLE_QUICK_MIN_CONTENT || height<PORTABLE_QUICK_MIN_CONTENT || left>=pqa_width(s) || top>=pqa_height(s) ||
       width>pqa_width(s)-left || height>pqa_height(s)-top)return false;
    s->safe_left=left;s->safe_top=top;s->safe_width=width;s->safe_height=height;return true;
}
#endif
static bool wifi_valid(const pqa_state *s) {
#if defined(PORTABLE_QUICK_CAPABILITY_SAFE) || defined(PORTABLE_QUICK_RADIO_AVAILABILITY)
    return s->wifi_valid;
#else
    return s->radios_valid;
#endif
}
static bool bluetooth_valid(const pqa_state *s) {
#if defined(PORTABLE_QUICK_CAPABILITY_SAFE) || defined(PORTABLE_QUICK_RADIO_AVAILABILITY)
    return s->bluetooth_valid;
#else
    return s->radios_valid;
#endif
}
static bool contexts_valid(const pqa_state *s) {
#ifdef PORTABLE_QUICK_CAPABILITY_SAFE
    return s->contexts_valid && s->contexts_available;
#else
    return s->contexts_valid;
#endif
}
void pqa_set_levels(pqa_state *s, bool bv, unsigned b, bool vv, unsigned v) {
    if (!s) return;
    s->brightness_valid = bv;
    s->volume_valid = vv;
    s->brightness = (uint8_t)(b > 100u ? 100u : b);
#ifdef PORTABLE_PAPER_TRANSITIONS
    if(s->brightness)s->last_nonzero_brightness=s->brightness;
#endif
    s->volume = (uint8_t)(v > 100u ? 100u : v);
    if (s->volume) s->last_nonzero_volume = s->volume;
}
bool pqa_visible(const pqa_state *s) {
    return s && (s->position_q8 > 0 || s->target_q8 > 0 || s->torch);
}
bool pqa_capture(const pqa_state *s) {
    return s && (s->neutral_gate || (s->gesture != PQA_IDLE && s->gesture != PQA_PASS_THROUGH));
}
uint32_t pqa_take_action(pqa_state *s) {
    uint32_t action = s ? s->pending : 0;
    if (s) s->pending = 0;
    return action;
}
void pqa_cancel_input(pqa_state *s) {
    if (!s) return;
#ifdef PORTABLE_QUICK_CAPABILITY_SAFE
    if(s->save_pending){s->save_cancelled=true;return;}
#endif
    if (s->gesture == PQA_BRIGHTNESS_DRAG && s->brightness != s->saved_brightness) {
        s->brightness = s->saved_brightness;
        emit_brightness(s, false);
    }
    if (s->gesture == PQA_VOLUME_DRAG) {
        s->volume = s->saved_volume;
        s->last_nonzero_volume = s->saved_last_nonzero_volume;
    }
#ifdef PORTABLE_FRONTLIGHT_TONE
    if(s->gesture==PQA_TONE_DRAG && s->tone!=s->saved_tone){s->tone=s->saved_tone;emit_tone(s,false);}
#endif
    s->gesture = PQA_IDLE;
    s->pressed_tile = -1;
    s->neutral_gate = true;
    s->velocity_q8 = 0;
    s->release_velocity_q8 = 0;
    s->route = PQA_CONSUMED;
}
void pqa_close(pqa_state *s) {
    if (!s) return;
#ifdef PORTABLE_QUICK_CAPABILITY_SAFE
    if(s->save_pending){s->save_cancelled=true;return;}
#endif
    pqa_cancel_input(s);
    s->target_q8 = 0;
#ifndef PORTABLE_PAPER_TRANSITIONS
    if(s->paper)s->position_q8=0;
#endif
    if (s->torch) { s->torch = false; emit_torch(s); }
}
void pqa_cancel(pqa_state *s) {
    if (!s) return;
#ifdef PORTABLE_QUICK_CAPABILITY_SAFE
    if(s->save_pending){s->save_cancelled=true;return;}
#endif
    pqa_close(s);
    s->position_q8 = 0;
}
#ifdef PORTABLE_RESIDENT_SHELL_HOST
bool pqa_paper_compact_sliders(const pqa_state *s) {
#ifdef PORTABLE_FRONTLIGHT_TONE
    return s && s->audio_controls && s->tone_controls;
#else
    (void)s;return false;
#endif
}
int pqa_paper_slider_y(const pqa_state *s,unsigned slider) {
    if(!s)return 0;
    const bool compact=pqa_paper_compact_sliders(s);
    if(slider==0)return compact?114:126;
    if(slider==1)return s->audio_controls?(compact?180:220):0;
#ifdef PORTABLE_FRONTLIGHT_TONE
    if(slider==2)return s->tone_controls?(compact?246:220):0;
#endif
    return 0;
}
static bool paper_slider_at(const pqa_state *s,unsigned slider,int x,int y) {
    const int top=pqa_paper_slider_y(s,slider),height=pqa_paper_compact_sliders(s)?30:44;
    return top && x>=32 && x<448 && y>=top-6 && y<top+height+10;
}
bool pqa_paper_tile(const pqa_state *s,unsigned tile,int *x,int *y) {
    if(!s || tile==6 || tile>9 || (!s->audio_controls && tile==0))return false;
#ifndef PORTABLE_QUICK_USB_TRANSFER
    if(tile==7)return false;
#endif
    if(tile==8 && !s->clean_refresh_valid)return false;
    if(tile==9 && !s->contexts_controls)return false;
    unsigned slot=s->audio_controls?tile:tile-1;
    if(tile>=7)--slot; /* Tile 6 is the legacy standalone frontlight switch. */
#ifndef PORTABLE_QUICK_USB_TRANSFER
    if(tile>=8)--slot;
#endif
    if(tile==9&&!s->clean_refresh_valid)--slot;
    int top=s->audio_controls?302:206;
#ifdef PORTABLE_FRONTLIGHT_TONE
    if(s->tone_controls)top=s->audio_controls?306:302;
#endif
    *x=32+(int)(slot%2)*212;*y=top+(int)(slot/2)*104;
    return true;
}
#endif
static int tile_at(const pqa_state *s,int x, int y) {
    if(s->native_width) {
        pqa_layout l=pqa_layout_for(s);
        if(s->contexts_controls&&pqa_rect_contains(l.contexts,x,y))return 9;
        for(int i=0;i<6;++i)if(pqa_rect_contains(l.tiles[i],x,y))return i;
        return -1;
    }
    if(s->paper) {
#ifdef PORTABLE_QUICK_USB_TRANSFER
#ifndef PORTABLE_RESIDENT_SHELL_HOST
        if(x>=32 && x<448 && y>=612 && y<676)return 7;
#endif
#endif
#if defined(PORTABLE_PAPER_TRANSITIONS) && !defined(PORTABLE_RESIDENT_SHELL_HOST)
        if(x>=326 && x<448 && y>=84 && y<120)return 6;
#endif
#ifdef PORTABLE_RESIDENT_SHELL_HOST
        for(unsigned tile=0;tile<10;tile++) {
            int tx,ty;
            if(pqa_paper_tile(s,tile,&tx,&ty) && x>=tx && x<tx+204 && y>=ty && y<ty+92)return (int)tile;
        }
#else
        for(int row=0;row<3;row++)for(int col=0;col<2;col++)
            if(x>=32+col*212 && x<236+col*212 && y>=302+row*104 && y<394+row*104)return row*2+col;
#endif
        return -1;
    }
    if(s->contexts_controls&&x>=20&&x<220&&y>=182&&y<222)return 9;
    if(s->contexts_controls)y+=30;
    static const int xs[3] = {20, 91, 162};
    for (int row = 0; row != 2; ++row)
        for (int col = 0; col != 3; ++col)
            if (x >= xs[col] && x < xs[col] + 62 &&
                y >= 118 + row * 48 && y < 160 + row * 48)
                return row * 3 + col;
    return -1;
}
static unsigned slider_value(int x, unsigned minimum) {
    int value = ((clampi(x, 44, 182) - 44) * 10 + 69) / 138;
    return (unsigned)clampi(value * 10, (int)minimum, 100);
}
static unsigned native_slider_value(const pqa_state *s,int x,unsigned minimum) {
    pqa_layout l=pqa_layout_for(s);int span=l.slider_right-l.slider_left;
    if(span<1)return minimum;
    int value=((clampi(x,l.slider_left,l.slider_right)-l.slider_left)*10+span/2)/span;
    return (unsigned)clampi(value*10,(int)minimum,100);
}
static void slide(pqa_state *s, int x) {
#ifdef PORTABLE_FRONTLIGHT_TONE
    if(s->gesture==PQA_TONE_DRAG){
        unsigned tone=slider_value(44+(clampi(x,88,364)-88)/2,0);
        if(tone!=s->tone){s->tone=(uint8_t)tone;emit_tone(s,false);}
        return;
    }
#endif
    if (s->gesture == PQA_BRIGHTNESS_DRAG) {
        unsigned b = s->native_width?native_slider_value(s,x,10):slider_value(s->paper?44+(clampi(x,88,364)-88)/2:x, 10);
        if (b != s->brightness) { s->brightness = (uint8_t)b; emit_brightness(s, false); }
    } else {
        s->volume = (uint8_t)(s->native_width?native_slider_value(s,x,0):slider_value(s->paper?44+(clampi(x,88,364)-88)/2:x, 0));
    }
}
static void begin_contact(pqa_state *s, uint32_t now, uint32_t id, int x, int y) {
    s->start_ms = s->last_ms = now;
    s->start_id = id;
    s->start_x = s->last_x = (int16_t)x;
    s->start_y = s->last_y = (int16_t)y;
    s->start_position_q8 = s->position_q8;
    s->release_velocity_q8 = 0;
    s->pressed_tile = -1;
    s->handle_pressed = false;
    s->moved = false;
    s->saved_brightness = s->brightness;
#ifdef PORTABLE_FRONTLIGHT_TONE
    s->saved_tone=s->tone;
#endif
    s->saved_volume = s->volume;
    s->saved_last_nonzero_volume = s->last_nonzero_volume;
}
static bool route(pqa_state *s, pqa_route r) {
    s->route = r;
    return r == PQA_RESERVED || r == PQA_CONSUMED;
}
static void release_panel(pqa_state *s, uint32_t now) {
    if(s->paper){
#ifdef PORTABLE_PAPER_TRANSITIONS
        const int distance=s->last_y-s->start_y;
        const bool open=s->start_position_q8==0?distance>=32:distance>-64;
        s->target_q8=open?pqa_open_q8(s):0;s->velocity_q8=0;
#else
        s->position_q8=s->target_q8;
#endif
        return;
    }
    int velocity = (now - s->last_ms > 100u) ? 0 : s->release_velocity_q8;
    bool open = s->position_q8 > pqa_open_q8(s) / 2 ? velocity > -384 : velocity > 512;
    s->target_q8 = open ? pqa_open_q8(s) : 0;
    s->velocity_q8 = velocity / 2;
}
bool pqa_input(pqa_state *s, uint32_t now, bool valid, unsigned count,
               uint32_t id, int x, int y, bool allow_open) {
    if (!s) return false;
#ifdef PORTABLE_QUICK_CAPABILITY_SAFE
    if(s->save_pending){
        if(!valid||count)s->save_cancelled=true;
        return route(s,PQA_CONSUMED);
    }
#endif
    /* Bound hostile/raw coordinates before subtraction or Q8 conversion. */
    x = clampi(x, -2048, 2048); y = clampi(y, -2048, 2048);
    pqa_layout native={0};if(s->native_width)native=pqa_layout_for(s);
    pqa_layout gesture_layout=native;
#ifdef PORTABLE_QUICK_SAFE_CONTENT
    gesture_layout.unit=(int)(pqa_width(s)<pqa_height(s)?pqa_width(s):pqa_height(s));
#endif
    int threshold=s->native_width?pqa_layout_size(&gesture_layout,6):6;
    if (!valid || count > 1u) {
        if (s->gesture == PQA_PANEL_DRAG)
            s->target_q8 = s->position_q8 >= pqa_open_q8(s) / 2 ? pqa_open_q8(s) : 0;
        pqa_cancel_input(s);
        return route(s, PQA_CONSUMED);
    }
    if (s->neutral_gate) {
        if (!count) s->neutral_gate = false;
        return route(s, PQA_CONSUMED);
    }
    if (!count) {
        pqa_gesture previous = s->gesture;
        s->gesture = PQA_IDLE;
        if (previous == PQA_TOP_PENDING) return route(s, PQA_REPLAY);
        if (previous == PQA_PASS_THROUGH || previous == PQA_IDLE)
            return route(s, pqa_visible(s) ? PQA_CONSUMED : PQA_PASS);
        if (previous == PQA_PANEL_DRAG) release_panel(s, now);
        else if (previous == PQA_BRIGHTNESS_DRAG) emit_brightness(s, true);
#ifdef PORTABLE_FRONTLIGHT_TONE
        else if(previous==PQA_TONE_DRAG)emit_tone(s,true);
#endif
        else if (previous == PQA_VOLUME_DRAG) {
            if (s->volume) s->last_nonzero_volume = s->volume;
            emit_volume(s, PQA_VOLUME_COMMIT);
        } else if (previous == PQA_TORCH_TAP) {
            if (!s->moved) { s->torch = false; emit_torch(s); }
        } else if (previous == PQA_PANEL_PENDING && !s->moved) {
            if (s->handle_pressed) s->target_q8 = 0;
#ifdef PORTABLE_PAPER_TRANSITIONS
            else if(s->paper && s->pressed_tile==6 && s->brightness_valid) {
                s->brightness=s->brightness?0:s->last_nonzero_brightness;
                emit_brightness(s,true);
            }
#endif
#ifdef PORTABLE_QUICK_USB_TRANSFER
            else if(s->paper && s->pressed_tile==7) {
                s->pending|=PQA_USB_TRANSFER;s->target_q8=0;
            }
#endif
            else if (s->pressed_tile == 0 && s->volume_valid) {
                if (s->volume) { s->last_nonzero_volume = s->volume; s->volume = 0; }
                else s->volume = s->last_nonzero_volume ? s->last_nonzero_volume : 50;
                emit_volume(s, PQA_SILENT);
            } else if(s->pressed_tile==9 && s->contexts_controls && contexts_valid(s)) {
                s->action_contexts=!s->contexts_enabled;s->pending|=PQA_CONTEXTS;
            } else if(s->pressed_tile==1 && s->dnd_valid) {
                s->dnd_enabled=!s->dnd_enabled;
                s->action_dnd=s->dnd_enabled;s->pending|=PQA_DND;
            } else if(s->pressed_tile==2 && s->radios_valid) {
                s->pending|=PQA_AIRPLANE;
            } else if(s->pressed_tile==4 && bluetooth_valid(s)) {
                s->pending|=PQA_BLUETOOTH;
            } else if (s->pressed_tile == 3 && (!s->radio_controls || wifi_valid(s))) {
                s->pending |= PQA_WIFI;if(!s->radio_controls)s->target_q8=0;
            }
#ifdef PORTABLE_RESIDENT_SHELL_HOST
            else if(s->pressed_tile==8 && s->clean_refresh_valid) {
                s->pending|=PQA_CLEAN_REFRESH;
            } else if(s->pressed_tile==5 && s->brightness_valid) {
                s->brightness=s->brightness?0:
#ifdef PORTABLE_PAPER_TRANSITIONS
                    s->last_nonzero_brightness;
#else
                    40;
#endif
                emit_brightness(s,true);
            }
#else
            else if (s->pressed_tile == 5 && s->torch_valid) {
                s->torch = true; s->target_q8 = 0; emit_torch(s);
            }
#endif
        }
        return route(s, PQA_CONSUMED);
    }
    if (s->gesture != PQA_IDLE && id != s->start_id) {
        pqa_cancel_input(s);
        return route(s, PQA_CONSUMED);
    }
    if (s->gesture == PQA_PASS_THROUGH) return route(s, PQA_PASS);
    if (s->gesture == PQA_IDLE) {
        begin_contact(s, now, id, x, y);
        if (s->torch) s->gesture = PQA_TORCH_TAP;
        else if (!pqa_visible(s)) {
            if (allow_open && y >= 0 && y < (s->paper?72:s->native_width?pqa_layout_size(&gesture_layout,PQA_TOP_EDGE):PQA_TOP_EDGE) && x >= 0 && x < (s->paper?480:(int)pqa_width(s)))
                s->gesture = PQA_TOP_PENDING;
            else s->gesture = PQA_PASS_THROUGH;
        } else {
            /* Moving panels are draggable but never activate controls. */
            int local_y = y + (pqa_open_q8(s) - s->position_q8) / 256;
            bool settled = s->position_q8 == pqa_open_q8(s) && s->target_q8 == pqa_open_q8(s);
            if (settled && s->brightness_valid && (s->paper ?
#ifdef PORTABLE_RESIDENT_SHELL_HOST
                paper_slider_at(s,0,x,y)
#else
                x>=32 && x<448 && y>=120 && y<180
#endif
                : s->native_width?pqa_rect_contains(native.slider_hit[0],x,local_y):x>=16 && x<224 && local_y>=(s->contexts_controls?24:48) && local_y<(s->contexts_controls?53:77)))
                s->gesture = PQA_BRIGHTNESS_DRAG;
#ifdef PORTABLE_FRONTLIGHT_TONE
            else if(settled && s->paper && s->tone_controls && s->tone_valid && paper_slider_at(s,2,x,y))
                s->gesture=PQA_TONE_DRAG;
#endif
            else if (settled &&
#ifdef PORTABLE_RESIDENT_SHELL_HOST
                s->audio_controls &&
#endif
                s->volume_valid && (s->paper ?
#ifdef PORTABLE_RESIDENT_SHELL_HOST
                paper_slider_at(s,1,x,y)
#else
                x>=32 && x<448 && y>=214 && y<274
#endif
                : s->native_width?pqa_rect_contains(native.slider_hit[1],x,local_y):x>=16 && x<224 && local_y>=(s->contexts_controls?54:78) && local_y<(s->contexts_controls?83:107)))
                s->gesture = PQA_VOLUME_DRAG;
            else {
                s->gesture = PQA_PANEL_PENDING;
                if (settled) {
                    s->pressed_tile = (int8_t)tile_at(s,x, local_y);
                    s->handle_pressed = s->paper ? x>=32 && x<448 &&
#ifdef PORTABLE_RESIDENT_SHELL_HOST
                        y>=714 && y<742
#else
#ifdef PORTABLE_QUICK_USB_TRANSFER
                        y>=684 && y<726
#else
                        y>=640 && y<714
#endif
#endif
                        : s->native_width?pqa_rect_contains(native.handle,x,local_y):x>=76 && x<164 && local_y>=(int)pqa_height(s)-(s->contexts_controls?18:33) && local_y<(int)pqa_height(s)-1;
                }
            }
        }
        if (s->gesture == PQA_BRIGHTNESS_DRAG || s->gesture == PQA_VOLUME_DRAG || s->gesture==PQA_TONE_DRAG) slide(s, x);
        return route(s, s->gesture == PQA_PASS_THROUGH ? PQA_PASS :
                       s->gesture == PQA_TOP_PENDING ? PQA_RESERVED : PQA_CONSUMED);
    }
    int dx = x - s->start_x, dy = y - s->start_y;
    if (absi(dx) > threshold || absi(dy) > threshold) s->moved = true;
    if (s->gesture == PQA_TOP_PENDING) {
        if (dy > (s->paper?24:threshold) && dy > absi(dx)) {
            s->gesture = PQA_PANEL_DRAG;
#ifndef PORTABLE_PAPER_TRANSITIONS
            if(s->paper)s->position_q8=s->target_q8=pqa_open_q8(s);
#endif
        }
        else if (absi(dx) > (s->paper?24:threshold) || dy < -(s->paper?24:threshold)) {
            s->gesture = PQA_PASS_THROUGH;
            return route(s, PQA_REPLAY);
        } else return route(s, PQA_RESERVED);
    } else if (s->gesture == PQA_PANEL_PENDING && absi(dy) > threshold && absi(dy) > absi(dx)) {
        s->gesture = PQA_PANEL_DRAG;
    }
    if (s->gesture == PQA_PANEL_DRAG) {
        if(s->paper) {
#ifdef PORTABLE_PAPER_TRANSITIONS
            s->position_q8=clampi(s->start_position_q8+(int32_t)dy*pqa_open_q8(s)/800,0,pqa_open_q8(s));
            s->target_q8=s->position_q8;s->velocity_q8=0;
#else
            if(s->start_position_q8 && dy < -32)s->position_q8=s->target_q8=0;
#endif
            s->last_x=(int16_t)x;s->last_y=(int16_t)y;s->last_ms=now;
            return route(s,PQA_CONSUMED);
        }
        s->position_q8 = clampi(s->start_position_q8 + dy * 256, 0, pqa_open_q8(s));
        uint32_t elapsed = now - s->last_ms;
        if (elapsed) s->release_velocity_q8 = clampi((y - s->last_y) * 2048 / (int)(elapsed > 1000u ? 1000u : elapsed), -8192, 8192);
        s->velocity_q8 = 0;
        s->last_y = (int16_t)y; s->last_x = (int16_t)x; s->last_ms = now;
    } else if (s->gesture == PQA_BRIGHTNESS_DRAG || s->gesture == PQA_VOLUME_DRAG || s->gesture==PQA_TONE_DRAG) slide(s, x);
    return route(s, PQA_CONSUMED);
}
bool pqa_animate(pqa_state *s, uint32_t now) {
#ifndef PORTABLE_PAPER_TRANSITIONS
    if(s && s->paper){bool changed=s->position_q8!=s->target_q8;s->position_q8=s->target_q8;s->velocity_q8=0;return changed;}
#endif
    if (!s) return false;
    if (!s->clock_valid) { s->clock_valid = true; s->animation_ms = now; return false; }
    uint32_t elapsed = now - s->animation_ms;
    s->animation_ms = now;
    /* A long suspension resolves to the target without an unbounded catch-up. */
    if (elapsed > 1000u && s->gesture != PQA_PANEL_DRAG) {
        bool changed = s->position_q8 != s->target_q8;
        s->position_q8 = s->target_q8; s->velocity_q8 = 0; s->animation_remainder_ms = 0;
        return changed;
    }
    s->animation_remainder_ms += elapsed > 1000u ? 1000u : elapsed;
    unsigned steps = s->animation_remainder_ms / PQA_ANIMATION_STEP_MS;
    s->animation_remainder_ms %= PQA_ANIMATION_STEP_MS;
    if (s->gesture == PQA_PANEL_DRAG || s->gesture == PQA_TOP_PENDING) return false;
    int32_t before = s->position_q8;
    while (steps--) {
        int delta = s->target_q8 - s->position_q8;
        s->velocity_q8 = (s->velocity_q8 + delta / 18) * 3 / 4;
        s->position_q8 = clampi(s->position_q8 + s->velocity_q8, 0, pqa_open_q8(s));
        if (absi(delta) < 64 && absi(s->velocity_q8) < 64) {
            s->position_q8 = s->target_q8; s->velocity_q8 = 0;
        }
        if ((s->position_q8 == 0 && s->velocity_q8 < 0) ||
            (s->position_q8 == pqa_open_q8(s) && s->velocity_q8 > 0)) s->velocity_q8 = 0;
    }
    return s->position_q8 != before;
}
