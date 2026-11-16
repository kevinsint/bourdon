/* A key: a short press toggles (when it is let go), a hold of a second selects (at the second, one time). */
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
    check("nothing pressed: nothing", key_gesture(&key, 0, 0) == KEY_NOTHING);
    check("a press alone does nothing yet", key_gesture(&key, 1, 100) == KEY_NOTHING);
    check("still down after half a second: nothing", key_gesture(&key, 1, 600) == KEY_NOTHING);
    check("let go before a second: toggle", key_gesture(&key, 0, 1050) == KEY_TOGGLE);
    check("and only one time", key_gesture(&key, 0, 1060) == KEY_NOTHING);

    key_gesture(&key, 1, 2000);
    for (t = 2001; t < 5000; t++)               /* held three seconds */
        selects += key_gesture(&key, 1, t) == KEY_SELECT;
    check("a hold selects one time", selects == 1);
    check("at one second", key.pressed_at == 2000 && KEY_HOLD_MS == 1000);
    check("letting go after a hold does not toggle", key_gesture(&key, 0, 5000) == KEY_NOTHING);

    key_gesture(&key, 1, 6000);
    check("a second less a millisecond is still short", key_gesture(&key, 0, 6999) == KEY_TOGGLE);
    key_gesture(&key, 1, 8000);
    check("a whole second is a hold", key_gesture(&key, 1, 9000) == KEY_SELECT && key_gesture(&key, 0, 9001) == KEY_NOTHING);

    key_gesture(&key, 1, 0xFFFFFF00u);          /* the millisecond count goes round after 49 days */
    check("across the count's turn: short", key_gesture(&key, 0, 0x00000010u) == KEY_TOGGLE);
    key_gesture(&key, 1, 0xFFFFFF00u);
    check("across the count's turn: hold", key_gesture(&key, 1, 0x00000300u) == KEY_SELECT);
    puts(failed ? "keys_test: FAILED" : "keys_test: ok");
    return failed;
}
