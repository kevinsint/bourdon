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

    check("it starts on the voice's page", panel.page == PANEL_VOICE && PANEL_NPAGE == 2 + DRONE_LFOS);
    for (i = 1; i <= DRONE_LFOS; i++) {
        panel_next_page(&panel);
        check("the LFO key: the next LFO's page", panel.page == i);
    }
    panel_next_page(&panel);
    check("after the last LFO: the reverb", panel.page == PANEL_REVERB);
    panel_next_page(&panel);
    check("after the reverb: the voice again", panel.page == PANEL_VOICE);

    for (i = 0; i < DRONE_NPARAM; i++)
        check("the voice's page: an encoder for each parameter", panel_spec(PANEL_VOICE, i) == &drone_param_spec[i]);
    check("an LFO's page: its four parameters, then nothing",
          panel_spec(2, 0) == &drone_lfo_spec[0] && panel_spec(2, DRONE_LFO_NPARAM - 1) == &drone_lfo_spec[DRONE_LFO_NPARAM - 1]
          && !panel_spec(2, DRONE_LFO_NPARAM));
    check("the reverb's page: the voice's send, the reverb's three, then nothing",
          panel_spec(PANEL_REVERB, 0) == &drone_send_spec && panel_spec(PANEL_REVERB, 1) == &drone_reverb_spec[0]
          && panel_spec(PANEL_REVERB, DRONE_REVERB_NPARAM) == &drone_reverb_spec[DRONE_REVERB_NPARAM - 1]
          && !panel_spec(PANEL_REVERB, DRONE_REVERB_NPARAM + 1));

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
