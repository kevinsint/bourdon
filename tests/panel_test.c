/* The panel: the LFO key goes round the pages; SEL held and the SELECT encoder choose the filter, and the choice
 * is the voice's filter only when SEL is let go. */
#include <stdio.h>
#include "panel.h"

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
    panel_t panel = { 0, 0, 0 };
    uint32_t filter = DRONE_LADDER, i;

    check("it starts on the voice's page", panel.page == PANEL_VOICE && PANEL_NPAGE == 1 + DRONE_LFOS);
    for (i = 1; i <= DRONE_LFOS; i++) {
        panel_next_page(&panel);
        check("the LFO key: the next LFO's page", panel.page == i);
    }
    panel_next_page(&panel);
    check("after the last LFO: the voice again", panel.page == PANEL_VOICE);

    filter = panel_filter(&panel, 0, 3, filter);
    check("the encoder without SEL does not choose a filter", filter == DRONE_LADDER && !panel.choosing);
    filter = panel_filter(&panel, 1, 0, filter);
    check("SEL down: the choice starts at the voice's filter", panel.choosing && panel.choice == DRONE_LADDER);
    filter = panel_filter(&panel, 1, 2, filter);
    check("two clicks: the choice is two further, the voice's filter is not yet", panel.choice == DRONE_POLIVOKS && filter == DRONE_LADDER);
    filter = panel_filter(&panel, 1, -1, filter);
    check("a click back", panel.choice == DRONE_MS20 && filter == DRONE_LADDER);
    filter = panel_filter(&panel, 0, 0, filter);
    check("SEL let go: the voice has the choice", filter == DRONE_MS20 && !panel.choosing);
    filter = panel_filter(&panel, 0, 0, filter);
    check("and keeps it", filter == DRONE_MS20);

    filter = panel_filter(&panel, 1, 0, filter);
    filter = panel_filter(&panel, 0, 0, filter);
    check("SEL down and up with no turn changes nothing", filter == DRONE_MS20);

    filter = panel_filter(&panel, 1, 100, filter);
    check("the choice stops at the last filter", panel.choice == DRONE_NFILTER - 1);
    filter = panel_filter(&panel, 1, -100, filter);
    check("and at the first", panel.choice == 0);
    filter = panel_filter(&panel, 0, 5, filter);
    check("clicks in the turn SEL is let go do not count", filter == 0);
    puts(failed ? "panel_test: FAILED" : "panel_test: ok");
    return failed;
}
