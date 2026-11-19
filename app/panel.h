/* The panel's pages. The LFO key goes from the voice's page through its LFOs' pages, then the feedback's, and back.
 * The GLO key opens the end of the chain (EOC), what is after the voices: the delay, the reverb and the compressor,
 * each a page. SELECT goes from one to the next, the four right encoders are the open one's values, and the GLO key
 * again goes back to the patch.
 *
 * A voice is a patch (drone.h) and has its chain on its page: the SELECT encoder moves along the places of the
 * chain (each block, then the empty place for a new one while there is room, then OUT). The ALGORITHM encoder is
 * the block's kind, and the four right ones its values. OCT- is the shift key. While it is held:
 *   SELECT chooses what block the place gets (none: the block is taken away), applied when OCT- is let go
 *   PRESETS moves the block along the chain
 *   a value's encoder moves it in the smallest steps; an oscillator's first one is its fine tuning
 * On a patch's LFO pages SELECT still moves along the chain, ALGORITHM is the LFO's wave, the first right encoder
 * its speed and the second its depth. A patch also has the page of its feedback cable: the four right encoders are
 * the block it comes from, the block it goes back in before, its level and its time.
 * While the LFO key is held, an encoder is a cable from the LFO (the open page's, or the one whose page was open
 * last): its amount, -100 .. 100. On the chain's page: to the value the encoder holds. On an LFO's page: the four
 * right encoders are the cables to the speed and the depth of the two other LFOs. On the feedback's: to its level. */
#pragma once
#include <stdint.h>
#include "drone.h"
#include "style.h"

/* a patch's pages (between the first two: that LFO of the voice), then the end of the chain's, then the page the
 * instrument is played on (from V0.20 the patch's pages have no key: they wait for the EDIT key) */
enum { PANEL_VOICE, PANEL_FEEDBACK = 1 + DRONE_LFOS, PANEL_NPATCH, PANEL_DELAY = PANEL_NPATCH, PANEL_REVERB, PANEL_COMP, PANEL_PLAY, PANEL_NPAGE };
static const char *const panel_page_name[PANEL_NPAGE] = { "VOICE", "LFO 1", "LFO 2", "LFO 3", "FEEDBACK", "DELAY", "REVERB", "COMP", "PLAY" };

static int32_t panel_page_lfo(uint32_t page)    /* the LFO a page is of, or -1 */
{
    return page > PANEL_VOICE && page < PANEL_FEEDBACK ? (int32_t)page - 1 : -1;
}
static const char *const panel_filter_name[DRONE_NFILTER] = {
    [DRONE_LADDER] = "LADDER", [DRONE_MS20] = "MS-20", [DRONE_POLIVOKS] = "POLIVOKS", [DRONE_WASP] = "WASP",
};

typedef struct {
    uint8_t page;
    uint8_t choosing;               /* the choice of a block is going on */
    uint8_t choice;                 /* the block chosen so far */
    uint8_t patch_page;             /* in the end of the chain: the patch's page the GLO key goes back to */
    uint8_t eoc_page;               /* the end of the chain's page that was open last; 0: none yet, the delay */
} panel_t;

static int panel_page_eoc(uint32_t page)        /* a page of the end of the chain */
{
    return page >= PANEL_DELAY && page <= PANEL_COMP;
}
static int panel_in_eoc(const panel_t *panel)
{
    return panel_page_eoc(panel->page);
}
static void panel_next_page(panel_t *panel)     /* the LFO key: a patch's pages */
{
    if (panel->page < PANEL_NPATCH)
        panel->page = (uint8_t)((panel->page + 1u) % PANEL_NPATCH);
}
static void panel_eoc_key(panel_t *panel)       /* the GLO key: into the end of the chain, or back out */
{
    if (panel_in_eoc(panel)) {
        panel->eoc_page = panel->page;
        panel->page = panel->patch_page;
    } else {
        panel->patch_page = panel->page;
        panel->page = panel_page_eoc(panel->eoc_page) ? panel->eoc_page : PANEL_DELAY;
    }
}
static void panel_eoc_select(panel_t *panel, int32_t clicks)    /* SELECT there: the delay, the reverb, the compressor */
{
    if (panel_in_eoc(panel))
        panel->page = (uint8_t)(PANEL_DELAY + param_turned(&(param_spec_t){ PANEL_COMP - PANEL_DELAY, 1, 0 }, panel->page - PANEL_DELAY, clicks));
}

/* call every turn of the main loop with `held` (the choice goes on: the shift key is down) and the SELECT encoder's
 * clicks; gives what is chosen (`spec` says how many there are): `current` as it is, or the choice in the turn
 * `held` ends */
static uint32_t panel_choice(panel_t *panel, int held, int32_t clicks, const param_spec_t *spec, uint32_t current)
{
    if (held && !panel->choosing)
        panel->choice = (uint8_t)current;
    if (held)
        panel->choice = (uint8_t)param_turned(spec, panel->choice, clicks);
    else if (panel->choosing)
        current = panel->choice;
    panel->choosing = held != 0;
    return current;
}

/* ---- a patch ---- */
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

/* the parameter an encoder holds on a page of the end of the chain, or 0: none. The four right encoders, in the
 * order of the engine's lists; the reverb's first one is the selected voice's send, then the reverb's own three */
_Static_assert(DRONE_DELAY_NPARAM == 4 && DRONE_REVERB_NPARAM == 3 && DRONE_COMP_NPARAM == 4, "an effect's page fits the four right encoders");
static const param_spec_t *panel_eoc_spec(uint32_t page, uint32_t encoder)
{
    uint32_t n = encoder - PANEL_ENC_VALUES;
    if (encoder < PANEL_ENC_VALUES || n >= 4u)
        return 0;
    return page == PANEL_DELAY ? &drone_delay_spec[n] : page == PANEL_COMP ? &drone_comp_spec[n]
         : page != PANEL_REVERB ? 0 : n ? &drone_reverb_spec[n - 1u] : &drone_send_spec;
}

/* which of an LFO's parameters an encoder holds on a patch's LFO page, or -1: its wave, its speed and its depth,
 * as its cables say what it moves */
static int32_t panel_patch_lfo_param(uint32_t encoder)
{
    return encoder == PANEL_ENC_KIND ? DRONE_LFO_WAVE : encoder == PANEL_ENC_VALUES ? DRONE_LFO_SPEED
         : encoder == PANEL_ENC_VALUES + 1u ? DRONE_LFO_DEPTH : -1;
}

/* the LFO key held on LFO `from`'s page: the right encoders are its cables to the two other LFOs, in their order:
 * the first one's speed and depth, then the second one's. Gives 0 when the encoder has none; else the LFO the
 * cable goes to, and DRONE_LFO_TO_SPEED or DRONE_LFO_TO_DEPTH */
static int panel_lfo_cable(uint32_t encoder, uint32_t from, uint32_t *to, uint32_t *what)
{
    uint32_t n = encoder - PANEL_ENC_VALUES;
    if (encoder < PANEL_ENC_VALUES || n >= 2u * DRONE_LFO_NCABLED)
        return 0;
    *to = n / DRONE_LFO_NCABLED < from ? n / DRONE_LFO_NCABLED : n / DRONE_LFO_NCABLED + 1u;
    *what = n % DRONE_LFO_NCABLED;
    return 1;
}

/* which of the feedback cable's parameters an encoder holds on its page, or -1 */
static int32_t panel_fb_param(uint32_t encoder)
{
    return encoder >= PANEL_ENC_VALUES && encoder < PANEL_ENC_VALUES + DRONE_FB_NPARAM ? (int32_t)(encoder - PANEL_ENC_VALUES) : -1;
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

/* ---- the play page ----
 * SELECT is the selected voice's frequency, ALGORITHM its style, KNOB1 to KNOB3 its TONE, MOVE and GRIT; with shift,
 * KNOB2 to KNOB4 are its SPEED, SPACE and LEVEL. */
#define PANEL_FREQ_STEP 50              /* a click of SELECT: 0.4 semitone, so the clicks do not fall on notes; with shift: 2 */
#define PANEL_FREQ_FINE 2
static const param_spec_t panel_style_spec = { STYLE_NSTYLE - 1, 1, STYLE_FLOOR };

/* the frequency after SELECT's clicks and the octave button (1: twice the frequency, -1: half). It stops at the
 * ends; an octave that does not fit is not taken */
static uint32_t panel_freq_turned(uint32_t freq, int32_t clicks, int fine, int32_t octaves)
{
    int32_t top = (int32_t)PLAY_FREQ_TOP, to;
    clicks = clicks > top ? top : clicks < -top ? -top : clicks;
    to = (int32_t)freq + clicks * (fine ? PANEL_FREQ_FINE : PANEL_FREQ_STEP);
    to = to < 0 ? 0 : to > top ? top : to;
    return (uint32_t)(to + 12 * 128 * octaves >= 0 && to + 12 * 128 * octaves <= top ? to + 12 * 128 * octaves : to);
}

/* the knob (PLAY_...) one of the four right encoders holds on the play page, or -1: none */
static int32_t panel_play_knob(uint32_t encoder, int shift)
{
    static const int8_t knob[2][4] = { { PLAY_TONE, PLAY_MOVE, PLAY_GRIT, -1 }, { -1, PLAY_SPEED, PLAY_SPACE, PLAY_LEVEL } };
    return encoder < PANEL_ENC_VALUES || encoder >= PANEL_ENC_VALUES + 4u ? -1 : knob[shift != 0][encoder - PANEL_ENC_VALUES];
}

/* a voice's key on the play page: a press selects the voice; a press on the selected voice turns it on, or off.
 * Gives the voice's `on` after the press */
static uint32_t panel_voice_key(uint32_t *selected, uint32_t voice, uint32_t on)
{
    if (*selected == voice)
        return !on;
    *selected = voice;
    return on;
}
