/* A style: a patch (drone.h) made in advance, for a voice that is played and not built. The one who plays chooses
 * the style and has a free frequency and a few knobs, the same on every style:
 *   TONE   dark to bright        MOVE   how deep the style's LFOs go        GRIT   clean to rough
 *   SPEED  the LFOs' speed       SPACE  the voice's send                    LEVEL  the voice's level
 *   LINK   how much the voice before it opens this one's filters (the engine's link)
 * A style says what TONE and GRIT move in its patch (its routes). A knob at 64 leaves the patch as the style has it.
 * style_patch() makes the engine's controls for a voice from its style, its frequency and its knobs. */
#pragma once
#include <stdint.h>
#include "drone.h"

enum { PLAY_TONE, PLAY_MOVE, PLAY_GRIT, PLAY_LINK, PLAY_SPEED, PLAY_SPACE, PLAY_LEVEL, PLAY_NKNOB };
#define PLAY_KNOB_MIDDLE 64
static const param_spec_t play_knob_spec[PLAY_NKNOB] = {
    [PLAY_TONE] = { 127, 4, PLAY_KNOB_MIDDLE }, [PLAY_MOVE] = { 127, 4, PLAY_KNOB_MIDDLE }, [PLAY_GRIT] = { 127, 4, PLAY_KNOB_MIDDLE },
    [PLAY_LINK] = { 127, 4, 0 }, [PLAY_SPEED] = { 127, 4, PLAY_KNOB_MIDDLE }, [PLAY_SPACE] = { 127, 4, 32 }, [PLAY_LEVEL] = { 127, 4, 32 },
};
/* the frequency of a voice's first oscillator, in 1/128 of a semitone from the engine's pitch 0 (8.2 Hz): no notes,
 * any value. Its other oscillators stay as far from the first one as the style has them */
#define PLAY_FREQ_TOP (127u * 128u)
#define PLAY_SPEED_REACH 24             /* SPEED at an end: the LFOs four times slower or faster */

typedef struct {
    uint8_t style;
    uint16_t freq;
    uint8_t knob[PLAY_NKNOB];
} play_voice_t;

enum { STYLE_FLOOR, STYLE_REED, STYLE_GLASS, STYLE_GROWL, STYLE_TIDE, STYLE_PULSE, STYLE_NSTYLE };
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

/* the engine's controls for a voice: its style's patch, at its frequency, moved by its knobs. The patch is off */
static void style_patch(const play_voice_t *play, drone_voice_controls_t *patch)
{
    const style_t *style = &style_list[play->style < STYLE_NSTYLE ? play->style : 0u];
    int32_t first = style_vco_freq(&style->patch.block[0]);
    uint32_t s, l, r;
    *patch = style->patch;
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
        *value = (uint8_t)style_kept(*value + moved, 127);
    }
    for (l = 0; l < DRONE_LFOS; l++) {
        uint8_t *lfo = patch->lfo[l];
        lfo[DRONE_LFO_DEPTH] = (uint8_t)style_kept(lfo[DRONE_LFO_DEPTH] * (play->knob[PLAY_MOVE] & 127) / PLAY_KNOB_MIDDLE, 127);
        lfo[DRONE_LFO_SPEED] = (uint8_t)style_kept(lfo[DRONE_LFO_SPEED] + ((int32_t)(play->knob[PLAY_SPEED] & 127) - PLAY_KNOB_MIDDLE)
                                                   * PLAY_SPEED_REACH / PLAY_KNOB_MIDDLE, 127);
    }
    patch->param[DRONE_LEVEL] = play->knob[PLAY_LEVEL] & 127u;
    patch->send = play->knob[PLAY_SPACE] & 127u;
    patch->link = play->knob[PLAY_LINK] & 127u;
    patch->on = 0;
}
