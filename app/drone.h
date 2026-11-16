/* The sound engine. For now it has one voice: an oscillator (saw, square or triangle) through a low-pass filter
 * with resonance, then its level and its fade. Each voice has its own seven parameters.
 *
 * Whole numbers only, and no product above 32 bits: felucca-bsp's docs/CPU.md reports wrong results from 64-bit
 * multiplies on this processor. It has no hardware code in it, so tests/drone_test.c runs it on a computer.
 *
 *   drone_init(&engine);
 *   drone_render(&engine, &controls, out, frames);    each audio block; out: left, right, 24-bit in int32_t
 */
#pragma once
#include <stdint.h>
#include "params.h"

#define DRONE_VOICES 1u
#define DRONE_BLOCK_FRAMES 256u         /* the most frames a call renders */
#define DRONE_MASTER_MAX 256u
enum { DRONE_PITCH, DRONE_WAVE, DRONE_FINE, DRONE_CUTOFF, DRONE_RESONANCE, DRONE_LEVEL, DRONE_FADE, DRONE_NPARAM };
enum { DRONE_SAW, DRONE_SQUARE, DRONE_TRIANGLE, DRONE_NWAVE };

static const param_spec_t drone_param_spec[DRONE_NPARAM] = {
    [DRONE_PITCH] = { 127, 1, 42 },             /* in semitones, as MIDI numbers them: 42 is F sharp 2, 92.5 Hz */
    [DRONE_WAVE] = { DRONE_NWAVE - 1, 1, DRONE_SAW },
    [DRONE_FINE] = { 127, 1, 64 },              /* half a semitone down .. up; 64 is in tune */
    [DRONE_CUTOFF] = { 127, 2, 90 },            /* the filter: 20 Hz .. 7 kHz, 15 steps an octave */
    [DRONE_RESONANCE] = { 127, 4, 0 },
    [DRONE_LEVEL] = { 127, 4, 64 },
    [DRONE_FADE] = { 127, 4, 96 },              /* on and off in 5 ms .. 6 s */
};

typedef struct {
    uint8_t on;
    uint8_t param[DRONE_NPARAM];
} drone_voice_controls_t;

typedef struct {
    drone_voice_controls_t voice[DRONE_VOICES];
    uint16_t master;                    /* 0..DRONE_MASTER_MAX */
} drone_controls_t;

typedef struct {
    uint32_t phase;
    int32_t low_q8, band_q8;            /* the filter's two states, 256 times a 16-bit sample */
    uint32_t level_q16;                 /* the fade: 0 .. DRONE_LEVEL_TOP */
} drone_voice_t;

typedef struct {
    drone_voice_t voice[DRONE_VOICES];
    int32_t mix[DRONE_BLOCK_FRAMES];
} drone_t;

#define DRONE_LEVEL_TOP 0x7FFF0000u
#define DRONE_STATE_TOP (1 << 26)       /* the filter's states stop here: 8 times a full sample, for the resonance */

/* the filter's coefficient for each cutoff value: 2 sin(pi f / rate) * 16384, f = 20 Hz * 2^(value / 15) */
static const uint16_t drone_cutoff_q14[128] = {
        47, 49, 51, 54, 56, 59, 62, 64, 68, 71, 74, 78, 81, 85, 89, 93,
        98, 102, 107, 112, 118, 123, 129, 135, 141, 148, 155, 163, 170, 178, 187, 195,
        205, 214, 225, 235, 246, 258, 270, 283, 296, 310, 325, 340, 356, 373, 391, 409,
        429, 449, 470, 493, 516, 540, 566, 593, 621, 650, 681, 713, 747, 782, 819, 858,
        898, 941, 985, 1032, 1080, 1132, 1185, 1241, 1300, 1361, 1425, 1493, 1563, 1637, 1715, 1796,
        1880, 1969, 2062, 2160, 2262, 2369, 2480, 2597, 2720, 2848, 2983, 3123, 3270, 3425, 3586, 3755,
        3932, 4117, 4310, 4513, 4725, 4946, 5178, 5421, 5675, 5940, 6218, 6508, 6812, 7129, 7460, 7806,
        8168, 8545, 8939, 9351, 9779, 10227, 10693, 11178, 11684, 12210, 12757, 13326, 13916, 14528, 15163, 15820
};

/* the phase step each sample, of 2^32 a cycle, at the FM-1's sample rate 24000000 / 544 Hz */
static uint32_t drone_pitch_step(uint32_t pitch, uint32_t fine)
{
    static const uint32_t f3_to_e4[12] = {      /* 174.614 Hz * 2^(n / 12) * 2^32 * 544 / 24000000 */
        16999137u, 18009958u, 19080886u, 20215494u, 21417570u, 22691125u,
        24040410u, 25469927u, 26984448u, 28589026u, 30289018u, 32090097u,
    };
    uint32_t from_f = pitch + 7u, octave = from_f / 12u;        /* F3 is pitch 53: 53 + 7 = 5 octaves */
    uint32_t step = octave < 5u ? f3_to_e4[from_f % 12u] >> (5u - octave) : f3_to_e4[from_f % 12u] << (octave - 5u);
    /* half a semitone is 2.93 %: 120 of 4096 */
    return step + (uint32_t)((int32_t)(step >> 12) * (((int32_t)fine - 64) * 120 / 64));
}

/* a * b / 16384 without a 64-bit product: |a| below 2^28, b 0..32768 */
static inline int32_t drone_times_q14(int32_t a, int32_t b)
{
    return (((a >> 12) * b) >> 2) + (((a & 0xFFF) * b) >> 14);
}

static inline int32_t drone_limited(int32_t value, int32_t top)
{
    return value > top ? top : value < -top ? -top : value;
}

static void drone_init(drone_t *d)
{
    uint32_t n;
    for (n = 0; n < DRONE_VOICES; n++) {
        d->voice[n].phase = 0;
        d->voice[n].low_q8 = 0;
        d->voice[n].band_q8 = 0;
        d->voice[n].level_q16 = 0;
    }
}

static uint32_t drone_voices_sounding(const drone_t *d)    /* after the last render: on, or still fading out */
{
    uint32_t n, count = 0;
    for (n = 0; n < DRONE_VOICES; n++)
        count += d->voice[n].level_q16 != 0;
    return count;
}

static void drone_render(drone_t *d, const drone_controls_t *c, int32_t *out, uint32_t frames)
{
    uint32_t i, n;
    int32_t master = c->master > DRONE_MASTER_MAX ? (int32_t)DRONE_MASTER_MAX : (int32_t)c->master;

    for (i = 0; i < frames; i++)
        d->mix[i] = 0;

    for (n = 0; n < DRONE_VOICES; n++) {
        drone_voice_t *v = &d->voice[n];
        const uint8_t *param = c->voice[n].param;
        uint32_t on = c->voice[n].on, wave = param[DRONE_WAVE];
        uint32_t step = drone_pitch_step(param[DRONE_PITCH], param[DRONE_FINE]);
        uint32_t slow = 127u - param[DRONE_FADE];
        uint32_t fade_step = 8000u + slow * slow * slow * 5u;                   /* 6 s .. 5 ms for the whole fade */
        int32_t cutoff = drone_cutoff_q14[param[DRONE_CUTOFF] & 127u];
        int32_t damping = 23170 - (int32_t)param[DRONE_RESONANCE] * 176;        /* 1.41 (flat) .. 0.05 (a sharp peak) */
        int32_t level = param[DRONE_LEVEL];
        if (!on && !v->level_q16)
            continue;                           /* silent: it costs nothing */
        for (i = 0; i < frames; i++) {
            int32_t wave_now, high, sample;
            v->phase += step;
            if (wave == DRONE_SQUARE) {
                wave_now = v->phase & 0x80000000u ? -32767 : 32767;
            } else if (wave == DRONE_TRIANGLE) {
                int32_t up = (int32_t)(v->phase >> 15);                         /* 0..131071 a cycle */
                wave_now = up < 65536 ? up - 32768 : 98303 - up;
            } else {
                wave_now = (int32_t)v->phase >> 16;
            }
            /* the filter (a state-variable one): low is the output, band feeds the resonance */
            v->low_q8 = drone_limited(v->low_q8 + drone_times_q14(v->band_q8, cutoff), DRONE_STATE_TOP);
            high = wave_now * 256 - v->low_q8 - drone_times_q14(v->band_q8, damping);
            v->band_q8 = drone_limited(v->band_q8 + drone_times_q14(high, cutoff), DRONE_STATE_TOP);
            if (on)
                v->level_q16 = DRONE_LEVEL_TOP - v->level_q16 > fade_step ? v->level_q16 + fade_step : DRONE_LEVEL_TOP;
            else
                v->level_q16 = v->level_q16 > fade_step ? v->level_q16 - fade_step : 0;
            sample = drone_limited(v->low_q8 >> 8, 32767) * level / 127;
            d->mix[i] += (sample * (int32_t)(v->level_q16 >> 16)) >> 15;
        }
        if (!v->level_q16) {                    /* faded out: the next start is from rest */
            v->low_q8 = 0;
            v->band_q8 = 0;
        }
    }

    for (i = 0; i < frames; i++) {
        int32_t sample = drone_limited(d->mix[i], 32767) * master;      /* 16 bits times 0..256: the 24-bit range */
        out[2u * i] = sample;
        out[2u * i + 1u] = sample;
    }
}
