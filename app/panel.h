/* The panel's pages and the choice of filter. The LFO key goes from the voice's page through its LFOs' pages, then
 * the reverb's, and back. While SEL is held, the SELECT encoder moves a choice of filter that the screen shows; the voice gets it
 * when SEL is let go.
 *
 * A voice that is a patch (drone.h) has its chain on its page: the SELECT encoder moves along the places of the
 * chain (each block, then the empty place for a new one while there is room, then OUT). The ALGORITHM encoder is
 * the block's kind, and the four right ones its values. OCT- is the shift key. While it is held:
 *   SELECT chooses what block the place gets (none: the block is taken away), applied when OCT- is let go
 *   PRESETS moves the block along the chain
 *   a value's encoder moves it in the smallest steps; an oscillator's first one is its fine tuning
 * On a patch's LFO pages SELECT still moves along the chain, ALGORITHM is the LFO's wave and the first right encoder
 * its speed. While the LFO key is held, on any of these pages, a value's encoder is the cable from the LFO to that
 * value: its amount, -100 .. 100 (the LFO: the open page's, or the one whose page was open last). */
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

static void panel_next_page(panel_t *panel)
{
    panel->page = (uint8_t)((panel->page + 1u) % PANEL_NPAGE);
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
/* the encoders' numbers, read from an FM-1 by turning each one (pannel-description.md): SELECT is 0, ALGORITHM 1,
 * KNOB1 to KNOB4 are 2 to 5, PRESETS is 6 */
#define PANEL_ENC_KIND 1u           /* ALGORITHM: the block's kind, an LFO's wave */
#define PANEL_ENC_VALUES 2u         /* KNOB1, the first of the four right encoders: the block's values */
#define PANEL_ENC_MOVE 6u           /* PRESETS, with shift: the block along the chain */
#define PANEL_AMOUNT_STEP 5         /* a click of a cable's amount; with shift: 1 */
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
    drone_block_controls_t block = { .type = (uint8_t)type, .kind = drone_block_kind_spec[type].start };
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

/* which of the place's values an encoder holds, or -1: none. A block: its value's number (shift: an oscillator's
 * fine tuning in place of its pitch). OUT: 0 the voice's level, 1 its fade, 2 its send */
enum { PANEL_OUT_LEVEL, PANEL_OUT_FADE, PANEL_OUT_SEND, PANEL_OUT_NVALUE };
static int32_t panel_patch_value(uint32_t at, uint32_t type, uint32_t encoder, int shift)
{
    uint32_t n = encoder - PANEL_ENC_VALUES;
    if (encoder < PANEL_ENC_VALUES)
        return -1;
    if (at == PANEL_AT_OUT)
        return n < PANEL_OUT_NVALUE ? (int32_t)n : -1;
    if (at != PANEL_AT_BLOCK || type >= DRONE_NBLOCK)
        return -1;
    if (type == DRONE_BLOCK_VCO)
        return n ? -1 : shift ? DRONE_VCO_FINE : DRONE_VCO_PITCH;
    return n < DRONE_BLOCK_NPARAM && drone_block_spec[type][n].top ? (int32_t)n : -1;
}

/* the parameter an encoder holds on a patch's page, where the cursor is; 0: none */
static const param_spec_t *panel_patch_spec(uint32_t at, uint32_t type, uint32_t encoder, int shift)
{
    static const param_spec_t *const out[PANEL_OUT_NVALUE] = { &drone_param_spec[DRONE_LEVEL], &drone_param_spec[DRONE_FADE], &drone_send_spec };
    int32_t value = panel_patch_value(at, type, encoder, shift);
    if (encoder == PANEL_ENC_KIND)
        return at == PANEL_AT_BLOCK && type < DRONE_NBLOCK && drone_block_kind_spec[type].top ? &drone_block_kind_spec[type] : 0;
    return value < 0 ? 0 : at == PANEL_AT_OUT ? out[value] : &drone_block_spec[type][value];
}

/* which of an LFO's parameters an encoder holds on a patch's LFO page, or -1: its wave and its speed only, as its
 * cables are its depth and what it moves */
static int32_t panel_patch_lfo_param(uint32_t encoder)
{
    return encoder == PANEL_ENC_KIND ? DRONE_LFO_WAVE : encoder == PANEL_ENC_VALUES ? DRONE_LFO_SPEED : -1;
}

/* the cable an encoder holds while the LFO key is down: on a block the value's number, on OUT DRONE_OUT_LEVEL or
 * DRONE_OUT_SEND; -1: the LFOs do not move what the encoder holds */
static int32_t panel_patch_cable(uint32_t at, uint32_t type, uint32_t encoder)
{
    int32_t value = panel_patch_value(at, type, encoder, 0);
    if (value < 0)
        return -1;
    if (at == PANEL_AT_OUT)
        return value == PANEL_OUT_LEVEL ? DRONE_OUT_LEVEL : value == PANEL_OUT_SEND ? DRONE_OUT_SEND : -1;
    return drone_block_reach[type][value] ? value : -1;
}

/* a cable's amount after the encoder's clicks: it stops at -100 and 100 */
static int32_t panel_amount_turned(int32_t amount, int32_t clicks, int fine)
{
    clicks = clicks > 2 * DRONE_AMOUNT_TOP ? 2 * DRONE_AMOUNT_TOP : clicks < -2 * DRONE_AMOUNT_TOP ? -2 * DRONE_AMOUNT_TOP : clicks;
    amount += clicks * (fine ? 1 : PANEL_AMOUNT_STEP);
    return amount > DRONE_AMOUNT_TOP ? DRONE_AMOUNT_TOP : amount < -DRONE_AMOUNT_TOP ? -DRONE_AMOUNT_TOP : amount;
}

/* a value's parameter with shift held: the smallest steps */
static param_spec_t panel_fine(const param_spec_t *spec)
{
    param_spec_t fine = *spec;
    fine.per_click = 1;
    return fine;
}
