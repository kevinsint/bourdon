/* An encoder moves its parameter in steps and stops at the ends; the bar is as tall as the value. */
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
    check("one click clockwise adds a step", param_turned(64, 1) == 64 + PARAM_PER_CLICK);
    check("one click the other way takes a step", param_turned(64, -1) == 64 - PARAM_PER_CLICK);
    check("no click changes nothing", param_turned(64, 0) == 64);
    check("it stops at the top", param_turned(126, 1) == PARAM_MAX && param_turned(PARAM_MAX, 5) == PARAM_MAX);
    check("it stops at the bottom", param_turned(1, -1) == 0 && param_turned(0, -5) == 0);
    check("a fast turn far past the end still stops", param_turned(0, 30000) == PARAM_MAX && param_turned(PARAM_MAX, -30000) == 0);
    check("the bar is empty at 0", param_bar_height(0, 100) == 0);
    check("the bar is full at the top", param_bar_height(PARAM_MAX, 100) == 100);
    check("the bar is half way in the middle", param_bar_height(64, 100) == 50);
    puts(failed ? "params_test: FAILED" : "params_test: ok");
    return failed;
}
