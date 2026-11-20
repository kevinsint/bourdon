/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kevin Filteau */
/* A key: a short press toggles (when it is let go), a hold of a second selects (at the second, one time); a control
 * moved while the key is down selects at once. */
#include <stdio.h>
#include "keys.h"

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
    key_hold_t key = { 0, 0, 0 };
    uint32_t t;
    int selects = 0;
    check("nothing pressed: nothing", key_gesture(&key, 0, 0, 0) == KEY_NOTHING);
    check("a press alone does nothing yet", key_gesture(&key, 1, 100, 0) == KEY_NOTHING);
    check("still down after half a second: nothing", key_gesture(&key, 1, 600, 0) == KEY_NOTHING);
    check("let go before a second: toggle", key_gesture(&key, 0, 1050, 0) == KEY_TOGGLE);
    check("and only one time", key_gesture(&key, 0, 1060, 0) == KEY_NOTHING);

    key_gesture(&key, 1, 2000, 0);
    for (t = 2001; t < 5000; t++)               /* held three seconds */
        selects += key_gesture(&key, 1, t, 0) == KEY_SELECT;
    check("a hold selects one time", selects == 1);
    check("at one second", key.pressed_at == 2000 && KEY_HOLD_MS == 1000);
    check("letting go after a hold does not toggle", key_gesture(&key, 0, 5000, 0) == KEY_NOTHING);

    key_gesture(&key, 1, 6000, 0);
    check("a second less a millisecond is still short", key_gesture(&key, 0, 6999, 0) == KEY_TOGGLE);
    key_gesture(&key, 1, 8000, 0);
    check("a whole second is a hold", key_gesture(&key, 1, 9000, 0) == KEY_SELECT && key_gesture(&key, 0, 9001, 0) == KEY_NOTHING);

    key_gesture(&key, 1, 0xFFFFFF00u, 0);          /* the millisecond count goes round after 49 days */
    check("across the count's turn: short", key_gesture(&key, 0, 0x00000010u, 0) == KEY_TOGGLE);
    key_gesture(&key, 1, 0xFFFFFF00u, 0);
    check("across the count's turn: hold", key_gesture(&key, 1, 0x00000300u, 0) == KEY_SELECT);
    key_gesture(&key, 0, 19000, 0);             /* let go after the hold above */
    key_gesture(&key, 1, 20000, 0);             /* held, and a control is moved before the second is over */
    check("a control moved with no key down: nothing", key_gesture(&(key_hold_t){ 0, 0, 0 }, 0, 20000, 1) == KEY_NOTHING);
    check("the key held, nothing moved yet: nothing", key_gesture(&key, 1, 20100, 0) == KEY_NOTHING);
    check("a control moved while the key is down: select at once", key_gesture(&key, 1, 20200, 1) == KEY_SELECT);
    check("moved again, and the second over: no second select", key_gesture(&key, 1, 20300, 1) == KEY_NOTHING
          && key_gesture(&key, 1, 21500, 0) == KEY_NOTHING);
    check("letting go after that does not toggle", key_gesture(&key, 0, 21600, 0) == KEY_NOTHING);
    check("a control moved in the same turn as the press: select", key_gesture(&key, 1, 30000, 1) == KEY_SELECT
          && key_gesture(&key, 0, 30100, 0) == KEY_NOTHING);
    puts(failed ? "keys_test: FAILED" : "keys_test: ok");
    return failed;
}
