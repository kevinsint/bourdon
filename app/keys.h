/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kevin Sint */
/* What a press of a key means: let go before KEY_HOLD_MS it is a toggle; held that long it is a select. A control
 * moved while the key is down makes it a select at once, so that the move is for what the key selects. */
#pragma once
#include <stdint.h>

#define KEY_HOLD_MS 1000u
typedef enum { KEY_NOTHING, KEY_TOGGLE, KEY_SELECT } key_gesture_t;
typedef struct {
    uint8_t down;                   /* the key at the last call */
    uint8_t held;                   /* this press has given its select already */
    uint32_t pressed_at;            /* ms */
} key_hold_t;

/* call with the key's state every turn of the main loop, and `moved`: a control has moved in this turn. The toggle
 * comes when the key is let go, the select while it is still down, one time */
static key_gesture_t key_gesture(key_hold_t *key, int down, uint32_t now_ms, int moved)
{
    key_gesture_t gesture = KEY_NOTHING;
    if (down && !key->down) {
        key->pressed_at = now_ms;
        key->held = 0;
    }
    if (down && !key->held && (moved || now_ms - key->pressed_at >= KEY_HOLD_MS)) {
        key->held = 1;
        gesture = KEY_SELECT;
    } else if (!down && key->down && !key->held) {
        gesture = KEY_TOGGLE;
    }
    key->down = down != 0;
    return gesture;
}
