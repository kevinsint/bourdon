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
    puts(failed ? "params_test: FAILED" : "params_test: ok");
    return failed;
}
