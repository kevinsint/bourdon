/* The panel's pages and the choice of filter. The LFO key goes from the voice's page through its LFOs' pages and
 * back. While SEL is held, the SELECT encoder moves a choice of filter that the screen shows; the voice gets it
 * when SEL is let go. */
#pragma once
#include <stdint.h>
#include "drone.h"

enum { PANEL_VOICE, PANEL_NPAGE = 1 + DRONE_LFOS };     /* page 1..: that LFO of the voice */
static const char *const panel_page_name[PANEL_NPAGE] = { "VOICE", "LFO 1", "LFO 2", "LFO 3" };
static const char *const panel_filter_name[DRONE_NFILTER] = {
    [DRONE_LADDER] = "LADDER", [DRONE_MS20] = "MS-20", [DRONE_POLIVOKS] = "POLIVOKS", [DRONE_WASP] = "WASP",
};

typedef struct {
    uint8_t page;
    uint8_t choosing;               /* SEL is down */
    uint8_t choice;                 /* the filter chosen so far, while SEL is down */
} panel_t;

static void panel_next_page(panel_t *panel)
{
    panel->page = (uint8_t)((panel->page + 1u) % PANEL_NPAGE);
}

/* call every turn of the main loop with SEL's state and the SELECT encoder's clicks; gives the voice's filter:
 * `filter` as it is, or the choice in the turn SEL is let go */
static uint32_t panel_filter(panel_t *panel, int sel_down, int32_t clicks, uint32_t filter)
{
    if (sel_down && !panel->choosing)
        panel->choice = (uint8_t)filter;
    if (sel_down)
        panel->choice = (uint8_t)param_turned(&drone_filter_spec, panel->choice, clicks);
    else if (panel->choosing)
        filter = panel->choice;
    panel->choosing = sel_down != 0;
    return filter;
}
