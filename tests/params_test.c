/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kevin Sint */
/* An encoder moves its parameter in the parameter's own steps and stops at its ends; the bar is as tall as the value. */
#include <stdio.h>
#include "params.h"

static int failed;
static void check(const char *what, int ok)
{
    if (!ok) {
        printf("FAIL: %s\n", what);
        failed = 1;
    }
}

int main(void)
{
    static const param_spec_t wide = { 127, 4, 64 }, fine = { 127, 1, 42 }, choice = { 2, 1, 0 };
    check("one click clockwise adds the parameter's step", param_turned(&wide, 64, 1) == 68 && param_turned(&fine, 64, 1) == 65);
    check("one click the other way takes it", param_turned(&wide, 64, -1) == 60);
    check("no click changes nothing", param_turned(&wide, 64, 0) == 64);
    check("it stops at the top", param_turned(&wide, 126, 1) == 127 && param_turned(&wide, 127, 5) == 127);
    check("it stops at the bottom", param_turned(&wide, 1, -1) == 0 && param_turned(&wide, 0, -5) == 0);
    check("a fast turn far past the end still stops", param_turned(&wide, 0, 30000) == 127 && param_turned(&wide, 127, -30000) == 0);
    check("a choice of three has three places", param_turned(&choice, 0, 1) == 1 && param_turned(&choice, 2, 1) == 2
          && param_turned(&choice, 1, -5) == 0);
    check("the bar is empty at 0", param_bar_height(&wide, 0, 100) == 0);
    check("the bar is full at the top", param_bar_height(&wide, 127, 100) == 100 && param_bar_height(&choice, 2, 100) == 100);
    check("the bar is half way in the middle", param_bar_height(&wide, 64, 100) == 50 && param_bar_height(&choice, 1, 100) == 50);
    {   /* the snap key: each click to the next of the value's places */
        const param_spec_t knob = { 127, 4, 64 }, level = { 127, 4, 96 }, count = { 16, 1, 4 }, offset = { 200, 5, 100 }, on_off = { 1, 1, 0 };
        check("a knob that starts at its middle: its bottom, its middle, its top", param_snapped(&knob, 10, 1) == 64 && param_snapped(&knob, 64, 1) == 127
              && param_snapped(&knob, 127, 1) == 127 && param_snapped(&knob, 100, -1) == 64 && param_snapped(&knob, 64, -1) == 0
              && param_snapped(&knob, 0, -1) == 0 && param_snapped(&knob, 63, 1) == 64 && param_snapped(&knob, 65, -1) == 64);
        check("a value that starts elsewhere: that place too, in its order", param_snapped(&level, 0, 1) == 64 && param_snapped(&level, 64, 1) == 96
              && param_snapped(&level, 96, 1) == 127 && param_snapped(&level, 127, -1) == 96 && param_snapped(&level, 96, -1) == 64);
        check("two clicks: two places; more clicks than places: its end", param_snapped(&level, 0, 2) == 96 && param_snapped(&level, 127, -2) == 64
              && param_snapped(&level, 10, 100000) == 127 && param_snapped(&level, 100, -100000) == 0);
        check("no click: the value as it is; a value out of its range: kept in it first", param_snapped(&level, 33, 0) == 33
              && param_snapped(&level, 900, -1) == 96 && param_snapped(&level, -5, 1) == 64);
        check("a count: 0, its half, its start, its top", param_snapped(&count, 0, 1) == 4 && param_snapped(&count, 4, 1) == 8
              && param_snapped(&count, 8, 1) == 16 && param_snapped(&count, 16, -3) == 0);
        check("an offset, its middle its start: -100 %, none, +100 %", param_snapped(&offset, 130, -1) == 100 && param_snapped(&offset, 100, -1) == 0
              && param_snapped(&offset, 100, 1) == 200);
        check("a value of two places: one, then the other", param_snapped(&on_off, 0, 1) == 1 && param_snapped(&on_off, 1, -1) == 0);
        check("places every 50: up and down, from between two and from one of them, below 0 too", param_snapped_every(37, 1, 50) == 50
              && param_snapped_every(37, -1, 50) == 0 && param_snapped_every(50, 1, 50) == 100 && param_snapped_every(50, -1, 50) == 0
              && param_snapped_every(-20, -1, 50) == -50 && param_snapped_every(-20, 1, 50) == 0 && param_snapped_every(-50, -1, 50) == -100
              && param_snapped_every(-50, 1, 50) == 0 && param_snapped_every(0, 2, 50) == 100 && param_snapped_every(13, 0, 50) == 13
              && param_snapped_every(63, 1, 10) == 70 && param_snapped_every(63, -1, 10) == 60);
    }
    puts(failed ? "params_test: FAILED" : "params_test: ok");
    return failed;
}
