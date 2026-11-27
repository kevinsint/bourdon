/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kevin Sint */
/* A style: a patch (drone.h) made in advance, for a voice that is played and not built. The one who plays chooses
 * the style with a knob, STYLE, and has a free frequency and a few knobs, the same on every style:
 *   TONE   dark to bright        GRIT   clean to rough        SPEED  the speed of the style's LFOs
 *   LINK   how much the voice takes from the voice before it (the engine's link, of the kind the voice has)
 * The voice's level, pan and sends are the mixer's (mixer.h): a style leaves them at 0.
 * A style says what TONE and GRIT move in its patch (its routes). A knob at 64 leaves the patch as the style has it.
 * The bank's LFOs (drone.h) move a voice through cables of its own, onto each value that a knob of the play page
 * holds: its FREQ (an octave each way), TONE, GRIT and SPEED (as if the knob went its whole range each way), LINK,
 * GAIN (its level, the mixer's) and OWN. They move its STYLE too (the whole list each way),
 * but not in the engine: a style is a whole patch, so the main loop gives the engine the patch of the style the
 * voice has now (style_pushed, style_now).
 * style_patch() makes the engine's controls for a voice from its style, its frequency and its knobs. */
#pragma once
#include <stdint.h>
#include "drone.h"

enum { STYLE_FLOOR, STYLE_REED, STYLE_GLASS, STYLE_GROWL, STYLE_TIDE, STYLE_PULSE, STYLE_NSTYLE };
enum { PLAY_TONE, PLAY_STYLE, PLAY_GRIT, PLAY_LINK, PLAY_SPEED, PLAY_OWN, PLAY_NKNOB };
#define PLAY_KNOB_MIDDLE 64
static const param_spec_t play_knob_spec[PLAY_NKNOB] = {
    [PLAY_TONE] = { 127, 4, PLAY_KNOB_MIDDLE }, [PLAY_GRIT] = { 127, 4, PLAY_KNOB_MIDDLE },
    [PLAY_STYLE] = { STYLE_NSTYLE - 1, 1, STYLE_FLOOR },        /* a choice: a style a click */
    [PLAY_LINK] = { 127, 4, 0 }, [PLAY_SPEED] = { 127, 4, PLAY_KNOB_MIDDLE },
    [PLAY_OWN] = { 127, 4, 127 },       /* a THROUGH voice: how much of its own oscillators is left; 0: only the other voice's sound */
};
/* the frequency of a voice's first oscillator, in 1/128 of a semitone from the engine's pitch 0 (8.2 Hz): no notes,
 * any value. Its other oscillators stay as far from the first one as the style has them */
#define PLAY_FREQ_TOP (127u * 128u)
#define PLAY_SPEED_REACH 24             /* SPEED at an end: the LFOs four times slower or faster */
enum { PLAY_BANKED_FREQ, PLAY_BANKED_TONE, PLAY_BANKED_STYLE, PLAY_BANKED_GRIT, PLAY_BANKED_LINK, PLAY_BANKED_SPEED, PLAY_BANKED_GAIN, PLAY_BANKED_OWN,
       PLAY_NBANKED };  /* what the bank moves */

typedef struct {
    uint8_t link_kind;                  /* DRONE_LINK_...: what its LINK does */
    int8_t style_push;                  /* what the bank's LFOs add to its STYLE knob now, in styles (style_pushed) */
    uint16_t freq;
    uint8_t knob[PLAY_NKNOB];           /* PLAY_STYLE: the style it is set on, STYLE_... */
    uint8_t dis[DRONE_DISRUPTORS];      /* how much of each disruptor it takes */
    uint8_t dis_to[DRONE_DISRUPTORS];   /* DRONE_TO_...: where it takes it */
    int8_t bank_amount[PLAY_NBANKED][DRONE_BANK_LFOS];      /* the bank's cables onto it, -100 .. 100; 0: none */
} play_voice_t;

#define STYLE_ROUTES 3u
#define STYLE_TO_FB DRONE_SLOTS         /* a route's block: not a block, the feedback cable's level */
typedef struct {                        /* a knob to a value: at an end of the knob the value is `reach` from the style's own */
    uint8_t knob, block, param;
    int8_t reach;                       /* 0: no route */
} style_route_t;
typedef struct {
    const char *name;
    drone_voice_controls_t patch;       /* the first block is an oscillator; no FX block: the one reverb stays at the end of the chain */
    style_route_t route[STYLE_ROUTES];
} style_t;
_Static_assert(STYLE_ROUTES == DRONE_BANK_ROUTES && STYLE_TO_FB == DRONE_SLOTS, "each route of a style is a route of the bank's cables");

#define STYLE_LFO(speed, wave, depth) { [DRONE_LFO_SPEED] = (speed), [DRONE_LFO_WAVE] = (wave), [DRONE_LFO_DEPTH] = (depth) }
/* a block: its type, its kind, its values, then its cables from the LFOs if it has any */
#define STYLE_BLOCK(block_type, block_kind, ...) { .type = (block_type), .kind = (block_kind), .param = __VA_ARGS__ }
#define STYLE_FADE { [DRONE_FADE] = 96 }
static const style_t style_list[STYLE_NSTYLE] = {
    [STYLE_FLOOR] = { "FLOOR", {                /* two saws a little apart, slow: a deep, wide drone */
        .block = { STYLE_BLOCK(DRONE_BLOCK_VCO, DRONE_SAW, { 42, 64 } ),
                   STYLE_BLOCK(DRONE_BLOCK_VCO, DRONE_SAW, { 42, 72 }, { [DRONE_VCO_PITCH] = { 0, 4 } } ),
                   STYLE_BLOCK(DRONE_BLOCK_VCF, DRONE_LADDER, { 70, 20 }, { [DRONE_VCF_CUTOFF] = { 30 } } ),
                   STYLE_BLOCK(DRONE_BLOCK_VCA, 0, { 80 } ) },
        .lfo = { STYLE_LFO(30, DRONE_LFO_SINE, 64), STYLE_LFO(22, DRONE_LFO_TRIANGLE, 64) },
        .param = STYLE_FADE,
    }, { { PLAY_TONE, 2, DRONE_VCF_CUTOFF, 40 }, { PLAY_GRIT, 2, DRONE_VCF_RESONANCE, 60 } } },
    [STYLE_REED] = { "REED", {                  /* a square near the MS-20's resonance, with vibrato: nasal, it sings */
        .block = { STYLE_BLOCK(DRONE_BLOCK_VCO, DRONE_SQUARE, { 42, 64 }, { [DRONE_VCO_PITCH] = { 3 } } ),
                   STYLE_BLOCK(DRONE_BLOCK_VCF, DRONE_MS20, { 84, 80 }, { [DRONE_VCF_CUTOFF] = { 0, 15 } } ),
                   STYLE_BLOCK(DRONE_BLOCK_VCA, 0, { 110 } ) },
        .lfo = { STYLE_LFO(100, DRONE_LFO_SINE, 64), STYLE_LFO(36, DRONE_LFO_TRIANGLE, 64) },
        .param = STYLE_FADE,
    }, { { PLAY_TONE, 1, DRONE_VCF_CUTOFF, 36 }, { PLAY_GRIT, 1, DRONE_VCF_RESONANCE, 47 } } },
    [STYLE_GLASS] = { "GLASS", {                /* a triangle with a short cable round the Wasp: thin, it rings as metal */
        .block = { STYLE_BLOCK(DRONE_BLOCK_VCO, DRONE_TRIANGLE, { 42, 64 } ),
                   STYLE_BLOCK(DRONE_BLOCK_VCF, DRONE_WASP, { 100, 90 } ),
                   STYLE_BLOCK(DRONE_BLOCK_VCA, 0, { 110 } ) },
        .lfo = { STYLE_LFO(50, DRONE_LFO_SINE, 64) },
        .fb = { [DRONE_FB_FROM] = 1, [DRONE_FB_TO] = 1, [DRONE_FB_LEVEL] = 50, [DRONE_FB_TIME] = 16 },
        .fb_amount = { 20 },
        .param = STYLE_FADE,
    }, { { PLAY_TONE, 1, DRONE_VCF_CUTOFF, 27 }, { PLAY_GRIT, STYLE_TO_FB, 0, 50 }, { PLAY_GRIT, 1, DRONE_VCF_RESONANCE, 20 } } },
    [STYLE_GROWL] = { "GROWL", {                /* a saw, the Polivoks and a loud cable, one LFO moves another: rough, not stable */
        .block = { STYLE_BLOCK(DRONE_BLOCK_VCO, DRONE_SAW, { 42, 64 } ),
                   STYLE_BLOCK(DRONE_BLOCK_VCF, DRONE_POLIVOKS, { 60, 100 }, { [DRONE_VCF_CUTOFF] = { 25 } } ),
                   STYLE_BLOCK(DRONE_BLOCK_VCA, 0, { 90 } ) },
        .lfo = { STYLE_LFO(70, DRONE_LFO_SAW, 64), STYLE_LFO(45, DRONE_LFO_TRIANGLE, 64), STYLE_LFO(80, DRONE_LFO_SQUARE, 64) },
        .lfo_amount = { [0] = { [DRONE_LFO_TO_SPEED] = { 0, 60 } } },
        .fb = { [DRONE_FB_FROM] = 1, [DRONE_FB_TO] = 0, [DRONE_FB_LEVEL] = 70, [DRONE_FB_TIME] = 120 },
        .fb_amount = { 0, 0, 10 },
        .param = STYLE_FADE,
    }, { { PLAY_TONE, 1, DRONE_VCF_CUTOFF, 40 }, { PLAY_GRIT, STYLE_TO_FB, 0, 57 }, { PLAY_GRIT, 1, DRONE_VCF_RESONANCE, 27 } } },
    [STYLE_TIDE] = { "TIDE", {                  /* a saw and a triangle an octave above, a level that a slow LFO moves: swells */
        .block = { STYLE_BLOCK(DRONE_BLOCK_VCO, DRONE_SAW, { 42, 64 } ),
                   STYLE_BLOCK(DRONE_BLOCK_VCO, DRONE_TRIANGLE, { 54, 70 } ),
                   STYLE_BLOCK(DRONE_BLOCK_VCF, DRONE_LADDER, { 80, 10 }, { [DRONE_VCF_CUTOFF] = { 0, 25 } } ),
                   STYLE_BLOCK(DRONE_BLOCK_VCA, 0, { 64 }, { [DRONE_VCA_LEVEL] = { 100 } } ) },
        .lfo = { STYLE_LFO(28, DRONE_LFO_SINE, 64), STYLE_LFO(20, DRONE_LFO_TRIANGLE, 64) },
        .out_amount = { [DRONE_OUT_SEND] = { 40 } },
        .param = STYLE_FADE,
    }, { { PLAY_TONE, 2, DRONE_VCF_CUTOFF, 40 }, { PLAY_GRIT, 2, DRONE_VCF_RESONANCE, 50 } } },
    [STYLE_PULSE] = { "PULSE", {                /* a square that a square LFO opens and closes: a rhythm */
        .block = { STYLE_BLOCK(DRONE_BLOCK_VCO, DRONE_SQUARE, { 42, 64 } ),
                   STYLE_BLOCK(DRONE_BLOCK_VCF, DRONE_LADDER, { 75, 60 }, { [DRONE_VCF_CUTOFF] = { 0, 30 } } ),
                   STYLE_BLOCK(DRONE_BLOCK_VCA, 0, { 64 }, { [DRONE_VCA_LEVEL] = { 100 } } ) },
        .lfo = { STYLE_LFO(96, DRONE_LFO_SQUARE, 64), STYLE_LFO(60, DRONE_LFO_SAW, 64) },
        .param = STYLE_FADE,
    }, { { PLAY_TONE, 1, DRONE_VCF_CUTOFF, 40 }, { PLAY_GRIT, 1, DRONE_VCF_RESONANCE, 60 } } },
};

static int32_t style_kept(int32_t value, int32_t top)
{
    return value < 0 ? 0 : value > top ? top : value;
}

/* an oscillator's place in the free scale: its pitch and its fine tuning as one number, 1/128 of a semitone */
static int32_t style_vco_freq(const drone_block_controls_t *vco)
{
    return vco->param[DRONE_VCO_PITCH] * 128 + vco->param[DRONE_VCO_FINE] - 64;
}

/* the frequency in tenths of a hertz: the engine's phase step is 2^32 * 544 / 24000000 a hertz */
static uint32_t style_freq_hz10(uint32_t freq)
{
    return (drone_step_q8((int32_t)freq * 2) + 4867u) / 9735u;
}

/* the style a voice has now: the one its STYLE knob is on, and what the bank's LFOs add; it stops at the list's ends */
static uint32_t style_now(const play_voice_t *play)
{
    return (uint32_t)style_kept((int32_t)play->knob[PLAY_STYLE] + play->style_push, STYLE_NSTYLE - 1);
}
/* what the bank's LFOs add to a voice's STYLE through their amounts, to the nearest style: at 100, an LFO at its top
 * is the whole list up, as a knob that an LFO moves goes its whole range. `out`: each LFO's output, -32767 .. 32767 */
static int32_t style_pushed(const play_voice_t *play, const int16_t out[DRONE_BANK_LFOS])
{
    int32_t bank_now[DRONE_BANK_LFOS];
    uint32_t l;
    for (l = 0; l < DRONE_BANK_LFOS; l++)
        bank_now[l] = out[l];
    return (drone_bank_cabled_q8(bank_now, play->bank_amount[PLAY_BANKED_STYLE], STYLE_NSTYLE - 1) + 128) >> 8;
}

/* the bank moves a knob's value no further than the knob's own ends. The room the knob has left down and up from
 * where it is, in the value's steps, for a value that is `reach` from the style's own at an end of the knob */
static void style_knob_room(drone_voice_controls_t *patch, uint32_t banked, int32_t reach, uint32_t knob_at)
{
    int32_t at = ((int32_t)(knob_at & 127u) - PLAY_KNOB_MIDDLE) * reach / PLAY_KNOB_MIDDLE;
    int32_t bottom = -reach, top = (127 - PLAY_KNOB_MIDDLE) * reach / PLAY_KNOB_MIDDLE;
    patch->bank_low[banked] = (int16_t)((bottom < top ? bottom : top) - at);
    patch->bank_high[banked] = (int16_t)((bottom < top ? top : bottom) - at);
}

/* the engine's controls for a voice: the patch of the style it has now, at its frequency, moved by its knobs. The
 * style's LFOs go as deep as the style has them. The patch is off */
static void style_patch(const play_voice_t *play, drone_voice_controls_t *patch)
{
    const style_t *style = &style_list[style_now(play)];
    int32_t first = style_vco_freq(&style->patch.block[0]);
    uint32_t s, l, r;
    *patch = style->patch;
    for (l = 0; l < DRONE_BANK_LFOS; l++) {
        patch->bank_amount[DRONE_BANKED_PITCH][l] = play->bank_amount[PLAY_BANKED_FREQ][l];
        patch->bank_amount[DRONE_BANKED_LINK][l] = play->bank_amount[PLAY_BANKED_LINK][l];
        patch->bank_amount[DRONE_BANKED_LFO_SPEED][l] = play->bank_amount[PLAY_BANKED_SPEED][l];
        patch->bank_amount[DRONE_BANKED_LEVEL][l] = play->bank_amount[PLAY_BANKED_GAIN][l];
        patch->bank_amount[DRONE_BANKED_OWN][l] = play->bank_amount[PLAY_BANKED_OWN][l];
    }
    for (s = 0; s < DRONE_SLOTS; s++)
        if (patch->block[s].type == DRONE_BLOCK_VCO) {
            int32_t freq = style_kept((play->freq > PLAY_FREQ_TOP ? (int32_t)PLAY_FREQ_TOP : (int32_t)play->freq)
                                      + style_vco_freq(&patch->block[s]) - first + 64, 127 * 128 + 127);
            patch->block[s].param[DRONE_VCO_PITCH] = (uint8_t)(freq / 128);
            patch->block[s].param[DRONE_VCO_FINE] = (uint8_t)(freq % 128);
        }
    for (r = 0; r < STYLE_ROUTES; r++) {
        const style_route_t *route = &style->route[r];
        int32_t moved = ((int32_t)(play->knob[route->knob % PLAY_NKNOB] & 127) - PLAY_KNOB_MIDDLE) * route->reach / PLAY_KNOB_MIDDLE;
        uint8_t *value = route->block == STYLE_TO_FB ? &patch->fb[DRONE_FB_LEVEL]
                       : &patch->block[route->block % DRONE_SLOTS].param[route->param % DRONE_BLOCK_NPARAM];
        uint32_t knob = route->knob % PLAY_NKNOB;
        /* the bank: as the knob going its whole range (127 steps) each way, and no further than the knob's ends */
        const int8_t *banked = knob == PLAY_TONE ? play->bank_amount[PLAY_BANKED_TONE] : knob == PLAY_GRIT ? play->bank_amount[PLAY_BANKED_GRIT] : 0;
        *value = (uint8_t)style_kept(*value + moved, 127);
        patch->bank_route[r] = (drone_bank_route_t){ .block = route->block, .param = route->param,
                                                     .reach = (int16_t)(banked ? route->reach * 127 / PLAY_KNOB_MIDDLE : 0) };
        for (l = 0; l < DRONE_BANK_LFOS; l++)
            patch->bank_amount[DRONE_BANKED_ROUTE + r][l] = banked ? banked[l] : 0;
        style_knob_room(patch, DRONE_BANKED_ROUTE + r, route->reach, play->knob[knob]);
    }
    style_knob_room(patch, DRONE_BANKED_LFO_SPEED, PLAY_SPEED_REACH, play->knob[PLAY_SPEED]);
    for (l = 0; l < DRONE_LFOS; l++)
        patch->lfo[l][DRONE_LFO_SPEED] = (uint8_t)style_kept(patch->lfo[l][DRONE_LFO_SPEED] + ((int32_t)(play->knob[PLAY_SPEED] & 127) - PLAY_KNOB_MIDDLE)
                                                             * PLAY_SPEED_REACH / PLAY_KNOB_MIDDLE, 127);
    patch->link = play->knob[PLAY_LINK] & 127u;
    patch->link_kind = play->link_kind < DRONE_NLINK ? play->link_kind : DRONE_LINK_FOLLOW;
    patch->own_less = 127u - (play->knob[PLAY_OWN] & 127u);
    for (s = 0; s < DRONE_DISRUPTORS; s++) {
        patch->dis[s] = play->dis[s] & 127u;
        patch->dis_to[s] = play->dis_to[s] < DRONE_NTO ? play->dis_to[s] : DRONE_TO_SOUND;
    }
    patch->on = 0;
}

/* EDIT: a voice at random. Its TONE, STYLE, GRIT, LINK and SPEED each get any value of the knob, and the bank's
 * amounts onto each of the five are made again: none, one or two LFOs, each with any amount. What makes the voice's
 * pitch and loudness stays: its frequency, its link's kind, its OWN, its disruptors, and the amounts onto its FREQ,
 * GAIN and OWN. The bank's LFOs are not a voice's: they stay too. `seed`: the random numbers', kept between calls */
#define PLAY_RANDOM_LFOS 2u
static uint32_t play_random(uint32_t *seed, uint32_t below)     /* 0 .. below - 1; below: 65536 at most */
{
    *seed = *seed * 1664525u + 1013904223u;
    return ((*seed >> 16) * below) >> 16;
}
static void play_randomize(play_voice_t *play, uint32_t *seed)
{
    static const uint8_t random[][2] = { { PLAY_TONE, PLAY_BANKED_TONE }, { PLAY_STYLE, PLAY_BANKED_STYLE }, { PLAY_GRIT, PLAY_BANKED_GRIT },
                                         { PLAY_LINK, PLAY_BANKED_LINK }, { PLAY_SPEED, PLAY_BANKED_SPEED } };       /* a knob, and what the bank moves of it */
    uint32_t r, l;
    for (r = 0; r < sizeof random / sizeof random[0]; r++) {
        int8_t *amount = play->bank_amount[random[r][1]];
        play->knob[random[r][0]] = (uint8_t)play_random(seed, play_knob_spec[random[r][0]].top + 1u);
        for (l = 0; l < DRONE_BANK_LFOS; l++)
            amount[l] = 0;
        for (l = play_random(seed, PLAY_RANDOM_LFOS + 1u); l; l--)
            amount[play_random(seed, DRONE_BANK_LFOS)] = (int8_t)((int32_t)play_random(seed, 201u) - 100);
    }
}
