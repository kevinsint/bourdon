/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kevin Filteau */
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

/* the height of the value's bar, in a space full_height high (rounded) */
static uint32_t param_bar_height(const param_spec_t *spec, int32_t value, uint32_t full_height)
{
    return ((uint32_t)value * full_height + spec->top / 2u) / spec->top;
}
