/* The drone engine: one voice for each of the 27 keys, latched. A voice is three saw oscillators, apart by the
 * detune, with noise mixed in, through a low-pass filter that a slow wave moves. All the voices then go through
 * the glitch (each value held for a number of samples) and the master level.
 *
 * Whole numbers only, and no product above 32 bits: felucca-bsp's docs/CPU.md reports wrong results from 64-bit
 * multiplies on this processor. It has no hardware code in it, so tests/drone_test.c runs it on a computer.
 *
 *   drone_init(&engine);
 *   drone_render(&engine, &controls, out, frames);    each audio block; out: left, right, 24-bit in int32_t
 */
#pragma once
#include <stdint.h>

#define DRONE_NOTES 27u                 /* note 0 is F3, as felucca-bsp numbers the keys */
#define DRONE_BLOCK_FRAMES 256u         /* the most frames a call renders */
#define DRONE_MASTER_MAX 256u
enum { DRONE_DETUNE, DRONE_CUTOFF, DRONE_LFO_RATE, DRONE_LFO_DEPTH, DRONE_NOISE, DRONE_CRUSH, DRONE_FADE, DRONE_NPARAM };

typedef struct {
    uint32_t notes_on;                  /* bit n: the voice of note n is latched on */
    uint8_t param[DRONE_NPARAM];        /* each 0..127 */
    int8_t octave;                      /* -3..2: all the voices, in octaves */
    uint16_t master;                    /* 0..DRONE_MASTER_MAX */
} drone_controls_t;

typedef struct {
    uint32_t phase[3];
    int32_t filter_q8;                  /* the filter's output, 256 times a 16-bit sample */
    uint32_t level_q16;                 /* the fade: 0 .. DRONE_LEVEL_TOP */
} drone_voice_t;

typedef struct {
    drone_voice_t voice[DRONE_NOTES];
    uint32_t lfo_phase, noise_state;
    uint32_t hold_left;                 /* the glitch: samples the held value still stays */
    int32_t held;
    int32_t mix[DRONE_BLOCK_FRAMES];
    int16_t noise[DRONE_BLOCK_FRAMES];
} drone_t;

#define DRONE_LEVEL_TOP 0x7FFF0000u

/* a note's phase step each sample, of 2^32 a cycle, at the FM-1's sample rate 24000000 / 544 Hz */
static uint32_t drone_note_step(uint32_t note, int32_t octave)
{
    static const uint32_t f3_to_e4[12] = {      /* 174.614 Hz * 2^(n / 12) * 2^32 * 544 / 24000000 */
        16999137u, 18009958u, 19080886u, 20215494u, 21417570u, 22691125u,
        24040410u, 25469927u, 26984448u, 28589026u, 30289018u, 32090097u,
    };
    uint32_t step = f3_to_e4[note % 12u] << (note / 12u);
    return octave < 0 ? step >> -octave : step << octave;
}

static void drone_init(drone_t *d)
{
    uint32_t i, j;
    for (i = 0; i < DRONE_NOTES; i++) {
        for (j = 0; j < 3u; j++)
            d->voice[i].phase[j] = 0;
        d->voice[i].filter_q8 = 0;
        d->voice[i].level_q16 = 0;
    }
    d->lfo_phase = 0;
    d->noise_state = 0x1234ABCDu;
    d->hold_left = 0;
    d->held = 0;
}

static uint32_t drone_voices_sounding(const drone_t *d)    /* after the last render: on, or still fading out */
{
    uint32_t i, n = 0;
    for (i = 0; i < DRONE_NOTES; i++)
        n += d->voice[i].level_q16 != 0;
    return n;
}

static void drone_render(drone_t *d, const drone_controls_t *c, int32_t *out, uint32_t frames)
{
    uint32_t i, n;
    int32_t detune = c->param[DRONE_DETUNE], noise_part = c->param[DRONE_NOISE], tone_part = 128 - noise_part;
    int32_t cutoff = (c->param[DRONE_CUTOFF] + 1) * (c->param[DRONE_CUTOFF] + 1);     /* 1..16384: of 16384 a sample */
    uint32_t fade_step = 8000u + (127u - c->param[DRONE_FADE]) * (127u - c->param[DRONE_FADE])
                                 * (127u - c->param[DRONE_FADE]) * 5u;                /* 6 s .. 5 ms for the whole fade */
    uint32_t hold = 1u + c->param[DRONE_CRUSH] * c->param[DRONE_CRUSH] * 63u / (127u * 127u);   /* 1..64 samples */
    int32_t master = c->master > DRONE_MASTER_MAX ? (int32_t)DRONE_MASTER_MAX : (int32_t)c->master;
    int32_t lfo;

    /* the slow wave, once a block: a triangle 0..32767 that closes the filter by up to the depth; 0.05 .. 20 Hz */
    d->lfo_phase += (4868u + c->param[DRONE_LFO_RATE] * c->param[DRONE_LFO_RATE] * 121u) * frames;
    lfo = (int32_t)((d->lfo_phase & 0x80000000u ? ~d->lfo_phase : d->lfo_phase) >> 16);
    cutoff -= (cutoff * ((c->param[DRONE_LFO_DEPTH] * lfo) >> 7)) >> 15;
    if (cutoff < 1)
        cutoff = 1;

    for (i = 0; i < frames; i++) {              /* one noise for all the voices */
        d->noise_state = d->noise_state * 1664525u + 1013904223u;
        d->noise[i] = (int16_t)(d->noise_state >> 16);
        d->mix[i] = 0;
    }

    for (n = 0; n < DRONE_NOTES; n++) {
        drone_voice_t *v = &d->voice[n];
        uint32_t on = c->notes_on >> n & 1u, step, step_up, step_down;
        if (!on && !v->level_q16)
            continue;                           /* silent: it costs nothing */
        step = drone_note_step(n, c->octave);
        step_up = step + (step >> 13) * (uint32_t)detune;       /* up to 1.5 % apart */
        step_down = step - (step >> 13) * (uint32_t)detune;
        for (i = 0; i < frames; i++) {
            int32_t tone, in;
            v->phase[0] += step;
            v->phase[1] += step_up;
            v->phase[2] += step_down;
            tone = ((int32_t)v->phase[0] >> 16) + ((int32_t)v->phase[1] >> 16) + ((int32_t)v->phase[2] >> 16);
            in = ((tone >> 2) * tone_part + d->noise[i] * noise_part) >> 7;
            v->filter_q8 += ((in - (v->filter_q8 >> 8)) * cutoff) >> 6;
            if (on)
                v->level_q16 = DRONE_LEVEL_TOP - v->level_q16 > fade_step ? v->level_q16 + fade_step : DRONE_LEVEL_TOP;
            else
                v->level_q16 = v->level_q16 > fade_step ? v->level_q16 - fade_step : 0;
            d->mix[i] += ((v->filter_q8 >> 8) * (int32_t)(v->level_q16 >> 16)) >> 15;
        }
    }

    for (i = 0; i < frames; i++) {
        int32_t sample;
        if (!d->hold_left) {                    /* the glitch: a new value only every `hold` samples */
            d->hold_left = hold;
            sample = d->mix[i] >> 2;            /* four voices at their loudest fill the range; more are clipped */
            d->held = sample > 32767 ? 32767 : sample < -32768 ? -32768 : sample;
        }
        d->hold_left--;
        sample = d->held * master;              /* 16 bits times 0..256: the 24-bit range */
        out[2u * i] = sample > 8388607 ? 8388607 : sample;
        out[2u * i + 1u] = out[2u * i];
    }
}
