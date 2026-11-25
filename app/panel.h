/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kevin Sint */
/* The panel's pages. The LFO key, pressed and let go, opens the bank's page (its twelve LFOs, V0.32; three sets of
 * each one's values, V0.33) from any page, and from there goes back to the page that was open before. Held on the
 * bank's page, it is the amounts of one bank LFO onto the shown one's values and onto the tempo; held on the play
 * page, the amounts of the bank's LFOs onto the selected voice.
 * The GLO key opens the end of the chain (EOC), what is after the voices: the delay, the reverb and the compressor,
 * each a page. SELECT goes from one to the next, the four right encoders are the open one's values, and the GLO key
 * again goes back to the patch.
 * The FX key opens the disruptors (drone.h), each a page: the noise, the pulse (CHOP), the sine (RING), then the
 * play page again.
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
#include "mixer.h"

/* a patch's pages (between the first two: that LFO of the voice), then the end of the chain's, then the page the
 * instrument is played on, the mixer's, the disruptors' and the bank's (from V0.20 the patch's pages have no key:
 * they wait for the EDIT key) */
enum { PANEL_VOICE, PANEL_FEEDBACK = 1 + DRONE_LFOS, PANEL_NPATCH, PANEL_DELAY = PANEL_NPATCH, PANEL_REVERB, PANEL_COMP, PANEL_PLAY, PANEL_MIXER, PANEL_NOISE, PANEL_CHOP, PANEL_RING,
       PANEL_BANK, PANEL_NPAGE };
_Static_assert(PANEL_RING - PANEL_NOISE + 1 == DRONE_DISRUPTORS, "a page for each disruptor, in the engine's order");
static const char *const panel_page_name[PANEL_NPAGE] = { "VOICE", "LFO 1", "LFO 2", "LFO 3", "FEEDBACK", "DELAY", "REVERB", "COMP", "PLAY", "MIXER", "NOISE", "CHOP", "RING", "BANK" };

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
    uint8_t eoc_page;               /* the end of the chain's page that was open last; 0: none yet, the delay */
    uint8_t before_bank;            /* the page the bank's was opened from: the LFO key goes back to it */
    uint8_t lfo_used;               /* this press of the LFO key has turned a knob: its release opens no page */
    uint8_t bank_set;               /* the bank's page: the layer in hand, a set of values for the four right encoders (PANEL_BANK_WAVE ..) */
    uint8_t play_layer;             /* the play page's: 0 TONE, MOVE, GRIT, LINK; 1 FREQ, SPEED, OWN */
} panel_t;

static int panel_page_eoc(uint32_t page)        /* a page of the end of the chain */
{
    return page >= PANEL_DELAY && page <= PANEL_COMP;
}
static int panel_in_eoc(const panel_t *panel)
{
    return panel_page_eoc(panel->page);
}
/* call every turn of the main loop with the LFO key's edges and `used`: a knob turned in this
 * turn while it is down. Let go with neither since it was pressed, it opens the bank's page, or goes back from it */
static void panel_lfo_key(panel_t *panel, int pressed, int released, int used)
{
    if (pressed)
        panel->lfo_used = 0;
    panel->lfo_used |= used != 0;
    if (!released || panel->lfo_used)
        return;
    if (panel->page == PANEL_BANK) {
        panel->page = panel->before_bank;
    } else {
        panel->before_bank = panel->page;
        panel->page = PANEL_BANK;
    }
}
static void panel_glo_key(panel_t *panel)       /* the GLO key: the play page, the mixer, the end of the chain, and round */
{
    if (panel_in_eoc(panel)) {
        panel->eoc_page = panel->page;
        panel->page = PANEL_PLAY;
    } else if (panel->page == PANEL_MIXER) {
        panel->page = panel_page_eoc(panel->eoc_page) ? panel->eoc_page : PANEL_DELAY;
    } else {
        panel->page = PANEL_MIXER;
    }
}
static int32_t panel_page_dis(uint32_t page)    /* the disruptor a page is of, or -1 */
{
    return page >= PANEL_NOISE && page <= PANEL_RING ? (int32_t)(page - PANEL_NOISE) : -1;
}
static void panel_fx_key(panel_t *panel)        /* the FX key: the first disruptor from any other page, then the next, then the play page */
{
    panel->page = (uint8_t)(panel_page_dis(panel->page) < 0 ? PANEL_NOISE : panel->page == PANEL_RING ? PANEL_PLAY : panel->page + 1u);
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
#define PANEL_AMOUNT_STEP 5         /* a cable's amount's own click (panel_knob_steps) */
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
 * order of the engine's lists; the reverb's first one has none (it was a voice's send: the mixer's now), then the
 * reverb's own three */
_Static_assert(DRONE_DELAY_NPARAM == 4 && DRONE_REVERB_NPARAM == 3 && DRONE_COMP_NPARAM == 4, "an effect's page fits the four right encoders");
static const param_spec_t *panel_eoc_spec(uint32_t page, uint32_t encoder)
{
    uint32_t n = encoder - PANEL_ENC_VALUES;
    if (encoder < PANEL_ENC_VALUES || n >= 4u)
        return 0;
    return page == PANEL_DELAY ? &drone_delay_spec[n] : page == PANEL_COMP ? &drone_comp_spec[n]
         : page != PANEL_REVERB || !n ? 0 : &drone_reverb_spec[n - 1u];
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

/* a cable's amount after its knob's steps (panel_knob_steps), or with `snap` after its clicks to the next of -100,
 * -50, 0, 50, 100 that way: it stops at -100 and 100 */
static int32_t panel_amount_turned(int32_t amount, int32_t steps, int snap)
{
    steps = steps > 2 * DRONE_AMOUNT_TOP ? 2 * DRONE_AMOUNT_TOP : steps < -2 * DRONE_AMOUNT_TOP ? -2 * DRONE_AMOUNT_TOP : steps;
    amount = snap ? param_snapped_every(amount, steps, DRONE_AMOUNT_TOP / 2) : amount + steps;
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
 * SELECT is the selected voice's frequency (with OCT- held: by notes; with OCT+ held: by octaves), ALGORITHM its
 * style, KNOB1 to KNOB4 its TONE, MOVE, GRIT and LINK; with
 * shift, ALGORITHM is the kind of its link (drone_link_kind_spec), KNOB2 is its SPEED and, when its link is THROUGH,
 * KNOB4 is OWN: how much of its own oscillators is left. */
static const char *const panel_link_name[DRONE_NLINK] = { [DRONE_LINK_FOLLOW] = "FOLLOW", [DRONE_LINK_THROUGH] = "THROUGH", [DRONE_LINK_FM] = "FM" };
static const param_spec_t panel_style_spec = { STYLE_NSTYLE - 1, 1, STYLE_FLOOR };

/* how SELECT moves the frequency: alone, by steps that the speed of the turn sets; with OCT- held, by notes; with
 * OCT+ held, by octaves */
enum { PANEL_FREQ_FREE, PANEL_FREQ_NOTES, PANEL_FREQ_OCTAVES };

/* the frequency's steps (1/128 of a semitone) for SELECT's clicks, by the time each click took since SELECT's last
 * one (`turned_ms`, which this keeps). A very slow turn is the smallest step, to set a beating; the fastest is 2
 * semitones a click: SELECT has 23 clicks a turn, so a quarter of a turn (6 clicks) is an octave */
static int32_t panel_freq_steps(int32_t clicks, uint32_t now_ms, uint32_t *turned_ms)
{
    uint32_t each;
    if (!clicks)
        return 0;
    each = (now_ms - *turned_ms) / (uint32_t)(clicks < 0 ? -clicks : clicks);
    *turned_ms = now_ms;
    return clicks * (each >= 250u ? 1 : each >= 100u ? 2 : each >= 50u ? 8 : each >= 25u ? 32 : each >= 15u ? 128 : 256);
}

/* the frequency after SELECT's clicks. PANEL_FREQ_FREE: it moves by `steps` (panel_freq_steps). PANEL_FREQ_NOTES: the
 * first click goes to the nearest note that way, then a semitone a click. PANEL_FREQ_OCTAVES: an octave a click; an
 * octave that does not fit is not taken. It stops at the ends */
static uint32_t panel_freq_turned(uint32_t freq, int32_t clicks, int32_t steps, uint32_t how)
{
    int32_t top = (int32_t)PLAY_FREQ_TOP, to = (int32_t)freq, octave;
    clicks = clicks > 100 ? 100 : clicks < -100 ? -100 : clicks;
    steps = steps > 25600 ? 25600 : steps < -25600 ? -25600 : steps;
    if (how == PANEL_FREQ_OCTAVES) {
        for (octave = clicks > 0 ? 12 * 128 : -12 * 128; clicks && to + octave >= 0 && to + octave <= top; clicks -= clicks > 0 ? 1 : -1)
            to += octave;
        return (uint32_t)to;
    }
    if (how == PANEL_FREQ_NOTES)
        to = clicks > 0 ? (to / 128 + clicks) * 128 : clicks < 0 ? ((to + 127) / 128 + clicks) * 128 : to;
    else
        to += steps;
    return (uint32_t)(to < 0 ? 0 : to > top ? top : to);
}

/* ---- the layers ----
 * The rightmost white keys choose the layer in hand: what the four right encoders hold. The play page has two (keys
 * 13 and 14), the bank's page four (keys 13 to 16). The layers' keys are lit, and the one in hand's blinks */
#define PANEL_LAYER_KEYS 4u
static const uint8_t panel_layer_key[PANEL_LAYER_KEYS] = { 21, 23, 24, 26 };     /* white keys 13 to 16; the note keys count from 0, the left one */
static int32_t panel_layer_of_keys(uint32_t notes, uint32_t layers)    /* the layer of a pressed key (two: the left one), or -1 */
{
    uint32_t l;
    for (l = 0; l < layers && l < PANEL_LAYER_KEYS; l++)
        if (notes >> panel_layer_key[l] & 1u)
            return (int32_t)l;
    return -1;
}
/* their lights: the layers' keys are lit, and the one in hand's blinks, off every other 256 ms of `now_ms` */
static uint32_t panel_layer_lights(uint32_t layers, uint32_t in_hand, uint32_t now_ms)
{
    uint32_t l, lit = 0;
    in_hand = in_hand < layers ? in_hand : 0;
    for (l = 0; l < layers && l < PANEL_LAYER_KEYS; l++)
        lit |= (uint32_t)(l != in_hand || !(now_ms >> 8 & 1u)) << panel_layer_key[l];
    return lit;
}

/* what one of the four right encoders holds on the play page, in layer `layer` (0 or 1): a knob (PLAY_...), the
 * voice's frequency (PANEL_PLAY_FREQ: the same one SELECT holds), or -1: none. `through`: the voice's link is
 * THROUGH, the only kind OWN is for */
#define PANEL_PLAY_LAYERS 2u
#define PANEL_PLAY_FREQ PLAY_NKNOB
static int32_t panel_play_knob(uint32_t encoder, int layer, int through)
{
    static const int8_t knob[PANEL_PLAY_LAYERS][4] = { { PLAY_TONE, PLAY_MOVE, PLAY_GRIT, PLAY_LINK }, { PANEL_PLAY_FREQ, PLAY_SPEED, -1, PLAY_OWN } };
    int32_t held = encoder < PANEL_ENC_VALUES || encoder >= PANEL_ENC_VALUES + 4u ? -1 : knob[layer != 0][encoder - PANEL_ENC_VALUES];
    return held == PLAY_OWN && !through ? -1 : held;
}

/* ---- the mixer's page ----
 * SELECT chooses a channel: 0 none, then 1 .. for each voice. With none, KNOB1 to KNOB3 are the three voices'
 * levels. With a channel, the four right encoders are its level, its pan, its send 1 and its send 2, and OCT+ changes
 * its sends from PRE to POST and back. */
static const param_spec_t panel_mixer_channel_spec = { DRONE_VOICES, 1, 0 };

/* the value an encoder holds on the mixer's page: whose channel and which of its values; 0: the encoder has none */
static int panel_mixer_value(uint32_t channel, uint32_t encoder, uint32_t *voice, uint32_t *value)
{
    uint32_t n = encoder - PANEL_ENC_VALUES;
    if (encoder < PANEL_ENC_VALUES || n >= MIXER_NVALUE || channel > DRONE_VOICES || (!channel && n >= DRONE_VOICES))
        return 0;
    *voice = channel ? channel - 1u : n;
    *value = channel ? n : MIXER_LEVEL;
    return 1;
}

/* ---- the disruptors' pages ----
 * SELECT is still the selected voice's frequency. KNOB1 to KNOB3 are the disruptor's own values, the same for all the
 * voices; KNOB4 is how much of it the selected voice takes, and ALGORITHM where that voice takes it. */
static const char *const panel_dis_name[DRONE_DISRUPTORS] = { [DRONE_DIS_NOISE] = "NOISE", [DRONE_DIS_CHOP] = "CHOP", [DRONE_DIS_RING] = "RING" };
static const char *const panel_dis_knob_name[DRONE_DISRUPTORS][DRONE_DIS_NPARAM] = {
    [DRONE_DIS_NOISE] = { "RATE", "BITS", "DUST" }, [DRONE_DIS_CHOP] = { "RATE", "WIDTH", "SKIP" }, [DRONE_DIS_RING] = { "FREQ", "FOLD", "DRIFT" },
};
/* where a voice takes a disruptor, as the screen says it: the noise is added to its sound; the pulse and the sine
 * change how loud it is */
static const char *panel_to_name(uint32_t dis, uint32_t to)
{
    return to == DRONE_TO_PITCH ? "PITCH" : to == DRONE_TO_CUTOFF ? "CUTOFF" : dis == DRONE_DIS_NOISE ? "SOUND" : "LEVEL";
}
_Static_assert(DRONE_DIS_NPARAM == 3, "a disruptor's values and a voice's amount fit the four right encoders");
static const param_spec_t *panel_dis_spec(uint32_t dis, uint32_t encoder)       /* the parameter an encoder holds there, or 0: none */
{
    uint32_t n = encoder - PANEL_ENC_VALUES;
    return dis >= DRONE_DISRUPTORS ? 0 : encoder == PANEL_ENC_KIND ? &drone_to_spec : encoder < PANEL_ENC_VALUES || n > DRONE_DIS_NPARAM ? 0
         : n < DRONE_DIS_NPARAM ? &drone_dis_spec[dis][n] : &drone_dis_send_spec;
}

/* ---- a knob's turn ----
 * The one rule for a knob that holds a value: how far its clicks move the value, in the value's smallest steps, by
 * the time each click took since the knob's last one (`turned_ms`, which this keeps). Notch by notch (120 ms a click
 * or more), a click is 1. Faster, it is half of the value's own click (`per_click`, param_spec_t's), then all of it,
 * then 2 and 4 times that. The value then takes the steps with param_turned and a click of 1 (panel_fine).
 * With OCT- held a knob does not go by steps: it snaps (param_snapped) */
static int32_t panel_knob_steps(uint32_t per_click, int32_t clicks, uint32_t now_ms, uint32_t *turned_ms)
{
    uint32_t each;
    if (!clicks)
        return 0;
    each = (now_ms - *turned_ms) / (uint32_t)(clicks < 0 ? -clicks : clicks);
    *turned_ms = now_ms;
    return clicks * (int32_t)(each >= 120u ? 1u : each >= 60u ? (per_click + 1u) / 2u : each >= 30u ? per_click
                              : each >= 15u ? 2u * per_click : 4u * per_click);
}

/* ---- the bank's page (V0.33) ----
 * SELECT shows one of the bank's twelve LFOs (the keys do not choose an LFO: SELECT alone). White keys 13 to 16
 * (lit; the one in hand blinks) choose the layer: what the four right encoders hold, a set of the shown LFO's values:
 *   WAVE   KNOB1 RATE (synced: RATIO), KNOB2 WIDTH, KNOB3 LEVEL, KNOB4 OFFSET
 *   STEPS  KNOB1 PROB, KNOB2 STEPS, KNOB3 TRIGS, KNOB4 SHIFT
 *   LOOP   KNOB1 LOOP, KNOB2 NAP, KNOB3 WAKE, KNOB4 LSHIFT
 *   MORE   KNOB1 PHASE, KNOB2 SLEW, KNOB3 PAD, KNOB4 RAND
 * ALGORITHM is the SHAPE, and a new SHAPE sets the RANGE it fits (in MORE: the RANGE). OCT+ turns
 * its SYNC on or off; PRESETS is the clock's tempo. The LFO key held there: SELECT chooses a source
 * LFO, the four right encoders are its amounts onto the values they hold, of the shown LFO (MORE's have none),
 * and PRESETS its amount onto the tempo; the shown LFO's own values do not move. The LFO key held on the play page:
 * SELECT chooses the LFO (the shown one), and the right encoders and PRESETS are its amounts onto the
 * selected voice. */
#define PANEL_BANK_TEMPO DRONE_BANK_NPARAM      /* what the bank's page shows turned last: a DRONE_BANK_... value, or this */
enum { PANEL_BANK_WAVE, PANEL_BANK_STEPS, PANEL_BANK_LOOP, PANEL_BANK_MORE, PANEL_BANK_NSET };
_Static_assert(PANEL_BANK_NSET <= PANEL_LAYER_KEYS, "a key for each layer");
static const param_spec_t panel_bank_lfo_spec = { DRONE_BANK_LFOS - 1u, 1, 0 };
/* each set's four values, KNOB1 to KNOB4. The first three sets' are in the order of the bank's cables onto an LFO
 * (DRONE_BANK_TO_...): set s, knob n is cable 4 s + n. MORE's values have no cable */
static const uint8_t panel_bank_knob[PANEL_BANK_NSET][4] = {
    [PANEL_BANK_WAVE] = { DRONE_BANK_RATE, DRONE_BANK_WIDTH, DRONE_BANK_LEVEL, DRONE_BANK_OFFSET },
    [PANEL_BANK_STEPS] = { DRONE_BANK_PROB, DRONE_BANK_STEPS, DRONE_BANK_TRIGS, DRONE_BANK_SHIFT },
    [PANEL_BANK_LOOP] = { DRONE_BANK_LOOP, DRONE_BANK_NAP, DRONE_BANK_WAKE, DRONE_BANK_LSHIFT },
    [PANEL_BANK_MORE] = { DRONE_BANK_PHASE, DRONE_BANK_SLEW, DRONE_BANK_PAD, DRONE_BANK_RAND },
};
_Static_assert(DRONE_BANK_NCABLED == 4 * PANEL_BANK_MORE && DRONE_BANK_TO_PROB == 4 && DRONE_BANK_TO_LOOP == 8,
               "a cable onto each value the first three sets' knobs hold, in the sets' order");
/* the cable (DRONE_BANK_TO_...) onto the value KNOB n + 1 holds in a set, or DRONE_BANK_NCABLED: none */
static uint32_t panel_bank_cable(uint32_t set, uint32_t n)
{
    return set < PANEL_BANK_MORE && n < 4u ? 4u * set + n : DRONE_BANK_NCABLED;
}

static uint32_t panel_bank_select(uint32_t lfo, int32_t clicks)        /* SELECT: the LFO, 1 to 12 */
{
    return (uint32_t)param_turned(&panel_bank_lfo_spec, (int32_t)(lfo < DRONE_BANK_LFOS ? lfo : DRONE_BANK_LFOS - 1u), clicks);
}
/* the bank's page: SELECT's clicks choose the LFO shown, or with the LFO key held (`held`) the source of the
 * amounts; the other one stays */
static void panel_bank_choose(uint32_t *shown, uint32_t *source, int held, int32_t clicks)
{
    uint32_t *chosen = held ? source : shown;
    *chosen = panel_bank_select(*chosen, clicks);
}

/* what an encoder holds on the bank's page: one of the shown LFO's values (`which`: DRONE_BANK_...), the tempo, an
 * amount of the source LFO onto one of its values (`which`: DRONE_BANK_TO_...), or onto the tempo */
enum { PANEL_HOLDS_NOTHING, PANEL_HOLDS_VALUE, PANEL_HOLDS_TEMPO, PANEL_HOLDS_AMOUNT, PANEL_HOLDS_TEMPO_AMOUNT };
typedef struct { uint8_t kind, which; } panel_bank_holds_t;
/* `set`: the layer in hand; `held`: the LFO key is down; `sync`: the shown LFO is synced. SELECT holds none of these */
static panel_bank_holds_t panel_bank_holds(uint32_t set, uint32_t encoder, int held, int sync)
{
    panel_bank_holds_t holds = { PANEL_HOLDS_NOTHING, 0 };
    uint32_t n = encoder - PANEL_ENC_VALUES;
    set = set < PANEL_BANK_NSET ? set : PANEL_BANK_WAVE;
    if (encoder == PANEL_ENC_MOVE) {
        holds.kind = held ? PANEL_HOLDS_TEMPO_AMOUNT : PANEL_HOLDS_TEMPO;
    } else if (encoder == PANEL_ENC_KIND && !held) {
        holds.kind = PANEL_HOLDS_VALUE;
        holds.which = set == PANEL_BANK_MORE ? DRONE_BANK_RANGE : DRONE_BANK_SHAPE;
    } else if (encoder >= PANEL_ENC_VALUES && n < 4u && held) {
        if (panel_bank_cable(set, n) < DRONE_BANK_NCABLED) {    /* MORE's values have no cable: nothing */
            holds.kind = PANEL_HOLDS_AMOUNT;
            holds.which = (uint8_t)panel_bank_cable(set, n);
        }
    } else if (encoder >= PANEL_ENC_VALUES && n < 4u) {
        holds.kind = PANEL_HOLDS_VALUE;
        holds.which = panel_bank_knob[set][n];
        holds.which = holds.which == DRONE_BANK_RATE && sync ? DRONE_BANK_RATIO : holds.which;
    }
    return holds;
}
/* a bank LFO's value after its knob's steps (its smallest ones: panel_knob_steps, or the clicks as they are for a
 * value of one step a click), or with `snap` after its clicks to the next of its places (param_snapped). The PHASE
 * goes round the cycle, as a phase does. TRIGS and WAKE never go below 1: the engine takes 0 as 1 */
static uint32_t panel_bank_turned(uint32_t param, uint32_t value, int32_t steps, int snap)
{
    const param_spec_t *spec = &drone_bank_spec[param < DRONE_BANK_NPARAM ? param : DRONE_BANK_RATE];
    param_spec_t by_one = panel_fine(spec);
    uint32_t turned;
    if (param == DRONE_BANK_PHASE && !snap) {
        int32_t phase = ((int32_t)(value & 127u) + steps % 128) % 128;
        return (uint32_t)(phase < 0 ? phase + 128 : phase);
    }
    turned = (uint32_t)(snap ? param_snapped(spec, (int32_t)value, steps) : param_turned(&by_one, (int32_t)value, steps));
    return !turned && (param == DRONE_BANK_TRIGS || param == DRONE_BANK_WAKE) ? 1u : turned;
}
/* the same in an LFO's values (`knob`, DRONE_BANK_NPARAM of them); a SHAPE that changes sets the RANGE it fits */
static void panel_bank_turn(volatile uint8_t *knob, uint32_t param, int32_t steps, int snap)
{
    uint32_t was = knob[param];
    knob[param] = (uint8_t)panel_bank_turned(param, was, steps, snap);
    if (param == DRONE_BANK_SHAPE && knob[param] != was)
        knob[DRONE_BANK_RANGE] = (uint8_t)!drone_bank_both_ways[knob[param]];
}
/* PRESETS: the tempo, 10 .. 300 BPM, after its steps of 1 BPM, or with `snap` after its clicks to the next ten */
static uint32_t panel_tempo_turned(uint32_t tempo, int32_t steps, int snap)
{
    int32_t to = (int32_t)(tempo > DRONE_TEMPO_HIGH ? DRONE_TEMPO_HIGH : tempo);
    steps = steps > (int32_t)DRONE_TEMPO_HIGH ? (int32_t)DRONE_TEMPO_HIGH : steps < -(int32_t)DRONE_TEMPO_HIGH ? -(int32_t)DRONE_TEMPO_HIGH : steps;
    to = snap ? param_snapped_every(to, steps, 10) : to + steps;
    return to < (int32_t)DRONE_TEMPO_LOW ? DRONE_TEMPO_LOW : to > (int32_t)DRONE_TEMPO_HIGH ? DRONE_TEMPO_HIGH : (uint32_t)to;
}

/* an encoder's clicks on the bank's page, onto what it holds (`holds`, panel_bank_holds): LFO `lfo`'s value or the
 * tempo, or an amount of LFO `source` onto LFO `lfo`'s value or onto the tempo. The audio interrupt reads them while
 * this writes them: a value read half written is one value, a byte. `steps`: the knob's steps (panel_knob_steps),
 * or with `snap` its clicks */
static void panel_bank_apply(volatile drone_controls_t *c, uint32_t lfo, uint32_t source, panel_bank_holds_t holds, int32_t steps, int snap)
{
    volatile int8_t *amount = holds.kind == PANEL_HOLDS_AMOUNT ? &c->bank_amount[lfo % DRONE_BANK_LFOS][holds.which % DRONE_BANK_NCABLED][source % DRONE_BANK_LFOS]
                            : &c->tempo_amount[source % DRONE_BANK_LFOS];
    if (holds.kind == PANEL_HOLDS_VALUE)
        panel_bank_turn(c->bank[lfo % DRONE_BANK_LFOS], holds.which, steps, snap);
    else if (holds.kind == PANEL_HOLDS_TEMPO)
        c->tempo = (uint16_t)panel_tempo_turned(c->tempo, steps, snap);
    else if (holds.kind != PANEL_HOLDS_NOTHING)
        *amount = (int8_t)panel_amount_turned(*amount, steps, snap);
}

/* the LFO key held on the play page: what of the selected voice an encoder's amount is onto (PLAY_BANKED_...), or -1.
 * KNOB1 to KNOB4 as the knobs they hold in the layer in hand: TONE, MOVE, GRIT, LINK, or in the second layer the FREQ
 * on KNOB1 (its SPEED and OWN take no LFO); PRESETS the FREQ in both */
static int32_t panel_banked(uint32_t encoder, int layer)
{
    static const int8_t banked[PANEL_PLAY_LAYERS][4] = { { PLAY_BANKED_TONE, PLAY_BANKED_MOVE, PLAY_BANKED_GRIT, PLAY_BANKED_LINK },
                                                         { PLAY_BANKED_FREQ, -1, -1, -1 } };
    uint32_t n = encoder - PANEL_ENC_VALUES;
    return encoder == PANEL_ENC_MOVE ? PLAY_BANKED_FREQ : encoder >= PANEL_ENC_VALUES && n < 4u ? banked[layer != 0][n] : -1;
}
/* bit l: a voice has an amount of the bank's LFO l, onto anything */
static uint32_t panel_bank_used(const play_voice_t voice[DRONE_VOICES])
{
    uint32_t v, b, l, used = 0;
    for (v = 0; v < DRONE_VOICES; v++)
        for (b = 0; b < PLAY_NBANKED; b++)
            for (l = 0; l < DRONE_BANK_LFOS; l++)
                used |= (uint32_t)(voice[v].bank_amount[b][l] != 0) << l;
    return used;
}
