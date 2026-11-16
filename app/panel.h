/* The panel's pages and the choice of filter. The LFO key goes from the voice's page through its LFOs' pages, then
 * the reverb's, and back. While SEL is held, the SELECT encoder moves a choice of filter that the screen shows; the voice gets it
 * when SEL is let go. */
#pragma once
#include <stdint.h>
#include "drone.h"

enum { PANEL_VOICE, PANEL_REVERB = 1 + DRONE_LFOS, PANEL_NPAGE };       /* the pages between: that LFO of the voice */
static const char *const panel_page_name[PANEL_NPAGE] = { "VOICE", "LFO 1", "LFO 2", "LFO 3", "REVERB" };
static const char *const panel_filter_name[DRONE_NFILTER] = {
    [DRONE_LADDER] = "LADDER", [DRONE_MS20] = "MS-20", [DRONE_POLIVOKS] = "POLIVOKS", [DRONE_WASP] = "WASP",
};

/* the parameter an encoder holds on a page, or 0: none. The reverb's page: the selected voice's send, then the
 * reverb's own, which all the voices share */
static const param_spec_t *panel_spec(uint32_t page, uint32_t encoder)
{
    if (page == PANEL_VOICE)
        return encoder < DRONE_NPARAM ? &drone_param_spec[encoder] : 0;
    if (page == PANEL_REVERB)
        return !encoder ? &drone_send_spec : encoder <= DRONE_REVERB_NPARAM ? &drone_reverb_spec[encoder - 1u] : 0;
    return encoder < DRONE_LFO_NPARAM ? &drone_lfo_spec[encoder] : 0;
}

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
