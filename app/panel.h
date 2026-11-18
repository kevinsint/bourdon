/* The panel's pages and the choice of filter. The LFO key goes from the voice's page through its LFOs' pages, then
 * the reverb's, and back. While SEL is held, the SELECT encoder moves a choice of filter that the screen shows; the voice gets it
 * when SEL is let go.
 *
 * A voice that is a patch (drone.h) has no LFO pages, and its page is its chain: the SELECT encoder moves along
 * the places of the chain (each block, then the empty place for a new one while there is room, then OUT). With SEL
 * held, SELECT chooses what block the place gets (none: the block is taken away), and the next encoder moves the
 * block along the chain. Without SEL that encoder is the block's kind, and the four right ones its values. */
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
    uint8_t choice;                 /* the filter, or a patch's block, chosen so far, while SEL is down */
} panel_t;

static void panel_next_page(panel_t *panel, int patch)      /* a patch: its chain, the reverb, and back */
{
    panel->page = (uint8_t)(patch ? (panel->page == PANEL_VOICE ? PANEL_REVERB : PANEL_VOICE) : (panel->page + 1u) % PANEL_NPAGE);
}

/* call every turn of the main loop with SEL's state and the SELECT encoder's clicks; gives what is chosen (a
 * filter, a block: `spec` says how many there are): `current` as it is, or the choice in the turn SEL is let go */
static uint32_t panel_choice(panel_t *panel, int sel_down, int32_t clicks, const param_spec_t *spec, uint32_t current)
{
    if (sel_down && !panel->choosing)
        panel->choice = (uint8_t)current;
    if (sel_down)
        panel->choice = (uint8_t)param_turned(spec, panel->choice, clicks);
    else if (panel->choosing)
        current = panel->choice;
    panel->choosing = sel_down != 0;
    return current;
}

/* ---- a patch ---- */
#define PANEL_PATCH_VOICES 3u       /* the first voices, OP1 to OP3, are patches */
#define PANEL_ENC_KIND 1u           /* the block's kind; with SEL: the block along the chain */
#define PANEL_ENC_VALUES 3u         /* the first of the four right encoders: the block's values */
enum { PANEL_AT_BLOCK, PANEL_AT_NEW, PANEL_AT_OUT };

static uint32_t panel_patch_count(const drone_block_controls_t *block)      /* the blocks of the chain: they have no gap */
{
    uint32_t count = 0;
    while (count < DRONE_SLOTS && block[count].type != DRONE_BLOCK_NONE && block[count].type < DRONE_NBLOCK)
        count++;
    return count;
}
static uint32_t panel_patch_places(uint32_t count)
{
    return count + (count < DRONE_SLOTS) + 1u;
}
static uint32_t panel_patch_at(uint32_t count, uint32_t place)
{
    return place < count ? PANEL_AT_BLOCK : place == count && count < DRONE_SLOTS ? PANEL_AT_NEW : PANEL_AT_OUT;
}

static drone_block_controls_t panel_block_new(uint32_t type)     /* a block as it is when it is put: its values at their start */
{
    drone_block_controls_t block = { (uint8_t)type, drone_block_kind_spec[type].start, { 0 } };
    uint32_t p;
    for (p = 0; p < DRONE_BLOCK_NPARAM; p++)
        block.param[p] = drone_block_spec[type][p].start;
    return block;
}

/* the place gets a block of that type: a block's place another block in its place, the empty place one more block at
 * the end. DRONE_BLOCK_NONE takes the place's block away, and those after it move down */
static void panel_patch_put(drone_block_controls_t *block, uint32_t place, uint32_t type)
{
    uint32_t count = panel_patch_count(block);
    if (place >= DRONE_SLOTS || place > count || type >= DRONE_NBLOCK)
        return;
    if (type != DRONE_BLOCK_NONE) {
        block[place] = panel_block_new(type);
        return;
    }
    for (; place + 1u < DRONE_SLOTS; place++)
        block[place] = block[place + 1u];
    block[DRONE_SLOTS - 1u] = panel_block_new(DRONE_BLOCK_NONE);
}

/* the place's block changes places with the one before it (direction below 0) or after it; gives its new place */
static uint32_t panel_patch_move(drone_block_controls_t *block, uint32_t place, int32_t direction)
{
    uint32_t count = panel_patch_count(block), to = direction < 0 ? place - 1u : place + 1u;
    drone_block_controls_t moved;
    if (!direction || place >= count || to >= count)        /* place 0 and a direction below 0: `to` is far above count */
        return place;
    moved = block[place];
    block[place] = block[to];
    block[to] = moved;
    return to;
}

/* the parameter an encoder holds on a patch's page, where the cursor is; 0: none. OUT has the voice's level, fade
 * and send */
static const param_spec_t *panel_patch_spec(uint32_t at, uint32_t type, uint32_t encoder)
{
    static const param_spec_t *const out[] = { &drone_param_spec[DRONE_LEVEL], &drone_param_spec[DRONE_FADE], &drone_send_spec };
    const param_spec_t *spec = 0;
    if (at == PANEL_AT_OUT)
        return encoder >= PANEL_ENC_VALUES && encoder < PANEL_ENC_VALUES + 3u ? out[encoder - PANEL_ENC_VALUES] : 0;
    if (at != PANEL_AT_BLOCK || type >= DRONE_NBLOCK)
        return 0;
    if (encoder == PANEL_ENC_KIND)
        spec = &drone_block_kind_spec[type];
    else if (encoder >= PANEL_ENC_VALUES && encoder < PANEL_ENC_VALUES + DRONE_BLOCK_NPARAM)
        spec = &drone_block_spec[type][encoder - PANEL_ENC_VALUES];
    return spec && spec->top ? spec : 0;
}
