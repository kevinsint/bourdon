/* A parameter: the value one encoder holds, 0 to PARAM_MAX. */
#pragma once
#include <stdint.h>

#define PARAM_MAX 127
#define PARAM_PER_CLICK 4           /* 32 clicks from one end to the other */

/* the value after an encoder's clicks (+ = clockwise); it stops at the ends */
static int32_t param_turned(int32_t value, int32_t clicks)
{
    if (clicks > PARAM_MAX)         /* more than the whole range: keeps the sum below small */
        clicks = PARAM_MAX;
    if (clicks < -PARAM_MAX)
        clicks = -PARAM_MAX;
    value += clicks * PARAM_PER_CLICK;
    return value < 0 ? 0 : value > PARAM_MAX ? PARAM_MAX : value;
}

/* the height of the value's bar, in a space full_height high (rounded) */
static uint32_t param_bar_height(int32_t value, uint32_t full_height)
{
    return ((uint32_t)value * full_height + PARAM_MAX / 2u) / PARAM_MAX;
}
