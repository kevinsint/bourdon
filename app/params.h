/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kevin Sint */
/* A parameter: the value one encoder holds, 0 to its top. */
#pragma once
#include <stdint.h>

typedef struct {
    uint8_t top;                    /* the highest value */
    uint8_t per_click;              /* what one click of the encoder moves it */
    uint8_t start;                  /* its value at power-on */
} param_spec_t;

/* the value after an encoder's clicks (+ = clockwise); it stops at the ends */
static int32_t param_turned(const param_spec_t *spec, int32_t value, int32_t clicks)
{
    if (clicks > spec->top)         /* more than the whole range: keeps the sum below small */
        clicks = spec->top;
    if (clicks < -(int32_t)spec->top)
        clicks = -(int32_t)spec->top;
    value += clicks * spec->per_click;
    return value < 0 ? 0 : value > spec->top ? spec->top : value;
}

/* the value after an encoder's clicks with the snap key held: each click takes it to the next of its places that
 * way: its bottom, its middle, its value at power-on and its top */
static int32_t param_snapped(const param_spec_t *spec, int32_t value, int32_t clicks)
{
    const int32_t place[4] = { 0, (spec->top + 1) / 2, spec->start, spec->top };
    int32_t i, to;
    clicks = clicks > 4 ? 4 : clicks < -4 ? -4 : clicks;        /* four places: no more clicks do anything */
    value = value < 0 ? 0 : value > spec->top ? spec->top : value;
    for (; clicks; clicks -= clicks > 0 ? 1 : -1) {
        to = clicks > 0 ? spec->top : 0;
        for (i = 0; i < 4; i++)
            if (clicks > 0 ? place[i] > value && place[i] < to : place[i] < value && place[i] > to)
                to = place[i];
        value = to;
    }
    return value;
}
/* the same for a value whose places are every `every`: -100, -50, 0, 50, 100. The caller keeps it in its range */
static int32_t param_snapped_every(int32_t value, int32_t clicks, int32_t every)
{
    int32_t below = value >= 0 ? value / every : -((-value + every - 1) / every);      /* rounded down */
    int32_t above = value >= 0 ? (value + every - 1) / every : -(-value / every);       /* rounded up */
    return clicks > 0 ? (below + clicks) * every : clicks < 0 ? (above + clicks) * every : value;
}

/* the height of the value's bar, in a space full_height high (rounded) */
static uint32_t param_bar_height(const param_spec_t *spec, int32_t value, uint32_t full_height)
{
    return ((uint32_t)value * full_height + spec->top / 2u) / spec->top;
}
