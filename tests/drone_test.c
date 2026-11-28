/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kevin Sint */
/* The voices on this computer: pitch, the three waves and their smooth edges, the four filters, the LFOs, the glide
 * of a changed value, the fade to silence, the output range, the blocks of a patch, and V0.16's sound to the bit. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "drone.h"

static int failed;
static void check(const char *what, int ok)
{
    if (!ok) {
        printf("FAIL: %s\n", what);
        failed = 1;
    }
}

#define RATE (24000000.0 / 544.0)               /* the FM-1's real sample rate */
#define SECOND 44118u
static drone_t engine;
static int32_t out[2u * DRONE_BLOCK_FRAMES];
static int32_t mono[SECOND + DRONE_BLOCK_FRAMES], kept[SECOND], side[SECOND + DRONE_BLOCK_FRAMES];    /* side: left less right */

/* the first voice: a saw at F3 (the oscillator), the filter open with no resonance, a level all open, then loud,
 * a fast fade. Its LFOs at their slowest, all there: their cables say how far they move a value */
enum { OSC, FILTER, AMP };                      /* the places of its blocks */
static drone_controls_t plain(void)
{
    drone_controls_t c;
    memset(&c, 0, sizeof c);
    c.voice[0].block[OSC] = (drone_block_controls_t){ .type = DRONE_BLOCK_VCO, .kind = DRONE_SAW, .param = { 53, 64 } };
    c.voice[0].block[FILTER] = (drone_block_controls_t){ .type = DRONE_BLOCK_VCF, .kind = DRONE_LADDER, .param = { 127, 0 } };
    c.voice[0].block[AMP] = (drone_block_controls_t){ .type = DRONE_BLOCK_VCA, .kind = 0, .param = { 127, 0 } };
    c.voice[0].lfo[0][DRONE_LFO_DEPTH] = c.voice[0].lfo[1][DRONE_LFO_DEPTH] = c.voice[0].lfo[2][DRONE_LFO_DEPTH] = 127;
    c.voice[0].param[DRONE_LEVEL] = 127;
    c.master = DRONE_MASTER_MAX;
    return c;
}
#define PITCH(c, n) ((c).voice[n].block[OSC].param[DRONE_VCO_PITCH])
#define WAVE(c, n) ((c).voice[n].block[OSC].kind)
#define CUTOFF(c, n) ((c).voice[n].block[FILTER].param[DRONE_VCF_CUTOFF])
#define RESONANCE(c, n) ((c).voice[n].block[FILTER].param[DRONE_VCF_RESONANCE])
#define FILTER_KIND(c, n) ((c).voice[n].block[FILTER].kind)

static int range_ok, stereo_same;
static void render(const drone_controls_t *c, uint32_t n)   /* into mono[]; every frame checked */
{
    uint32_t at, i;
    for (at = 0; at < n; at += DRONE_BLOCK_FRAMES) {
        drone_render(&engine, c, out, DRONE_BLOCK_FRAMES);
        for (i = 0; i < DRONE_BLOCK_FRAMES; i++) {
            if (out[2 * i] < -8388608 || out[2 * i] > 8388607)
                range_ok = 0;
            if (out[2 * i] != out[2 * i + 1])
                stereo_same = 0;
            mono[at + i] = out[2 * i];
        }
    }
}
static void render_from(const drone_controls_t *c, uint32_t from, uint32_t n)      /* into mono[from ..]: whole blocks */
{
    uint32_t at, i;
    for (at = from; at < from + n; at += DRONE_BLOCK_FRAMES) {
        drone_render(&engine, c, out, DRONE_BLOCK_FRAMES);
        for (i = 0; i < DRONE_BLOCK_FRAMES; i++) {
            if (out[2 * i] < -8388608 || out[2 * i] > 8388607)
                range_ok = 0;
            mono[at + i] = out[2 * i];
            side[at + i] = out[2 * i] - out[2 * i + 1];
        }
    }
}
static int32_t peak_in(const int32_t *sound, uint32_t from, uint32_t n)
{
    uint32_t i;
    int32_t p = 0;
    for (i = from; i < from + n; i++)
        if (sound[i] > p || -sound[i] > p)
            p = sound[i] < 0 ? -sound[i] : sound[i];
    return p;
}
/* a voice for the end of the chain's tests: the first one, with the EOC effects out of the way (no delay heard, no
 * reverb back, a compressor that does nothing) */
#define BURST (16u * DRONE_BLOCK_FRAMES)        /* 93 ms of sound */
static void burst_then(drone_controls_t *c, uint32_t n)     /* from rest: the voice on for a burst, then off; n frames in mono[] */
{
    drone_init(&engine);
    c->voice[0].on = 1;
    render_from(c, 0, BURST);
    c->voice[0].on = 0;
    render_from(c, BURST, n - BURST);
}
static int silent(uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        if (mono[i])
            return 0;
    return 1;
}
static int32_t peak(uint32_t n)
{
    uint32_t i;
    int32_t p = 0;
    for (i = 0; i < n; i++)
        if (mono[i] > p || -mono[i] > p)
            p = mono[i] < 0 ? -mono[i] : mono[i];
    return p;
}
static uint32_t cycles(uint32_t n)              /* times it goes up through zero, from well below to well above */
{
    uint32_t i, count = 0;
    int below = 0;
    int32_t well = peak(n) / 4;
    for (i = 0; i < n; i++) {
        if (mono[i] < -well)
            below = 1;
        if (mono[i] > well && below) {
            below = 0;
            count++;
        }
    }
    return count;
}
/* how much of the signal is at this frequency. The signal is faded in and out (a Hann window), so that a strong
 * tone at another frequency does not count */
static double part_at(uint32_t n, double hz)
{
    double re = 0, im = 0;
    uint32_t i;
    for (i = 0; i < n; i++) {
        double faded = mono[i] * (1.0 - cos(2.0 * M_PI * i / n));
        re += faded * cos(2.0 * M_PI * hz * i / RATE);
        im += faded * sin(2.0 * M_PI * hz * i / RATE);
    }
    return sqrt(re * re + im * im) / n;
}

static uint32_t swells(uint32_t n)              /* times the loudness, block by block, comes up through its middle */
{
    uint32_t at, i, count = 0;
    int32_t middle = peak(n) / 2;
    int below = 0;
    for (at = 0; at + DRONE_BLOCK_FRAMES <= n; at += DRONE_BLOCK_FRAMES) {
        int32_t loud = 0;
        for (i = 0; i < DRONE_BLOCK_FRAMES; i++)
            if (mono[at + i] > loud)
                loud = mono[at + i];
        if (loud < middle / 2)
            below = 1;
        if (loud > middle && below) {
            below = 0;
            count++;
        }
    }
    return count;
}

/* the oscillator alone, a second of it: how much of what is above half the sample rate comes back down as a false
 * tone at `false_hz`, against the note itself */
static double false_tone(uint32_t wave, uint32_t pitch, double false_hz)
{
    static drone_block_t voice;
    uint32_t at;
    memset(&voice, 0, sizeof voice);
    memset(mono, 0, sizeof mono);               /* the oscillator adds to what is there */
    voice.phase_step = drone_pitch_step(pitch, 64);
    for (at = 0; at < SECOND; at += DRONE_BLOCK_FRAMES)
        drone_oscillator(&voice, wave, drone_pitch_step(pitch, 64), mono + at, DRONE_BLOCK_FRAMES, 0, 0, 128);
    return part_at(SECOND, false_hz) / part_at(SECOND, 440.0 * pow(2.0, ((double)pitch - 69.0) / 12.0));
}

/* ---- the sound of V0.16 (92b3a57), pinned: a hash of every sample it rendered for a few patches. V0.17 took the
 * voices that were not patches away; the patches must sound the same, to the bit ---- */
static uint32_t heard(const drone_controls_t *c, uint32_t calls, uint32_t frames, uint32_t hash)
{
    uint32_t n, i, b;
    for (n = 0; n < calls; n++) {
        drone_render(&engine, c, out, frames);
        for (i = 0; i < 2u * frames; i++)
            for (b = 0; b < 32u; b += 8u)               /* FNV-1a, a byte at a time */
                hash = (hash ^ ((uint32_t)out[i] >> b & 255u)) * 16777619u;
    }
    return hash;
}
static void check_pinned(const char *what, uint32_t got, uint32_t want)
{
    if (got != want) {
        printf("FAIL: %s: the sound is not V0.16's (0x%08X)\n", what, got);
        failed = 1;
    }
}
static drone_block_controls_t block_of(uint32_t type, uint32_t kind, uint32_t p0, uint32_t p1, uint32_t p2)
{
    return (drone_block_controls_t){ .type = (uint8_t)type, .kind = (uint8_t)kind, .param = { (uint8_t)p0, (uint8_t)p1, (uint8_t)p2 } };
}
static drone_controls_t three_on(void)          /* the three voices on, with the values the FM-1 starts them with */
{
    drone_controls_t c;
    uint32_t v, l;
    memset(&c, 0, sizeof c);
    for (v = 0; v < 3u; v++) {
        c.voice[v].on = 1;
        c.voice[v].param[DRONE_LEVEL] = 32;
        c.voice[v].param[DRONE_FADE] = 96;
        c.voice[v].send = c.voice[v].send_reverb = 32;
        for (l = 0; l < DRONE_LFOS; l++) {
            c.voice[v].lfo[l][DRONE_LFO_SPEED] = 40;
            c.voice[v].lfo[l][DRONE_LFO_DEPTH] = 127;
        }
    }
    c.reverb[DRONE_REVERB_DECAY] = c.reverb[DRONE_REVERB_TONE] = 64;
    c.reverb[DRONE_REVERB_RETURN] = 127;
    c.master = DRONE_MASTER_MAX;
    return c;
}

static const char *const filter_called[DRONE_NFILTER] = { "ladder", "MS-20", "Polivoks", "Wasp" };
static void check_filter(uint32_t filter, const char *what, int ok)
{
    if (!ok) {
        printf("FAIL: %s: %s\n", filter_called[filter], what);
        failed = 1;
    }
}

/* ---- the bank of LFOs: the controls at their start, the voices silent ---- */
static drone_controls_t banked(void)
{
    drone_controls_t c;
    uint32_t l, p;
    memset(&c, 0, sizeof c);
    for (l = 0; l < DRONE_BANK_LFOS; l++)
        for (p = 0; p < DRONE_BANK_NPARAM; p++)
            c.bank[l][p] = drone_bank_spec[p].start;
    c.tempo = DRONE_TEMPO_START;
    c.master = DRONE_MASTER_MAX;
    return c;
}
static void bank_calls(const drone_controls_t *c, uint32_t calls)
{
    uint32_t n;
    for (n = 0; n < calls; n++)
        drone_render(&engine, c, out, DRONE_BLOCK_FRAMES);
}
#define CALLS_A_SECOND ((double)SECOND / DRONE_BLOCK_FRAMES)
/* how many cycles a bank LFO goes in so many calls: the times its phase goes round, and where it is */
static double bank_cycles(const drone_controls_t *c, uint32_t lfo, uint32_t calls)
{
    uint32_t n, rounds = 0, first = engine.bank.lfo[lfo].phase, was = first;
    for (n = 0; n < calls; n++) {
        drone_render(&engine, c, out, DRONE_BLOCK_FRAMES);
        rounds += engine.bank.lfo[lfo].phase < was;
        was = engine.bank.lfo[lfo].phase;
    }
    return rounds + (double)was / 4294967296.0 - (double)first / 4294967296.0;
}
static double beats_in(const drone_controls_t *c, uint32_t calls)    /* the clock's beats in so many calls */
{
    uint32_t n, beats = 0, was = engine.bank.clock_phase, first = was;
    for (n = 0; n < calls; n++) {
        drone_render(&engine, c, out, DRONE_BLOCK_FRAMES);
        beats += engine.bank.clock_phase < was;
        was = engine.bank.clock_phase;
    }
    return beats + (double)was / 4294967296.0 - (double)first / 4294967296.0;
}
static uint32_t apart(uint32_t a, uint32_t b)   /* how far two phases are, the short way round */
{
    uint32_t d = a - b;
    return d < 0x80000000u ? d : 0u - d;
}
#define BANK_UP 11u                             /* a bank LFO that is always up: no LEVEL, all the OFFSET up */
#define BANK_DOWN 10u                           /* and one always down */
static void bank_steady(drone_controls_t *c)
{
    c->bank[BANK_UP][DRONE_BANK_LEVEL] = c->bank[BANK_DOWN][DRONE_BANK_LEVEL] = 0;
    c->bank[BANK_UP][DRONE_BANK_OFFSET] = 200;
    c->bank[BANK_DOWN][DRONE_BANK_OFFSET] = 0;
}
static void bank_start(uint8_t *value)          /* a bank LFO's values at their start */
{
    uint32_t p;
    for (p = 0; p < DRONE_BANK_NPARAM; p++)
        value[p] = drone_bank_spec[p].start;
}
#define AT(where) ((uint32_t)((where) * 4294967296.0))      /* a place in a step, 0 .. 1, as a phase */
/* the approved draft's shapes, in floating point: a shape's height, 0 .. 1, at `where` (0 .. 1) of its step for a
 * WIDTH 0 .. 100; `now`, `next`: this step's random number and the next one's, 0 .. 1 */
static double shape_want(uint32_t shape, double where, double width, double now, double next)
{
    double w = width / 100.0, x, lean;
    uint32_t slots;
    switch (shape) {
    case DRONE_BANK_GATE:
        return where < w ? 1 : 0;
    case DRONE_BANK_RATCHET2:
    case DRONE_BANK_RATCHET4:
        slots = shape == DRONE_BANK_RATCHET2 ? 4u : 8u;
        x = where * slots;
        return floor(x) < floor(w * slots + 0.5) && x - floor(x) < 0.5 ? 1 : 0;
    case DRONE_BANK_SOFT_GATE:
        return where < w ? fmin(1, where / 0.04) : w > 0 ? fmax(0, 1 - (where - w) / 0.04) : 0;
    case DRONE_BANK_TRIANGLE:
        return where < w ? where / w : 1 - (where - w) / (1 - w);
    case DRONE_BANK_TRAPEZOID:
        x = (1 - w) / 2;
        return x <= 0 ? 1 : where < x ? where / x : where > 1 - x ? (1 - where) / x : 1;
    case DRONE_BANK_SINE:
    case DRONE_BANK_HUMP:
        lean = where < w ? 0.5 * where / w : 0.5 + 0.5 * (where - w) / (1 - w);
        return shape == DRONE_BANK_SINE ? 0.5 - 0.5 * cos(2 * M_PI * lean) : sin(M_PI * lean);
    case DRONE_BANK_EXP:
    case DRONE_BANK_SOFT_EXP:
        x = exp(-where / (0.03 + 0.4 * w));
        return shape == DRONE_BANK_SOFT_EXP ? x * fmin(1, where / 0.04) : x;
    case DRONE_BANK_LOG:
    case DRONE_BANK_SOFT_LOG:
        x = 0.25 + 0.75 * w;
        x = where < x ? 1 - pow(where / x, 3) : 0;
        return shape == DRONE_BANK_SOFT_LOG ? x * fmin(1, where / 0.04) : x;
    case DRONE_BANK_RANDOM:
        return fmin(1, fmax(0, 0.5 + (now - 0.5) * 2 * w));
    default:
        return now + (next - now) * (where * (1 - w) + w * (0.5 - 0.5 * cos(M_PI * where)));
    }
}

int main(void)
{
    drone_controls_t c;
    uint32_t p, i, filter, hash;
    int32_t x;
    double f3 = 174.614115717, c4 = 261.625565301, g4 = 391.995435982, third_saw, third_tri, third_square, second_square, low, peaked;
    double eighth[DRONE_NFILTER], second_of_triangle[DRONE_NFILTER], tail[2];
    range_ok = stereo_same = 1;

    for (p = 0; p < 128; p++) {                 /* the pitch and the filter's table against their formulas */
        double want = 440.0 * pow(2.0, ((double)p - 69.0) / 12.0) * 4294967296.0 / RATE;
        double table = 2.0 * sin(M_PI * 20.0 * pow(2.0, p / 15.0) / RATE) * 16384.0;
        if (fabs((double)drone_pitch_step(p, 64) - want) > want / 100000.0 + 1.0) {
            printf("FAIL: step of pitch %u\n", p);
            failed = 1;
        }
        if (fabs((double)drone_cutoff_q14[p] - table) > 0.51) {
            printf("FAIL: filter table at %u\n", p);
            failed = 1;
        }
        table = (1.0 - exp(-2.0 * M_PI * 20.0 * pow(2.0, p / 15.0) / RATE)) * 16384.0;
        if (fabs((double)drone_pole_q14[p] - table) > 0.51) {
            printf("FAIL: one-pole table at %u\n", p);
            failed = 1;
        }
    }
    check("fine tune at the top is close to half a semitone up",
          fabs((double)drone_pitch_step(60, 127) / drone_pitch_step(60, 64) - pow(2.0, 0.5 / 12.0)) < 0.001);
    check("a pitch between two semitones is between their steps",
          fabs((double)drone_step_q8(60 * 256 + 128) / drone_pitch_step(60, 64) - pow(2.0, 0.5 / 12.0)) < 0.001);
    check("the pitch stops at its ends", drone_step_q8(-5000) == drone_pitch_step(0, 64)
          && drone_step_q8(200 * 256) == drone_pitch_step(127, 64));

    /* the distortion curves of the filters: up all the way (but for one step of rounding where they are flat) */
    for (x = -200000; x < 200000; x += 37)
        if (drone_soft(x + 37) < drone_soft(x) - 1 || drone_soft(x) != -drone_soft(-x) || drone_soft(x) > 32767
            || drone_uneven(x + 37) < drone_uneven(x) - 1 || drone_uneven(x) > 32767 || drone_uneven(x) < -32767) {
            printf("FAIL: the distortion curves at %d\n", x);
            failed = 1;
            break;
        }
    check("soft: a small signal goes through as it is", abs(drone_soft(1000) - 1000) < 3 && drone_soft(0) == 0);
    check("soft: a big one stops at full scale", drone_soft(60000) == 32767 && drone_soft(30000) < 27000);
    check("uneven: the top half is squeezed more than the bottom half", drone_uneven(20000) + drone_uneven(-20000) < -500);

    /* the LFO's waves: one cycle is 2^32 of phase, the value -32767..32767 */
    check("LFO sine: 0, the top at a quarter, 0 at the half, the bottom at three quarters",
          drone_lfo_wave(DRONE_LFO_SINE, 0) == 0 && drone_lfo_wave(DRONE_LFO_SINE, 0x40000000u) == 32767
          && abs(drone_lfo_wave(DRONE_LFO_SINE, 0x80000000u)) < 4 && drone_lfo_wave(DRONE_LFO_SINE, 0xC0000000u) == -32767);
    check("LFO sine is close to a true sine",
          fabs(drone_lfo_wave(DRONE_LFO_SINE, 0x15555555u) / 32767.0 - sin(M_PI / 6.0)) < 0.06);
    check("LFO triangle: straight to the top at a quarter",
          drone_lfo_wave(DRONE_LFO_TRIANGLE, 0x40000000u) == 32767 && abs(drone_lfo_wave(DRONE_LFO_TRIANGLE, 0x20000000u) - 16384) < 4
          && drone_lfo_wave(DRONE_LFO_TRIANGLE, 0xC0000000u) == -32767);
    check("LFO saw: up all the cycle", drone_lfo_wave(DRONE_LFO_SAW, 0x7FFFFFFFu) == 32767
          && drone_lfo_wave(DRONE_LFO_SAW, 0x80010000u) == -32767 && drone_lfo_wave(DRONE_LFO_SAW, 0x20000000u) == 8192);
    check("LFO square: high the first half, low the second",
          drone_lfo_wave(DRONE_LFO_SQUARE, 5) == 32767 && drone_lfo_wave(DRONE_LFO_SQUARE, 0x80000005u) == -32767);

    /* the oscillator's edges: at C7 (2093 Hz) the saw's harmonic 20 is at 41860 Hz, which a plain saw gives back
     * at 2258 Hz, 1/20 as loud as the note; the square's harmonic 21 comes back at 164 Hz */
    check("a saw's edge is smooth: the false tone is far down", false_tone(DRONE_SAW, 96, RATE - 20 * 2093.0045) < 0.005);
    check("a square's edges are smooth", false_tone(DRONE_SQUARE, 96, RATE - 21 * 2093.0045) < 0.005);
    check("a low saw is still a saw", fabs(false_tone(DRONE_SAW, 45, 220.0) - 0.5) < 0.02);

    check("fine tune at the bottom is half a semitone down",
          fabs((double)drone_pitch_step(60, 0) / drone_pitch_step(60, 64) - pow(2.0, -0.5 / 12.0)) < 0.001);

    drone_init(&engine);
    c = plain();
    render(&c, SECOND);
    check("the voice off: silence", silent(SECOND) && drone_voices_sounding(&engine) == 0);

    c.voice[0].on = 1;
    render(&c, SECOND);
    check("the voice on: it sounds, loud", peak(SECOND) > 4000000 && drone_voices_sounding(&engine) == 1);
    check("F3 is 174 or 175 cycles a second", cycles(SECOND) >= 174 && cycles(SECOND) <= 175);
    third_saw = part_at(SECOND, 3 * f3) / part_at(SECOND, f3);
    check("a saw has its second harmonic", part_at(SECOND, 2 * f3) / part_at(SECOND, f3) > 0.3);

    PITCH(c, 0) = 41;         /* an octave down */
    render(&c, SECOND);
    check("an octave down is 87 or 88", cycles(SECOND) >= 87 && cycles(SECOND) <= 88);
    PITCH(c, 0) = 53;

    WAVE(c, 0) = DRONE_SQUARE;
    render(&c, SECOND);
    second_square = part_at(SECOND, 2 * f3) / part_at(SECOND, f3);
    third_square = part_at(SECOND, 3 * f3) / part_at(SECOND, f3);
    check("a square has no second harmonic, and its third", second_square < 0.02 && third_square > 0.25);
    WAVE(c, 0) = DRONE_TRIANGLE;
    render(&c, SECOND);
    third_tri = part_at(SECOND, 3 * f3) / part_at(SECOND, f3);
    check("a triangle is the softest", third_tri < 0.15 && third_tri < third_saw && peak(SECOND) > 4000000);
    WAVE(c, 0) = DRONE_SAW;

    CUTOFF(c, 0) = 30;        /* the filter at 80 Hz takes the harmonics away */
    render(&c, SECOND);
    render(&c, SECOND);
    check("the filter low: the third harmonic is far down", part_at(SECOND, 3 * f3) / part_at(SECOND, f3) < third_saw / 4);
    CUTOFF(c, 0) = 0;
    render(&c, SECOND);
    render(&c, SECOND);
    low = peak(SECOND);
    check("the filter at the bottom: nearly nothing left", low < 8388607 / 20);

    CUTOFF(c, 0) = 61;        /* the filter near the third harmonic (335 of 524 Hz), with resonance */
    c.voice[0].param[DRONE_LEVEL] = 16;         /* low, so the peak is not clipped */
    render(&c, SECOND);
    render(&c, SECOND);
    low = part_at(SECOND, 2 * f3);
    RESONANCE(c, 0) = 127;
    render(&c, SECOND);
    render(&c, SECOND);
    peaked = part_at(SECOND, 2 * f3);
    check("resonance lifts what is at the filter's frequency", peaked > 3.0 * low);
    RESONANCE(c, 0) = 0;
    CUTOFF(c, 0) = 127;

    c.voice[0].param[DRONE_LEVEL] = 32;         /* quiet: the sum bends a loud voice, and leaves this one as it is */
    render(&c, SECOND);
    render(&c, SECOND);
    low = peak(SECOND);
    c.voice[0].param[DRONE_LEVEL] = 16;
    render(&c, DRONE_BLOCK_FRAMES);             /* a changed value takes one block to get there */
    render(&c, SECOND);
    check("the level at half is half", fabs(peak(SECOND) / low - 0.5) < 0.02);
    c.voice[0].param[DRONE_LEVEL] = 0;
    render(&c, DRONE_BLOCK_FRAMES);
    render(&c, SECOND);
    check("the level at zero: silence", silent(SECOND));
    c.voice[0].param[DRONE_LEVEL] = 127;

    c.master = 0;
    render(&c, SECOND);
    check("master at zero: silence", silent(SECOND));
    c.master = DRONE_MASTER_MAX;

    c.voice[0].on = 0;                          /* off: it fades, then it is exactly silent */
    render(&c, SECOND);
    render(&c, SECOND);
    check("off: it ends in silence and the voice is free", silent(SECOND) && drone_voices_sounding(&engine) == 0);

    c.voice[0].param[DRONE_FADE] = 127;         /* the slowest fade: still rising after a second */
    c.voice[0].on = 1;
    render(&c, SECOND);
    low = peak(SECOND);
    render(&c, SECOND);
    check("a slow fade is still rising after one second", peak(SECOND) > 1.25 * low);

    /* each of the four filters: it lets the note through, takes the top away, and its resonance lifts */
    for (filter = 0; filter < DRONE_NFILTER; filter++) {
        drone_init(&engine);
        c = plain();
        FILTER_KIND(c, 0) = (uint8_t)filter;
        c.voice[0].on = 1;
        render(&c, SECOND);
        check_filter(filter, "open: loud, at the note's pitch",
                     peak(SECOND) > 4000000 && cycles(SECOND) >= 174 && cycles(SECOND) <= 175);
        peaked = part_at(SECOND, 3 * f3) / part_at(SECOND, f3);
        CUTOFF(c, 0) = 30;
        render(&c, SECOND);
        render(&c, SECOND);
        check_filter(filter, "low: the third harmonic is far down", part_at(SECOND, 3 * f3) / part_at(SECOND, f3) < peaked / 4);
        CUTOFF(c, 0) = 0;
        render(&c, SECOND);
        render(&c, SECOND);
        check_filter(filter, "at the bottom: nearly nothing left", peak(SECOND) < 8388607 / 20);

        CUTOFF(c, 0) = 47;    /* at the note: how steep is it eight harmonics up? */
        render(&c, SECOND);
        render(&c, SECOND);
        eighth[filter] = part_at(SECOND, 8 * f3) / part_at(SECOND, f3);

        CUTOFF(c, 0) = 61;
        c.voice[0].param[DRONE_LEVEL] = 16;
        render(&c, SECOND);
        render(&c, SECOND);
        low = part_at(SECOND, 2 * f3);
        RESONANCE(c, 0) = 100;            /* a strong peak; at the top some filters sing their own note */
        render(&c, SECOND);
        render(&c, SECOND);
        check_filter(filter, "resonance lifts what is at the filter's frequency", part_at(SECOND, 2 * f3) > 2.0 * low);
        WAVE(c, 0) = DRONE_TRIANGLE;      /* a triangle has no second harmonic of its own */
        CUTOFF(c, 0) = 47;                /* the peak on the note: the filter works hard */
        c.voice[0].param[DRONE_LEVEL] = 127;
        render(&c, SECOND);
        render(&c, SECOND);
        second_of_triangle[filter] = part_at(SECOND, 2 * f3) / part_at(SECOND, f3);

        c.voice[0].on = 0;
        render(&c, SECOND);
        render(&c, SECOND);
        check_filter(filter, "off: it ends in silence", silent(SECOND) && drone_voices_sounding(&engine) == 0);
    }
    check("the ladder is the steepest: 24 dB an octave against 12",
          eighth[DRONE_LADDER] < eighth[DRONE_MS20] / 8 && eighth[DRONE_LADDER] < eighth[DRONE_POLIVOKS] / 8
          && eighth[DRONE_LADDER] < eighth[DRONE_WASP] / 8);
    check("the Wasp's distortion is uneven: it adds a second harmonic, the others do not",
          second_of_triangle[DRONE_WASP] > 0.01 && second_of_triangle[DRONE_LADDER] < 0.002
          && second_of_triangle[DRONE_MS20] < 0.002 && second_of_triangle[DRONE_POLIVOKS] < 0.002);

    /* a change of filter while the voice sounds: it goes on, in range */
    drone_init(&engine);
    c = plain();
    c.voice[0].on = 1;
    for (i = 0; i < 400; i++) {
        FILTER_KIND(c, 0) = (uint8_t)(i / 7 % DRONE_NFILTER);
        RESONANCE(c, 0) = 127;
        render(&c, DRONE_BLOCK_FRAMES);
    }
    render(&c, SECOND);
    check("after many changes of filter it still sounds", peak(SECOND) > 1000000);

    /* the LFOs' cables. A square LFO at its slowest is high for the first half minute */
    drone_init(&engine);
    c = plain();
    c.voice[0].on = 1;
    c.voice[0].lfo[0][DRONE_LFO_WAVE] = DRONE_LFO_SQUARE;
    c.voice[0].lfo[0][DRONE_LFO_DEPTH] = 0;
    c.voice[0].block[OSC].amount[DRONE_VCO_PITCH][0] = 100;
    render(&c, SECOND);
    check("an LFO with no depth changes nothing", cycles(SECOND) >= 174 && cycles(SECOND) <= 175);
    c.voice[0].lfo[0][DRONE_LFO_DEPTH] = 127;
    render(&c, SECOND);
    check("a cable to the pitch, all of it: an octave up", cycles(SECOND) >= 348 && cycles(SECOND) <= 350);
    c.voice[0].block[OSC].amount[DRONE_VCO_PITCH][0] = 0;
    c.voice[0].lfo[1][DRONE_LFO_WAVE] = c.voice[0].lfo[2][DRONE_LFO_WAVE] = DRONE_LFO_SQUARE;
    c.voice[0].block[OSC].amount[DRONE_VCO_PITCH][1] = c.voice[0].block[OSC].amount[DRONE_VCO_PITCH][2] = 25;
    render(&c, SECOND);
    check("two LFOs' cables to one value add: 3 and 3 semitones up", cycles(SECOND) >= 246 && cycles(SECOND) <= 248);
    c.voice[0].block[OSC].amount[DRONE_VCO_PITCH][1] = c.voice[0].block[OSC].amount[DRONE_VCO_PITCH][2] = 0;

    CUTOFF(c, 0) = 30;
    render(&c, SECOND);
    render(&c, SECOND);
    low = part_at(SECOND, 3 * f3) / part_at(SECOND, f3);
    c.voice[0].block[FILTER].amount[DRONE_VCF_CUTOFF][2] = 100;
    render(&c, SECOND);
    render(&c, SECOND);
    check("LFO 3's cable to the cutoff opens the filter", part_at(SECOND, 3 * f3) / part_at(SECOND, f3) > 4.0 * low);
    c.voice[0].block[FILTER].amount[DRONE_VCF_CUTOFF][2] = 0;
    CUTOFF(c, 0) = 61;
    c.voice[0].param[DRONE_LEVEL] = 16;
    render(&c, SECOND);
    render(&c, SECOND);
    low = part_at(SECOND, 2 * f3);
    c.voice[0].block[FILTER].amount[DRONE_VCF_RESONANCE][0] = 100;
    render(&c, SECOND);
    render(&c, SECOND);
    check("a cable to the resonance lifts the peak", part_at(SECOND, 2 * f3) > 2.0 * low);

    c = plain();                                /* speed 96 is 4.09 Hz: the level swells four times a second */
    c.voice[0].on = 1;
    c.voice[0].param[DRONE_LEVEL] = 64;
    c.voice[0].lfo[1][DRONE_LFO_SPEED] = 96;
    c.voice[0].out_amount[DRONE_OUT_LEVEL][1] = 100;
    render(&c, SECOND);
    render(&c, SECOND);
    check("a cable to the level at 4 Hz: four swells a second", swells(SECOND) >= 3 && swells(SECOND) <= 5);
    c.voice[0].lfo[1][DRONE_LFO_SPEED] = 108;
    render(&c, SECOND);
    render(&c, SECOND);
    check("an octave faster: eight", swells(SECOND) >= 7 && swells(SECOND) <= 9);

    /* a changed value glides over one block: no step in the sound */
    drone_init(&engine);
    c = plain();
    c.voice[0].on = 1;
    WAVE(c, 0) = DRONE_SQUARE;
    PITCH(c, 0) = 77;         /* 698 Hz: a few cycles in every part of a block */
    render(&c, SECOND);
    c.voice[0].param[DRONE_LEVEL] = 0;
    render(&c, DRONE_BLOCK_FRAMES);
    low = peak(DRONE_BLOCK_FRAMES / 4);
    memmove(mono, mono + DRONE_BLOCK_FRAMES / 2, sizeof mono[0] * DRONE_BLOCK_FRAMES / 4);
    peaked = peak(DRONE_BLOCK_FRAMES / 4);
    check("the level to zero: still loud at the start of the block, less in its middle", low > 5000000 && peaked < low * 0.6 && peaked > low * 0.2);
    render(&c, DRONE_BLOCK_FRAMES);
    check("and silent after it", silent(DRONE_BLOCK_FRAMES));

    /* three voices: each has its own parameters, and the sound is the sum of those that are on */
    drone_init(&engine);
    c = plain();
    for (i = 0; i < DRONE_VOICES; i++) {
        c.voice[i] = c.voice[0];
        WAVE(c, i) = DRONE_TRIANGLE;
        c.voice[i].param[DRONE_LEVEL] = 32;     /* a quarter each: the sum stays below the limit */
    }
    PITCH(c, 1) = 60;         /* C4 */
    PITCH(c, 2) = 67;         /* G4 */
    c.voice[0].on = c.voice[2].on = 1;
    render(&c, SECOND);
    check("three voices: two on, two sound", DRONE_VOICES == 3 && drone_voices_sounding(&engine) == 2);
    check("the first and the third are in the sound, the second is not",
          part_at(SECOND, f3) > 100000 && part_at(SECOND, g4) > 100000 && part_at(SECOND, c4) < 1000);
    c.voice[1].on = 1;
    render(&c, SECOND);
    check("the second on: all three sound, in range", drone_voices_sounding(&engine) == 3 && part_at(SECOND, c4) > 100000 && range_ok);

    /* the sum of the voices is bent near the top, not cut flat: saws that together go to 1.5 times a full sample */
    drone_init(&engine);
    c = plain();
    for (i = 0; i < DRONE_VOICES; i++) {
        c.voice[i] = c.voice[0];
        PITCH(c, i) = (uint8_t)(53 + 2 * i);
        c.voice[i].param[DRONE_LEVEL] = (uint8_t)(64u * 3u / DRONE_VOICES);
        c.voice[i].on = 1;
    }
    render(&c, SECOND);
    for (x = 0, i = 0; i < SECOND; i++)
        x += abs(mono[i]) >= 32767 * (int32_t)DRONE_MASTER_MAX;
    check("all the voices on: loud, and no frame flat at the limit", x == 0 && peak(SECOND) > 6000000);

    check("with no send, left and right are the same", stereo_same);

    /* the reverb: each voice sends to it from before its level; its sound comes back through RETURN */
    drone_init(&engine);
    c = plain();
    c.voice[0].on = 1;
    c.reverb[DRONE_REVERB_DECAY] = 64;
    c.reverb[DRONE_REVERB_TONE] = 127;
    c.reverb[DRONE_REVERB_RETURN] = 127;        /* RETURN up, no send: the dry sound and nothing more */
    render(&c, SECOND);
    memcpy(kept, mono, sizeof kept);
    drone_init(&engine);
    c.voice[0].send = c.voice[0].send_reverb = 127;                      /* a send, RETURN down: the same */
    c.reverb[DRONE_REVERB_RETURN] = 0;
    stereo_same = 1;
    render(&c, SECOND);
    check("no send, or no return: the dry sound as it is", stereo_same && !memcmp(kept, mono, sizeof kept));

    drone_init(&engine);
    c.reverb[DRONE_REVERB_RETURN] = 127;
    c.voice[0].param[DRONE_LEVEL] = 0;          /* the send is from before the level */
    stereo_same = 1;
    render(&c, SECOND);
    check("level down and send up: the reverb alone, and not the same left and right", peak(SECOND) > 500000 && !stereo_same);
    c.voice[0].on = 0;
    c.voice[1] = c.voice[0];
    c.voice[1].param[DRONE_LEVEL] = 127;
    c.voice[1].send = c.voice[1].send_reverb = 0;
    c.voice[1].on = 1;
    for (i = 0; i < 25; i++)                    /* the first voice's tail goes */
        render(&c, SECOND);
    stereo_same = 1;
    render(&c, SECOND);
    check("a voice's send is its own: another voice with none stays dry", stereo_same && peak(SECOND) > 4000000);

    /* the mute: the channel and its sends fade to silence at the voice's FADE, as an on and off; the voice still
     * runs for its links */
    drone_init(&engine);
    c = plain();
    c.voice[0].on = 1;
    c.voice[0].param[DRONE_LEVEL] = 127;
    c.voice[0].param[DRONE_FADE] = 96;          /* as the styles have it: about 0.3 s */
    c.voice[0].send = c.voice[0].send_reverb = 127;
    c.voice[0].post = 0;                        /* sends from before the level: the mute cuts them too */
    c.reverb[DRONE_REVERB_DECAY] = 0;
    c.reverb[DRONE_REVERB_RETURN] = 127;
    c.delay[DRONE_DELAY_LEVEL] = 0;
    render(&c, SECOND);
    x = peak(SECOND);
    c.voice[0].muted = 1;
    render(&c, SECOND / 20);
    check("muted: in its first 50 ms it is still loud: it fades, it is not cut", peak(SECOND / 20) > x / 2);
    for (i = 0; i < 25; i++)
        render(&c, SECOND);
    check("muted: the mix and the sends are silent", silent(SECOND));
    check("muted: the voice still runs, so its links still hear it", engine.voice[0].loud > 1000);
    c.voice[0].muted = 0;
    render(&c, SECOND / 20);
    check("not muted again: in its first 50 ms it is still low: it fades in", peak(SECOND / 20) < x / 2);
    render(&c, SECOND);
    check("and then it is heard", peak(SECOND) > x / 2);
    drone_init(&engine);                        /* as at power-on: on and muted from rest */
    c.voice[0].muted = 1;
    render(&c, SECOND);
    check("muted from rest: silent from its first frame, no blip while it starts", silent(SECOND) && engine.voice[0].loud > 1000);

    for (p = 0; p < 2; p++) {                   /* a short decay and a long one: what is left a second after the voice */
        drone_init(&engine);
        c = plain();
        c.voice[0].on = 1;
        c.voice[0].send = c.voice[0].send_reverb = 127;
        c.voice[0].param[DRONE_LEVEL] = 0;
        c.reverb[DRONE_REVERB_DECAY] = p ? 64 : 0;
        c.reverb[DRONE_REVERB_TONE] = 127;
        c.reverb[DRONE_REVERB_RETURN] = 127;
        render(&c, SECOND);
        c.voice[0].on = 0;
        render(&c, SECOND);
        render(&c, SECOND / 4);
        tail[p] = peak(SECOND / 4);
        if (p) {                                /* decay 64 is 3 s to a thousandth: a second later, a tenth is left */
            render(&c, SECOND);
            render(&c, SECOND / 4);
            check("the tail falls at the decay's rate", peak(SECOND / 4) > tail[1] / 30 && peak(SECOND / 4) < tail[1] / 3);
            for (i = 0; i < 20; i++)
                render(&c, SECOND);
            check("and ends in silence", silent(SECOND));
        }
    }
    check("a longer decay: a longer tail", tail[1] > 1000 && tail[1] > 20 * tail[0]);

    for (p = 0; p < 2; p++) {                   /* the tone: how much of the tail is high */
        drone_init(&engine);
        c = plain();
        c.voice[0].on = 1;
        c.voice[0].send = c.voice[0].send_reverb = 127;
        c.voice[0].param[DRONE_LEVEL] = 0;
        c.reverb[DRONE_REVERB_DECAY] = 96;
        c.reverb[DRONE_REVERB_TONE] = p ? 127 : 0;
        c.reverb[DRONE_REVERB_RETURN] = 127;
        render(&c, SECOND);
        c.voice[0].on = 0;
        render(&c, SECOND / 2);
        render(&c, SECOND);
        tail[p] = part_at(SECOND, 16 * f3) / part_at(SECOND, f3);
    }
    check("a low tone: a darker tail", tail[0] < tail[1] / 4);

    /* ---- a patch's blocks ---- */
    drone_init(&engine);
    stereo_same = 1;
    c = plain();
    c.voice[0].on = 1;
    render(&c, SECOND);
    x = peak(SECOND);
    low = part_at(SECOND, f3);
    eighth[0] = part_at(SECOND, 8 * f3);
    c.voice[0].block[1].param[DRONE_VCF_CUTOFF] = 45;       /* 160 Hz */
    render(&c, SECOND / 4);
    render(&c, SECOND);
    check("the patch's filter closed: the highs are gone", part_at(SECOND, 8 * f3) < eighth[0] / 50);
    c = plain();
    c.voice[0].on = 1;
    c.voice[0].block[2].param[DRONE_VCA_LEVEL] = 64;
    render(&c, SECOND / 4);
    render(&c, SECOND);
    check("the patch's level at half: half as loud", peak(SECOND) > x * 45 / 100 && peak(SECOND) < x * 55 / 100);
    c.voice[0].block[2].param[DRONE_VCA_LEVEL] = 0;
    render(&c, SECOND / 4);
    render(&c, SECOND);
    check("the patch's level at 0: silence", silent(SECOND));
    c = plain();
    c.voice[0].on = 1;
    c.voice[0].block[2] = (drone_block_controls_t){ .type = DRONE_BLOCK_VCO, .kind = DRONE_TRIANGLE, .param = { 60, 64 } };     /* C4, after the filter */
    render(&c, SECOND / 4);
    render(&c, SECOND);
    check("a second oscillator adds its note to the first one's", part_at(SECOND, c4) > low / 4 && part_at(SECOND, f3) > low / 4);
    c.voice[0].block[0] = c.voice[0].block[2];              /* the first one taken away: the others move down */
    memset(&c.voice[0].block[1], 0, 2 * sizeof c.voice[0].block[1]);
    render(&c, SECOND / 4);
    render(&c, SECOND);
    check("a block taken away while it sounds: the others go on", part_at(SECOND, c4) > low / 4 && part_at(SECOND, f3) < low / 100);
    memset(c.voice[0].block, 0, sizeof c.voice[0].block);
    render(&c, SECOND / 4);
    render(&c, SECOND);
    check("a patch with no block is silent", silent(SECOND));
    check("a patch is the same left and right", stereo_same);

    /* ---- a patch's LFOs and their cables ---- */
    drone_init(&engine);
    c = plain();
    c.voice[0].on = 1;
    render(&c, SECOND);
    memcpy(kept, mono, sizeof kept);
    drone_init(&engine);
    c.voice[0].lfo[0][DRONE_LFO_SPEED] = 96;                /* 4 Hz */
    c.voice[0].lfo[0][DRONE_LFO_WAVE] = DRONE_LFO_SQUARE;
    render(&c, SECOND);
    check("an LFO with no cable changes nothing", !memcmp(kept, mono, sizeof kept));
    c.voice[0].block[2].param[DRONE_VCA_LEVEL] = 64;
    c.voice[0].block[2].amount[DRONE_VCA_LEVEL][0] = 50;
    drone_init(&engine);
    render(&c, 2 * DRONE_BLOCK_FRAMES);
    render(&c, 8 * DRONE_BLOCK_FRAMES);                     /* the square starts in its top half: an eighth of a second */
    x = peak(8 * DRONE_BLOCK_FRAMES);
    render(&c, SECOND);
    check("a cable to a level block: the LFO moves the level, four swells a second", swells(SECOND) >= 3 && swells(SECOND) <= 5);
    c.voice[0].block[2].amount[DRONE_VCA_LEVEL][0] = -50;
    drone_init(&engine);
    render(&c, 2 * DRONE_BLOCK_FRAMES);
    render(&c, 8 * DRONE_BLOCK_FRAMES);
    check("a negative amount: the LFO upside down, quiet where it was loud", x > 4000000 && peak(8 * DRONE_BLOCK_FRAMES) < x / 20);
    c.voice[0].block[2].amount[DRONE_VCA_LEVEL][0] = 10;
    drone_init(&engine);
    render(&c, 2 * DRONE_BLOCK_FRAMES);
    render(&c, 8 * DRONE_BLOCK_FRAMES);
    check("a small amount moves it a little", peak(8 * DRONE_BLOCK_FRAMES) > x * 55 / 100 && peak(8 * DRONE_BLOCK_FRAMES) < x * 70 / 100);
    c = plain();
    c.voice[0].on = 1;
    c.voice[0].lfo[1][DRONE_LFO_SPEED] = 60;                /* half a hertz: its top half lasts a second */
    c.voice[0].lfo[1][DRONE_LFO_WAVE] = DRONE_LFO_SQUARE;
    c.voice[0].block[0].amount[DRONE_VCO_PITCH][1] = 100;
    drone_init(&engine);
    render(&c, SECOND / 8);
    render(&c, SECOND / 2);
    check("the second LFO's cable to an oscillator's pitch, at the top: an octave up", cycles(SECOND / 2) >= 173 && cycles(SECOND / 2) <= 176);
    c.voice[0].block[0].amount[DRONE_VCO_PITCH][1] = 0;
    c.voice[0].block[1].param[DRONE_VCF_CUTOFF] = 110;
    c.voice[0].block[1].amount[DRONE_VCF_CUTOFF][1] = -100;
    drone_init(&engine);
    render(&c, SECOND / 8);
    render(&c, SECOND / 2);
    check("a cable to a filter's cutoff, inverted: it closes", part_at(SECOND / 2, 8 * f3) < eighth[0] / 50);
    c = plain();
    c.voice[0].on = 1;
    c.voice[0].lfo[0][DRONE_LFO_SPEED] = 96;
    c.voice[0].param[DRONE_LEVEL] = 64;
    c.voice[0].out_amount[DRONE_OUT_LEVEL][0] = 60;
    drone_init(&engine);
    render(&c, SECOND / 4);
    render(&c, SECOND);
    check("a cable to the voice's level", swells(SECOND) >= 3 && swells(SECOND) <= 5);
    c.voice[0].out_amount[DRONE_OUT_LEVEL][0] = 0;
    c.voice[0].param[DRONE_LEVEL] = 0;
    c.voice[0].send = c.voice[0].send_reverb = 64;
    c.voice[0].out_amount[DRONE_OUT_SEND][0] = 60;
    c.reverb[DRONE_REVERB_RETURN] = 127;
    c.reverb[DRONE_REVERB_DECAY] = 0;
    drone_init(&engine);
    render(&c, SECOND / 4);
    render(&c, SECOND);
    check("a cable to the voice's send: the reverb gets the sound in swells", peak(SECOND) > 100000 && swells(SECOND) >= 3 && swells(SECOND) <= 5);
    stereo_same = 1;

    /* ---- an LFO's depth, and an LFO's cables to an LFO ---- */
    c = plain();
    c.voice[0].on = 1;
    c.voice[0].lfo[0][DRONE_LFO_SPEED] = 96;                /* 4 Hz */
    c.voice[0].lfo[0][DRONE_LFO_WAVE] = DRONE_LFO_SQUARE;
    c.voice[0].block[2].param[DRONE_VCA_LEVEL] = 64;
    c.voice[0].block[2].amount[DRONE_VCA_LEVEL][0] = 50;
    c.voice[0].lfo[0][DRONE_LFO_DEPTH] = 0;
    drone_init(&engine);
    render(&c, 2 * DRONE_BLOCK_FRAMES);
    render(&c, SECOND);
    check("an LFO with no depth moves nothing", swells(SECOND) == 0);
    c.voice[0].lfo[0][DRONE_LFO_DEPTH] = 127;
    c.voice[0].lfo[1][DRONE_LFO_SPEED] = 48;                /* a square of 4 s: its top half is two seconds */
    c.voice[0].lfo[1][DRONE_LFO_WAVE] = DRONE_LFO_SQUARE;
    c.voice[0].lfo_amount[0][DRONE_LFO_TO_SPEED][1] = 50;   /* the second LFO to the first one's speed: an octave up */
    drone_init(&engine);
    render(&c, 2 * DRONE_BLOCK_FRAMES);
    render(&c, SECOND);
    check("an LFO's cable to another LFO's speed: twice as fast", swells(SECOND) >= 7 && swells(SECOND) <= 9);
    c.voice[0].lfo_amount[0][DRONE_LFO_TO_SPEED][1] = -50;
    drone_init(&engine);
    render(&c, 2 * DRONE_BLOCK_FRAMES);
    render(&c, SECOND);
    check("inverted: half as fast", swells(SECOND) >= 1 && swells(SECOND) <= 3);
    c.voice[0].lfo_amount[0][DRONE_LFO_TO_SPEED][1] = 0;
    c.voice[0].lfo_amount[0][DRONE_LFO_TO_DEPTH][1] = -100; /* and to its depth: none left */
    drone_init(&engine);
    render(&c, 2 * DRONE_BLOCK_FRAMES);
    render(&c, SECOND);
    check("an LFO's cable to another LFO's depth, inverted: that LFO moves nothing", swells(SECOND) == 0);

    /* ---- the feedback cable ---- */
    c = plain();                              /* oscillator, a quiet level, the filter open: room for the loop */
    c.voice[0].on = 1;
    c.voice[0].block[1] = c.voice[0].block[2];
    c.voice[0].block[1].param[DRONE_VCA_LEVEL] = 16;
    c.voice[0].block[2] = (drone_block_controls_t){ .type = DRONE_BLOCK_VCF, .kind = DRONE_LADDER, .param = { 127, 0 } };
    drone_init(&engine);
    render(&c, SECOND);
    memcpy(kept, mono, sizeof kept);
    low = part_at(SECOND, f3);
    c.voice[0].fb[DRONE_FB_FROM] = 2;
    c.voice[0].fb[DRONE_FB_TO] = 2;
    c.voice[0].fb[DRONE_FB_TIME] = 240;         /* 256 frames: 172.3 Hz, next to the note */
    drone_init(&engine);
    render(&c, SECOND);
    check("the feedback cable with no level changes nothing", !memcmp(kept, mono, sizeof kept));
    c.voice[0].fb[DRONE_FB_LEVEL] = 32;         /* half of it back */
    drone_init(&engine);
    render(&c, SECOND / 2);
    render(&c, SECOND);
    peaked = part_at(SECOND, f3);
    check("half of the sound back round the filter, a cycle of the note late: the note nearly twice as strong", peaked > 1.5 * low && peaked < 2.2 * low);
    c.voice[0].fb[DRONE_FB_TIME] = 16;          /* 32 frames: an eighth of a cycle late */
    drone_init(&engine);
    render(&c, SECOND / 2);
    render(&c, SECOND);
    check("a shorter time: the note less strong than that", part_at(SECOND, f3) > 1.1 * low && part_at(SECOND, f3) < 0.85 * peaked);
    c.voice[0].fb[DRONE_FB_TIME] = 237;         /* 253 frames: a cycle of the note, to the frame */
    drone_init(&engine);
    render(&c, SECOND / 2);
    render(&c, SECOND);
    check("the time is fine: a cycle of the note to the frame, and the note is stronger again", part_at(SECOND, f3) > peaked);
    c.voice[0].fb[DRONE_FB_TIME] = 110;         /* half a cycle late: it takes from the note */
    drone_init(&engine);
    render(&c, SECOND / 2);
    render(&c, SECOND);
    check("half a cycle late: the note is weaker than with no feedback", part_at(SECOND, f3) < 0.8 * low);

    /* the scope: the two sounds where the cable goes back in */
    c.voice[0].fb[DRONE_FB_TIME] = 237;
    drone_init(&engine);
    render(&c, SECOND / 2);
    check("no picture is taken when none is wanted", engine.scope.at == 0 && !engine.scope.wanted);
    engine.scope.voice = 0;
    engine.scope.wanted = 1;
    render(&c, SECOND / 4);
    check("a picture wanted: it is taken, and that is said", !engine.scope.wanted);
    check("it starts where the sound comes up through zero", engine.scope.in[0] >= 0 && engine.scope.in[0] < 400 && engine.scope.in[8] > engine.scope.in[0]);
    for (x = 0, i = 0; i < DRONE_SCOPE_POINTS; i++)
        x += (engine.scope.in[i] > 0) == (engine.scope.back[i] > 0);
    check("the time a cycle of the note: what comes back goes up and down with what comes down the chain", x > (int32_t)DRONE_SCOPE_POINTS * 85 / 100);
    memcpy(kept, engine.scope.in, sizeof engine.scope.in);
    engine.scope.wanted = 1;
    render(&c, SECOND / 4);
    for (x = 0, i = 0; i < DRONE_SCOPE_POINTS; i++)
        x += abs(engine.scope.in[i] - ((int16_t *)kept)[i]) < 600;
    check("the next picture of the same sound is nearly the same", x > (int32_t)DRONE_SCOPE_POINTS * 85 / 100);
    c.voice[0].fb[DRONE_FB_TIME] = 110;
    render(&c, SECOND / 2);
    engine.scope.wanted = 1;
    render(&c, SECOND / 4);
    for (x = 0, i = 0; i < DRONE_SCOPE_POINTS; i++)
        x += (engine.scope.in[i] > 0) == (engine.scope.back[i] > 0);
    /* not all of the time: this loop takes from the note and adds to its octave, so what comes back has much of that */
    check("half a cycle late: more of the time it goes down where the other goes up", !engine.scope.wanted && x < (int32_t)DRONE_SCOPE_POINTS * 45 / 100);
    c.voice[0].fb[DRONE_FB_LEVEL] = 0;
    engine.scope.wanted = 1;
    render(&c, SECOND / 4);
    for (x = 0, i = 0; i < DRONE_SCOPE_POINTS; i++)
        x += engine.scope.back[i] != 0;
    check("no feedback: the picture has the sound down the chain, and nothing from the cable", !engine.scope.wanted && x == 0 && engine.scope.in[20] != 0);
    engine.scope.out = 1;                       /* the play page's picture: the voice's sound after its chain */
    engine.scope.wanted = 1;
    render(&c, SECOND / 4);
    for (x = 0, i = 0; i < DRONE_SCOPE_POINTS; i++)
        x += engine.scope.back[i] != 0;
    check("the scope at the voice's end: its sound, and nothing from the cable", !engine.scope.wanted && x == 0 && engine.scope.in[20] != 0);
    c.voice[0].fb[DRONE_FB_LEVEL] = 32;
    engine.scope.wanted = 1;
    render(&c, SECOND / 4);
    for (x = 0, i = 0; i < DRONE_SCOPE_POINTS; i++)
        x += engine.scope.back[i] != 0;
    check("with the feedback on it is still the voice's end: nothing from the cable in the picture", !engine.scope.wanted && x == 0 && engine.scope.in[20] != 0);
    c.voice[0].fb[DRONE_FB_LEVEL] = 0;
    engine.scope.out = 0;
    engine.scope.voice = 1;
    engine.scope.wanted = 1;
    render(&c, SECOND / 4);
    check("a picture wanted of a voice that is silent: none comes", engine.scope.wanted);
    engine.scope.wanted = 0;
    c.voice[0].fb[DRONE_FB_LEVEL] = 32;

    c.voice[0].block[0].type = DRONE_BLOCK_VCA; /* no oscillator: only the loop, at twice round */
    c.voice[0].block[1].param[DRONE_VCA_LEVEL] = 127;
    c.voice[0].fb[DRONE_FB_FROM] = 2;
    c.voice[0].fb[DRONE_FB_TO] = 1;
    c.voice[0].fb[DRONE_FB_LEVEL] = 127;
    drone_init(&engine);
    render(&c, SECOND);
    check("a loop with nothing in it stays silent", silent(SECOND));
    c.voice[0].block[0] = (drone_block_controls_t){ .type = DRONE_BLOCK_VCO, .kind = DRONE_SAW, .param = { 53, 64 } };
    drone_init(&engine);
    render(&c, SECOND);
    check("a loop at twice round does not grow without end", range_ok && peak(SECOND) > 1000000);
    c.voice[0].on = 0;
    render(&c, SECOND);
    render(&c, SECOND);
    check("and the voice off ends it", silent(SECOND));
    c = plain();                              /* again the quiet level before the open filter, the loop round the filter */
    c.voice[0].on = 1;
    c.voice[0].block[1] = c.voice[0].block[2];
    c.voice[0].block[1].param[DRONE_VCA_LEVEL] = 8;
    c.voice[0].block[2] = (drone_block_controls_t){ .type = DRONE_BLOCK_VCF, .kind = DRONE_LADDER, .param = { 127, 0 } };
    c.voice[0].fb[DRONE_FB_FROM] = 2;
    c.voice[0].fb[DRONE_FB_TO] = 2;
    c.voice[0].fb[DRONE_FB_TIME] = 240;
    c.voice[0].lfo[0][DRONE_LFO_SPEED] = 96;
    c.voice[0].fb_amount[0] = 45;               /* no level of its own: the LFO's top half gives it up to nine tenths */
    drone_init(&engine);
    render(&c, SECOND / 4);
    render(&c, SECOND);
    check("an LFO's cable to the feedback's level: the loop comes and goes", swells(SECOND) >= 3 && swells(SECOND) <= 5);
    stereo_same = 1;

    /* ---- the reverb as a block ---- */
    c = plain();                              /* oscillator, filter, level, then the reverb */
    c.voice[0].on = 1;
    drone_init(&engine);
    render(&c, SECOND);
    memcpy(kept, mono, sizeof kept);
    x = peak(SECOND);
    c.voice[0].block[3] = (drone_block_controls_t){ .type = DRONE_BLOCK_FX, .param = { 0, 100, 127 } };
    drone_init(&engine);
    render(&c, SECOND);
    check("an FX block with its mix at 0: the sound as it came", !memcmp(kept, mono, sizeof kept));
    c.voice[0].block[2].param[DRONE_VCA_LEVEL] = 0;         /* the level before it closed: only what the reverb still has */
    render(&c, SECOND / 4);
    render(&c, SECOND / 4);
    check("and no reverb is heard", silent(SECOND / 4));
    c.voice[0].block[2].param[DRONE_VCA_LEVEL] = 127;
    c.voice[0].block[3].param[DRONE_FX_MIX] = 127;
    drone_init(&engine);
    render(&c, SECOND);
    c.voice[0].block[2].param[DRONE_VCA_LEVEL] = 0;
    render(&c, SECOND / 4);
    render(&c, SECOND / 4);
    tail[0] = peak(SECOND / 4);
    check("its mix at the top, the level before it closed: the reverb's tail goes on", tail[0] > x / 100);
    for (i = 0; i < 4; i++)
        render(&c, SECOND);
    check("and dies away", peak(SECOND) < tail[0] / 4);
    check("the reverb in a chain is the same left and right", stereo_same);
    c.voice[0].block[2].param[DRONE_VCA_LEVEL] = 127;
    c.voice[0].block[4] = (drone_block_controls_t){ .type = DRONE_BLOCK_VCF, .kind = DRONE_LADDER, .param = { 45, 0 } };
    drone_init(&engine);
    render(&c, SECOND);
    render(&c, SECOND);
    check("a filter after the reverb: the reverb's highs are gone too", part_at(SECOND, 8 * f3) < eighth[0] / 50);
    c.voice[0].block[4].type = DRONE_BLOCK_NONE;
    c.voice[0].block[2].param[DRONE_VCA_LEVEL] = 0;
    c.voice[0].block[3].param[DRONE_FX_DECAY] = 127;        /* a tail of 30 s */
    c.voice[0].block[2].param[DRONE_VCA_LEVEL] = 127;
    render(&c, SECOND);
    c.voice[0].on = 0;
    for (i = 0; i < 3; i++)
        render(&c, SECOND);
    c.voice[0].block[2].param[DRONE_VCA_LEVEL] = 0;
    c.voice[0].on = 1;
    render(&c, SECOND / 4);
    check("the voice off and on again: nothing of the old tail", silent(SECOND / 4));
    c = plain();                              /* another voice's send while a patch has the reverb */
    c.voice[0].block[3] = (drone_block_controls_t){ .type = DRONE_BLOCK_FX, .param = { 64, 64, 64 } };
    c.voice[2] = plain().voice[0];
    c.voice[2].on = 1;
    c.voice[2].send = c.voice[2].send_reverb = 127;
    c.voice[2].param[DRONE_LEVEL] = 0;
    c.reverb[DRONE_REVERB_RETURN] = 127;
    c.reverb[DRONE_REVERB_DECAY] = 64;
    drone_init(&engine);
    render(&c, SECOND);
    check("a patch has an FX block: the other voices' sends go nowhere", silent(SECOND));
    c.voice[0].block[3].type = DRONE_BLOCK_NONE;
    render(&c, SECOND);
    check("the block taken away: the reverb is the shared one again", peak(SECOND) > 100000);
    c.voice[0].block[3].type = DRONE_BLOCK_FX;
    c.voice[0].block[4] = c.voice[0].block[3];              /* two FX blocks, and a second patch with one */
    c.voice[1] = c.voice[0];
    c.voice[0].on = c.voice[1].on = 1;
    c.voice[2].on = 0;
    drone_init(&engine);
    render(&c, SECOND);
    check("more FX blocks than the one reverb: the first one has it, the sound goes through the others", range_ok && peak(SECOND) > 1000000);
    stereo_same = 1;

    drone_init(&engine);
    c = plain();
    c.voice[0].on = 1;
    for (i = 0; i < 4000; i++) {                /* any blocks in any order, any values, also ones out of range */
        uint32_t r = i * 2654435761u;
        if (i % 5 == 0)
            for (p = 0; p < DRONE_SLOTS; p++) {
                c.voice[0].block[p].type = (uint8_t)((r >> (p * 3)) % (i % 40 == 0 ? 7u : DRONE_NBLOCK));
                c.voice[0].block[p].kind = (uint8_t)(r >> (p + 9) & 7u);
                c.voice[0].block[p].param[0] = (uint8_t)(r >> (p * 2 + 1));
                c.voice[0].block[p].param[1] = (uint8_t)(i % 3 ? r >> (p + 4) : 127u);
                c.voice[0].block[p].param[2] = (uint8_t)(r >> (p + 10));
                c.voice[0].block[p].amount[p & 1][p % DRONE_LFOS] = (int8_t)(r >> (p + 6));       /* also beyond -100 .. 100 */
                c.voice[0].lfo[p % DRONE_LFOS][DRONE_LFO_SPEED] = (uint8_t)(r >> (p + 2));
                c.voice[0].lfo[p % DRONE_LFOS][DRONE_LFO_WAVE] = (uint8_t)(r >> (p + 11) & 7u);
                c.voice[0].out_amount[p & 1][p % DRONE_LFOS] = (int8_t)(r >> (p + 15));
                c.voice[0].lfo[p % DRONE_LFOS][DRONE_LFO_DEPTH] = (uint8_t)(r >> (p + 5));
                c.voice[0].lfo_amount[p % DRONE_LFOS][p & 1][(p + 1) % DRONE_LFOS] = (int8_t)(r >> (p + 12));
                c.voice[0].fb[p % DRONE_FB_NPARAM] = (uint8_t)(i % 7 ? r >> (p + 3) : 255u);
                c.voice[0].fb_amount[p % DRONE_LFOS] = (int8_t)(r >> (p + 17));
            }
        if (i == 2000)                          /* six oscillators at once */
            for (p = 0; p < DRONE_SLOTS; p++)
                c.voice[0].block[p] = (drone_block_controls_t){ .type = DRONE_BLOCK_VCO, .kind = DRONE_SQUARE, .param = { 40, 64 } };
        c.voice[0].on = (r >> 29) != 0;
        engine.scope.voice = 0;
        engine.scope.wanted = 1;
        render(&c, DRONE_BLOCK_FRAMES);
    }
    engine.scope.wanted = 0;
    check("a patch's output is always in the 24-bit range", range_ok);
    stereo_same = 1;

    drone_init(&engine);
    c = plain();
    for (i = 0; i < DRONE_VOICES; i++)
        c.voice[i].send = c.voice[i].send_reverb = 127;
    for (i = 0; i < DRONE_REVERB_NPARAM; i++)   /* the longest decay, all open, every send up: the stress below feeds it */
        c.reverb[i] = 127;
    for (i = 1; i < DRONE_VOICES; i++) {        /* the other voices loud all the time, each at its own note */
        c.voice[i] = c.voice[0];
        PITCH(c, i) = (uint8_t)(53 + 7 * i);
        c.voice[i].on = 1;
    }
    for (i = 0; i < 4000; i++) {                /* the controls jumping, with full resonance: never out of range */
        uint32_t r = i * 2654435761u;
        for (p = 0; p < DRONE_NPARAM; p++)
            c.voice[0].param[p] = (uint8_t)((r >> (p * 4)) % (drone_param_spec[p].top + 1u));
        for (p = 0; p < DRONE_LFOS * DRONE_LFO_NPARAM; p++)
            c.voice[0].lfo[p / DRONE_LFO_NPARAM][p % DRONE_LFO_NPARAM] =
                (uint8_t)(((r ^ r >> 7) >> p) % (drone_lfo_spec[p % DRONE_LFO_NPARAM].top + 1u));
        PITCH(c, 0) = (uint8_t)(r >> 8 & 127u);
        WAVE(c, 0) = (uint8_t)((r >> 12) % DRONE_NWAVE);
        c.voice[0].block[OSC].param[DRONE_VCO_FINE] = (uint8_t)(r >> 16 & 127u);
        CUTOFF(c, 0) = (uint8_t)(r >> 20 & 127u);
        RESONANCE(c, 0) = (uint8_t)(i % 3 == 0 ? 127u : r >> 24 & 127u);
        FILTER_KIND(c, 0) = (uint8_t)(i / 50 % DRONE_NFILTER);
        c.voice[0].on = (r >> 29) != 0;
        render(&c, DRONE_BLOCK_FRAMES);
    }
    check("the output is always in the 24-bit range", range_ok);
    /* ---- V0.16's sound, pinned ---- */
    c = three_on();                             /* three chains, the four filters, the three waves, the shared reverb */
    c.voice[0].block[0] = block_of(DRONE_BLOCK_VCO, DRONE_SAW, 42, 64, 0);
    c.voice[0].block[1] = block_of(DRONE_BLOCK_VCF, DRONE_LADDER, 70, 60, 0);
    c.voice[0].block[2] = block_of(DRONE_BLOCK_VCA, 0, 127, 0, 0);
    c.voice[1].block[0] = block_of(DRONE_BLOCK_VCO, DRONE_SQUARE, 44, 70, 0);
    c.voice[1].block[1] = block_of(DRONE_BLOCK_VCF, DRONE_MS20, 80, 100, 0);
    c.voice[1].block[2] = block_of(DRONE_BLOCK_VCO, DRONE_TRIANGLE, 56, 64, 0);
    c.voice[1].block[3] = block_of(DRONE_BLOCK_VCA, 0, 90, 0, 0);
    c.voice[2].block[0] = block_of(DRONE_BLOCK_VCO, DRONE_TRIANGLE, 46, 60, 0);
    c.voice[2].block[1] = block_of(DRONE_BLOCK_VCF, DRONE_POLIVOKS, 60, 120, 0);
    c.voice[2].block[2] = block_of(DRONE_BLOCK_VCF, DRONE_WASP, 90, 80, 0);
    c.voice[2].block[3] = block_of(DRONE_BLOCK_VCA, 0, 127, 0, 0);
    c.voice[2].send = c.voice[2].send_reverb = 100;
    drone_init(&engine);
    hash = heard(&c, 1, 100, 2166136261u);
    hash = heard(&c, 120, DRONE_BLOCK_FRAMES, hash);
    c.voice[1].on = 0;                          /* one off, to silence; a cutoff and a kind changed while they sound */
    c.voice[0].block[1].param[DRONE_VCF_CUTOFF] = 30;
    c.voice[2].block[2].kind = DRONE_LADDER;
    hash = heard(&c, 120, DRONE_BLOCK_FRAMES, hash);
    check_pinned("three patches", hash, 0x60C97F10u);

    c = three_on();                             /* the feedback cable, its time not a whole number of pieces */
    c.voice[0].block[0] = block_of(DRONE_BLOCK_VCO, DRONE_SAW, 53, 64, 0);
    c.voice[0].block[1] = block_of(DRONE_BLOCK_VCA, 0, 16, 0, 0);
    c.voice[0].block[2] = block_of(DRONE_BLOCK_VCF, DRONE_LADDER, 127, 0, 0);
    c.voice[0].fb[DRONE_FB_FROM] = 2;
    c.voice[0].fb[DRONE_FB_TO] = 2;
    c.voice[0].fb[DRONE_FB_LEVEL] = 40;
    c.voice[0].fb[DRONE_FB_TIME] = 237;
    c.voice[0].fb_amount[0] = 20;
    c.voice[0].lfo[0][DRONE_LFO_SPEED] = 96;
    c.voice[1].block[0] = block_of(DRONE_BLOCK_VCO, DRONE_SQUARE, 60, 64, 0);
    c.voice[1].block[1] = block_of(DRONE_BLOCK_VCF, DRONE_POLIVOKS, 100, 30, 0);
    c.voice[1].fb[DRONE_FB_FROM] = 1;
    c.voice[1].fb[DRONE_FB_TO] = 0;
    c.voice[1].fb[DRONE_FB_LEVEL] = 127;
    c.voice[1].fb[DRONE_FB_TIME] = 5;
    c.voice[2].on = 0;
    drone_init(&engine);
    hash = heard(&c, 150, DRONE_BLOCK_FRAMES, 2166136261u);
    c.voice[0].fb[DRONE_FB_TIME] = 50;
    c.voice[1].fb[DRONE_FB_TO] = 1;
    hash = heard(&c, 100, DRONE_BLOCK_FRAMES, hash);
    check_pinned("feedback", hash, 0xC5CDCA99u);

    c = three_on();                             /* an FX block has the reverb; a second one and another voice's send */
    c.voice[0].block[0] = block_of(DRONE_BLOCK_VCO, DRONE_SAW, 50, 64, 0);
    c.voice[0].block[1] = block_of(DRONE_BLOCK_VCF, DRONE_MS20, 90, 40, 0);
    c.voice[0].block[2] = block_of(DRONE_BLOCK_FX, 0, 80, 100, 90);
    c.voice[0].block[2].amount[DRONE_FX_MIX][0] = 50;
    c.voice[0].block[3] = block_of(DRONE_BLOCK_VCA, 0, 127, 0, 0);
    c.voice[0].lfo[0][DRONE_LFO_SPEED] = 80;
    c.voice[1].block[0] = block_of(DRONE_BLOCK_VCO, DRONE_TRIANGLE, 57, 64, 0);
    c.voice[1].block[1] = block_of(DRONE_BLOCK_FX, 0, 127, 20, 10);
    c.voice[1].send = c.voice[1].send_reverb = 127;
    c.voice[2].block[0] = block_of(DRONE_BLOCK_VCO, DRONE_SQUARE, 30, 64, 0);
    c.voice[2].block[1] = block_of(DRONE_BLOCK_VCF, DRONE_WASP, 70, 127, 0);
    c.voice[2].send = c.voice[2].send_reverb = 127;
    drone_init(&engine);
    hash = heard(&c, 100, DRONE_BLOCK_FRAMES, 2166136261u);
    c.voice[0].on = 0;                          /* the voice with the reverb silent: the reverb is cleared */
    hash = heard(&c, 120, DRONE_BLOCK_FRAMES, hash);
    c.voice[0].on = 1;
    hash = heard(&c, 60, DRONE_BLOCK_FRAMES, hash);
    check_pinned("an FX block", hash, 0xF8AEE13Du);

    c = three_on();                             /* the LFOs' cables: to a block's value, to OUT, to the LFOs */
    c.voice[0].block[0] = block_of(DRONE_BLOCK_VCO, DRONE_SAW, 45, 64, 0);
    c.voice[0].block[0].amount[DRONE_VCO_PITCH][0] = 30;
    c.voice[0].block[1] = block_of(DRONE_BLOCK_VCF, DRONE_LADDER, 80, 70, 0);
    c.voice[0].block[1].amount[DRONE_VCF_CUTOFF][1] = -60;
    c.voice[0].block[1].amount[DRONE_VCF_RESONANCE][2] = 45;
    c.voice[0].block[2] = block_of(DRONE_BLOCK_VCA, 0, 100, 0, 0);
    c.voice[0].block[2].amount[DRONE_VCA_LEVEL][2] = 40;
    c.voice[0].out_amount[DRONE_OUT_LEVEL][0] = 50;
    c.voice[0].out_amount[DRONE_OUT_SEND][1] = -30;
    c.voice[0].lfo_amount[0][DRONE_LFO_TO_SPEED][1] = 50;
    c.voice[0].lfo_amount[1][DRONE_LFO_TO_DEPTH][2] = -70;
    c.voice[0].lfo_amount[2][DRONE_LFO_TO_SPEED][0] = 25;
    c.voice[0].lfo[0][DRONE_LFO_SPEED] = 96;
    c.voice[0].lfo[1][DRONE_LFO_SPEED] = 60;
    c.voice[0].lfo[1][DRONE_LFO_WAVE] = DRONE_LFO_TRIANGLE;
    c.voice[0].lfo[2][DRONE_LFO_SPEED] = 80;
    c.voice[0].lfo[2][DRONE_LFO_WAVE] = DRONE_LFO_SAW;
    c.voice[0].lfo[2][DRONE_LFO_DEPTH] = 90;
    c.voice[1].block[0] = block_of(DRONE_BLOCK_VCO, DRONE_SQUARE, 52, 64, 0);
    c.voice[1].block[0].amount[DRONE_VCO_PITCH][1] = -100;
    c.voice[1].lfo[1][DRONE_LFO_WAVE] = DRONE_LFO_SQUARE;
    c.voice[1].lfo[1][DRONE_LFO_SPEED] = 108;
    c.voice[1].param[DRONE_LEVEL] = 20;
    c.voice[2].on = 0;
    drone_init(&engine);
    hash = heard(&c, 250, DRONE_BLOCK_FRAMES, 2166136261u);
    check_pinned("the LFOs' cables", hash, 0xFD64190Fu);

    /* ---- the end of the chain: the delay ---- */
    for (p = 0; p < 256; p++)
        if (drone_delay_length(p) >= DRONE_DELAY_FRAMES || (p && drone_delay_length(p) < drone_delay_length(p - 1))) {
            printf("FAIL: the delay's length at %u\n", p);
            failed = 1;
        }
    check("the delay: 1 ms to nearly a second", drone_delay_length(0) == 44 && drone_delay_length(255) > SECOND * 99 / 100);

    c = plain();                                /* the echoes alone: the voice's level down, its send up, no reverb back */
    c.voice[0].param[DRONE_LEVEL] = 0;
    c.voice[0].send = c.voice[0].send_reverb = 127;
    c.delay[DRONE_DELAY_TIME] = 128;
    c.delay[DRONE_DELAY_TONE] = 127;
    c.delay[DRONE_DELAY_LEVEL] = 127;
    range_ok = 1;
    x = (int32_t)drone_delay_length(128);       /* 11063 frames: a quarter of a second */
    burst_then(&c, SECOND);
    check("the delay: nothing before its time, then the sound", !peak_in(mono, 0, (uint32_t)x) && peak_in(mono, (uint32_t)x, 64) > 100000);
    check("no repeats: one echo, then silence", !peak_in(mono, (uint32_t)x + BURST + 2048u, SECOND - (uint32_t)x - BURST - 2048u));
    kept[0] = peak_in(mono, (uint32_t)x, BURST);
    c.delay[DRONE_DELAY_REPEATS] = 64;
    burst_then(&c, SECOND);
    kept[1] = peak_in(mono, 2u * (uint32_t)x, BURST);
    check("repeats at half: the second echo is half the first", peak_in(mono, (uint32_t)x, BURST) == kept[0]
          && kept[1] > kept[0] * 45 / 100 && kept[1] < kept[0] * 58 / 100);
    c.delay[DRONE_DELAY_LEVEL] = 64;
    burst_then(&c, SECOND);
    check("the delay's level at half: the echo at half", abs(peak_in(mono, (uint32_t)x, BURST) - kept[0] / 2) < kept[0] / 20);
    c.delay[DRONE_DELAY_LEVEL] = 0;
    burst_then(&c, SECOND);
    check("the delay's level down: no echo", silent(SECOND));

    c.delay[DRONE_DELAY_LEVEL] = 127;           /* its tone: the echoes lose their highs */
    c.delay[DRONE_DELAY_REPEATS] = 0;
    for (p = 0; p < 2; p++) {
        drone_init(&engine);
        c.delay[DRONE_DELAY_TONE] = p ? 127 : 0;
        c.voice[0].on = 1;
        render(&c, SECOND);
        render(&c, SECOND);
        tail[p] = part_at(SECOND, 32 * f3) / part_at(SECOND, f3);        /* 5.6 kHz: the filter is at 0.9 kHz */
    }
    check("the delay's tone down: the echo's highs go", tail[0] < tail[1] / 3);

    c.delay[DRONE_DELAY_TONE] = 127;            /* a new time while it runs: the echo comes at the new time, with no jump */
    drone_init(&engine);
    c.voice[0].on = 0;
    render(&c, DRONE_BLOCK_FRAMES);
    c.delay[DRONE_DELAY_TIME] = 64;
    x = (int32_t)drone_delay_length(64);
    for (i = 0; i < 2; i++)
        render(&c, SECOND);
    c.voice[0].on = 1;
    render_from(&c, 0, BURST);
    c.voice[0].on = 0;
    render_from(&c, BURST, SECOND - BURST);
    check("the delay's time changed: the echo is at the new time", !peak_in(mono, 0, (uint32_t)x) && peak_in(mono, (uint32_t)x, 64) > 100000);

    c.delay[DRONE_DELAY_TIME] = 216;            /* the delay goes into the reverb: 0.7 s after the burst, the reverb sounds again */
    c.reverb[DRONE_REVERB_DECAY] = 0;
    c.reverb[DRONE_REVERB_TONE] = 127;
    c.reverb[DRONE_REVERB_RETURN] = 127;
    x = (int32_t)drone_delay_length(216);
    c.delay[DRONE_DELAY_LEVEL] = 0;
    burst_then(&c, SECOND);
    kept[0] = peak_in(side, (uint32_t)x, BURST);
    c.delay[DRONE_DELAY_LEVEL] = 127;
    burst_then(&c, SECOND);
    check("the echoes go into the reverb", peak_in(side, (uint32_t)x, BURST) > 100000 && peak_in(side, (uint32_t)x, BURST) > 10 * kept[0]);

    c.delay[DRONE_DELAY_TIME] = 128;            /* a patch has the reverb: the sends still go into the delay */
    c.voice[0].block[3] = (drone_block_controls_t){ .type = DRONE_BLOCK_FX, .param = { 0, 64, 64 } };
    x = (int32_t)drone_delay_length(128);
    burst_then(&c, SECOND);
    check("a patch has the reverb: the delay still gets the sends", peak_in(mono, (uint32_t)x, BURST) > 100000);
    c.voice[0].block[3].type = DRONE_BLOCK_NONE;

    c.delay[DRONE_DELAY_REPEATS] = 127;         /* repeats at the top: the echoes go on, and stay in the range */
    c.reverb[DRONE_REVERB_RETURN] = 0;
    burst_then(&c, SECOND);
    for (i = 0; i < 20; i++)
        render(&c, SECOND);
    check("repeats at the top: the echoes do not die", range_ok && peak(SECOND) > 100000);

    /* ---- the end of the chain: the compressor ---- */
    for (p = 1; p < 300000; p += 1 + p / 7) {
        double want = log2((double)p) * 4096.0;
        if (fabs((double)drone_log_q12(p) - want) > 48.0 || fabs((double)drone_exp_q12(drone_log_q12(p)) - (double)p) > p / 100.0 + 1.0) {
            printf("FAIL: the compressor's log or its way back at %u\n", p);
            failed = 1;
        }
    }
    check("the compressor's gain of one is 256, to the bit", drone_exp_q12(8 << 12) == 256);
    for (p = 0; p < 128; p++)
        if (fabs((double)drone_release_ms[p] - 20.0 * pow(100.0, p / 127.0)) > 0.51) {
            printf("FAIL: the release table at %u\n", p);
            failed = 1;
        }

    c = plain();                                /* a loud voice: its peak with no compressor */
    c.voice[0].on = 1;
    c.comp[DRONE_COMP_THRESHOLD] = 95;          /* 12 dB below a full sample: a quarter */
    c.comp[DRONE_COMP_RELEASE] = 127;
    drone_init(&engine);
    render(&c, SECOND);
    render(&c, SECOND);
    memcpy(kept, mono, sizeof kept);
    x = peak(SECOND);
    check("the compressor's ratio at 1: the sound as it is", x > 20000 * 256);
    c.comp[DRONE_COMP_RATIO] = 127;
    drone_init(&engine);
    render(&c, SECOND);
    render(&c, SECOND);
    check("the ratio at the top: the peak stops at the threshold", abs(peak(SECOND) - 8192 * 256) < 8192 * 256 / 20);
    c.comp[DRONE_COMP_RATIO] = 64;              /* 2 to 1: half of what is above the threshold, in dB */
    drone_init(&engine);
    render(&c, SECOND);
    render(&c, SECOND);
    low = 8192.0 * 256.0 * pow((double)x / (8192.0 * 256.0), 1.0 - 64.0 / 127.0);
    check("the ratio at 2 to 1", fabs((double)peak(SECOND) - low) < low / 15.0);
    c.comp[DRONE_COMP_RATIO] = 127;             /* a sound below the threshold: not touched */
    c.voice[0].param[DRONE_LEVEL] = 16;
    drone_init(&engine);
    render(&c, SECOND);
    memcpy(kept, mono, sizeof kept);
    c.comp[DRONE_COMP_RATIO] = 0;
    drone_init(&engine);
    render(&c, SECOND);
    check("below the threshold: the sound as it is, to the bit", peak(SECOND) < 8192 * 256 && !memcmp(kept, mono, sizeof kept));
    x = peak(SECOND);
    c.comp[DRONE_COMP_GAIN] = 64;               /* 12 dB: four times */
    drone_init(&engine);
    render(&c, SECOND);
    check("the compressor's gain: 12 dB is four times", abs(peak(SECOND) - 4 * x) < 4 * x / 20);
    c.comp[DRONE_COMP_GAIN] = 127;
    c.voice[0].param[DRONE_LEVEL] = 127;
    range_ok = 1;
    drone_init(&engine);
    render(&c, SECOND);
    check("all the gain on a loud sound: it stops at a full sample", range_ok && peak(SECOND) == 32767 * 256);

    c = plain();                                /* the release: a loud voice stops, a quiet one comes back up */
    c.voice[1] = c.voice[0];
    c.voice[1].param[DRONE_LEVEL] = 6;
    c.voice[1].on = 1;
    c.comp[DRONE_COMP_THRESHOLD] = 63;          /* 24 dB below a full sample */
    drone_init(&engine);
    render(&c, SECOND);
    x = peak(SECOND);                           /* the quiet one alone: below the threshold */
    c.comp[DRONE_COMP_RATIO] = 127;
    for (p = 0; p < 2; p++) {
        c.comp[DRONE_COMP_RELEASE] = p ? 127 : 0;
        drone_init(&engine);
        c.voice[0].on = 1;
        render(&c, SECOND);
        c.voice[0].on = 0;
        render(&c, SECOND / 4);
        kept[p] = peak_in(mono, SECOND / 8, SECOND / 8);
    }
    check("a short release: the quiet sound is back at once", x < 2068 * 256 && abs(kept[0] - x) < x / 20);
    check("a long release: the quiet sound stays down", kept[1] < x / 4);

    /* ---- the mixer: a voice's pan, its two sends, and the sends from before or after its level ---- */
    {
        drone_controls_t mixed = plain();
        int32_t left = 0, right = 0, centre;
        uint32_t block;
        mixed.voice[0].on = 1;
        drone_init(&engine);
        stereo_same = 1;
        render(&mixed, SECOND / 4);
        centre = peak(SECOND / 4);
        check("the pan in the centre: the same left and right", stereo_same && centre > 1000000);
        for (i = 0; i < 2u; i++) {              /* all to the left, then all to the right */
            mixed.voice[0].pan = i ? 64 : -64;
            drone_init(&engine);
            for (left = right = 0, block = 0; block < 40u; block++) {
                uint32_t f;
                drone_render(&engine, &mixed, out, DRONE_BLOCK_FRAMES);
                for (f = 0; f < DRONE_BLOCK_FRAMES; f++) {
                    left = abs(out[2 * f]) > left ? abs(out[2 * f]) : left;
                    right = abs(out[2 * f + 1]) > right ? abs(out[2 * f + 1]) : right;
                }
            }
            check(i ? "the pan all to the right: nothing at the left, the right as loud as in the centre"
                    : "the pan all to the left: nothing at the right, the left as loud as in the centre",
                  i ? left == 0 && abs(right - centre) < centre / 100 : right == 0 && abs(left - centre) < centre / 100);
        }
        mixed.voice[0].pan = 32;
        drone_init(&engine);
        for (left = right = 0, block = 0; block < 40u; block++) {
            uint32_t f;
            drone_render(&engine, &mixed, out, DRONE_BLOCK_FRAMES);
            for (f = 0; f < DRONE_BLOCK_FRAMES; f++) {
                left = abs(out[2 * f]) > left ? abs(out[2 * f]) : left;
                right = abs(out[2 * f + 1]) > right ? abs(out[2 * f + 1]) : right;
            }
        }
        check("half way to the right: the left about half as loud, the right as it was", left > centre * 2 / 5 && left < centre * 3 / 5
              && abs(right - centre) < centre / 100);
        mixed.voice[0].pan = 0;

        mixed.voice[0].level_log = 1;           /* the level as a fader: a log curve, 50 dB from its top to nearly its bottom */
        drone_init(&engine);
        render(&mixed, SECOND / 4);
        check("the fader at the top: as loud as the level all open", abs(peak(SECOND / 4) - centre) < centre / 100);
        mixed.voice[0].param[DRONE_LEVEL] = 96;
        drone_init(&engine);
        render(&mixed, SECOND / 4);
        check("the fader at 96: 12 dB down, a quarter", peak(SECOND / 4) > centre * 22 / 100 && peak(SECOND / 4) < centre * 28 / 100);
        mixed.voice[0].param[DRONE_LEVEL] = 64;
        drone_init(&engine);
        render(&mixed, SECOND / 4);
        check("the fader in its middle: 25 dB down", peak(SECOND / 4) > centre * 5 / 100 && peak(SECOND / 4) < centre * 7 / 100);
        mixed.voice[0].param[DRONE_LEVEL] = 1;
        drone_init(&engine);
        render(&mixed, SECOND / 4);
        check("the fader one step from the bottom: very quiet, still there", peak(SECOND / 4) > 0 && peak(SECOND / 4) < centre / 200);
        mixed.voice[0].param[DRONE_LEVEL] = 0;
        drone_init(&engine);
        render(&mixed, SECOND / 4);
        check("the fader at the bottom: silent", silent(SECOND / 4));
        mixed.voice[0].param[DRONE_LEVEL] = 127;
        mixed.voice[0].level_log = 0;

        mixed.reverb[DRONE_REVERB_DECAY] = mixed.reverb[DRONE_REVERB_TONE] = 64;
        mixed.reverb[DRONE_REVERB_RETURN] = 127;
        mixed.voice[0].param[DRONE_LEVEL] = 0;  /* the voice not in the mix: what is heard is the effects only */
        mixed.voice[0].send = 127;              /* the delay's send up, its LEVEL at 0, the reverb's send at 0 */
        drone_init(&engine);
        render(&mixed, SECOND / 2);
        check("the delay's send alone, the delay not heard: nothing in the reverb", silent(SECOND / 2));
        mixed.voice[0].send = 0;
        mixed.voice[0].send_reverb = 127;
        drone_init(&engine);
        render(&mixed, SECOND / 2);
        check("the reverb's send: the reverb is heard, with no delay", peak(SECOND / 2) > 100000);
        mixed.voice[0].post = 1;                /* after the level, which is 0 */
        drone_init(&engine);
        render(&mixed, SECOND / 2);
        check("the sends after the level, the level at 0: nothing goes to the reverb", silent(SECOND / 2));
        mixed.voice[0].param[DRONE_LEVEL] = 64;
        drone_init(&engine);
        stereo_same = 1;
        render(&mixed, SECOND / 2);
        check("the level up again: the reverb is back", !stereo_same);
        mixed.voice[0].send_reverb = 0;
        mixed.voice[0].send = 127;
        mixed.delay[DRONE_DELAY_TIME] = 60;
        mixed.delay[DRONE_DELAY_LEVEL] = 127;
        mixed.reverb[DRONE_REVERB_RETURN] = 0;
        mixed.voice[0].param[DRONE_LEVEL] = 0;
        drone_init(&engine);
        render(&mixed, SECOND / 2);
        check("the delay's send after the level, the level at 0: no echo", silent(SECOND / 2));
        mixed.voice[0].post = 0;
        drone_init(&engine);
        render(&mixed, SECOND / 2);
        check("before the level: the echoes are heard", peak(SECOND / 2) > 100000);
        check("the mixer keeps every sample in its range", range_ok);
    }

    /* ---- a picture asked of a silent voice must not stop the next voice's picture (V0.22 lost the wave that way) ---- */
    {
        drone_controls_t two = plain();
        two.voice[0].on = 1;
        drone_init(&engine);
        drone_scope_ask(&engine.scope, 1, 1);
        render(&two, SECOND / 4);
        check("a picture asked of a silent voice: none comes", engine.scope.wanted && engine.scope.voice == 1);
        drone_scope_ask(&engine.scope, 0, 1);   /* the one who plays selects the voice that sounds */
        render(&two, SECOND / 4);
        check("then asked of the voice that sounds: it comes", !engine.scope.wanted && engine.scope.voice == 0 && engine.scope.out
              && engine.scope.in[20] != 0);
        engine.scope.in[20] = 0;
        render(&two, SECOND / 4);
        check("no picture is taken when none is asked", engine.scope.in[20] == 0);
        drone_scope_ask(&engine.scope, 0, 0);
        check("asked for the feedback's page: the picture is of the cable again", engine.scope.wanted && !engine.scope.out);
        engine.scope.wanted = 0;
    }

    /* ---- the link: a voice's filters open with the loudness of the voice before it, in a ring ---- */
    {
        drone_controls_t ring = plain();
        double dark, bright, heard_alone;
        ring.voice[0].on = 1;
        ring.voice[0].param[DRONE_LEVEL] = 0;   /* loud in itself, and not in the mix: only the second voice is heard */
        ring.voice[1] = ring.voice[0];
        ring.voice[1].param[DRONE_LEVEL] = 127;
        PITCH(ring, 1) = 42;                    /* 92.5 Hz, its filter nearly closed: little of its tenth part, at 925 Hz */
        CUTOFF(ring, 1) = 40;
        drone_init(&engine);
        render(&ring, SECOND / 2);
        dark = part_at(SECOND / 2, 925.0);
        ring.voice[1].link = 127;
        drone_init(&engine);
        render(&ring, SECOND / 2);
        bright = part_at(SECOND / 2, 925.0);
        check("the link all open: the first voice's loudness opens the second voice's filter", dark > 0 && bright > 3.0 * dark);
        ring.voice[1].link = 40;
        drone_init(&engine);
        render(&ring, SECOND / 2);
        heard_alone = part_at(SECOND / 2, 925.0);
        check("a link less open: it opens less", heard_alone > dark && heard_alone < bright);
        ring.voice[1].link = 127;
        ring.voice[0].on = 0;
        drone_init(&engine);
        render(&ring, SECOND / 2);
        heard_alone = part_at(SECOND / 2, 925.0);
        check("the voice before it is silent: the link does nothing", fabs(heard_alone - dark) < dark / 50.0);
        ring.voice[0].on = 1;                   /* the first voice sounds, then stops: the second one closes again */
        render(&ring, SECOND / 2);
        ring.voice[0].on = 0;
        render(&ring, SECOND / 2);
        render(&ring, SECOND / 2);
        heard_alone = part_at(SECOND / 2, 925.0);
        check("the voice before it stops: the filter closes again", fabs(heard_alone - dark) < dark / 50.0);
        ring.voice[2] = ring.voice[1];          /* the third voice listens to the second one, not to the first */
        ring.voice[1].on = 0;
        ring.voice[0].on = 1;
        drone_init(&engine);
        render(&ring, SECOND / 2);
        heard_alone = part_at(SECOND / 2, 925.0);
        check("a voice does not hear the voice two before it", fabs(heard_alone - dark) < dark / 50.0);
        ring.voice[0] = ring.voice[2];          /* the ring closes: the first voice listens to the third one */
        ring.voice[2].param[DRONE_LEVEL] = 0;
        ring.voice[2].link = 0;
        CUTOFF(ring, 2) = 127;
        drone_init(&engine);
        render(&ring, SECOND / 2);
        check("the first voice hears the third one", part_at(SECOND / 2, 925.0) > 3.0 * dark);
        check("the link keeps every sample in its range", range_ok);
    }

    /* ---- the link's other kind, THROUGH: the sound of the voice before goes down this voice's chain ---- */
    {
        drone_controls_t ring = plain();        /* the first voice: a saw at F3, 174.6 Hz, loud in itself and not in the mix */
        double none, through, closed;
        uint32_t v;
        ring.voice[0].on = 1;
        ring.voice[0].param[DRONE_LEVEL] = 0;
        ring.voice[1] = ring.voice[0];          /* the second one is heard: its own saw at 92.5 Hz, which has no part at 174.6 Hz */
        ring.voice[1].param[DRONE_LEVEL] = 127;
        PITCH(ring, 1) = 42;
        ring.voice[1].link = 127;               /* the link all open, as FOLLOW: the first voice's sound is not in the second's */
        drone_init(&engine);
        render(&ring, SECOND / 2);
        none = part_at(SECOND / 2, 174.614);
        ring.voice[1].link_kind = DRONE_LINK_THROUGH;
        drone_init(&engine);
        render(&ring, SECOND / 2);
        through = part_at(SECOND / 2, 174.614);
        check("THROUGH: the first voice's sound is heard in the second one", through > 2000.0 && through > 20.0 * none);
        ring.voice[1].link = 30;
        drone_init(&engine);
        render(&ring, SECOND / 2);
        closed = part_at(SECOND / 2, 174.614);
        check("the link less open: less of it", closed > 10.0 * none && closed < through / 2.0);
        ring.voice[1].link = 0;
        drone_init(&engine);
        render(&ring, SECOND / 2);
        check("the link closed: none of it", part_at(SECOND / 2, 174.614) < 2.0 * none + 1.0);
        ring.voice[1].link = 127;
        CUTOFF(ring, 1) = 0;                    /* the second voice's filter at 20 Hz */
        drone_init(&engine);
        render(&ring, SECOND / 2);
        closed = part_at(SECOND / 2, 174.614);
        check("it goes through the second voice's filter: that filter closed takes most of it away", closed < through / 10.0);
        CUTOFF(ring, 1) = 127;
        ring.voice[1].on = 0;
        drone_init(&engine);
        render(&ring, SECOND / 2);
        check("the second voice off: the first one is not heard through it", silent(SECOND / 2));
        ring.voice[1].on = 1;
        ring.voice[0].on = 0;
        drone_init(&engine);
        render(&ring, SECOND / 2);
        check("the first voice off: nothing of it in the second", part_at(SECOND / 2, 174.614) < 2.0 * none + 1.0);
        ring.voice[2] = ring.voice[0];          /* the ring closes: the first voice has the third one's sound, a call late */
        ring.voice[2].on = 1;
        ring.voice[0] = ring.voice[1];
        ring.voice[1].on = 0;
        drone_init(&engine);
        render(&ring, SECOND / 2);
        check("the first voice has the third one's sound", part_at(SECOND / 2, 174.614) > through / 4.0);
        ring.voice[0].link_kind = 200;
        drone_init(&engine);
        render(&ring, SECOND / 2);
        check("a kind that does not exist is FOLLOW", part_at(SECOND / 2, 174.614) < 2.0 * none + 1.0);
        range_ok = 1;
        for (v = 0; v < 3u; v++) {              /* all three through each other, all open, with resonance: loud, and in range */
            ring.voice[v].on = 1;
            ring.voice[v].link = 127;
            ring.voice[v].link_kind = DRONE_LINK_THROUGH;
            ring.voice[v].param[DRONE_LEVEL] = 127;
            RESONANCE(ring, v) = 127;
        }
        drone_init(&engine);
        render(&ring, SECOND);
        check("the whole ring THROUGH, all open: every sample in its range", range_ok);
    }

    /* ---- the link's third kind, FM: the sound of the voice before bends the pitch of this voice's oscillators ---- */
    {
        drone_controls_t ring = plain();        /* the first voice: a triangle at 440 Hz, loud in itself and not in the mix */
        double none, bent, less;
        uint32_t alone, v;
        ring.voice[0].on = 1;
        ring.voice[0].param[DRONE_LEVEL] = 0;
        WAVE(ring, 0) = DRONE_TRIANGLE;
        PITCH(ring, 0) = 69;
        ring.voice[1] = ring.voice[0];          /* the second one is heard: a triangle at 92.5 Hz. Bent 440 times a second, it has a part at 440 - 92.5 Hz */
        ring.voice[1].param[DRONE_LEVEL] = 127;
        PITCH(ring, 1) = 42;
        drone_init(&engine);
        render(&ring, SECOND / 2);
        none = part_at(SECOND / 2, 347.5);
        drone_init(&engine);
        alone = heard(&ring, 40, 256, 2166136261u);
        ring.voice[1].link_kind = DRONE_LINK_FM;
        drone_init(&engine);
        check("FM with the link closed: the same sound to the bit", heard(&ring, 40, 256, 2166136261u) == alone);
        ring.voice[1].link = 127;
        drone_init(&engine);
        render(&ring, SECOND / 2);
        bent = part_at(SECOND / 2, 347.5);
        check("FM: the first voice bends the second one's pitch, which gets new parts", bent > 300.0 && bent > 20.0 * none);
        check("its own sound is not in the second one", part_at(SECOND / 2, 440.0) < bent);
        ring.voice[1].link = 30;
        drone_init(&engine);
        render(&ring, SECOND / 2);
        less = part_at(SECOND / 2, 347.5);
        check("the link less open: it bends less", less > 3.0 * none && less < bent / 2.0);
        ring.voice[1].link = 127;
        ring.voice[0].on = 0;
        drone_init(&engine);
        check("the first voice off: the second one is not bent, to the bit", heard(&ring, 40, 256, 2166136261u) == alone);
        ring.voice[0].on = 1;                   /* a slow voice before: the pitch goes up and down, here from near nothing to near twice */
        PITCH(ring, 0) = 0;
        PITCH(ring, 1) = 69;
        ring.voice[1].link = 0;
        drone_init(&engine);
        render(&ring, SECOND);
        v = cycles(SECOND);
        ring.voice[1].link = 127;
        drone_init(&engine);
        render(&ring, SECOND / 32);             /* 8.2 Hz: a quarter of its cycle is 31 ms, and a triangle starts at its bottom */
        check("a slow voice before, at its low side: this voice's pitch is lower", v > 400u && cycles(SECOND / 32) < v / 32u - v / 128u);
        PITCH(ring, 0) = 69;
        ring.voice[2] = ring.voice[0];          /* the ring closes: the first voice is bent by the third one, a call late */
        ring.voice[0] = ring.voice[1];
        PITCH(ring, 0) = 42;
        ring.voice[1].on = 0;
        drone_init(&engine);
        render(&ring, SECOND / 2);
        check("the first voice is bent by the third one", part_at(SECOND / 2, 347.5) > bent / 2.0);
        range_ok = 1;
        for (v = 0; v < 3u; v++) {              /* all three bend each other, all open, at the top pitch, with the feedback on */
            ring.voice[v].on = 1;
            ring.voice[v].link = 127;
            ring.voice[v].link_kind = DRONE_LINK_FM;
            ring.voice[v].param[DRONE_LEVEL] = 127;
            ring.voice[v].fb[DRONE_FB_FROM] = FILTER;
            ring.voice[v].fb[DRONE_FB_LEVEL] = 100;
            PITCH(ring, v) = 127;
            WAVE(ring, v) = (uint8_t)(v % 3u);
        }
        drone_init(&engine);
        render(&ring, SECOND);
        check("the whole ring FM, all open: every sample in its range", range_ok);
    }

    /* ---- a THROUGH voice's own oscillators can be made quieter, down to nothing: only the other voice's sound is left ---- */
    {
        drone_controls_t ring = plain();        /* as the THROUGH test: a saw at 174.6 Hz into the second voice, whose own saw is at 92.5 Hz */
        double own, other, half;
        uint32_t whole;
        ring.voice[0].on = 1;
        ring.voice[0].param[DRONE_LEVEL] = 0;
        ring.voice[1] = ring.voice[0];
        ring.voice[1].param[DRONE_LEVEL] = 127;
        PITCH(ring, 1) = 42;
        ring.voice[1].link = 127;
        ring.voice[1].link_kind = DRONE_LINK_THROUGH;
        drone_init(&engine);
        render(&ring, SECOND / 2);
        own = part_at(SECOND / 2, 92.5);
        other = part_at(SECOND / 2, 174.614);
        ring.voice[1].own_less = 127;
        drone_init(&engine);
        render(&ring, SECOND / 2);
        check("THROUGH with its own oscillators taken away: its own sound is gone", own > 2000.0 && part_at(SECOND / 2, 92.5) < own / 200.0);
        check("and the other voice's sound is still there", part_at(SECOND / 2, 174.614) > other / 2.0);
        ring.voice[1].own_less = 64;
        drone_init(&engine);
        render(&ring, SECOND / 2);
        half = part_at(SECOND / 2, 92.5);
        check("half taken away: about half of its own sound", half > own / 3.0 && half < own / 1.5);
        ring.voice[1].own_less = 0;
        ring.voice[1].link_kind = DRONE_LINK_FOLLOW;
        drone_init(&engine);
        whole = heard(&ring, 40, 256, 2166136261u);
        ring.voice[1].own_less = 127;
        drone_init(&engine);
        check("with FOLLOW it does nothing: a voice is never silenced by it", heard(&ring, 40, 256, 2166136261u) == whole);
        ring.voice[1].link_kind = DRONE_LINK_FM;
        ring.voice[1].own_less = 0;
        drone_init(&engine);
        whole = heard(&ring, 40, 256, 2166136261u);
        ring.voice[1].own_less = 127;
        drone_init(&engine);
        check("and nothing with FM", heard(&ring, 40, 256, 2166136261u) == whole);
    }

    /* ---- the first disruptor, the noise: a new value at its RATE, in so many BITS, some left out by DUST; each voice takes an
     * amount of it, into its SOUND, its PITCH or its CUTOFF. With no amount anywhere it does not run ---- */
    {
        drone_controls_t noisy;
        uint32_t seed = 1u, zeros = 0, tops = 0, others = 0, alone, changes = 0, runs = 0;
        int32_t before = 0;
        double clean, rough;
        for (i = 0; i < 4000; i++) {
            x = drone_noise_value(&seed, 0, 0);
            tops += x == 32767 || x == -32767;
        }
        check("the noise in 1 bit: two heights only, a full sample up or down", tops == 4000);
        for (i = 0; i < 4000; i++) {
            x = drone_noise_value(&seed, 11, 0);
            others += x != 32767 && x != -32767 && x != 0;
            if (x < -32767 || x > 32767)
                tops = 0;
        }
        check("in 12 bits: many heights, all in a sample's range", others > 3900 && tops == 4000);
        for (i = 0; i < 4000; i++)
            zeros += drone_noise_value(&seed, 11, 127) == 0;
        check("DUST all up: nearly every value is left out, a crack is rare", zeros > 3900 && zeros < 4000);
        for (i = 0, zeros = 0; i < 4000; i++)
            zeros += drone_noise_value(&seed, 0, 64) == 0;
        check("DUST in the middle: about half of them", zeros > 1700 && zeros < 2300);
        check("RATE at the top: a new value every frame; 8 steps down: half as often",
              drone_noise_step(127) == 0xFFFFFFFFu && drone_noise_step(119) == 0x80000000u && drone_noise_step(111) == 0x40000000u
              && drone_noise_step(126) < drone_noise_step(127) && drone_noise_step(120) > drone_noise_step(119) && drone_noise_step(0) > 0);

        noisy = plain();                        /* the voice with no oscillator: a level only, so what is heard is the noise */
        noisy.voice[0].block[OSC].type = DRONE_BLOCK_VCA;
        noisy.voice[0].block[OSC].param[0] = 127;
        noisy.voice[0].block[FILTER].type = noisy.voice[0].block[AMP].type = DRONE_BLOCK_NONE;
        noisy.voice[0].on = 1;
        noisy.dis[DRONE_DIS_NOISE][DRONE_NOISE_RATE] = 127;
        noisy.dis[DRONE_DIS_NOISE][DRONE_NOISE_BITS] = 11;
        drone_init(&engine);
        seed = engine.noise.seed;
        render(&noisy, SECOND / 4);
        check("no amount in any voice: the noise does not run, and nothing is heard", silent(SECOND / 4) && engine.noise.seed == seed);
        noisy.voice[0].dis[DRONE_DIS_NOISE] = 127;
        range_ok = 1;
        render(&noisy, SECOND / 4);
        check("an amount into the voice's sound: the noise is heard, in its range", peak(SECOND / 4) > 4000000 && range_ok && engine.noise.seed != seed);
        for (i = SECOND / 8; i < SECOND / 4; i++)
            changes += mono[i] != mono[i - 1];
        check("at the top RATE it changes every frame", changes > SECOND / 8 - 100);
        noisy.voice[0].dis[DRONE_DIS_NOISE] = 32;              /* quiet enough for the mix's soft top to leave it as it is */
        render(&noisy, SECOND / 4);
        kept[0] = peak_in(mono, SECOND / 8, SECOND / 8);
        noisy.voice[0].dis[DRONE_DIS_NOISE] = 16;
        render(&noisy, SECOND / 4);
        check("half the amount: half as loud", kept[0] > 1000000 && abs(peak_in(mono, SECOND / 8, SECOND / 8) - kept[0] / 2) < kept[0] / 16);
        noisy.voice[0].dis[DRONE_DIS_NOISE] = 127;
        noisy.dis[DRONE_DIS_NOISE][DRONE_NOISE_RATE] = 127 - 40;       /* five octaves down: a new value every 32 frames */
        render(&noisy, SECOND / 4);
        for (i = SECOND / 8, changes = 0; i < SECOND / 4; i++)
            changes += mono[i] != mono[i - 1];
        check("RATE five octaves down: a value is held 32 frames", changes > SECOND / 8 / 32 - 20 && changes <= SECOND / 8 / 32 + 2);
        noisy.dis[DRONE_DIS_NOISE][DRONE_NOISE_RATE] = 127;
        noisy.dis[DRONE_DIS_NOISE][DRONE_NOISE_BITS] = 0;
        render(&noisy, SECOND / 4);
        for (i = SECOND / 8; i < SECOND / 4; i++) {
            runs += abs(abs(mono[i]) - abs(mono[SECOND / 8])) > 2 * 256;        /* the level rounds down: a step apart, up and down */
            before = mono[i];
        }
        check("1 bit: one loudness only, up or down", runs == 0 && abs(before) > 4000000);
        noisy.voice[0].on = 0;
        render(&noisy, SECOND / 4);
        seed = engine.noise.seed;
        render(&noisy, SECOND / 4);
        check("the voice off: no noise from it, and the noise stops", silent(SECOND / 4) && engine.noise.seed == seed);

        noisy = plain();                        /* a saw at 174.6 Hz, its filter half open */
        noisy.voice[0].on = 1;
        noisy.dis[DRONE_DIS_NOISE][DRONE_NOISE_RATE] = 127;
        noisy.dis[DRONE_DIS_NOISE][DRONE_NOISE_BITS] = 11;
        CUTOFF(noisy, 0) = 80;
        drone_init(&engine);
        alone = heard(&noisy, 40, 256, 2166136261u);
        noisy.voice[0].dis_to[DRONE_DIS_NOISE] = DRONE_TO_PITCH;
        drone_init(&engine);
        check("a target with no amount: the same sound to the bit", heard(&noisy, 40, 256, 2166136261u) == alone);
        drone_init(&engine);
        render(&noisy, SECOND / 2);
        clean = part_at(SECOND / 2, 300.0);     /* between the saw's first two parts: nearly nothing there */
        noisy.voice[0].dis[DRONE_DIS_NOISE] = 127;
        range_ok = 1;
        drone_init(&engine);
        render(&noisy, SECOND / 2);
        rough = part_at(SECOND / 2, 300.0);
        check("into the pitch: the oscillator's parts are spread, in its range", rough > 20.0 * clean && rough > 100.0 && range_ok);
        noisy.voice[1] = noisy.voice[0];        /* with an FM link too: the two bend the pitch together */
        noisy.voice[1].link = 127;
        noisy.voice[1].link_kind = DRONE_LINK_FM;
        drone_init(&engine);
        render(&noisy, SECOND / 2);
        check("the noise and an FM link into the same pitch: every sample in its range", range_ok && peak(SECOND / 2) > 100000);
        noisy.voice[1].on = 0;
        noisy.voice[0].dis_to[DRONE_DIS_NOISE] = DRONE_TO_CUTOFF;
        noisy.dis[DRONE_DIS_NOISE][DRONE_NOISE_RATE] = 127 - 64;       /* slow: a new cutoff a few times a second, and the loudness goes with it */
        noisy.dis[DRONE_DIS_NOISE][DRONE_NOISE_BITS] = 2;
        noisy.voice[0].dis[DRONE_DIS_NOISE] = 0;
        drone_init(&engine);
        render(&noisy, SECOND);
        kept[0] = (int32_t)swells(SECOND);
        noisy.voice[0].dis[DRONE_DIS_NOISE] = 127;
        range_ok = 1;
        drone_init(&engine);
        render(&noisy, SECOND);
        check("into the cutoff: the filter opens and closes with the noise's steps", kept[0] <= 1 && swells(SECOND) >= 3 && range_ok);
    }

    /* ---- the second disruptor, the pulse: its RATE, its WIDTH, and SKIP leaves pulses out. It cuts a voice, or lifts its
     * pitch or its cutoff ---- */
    {
        static int16_t pulse[SECOND];
        drone_chop_t chop = { .seed = 2u };
        drone_controls_t chopped;
        uint8_t values[DRONE_DIS_NPARAM] = { [DRONE_CHOP_RATE] = 60, [DRONE_CHOP_WIDTH] = 63, [DRONE_CHOP_SKIP] = 0 };
        uint32_t rises = 0, open = 0, alone, plain_cycles;
        for (i = 0; i + 256u <= SECOND; i += 256u)
            drone_chop(&chop, values, pulse + i, 256);
        for (i = 1; i < SECOND - 256u; i++) {
            rises += pulse[i - 1] < 16384 && pulse[i] >= 16384;
            open += pulse[i] == 32767;
            if (pulse[i] < 0 || abs(pulse[i] - pulse[i - 1]) > DRONE_CHOP_EDGE)
                failed = 1, puts("FAIL: the pulse is below zero, or an edge is a jump");
        }
        check("the pulse at RATE 60: 8 a second (five octaves under pitch 60), open half of the time", rises >= 7 && rises <= 9
              && open > SECOND * 45 / 100 && open < SECOND * 55 / 100);
        check("12 steps up: twice as fast", drone_chop_step(72) > 2 * drone_chop_step(60) - 8 && drone_chop_step(72) < 2 * drone_chop_step(60) + 8);
        values[DRONE_CHOP_WIDTH] = 15;
        for (i = 0, open = 0; i + 256u <= SECOND; i += 256u)
            drone_chop(&chop, values, pulse + i, 256);
        for (i = 0; i < SECOND - 256u; i++)
            open += pulse[i] == 32767;
        check("a narrow WIDTH: open an eighth of the time", open > SECOND * 9 / 100 && open < SECOND * 14 / 100);
        values[DRONE_CHOP_RATE] = 96;           /* 64 a second */
        values[DRONE_CHOP_SKIP] = 64;
        for (i = 0, rises = 0; i + 256u <= SECOND; i += 256u)
            drone_chop(&chop, values, pulse + i, 256);
        for (i = 1; i < SECOND - 256u; i++)
            rises += pulse[i - 1] < 16384 && pulse[i] >= 16384;
        check("SKIP in the middle: about half of the 64 pulses are left out", rises > 18 && rises < 46);

        chopped = plain();                      /* a saw at 174.6 Hz */
        chopped.voice[0].on = 1;
        chopped.dis[DRONE_DIS_CHOP][DRONE_CHOP_RATE] = 60;
        chopped.dis[DRONE_DIS_CHOP][DRONE_CHOP_WIDTH] = 63;
        drone_init(&engine);
        alone = heard(&chopped, 40, 256, 2166136261u);
        chopped.voice[0].dis_to[DRONE_DIS_CHOP] = DRONE_TO_PITCH;
        drone_init(&engine);
        check("the pulse with no amount: the same sound to the bit, and it does not run",
              heard(&chopped, 40, 256, 2166136261u) == alone && engine.chop.phase == 0);
        drone_init(&engine);
        render(&chopped, SECOND);
        plain_cycles = cycles(SECOND);
        chopped.voice[0].dis[DRONE_DIS_CHOP] = 127;
        range_ok = 1;
        drone_init(&engine);
        render(&chopped, SECOND);
        check("into the pitch: up while the pulse is open, so more cycles in the second", cycles(SECOND) > plain_cycles * 13 / 10
              && cycles(SECOND) < plain_cycles * 16 / 10 && range_ok && engine.chop.phase != 0);
        chopped.voice[0].dis_to[DRONE_DIS_CHOP] = DRONE_TO_SOUND;
        drone_init(&engine);
        render(&chopped, SECOND);
        check("into the sound: the voice is cut 8 times a second", swells(SECOND) >= 7 && swells(SECOND) <= 9);
        chopped.voice[0].dis[DRONE_DIS_CHOP] = 40;
        drone_init(&engine);
        render(&chopped, SECOND);
        check("a small amount: the voice only gets a little quieter, it is not cut", swells(SECOND) <= 1);
        chopped.voice[0].dis[DRONE_DIS_CHOP] = 127;
        chopped.voice[0].dis_to[DRONE_DIS_CHOP] = DRONE_TO_CUTOFF;
        CUTOFF(chopped, 0) = 30;
        drone_init(&engine);
        render(&chopped, SECOND);
        check("into the cutoff: the filter opens on each pulse", swells(SECOND) >= 7 && swells(SECOND) <= 9 && range_ok);
    }

    /* ---- the third disruptor, the ring's sine: its FREQ, FOLD bends it over, DRIFT lets it wander. It multiplies a voice's
     * sound, or bends its pitch or its cutoff ---- */
    {
        static int16_t sine[SECOND];
        drone_ring_t ring = { .seed = 3u };
        drone_controls_t rung;
        uint8_t values[DRONE_DIS_NPARAM] = { [DRONE_RING_FREQ] = 57 };
        uint32_t ups = 0, folded_ups = 0, slow = SECOND, fast = 0, alone;
        double none, there, carrier;
        int32_t top = 0;
        for (i = 0; i + 256u <= SECOND; i += 256u)
            drone_ring(&ring, values, sine + i, 256);
        for (i = 1; i < SECOND - 256u; i++) {
            ups += sine[i - 1] < 0 && sine[i] >= 0;
            top = sine[i] > top ? sine[i] : top;
        }
        check("the sine at FREQ 57: 220 Hz, a full sample high", ups >= 218 && ups <= 221 && top > 32000);
        values[DRONE_RING_FOLD] = 127;
        for (i = 0; i + 256u <= SECOND; i += 256u)
            drone_ring(&ring, values, sine + i, 256);
        for (i = 1; i < SECOND - 256u; i++)
            folded_ups += sine[i - 1] < 0 && sine[i] >= 0;
        check("FOLD all up: the sine is folded on itself, three times as many ways up through zero", folded_ups > 3 * ups - 10 && folded_ups < 3 * ups + 10);
        check("a fold stays in a sample's range, and none leaves the sine as it is", drone_ring_fold(32767, 127) <= 32767 && drone_ring_fold(32767, 127) >= -32767
              && drone_ring_fold(-32767, 64) >= -32767 && drone_ring_fold(12345, 0) == 12345 && drone_ring_fold(-32767, 0) == -32767);
        values[DRONE_RING_FOLD] = 0;
        values[DRONE_RING_DRIFT] = 127;
        for (p = 0; p < 40; p++) {              /* a quarter of a second at a time */
            for (i = 0, ups = 0; i + 256u <= SECOND / 4; i += 256u)
                drone_ring(&ring, values, sine + i, 256);
            for (i = 1; i < SECOND / 4 - 256u; i++)
                ups += sine[i - 1] < 0 && sine[i] >= 0;
            slow = ups < slow ? ups : slow;
            fast = ups > fast ? ups : fast;
        }
        check("DRIFT all up: its frequency wanders, below and above its own, not more than an octave", slow < 48 && fast > 62 && slow > 26 && fast < 112);

        rung = plain();                         /* a triangle at 440 Hz */
        rung.voice[0].on = 1;
        WAVE(rung, 0) = DRONE_TRIANGLE;
        PITCH(rung, 0) = 69;
        rung.dis[DRONE_DIS_RING][DRONE_RING_FREQ] = 57;
        drone_init(&engine);
        alone = heard(&rung, 40, 256, 2166136261u);
        rung.voice[0].dis_to[DRONE_DIS_RING] = DRONE_TO_CUTOFF;
        drone_init(&engine);
        check("the sine with no amount: the same sound to the bit, and it does not run",
              heard(&rung, 40, 256, 2166136261u) == alone && engine.ring.phase == 0);
        rung.voice[0].dis_to[DRONE_DIS_RING] = DRONE_TO_SOUND;
        drone_init(&engine);
        render(&rung, SECOND / 2);
        none = part_at(SECOND / 2, 660.0);
        carrier = part_at(SECOND / 2, 440.0);
        rung.voice[0].dis[DRONE_DIS_RING] = 127;
        range_ok = 1;
        drone_init(&engine);
        render(&rung, SECOND / 2);
        there = part_at(SECOND / 2, 660.0);
        check("into the sound: the voice times the sine has the sum and the difference of the two frequencies, and not its own",
              there > 50.0 * none && part_at(SECOND / 2, 220.0) > 50.0 * none && part_at(SECOND / 2, 440.0) < carrier / 20.0 && range_ok);
        rung.voice[0].dis[DRONE_DIS_RING] = 64;
        drone_init(&engine);
        render(&rung, SECOND / 2);
        check("half the amount: half of its own sound is left", part_at(SECOND / 2, 440.0) > carrier / 3.0 && part_at(SECOND / 2, 440.0) < carrier / 1.5);
        rung.voice[0].dis[DRONE_DIS_RING] = 60;
        rung.voice[0].dis_to[DRONE_DIS_RING] = DRONE_TO_PITCH;
        drone_init(&engine);
        render(&rung, SECOND / 2);
        check("into the pitch: bent 220 times a second, it gets new parts", part_at(SECOND / 2, 660.0) > 50.0 * none && range_ok);
        rung.voice[0].dis[DRONE_DIS_NOISE] = rung.voice[0].dis[DRONE_DIS_CHOP] = rung.voice[0].dis[DRONE_DIS_RING] = 127;
        rung.voice[0].dis_to[DRONE_DIS_NOISE] = rung.voice[0].dis_to[DRONE_DIS_CHOP] = DRONE_TO_PITCH;
        rung.dis[DRONE_DIS_NOISE][DRONE_NOISE_RATE] = 127;
        rung.dis[DRONE_DIS_NOISE][DRONE_NOISE_BITS] = 11;
        rung.dis[DRONE_DIS_CHOP][DRONE_CHOP_RATE] = 100;
        rung.dis[DRONE_DIS_CHOP][DRONE_CHOP_WIDTH] = 63;
        rung.voice[1] = rung.voice[0];
        rung.voice[1].link = 127;
        rung.voice[1].link_kind = DRONE_LINK_FM;
        rung.voice[2] = rung.voice[0];
        for (i = 0; i < DRONE_DISRUPTORS; i++)
            rung.voice[2].dis_to[i] = DRONE_TO_SOUND;
        rung.voice[0].dis_to[DRONE_DIS_RING] = DRONE_TO_CUTOFF;
        drone_init(&engine);
        render(&rung, SECOND);
        check("the three disruptors all open into every place, with an FM link: every sample in its range", range_ok && peak(SECOND) > 100000);
    }

    /* ---- the bank: 12 LFOs for the whole instrument, and a clock ---- */
    {
        drone_controls_t bank;
        int32_t most, least, was_height, change, slow_change = 0, fast_change = 0;
        uint32_t n, l, k, up, near, ok, step = 1;
        double got;

        /* the shapes: the pure function the engine and the screen share, against the draft's at WIDTH 0, 50, 100 */
        {
            static const double place[10] = { 0.0, 0.02, 0.13, 0.26, 0.49, 0.51, 0.62, 0.77, 0.97, 0.999 };
            static const uint32_t width[3] = { 0, 50, 100 };
            static const char *const name[DRONE_BANK_NSHAPE] = {
                "GATE", "RATCHET2", "RATCHET4", "SOFT_GATE", "TRIANGLE", "TRAPEZOID", "SINE", "HUMP", "EXP", "SOFT_EXP",
                "LOG", "SOFT_LOG", "RANDOM", "SOFT_RANDOM",
            };
            uint32_t shape, w, at;
            for (shape = 0; shape < DRONE_BANK_NSHAPE; shape++) {
                /* the sine's shapes come from drone_lfo_wave, two parabolas: HUMP within 6 % of a true half sine,
                 * the raised cosines (SINE, SOFT_RANDOM's ease) within 3 % */
                double near_enough = shape == DRONE_BANK_HUMP ? 0.06 : shape == DRONE_BANK_SINE || shape == DRONE_BANK_SOFT_RANDOM ? 0.03 : 0.01;
                for (w = 0, ok = 1; w < 3u; w++)
                    for (at = 0; at < 10u; at++) {
                        int32_t h = drone_bank_shape(shape, AT(place[at]), width[w], 0x3000u, 0xD000u);
                        ok &= fabs(h / 32767.0 - shape_want(shape, place[at], width[w], 0x3000 / 65536.0, 0xD000 / 65536.0)) < near_enough;
                    }
                if (!ok) {
                    printf("FAIL: the shape %s is not the draft's\n", name[shape]);
                    failed = 1;
                }
                for (w = 0, ok = 1; w <= 120u; w += 3u)     /* every WIDTH, and above its top: 0 .. 32767 */
                    for (at = 0; at < 256u; at++) {
                        int32_t h = drone_bank_shape(shape, at << 24 | at, w, at * 257u, 65535u - at * 251u);
                        ok &= h >= 0 && h <= 32767;
                    }
                check("every shape, any WIDTH and any place: a height 0 .. 32767", ok);
            }
            for (at = 0, ok = 1, up = 0; at < 4096u; at++) {
                ok &= drone_bank_shape(DRONE_BANK_GATE, at << 20, 0, 0, 0) == 0 && drone_bank_shape(DRONE_BANK_GATE, at << 20, 100, 0, 0) == 32767;
                up += drone_bank_shape(DRONE_BANK_GATE, at << 20, 25, 0, 0) == 32767;
            }
            check("GATE: WIDTH 0 always down, 100 always up, 25 up for a quarter of the step", ok && up == 1024);
            check("TRIANGLE: WIDTH 0 a ramp down, 100 a ramp up, 50 its top at the half",
                  drone_bank_shape(DRONE_BANK_TRIANGLE, 0, 0, 0, 0) == 32767 && drone_bank_shape(DRONE_BANK_TRIANGLE, 0xFFFFFFFFu, 0, 0, 0) < 10
                  && drone_bank_shape(DRONE_BANK_TRIANGLE, 0, 100, 0, 0) == 0 && drone_bank_shape(DRONE_BANK_TRIANGLE, 0xFFFFFFFFu, 100, 0, 0) > 32750
                  && drone_bank_shape(DRONE_BANK_TRIANGLE, 0x80000000u, 50, 0, 0) == 32767);
            check("SINE: 0 at both ends of the step, 1 at WIDTH", drone_bank_shape(DRONE_BANK_SINE, 0, 50, 0, 0) == 0
                  && drone_bank_shape(DRONE_BANK_SINE, 0x80000000u, 50, 0, 0) == 32767 && drone_bank_shape(DRONE_BANK_SINE, AT(0.2), 20, 0, 0) == 32767
                  && drone_bank_shape(DRONE_BANK_SINE, 0xFFFFFFFFu, 50, 0, 0) < 10);
            check("RANDOM: WIDTH 0 the middle whatever the number, 100 all of its way", drone_bank_shape(DRONE_BANK_RANDOM, 0, 0, 0, 0) == 16384
                  && drone_bank_shape(DRONE_BANK_RANDOM, 0, 0, 65535, 0) == 16384 && drone_bank_shape(DRONE_BANK_RANDOM, AT(0.5), 100, 0, 0) == 0
                  && drone_bank_shape(DRONE_BANK_RANDOM, AT(0.5), 100, 65535, 0) == 32767);
            check("SOFT_RANDOM: from this step's height to the next one's", drone_bank_shape(DRONE_BANK_SOFT_RANDOM, 0, 30, 0x2000u, 0xE000u) == 0x1000
                  && abs(drone_bank_shape(DRONE_BANK_SOFT_RANDOM, 0xFFFFFFFFu, 30, 0x2000u, 0xE000u) - 0x7000) < 8);
        }

        /* the Euclidean pattern */
        {
            uint8_t value[DRONE_BANK_NPARAM];
            uint32_t trigs, randoms, empties, cell, lfo, other_order;
            bank_start(value);
            check("STEPS 0: no pattern", drone_bank_cells(value) == 0 && drone_bank_spec[DRONE_BANK_STEPS].start == 0);
            value[DRONE_BANK_STEPS] = 8;
            value[DRONE_BANK_TRIGS] = 3;
            for (cell = 0, ok = 1; cell < 8u; cell++)
                ok &= drone_bank_cell(0, value, cell) == (cell == 0 || cell == 3 || cell == 6 ? DRONE_BANK_TRIG : DRONE_BANK_EMPTY);
            check("8 steps, 3 trigs: x..x..x.", ok && drone_bank_cells(value) == 8);
            value[DRONE_BANK_SHIFT] = 1;
            for (cell = 0, ok = 1; cell < 8u; cell++)
                ok &= drone_bank_cell(0, value, cell) == (cell == 1 || cell == 4 || cell == 7 ? DRONE_BANK_TRIG : DRONE_BANK_EMPTY);
            value[DRONE_BANK_SHIFT] = 9;
            for (cell = 0; cell < 8u; cell++)
                ok &= drone_bank_cell(0, value, cell) == (cell == 1 || cell == 4 || cell == 7 ? DRONE_BANK_TRIG : DRONE_BANK_EMPTY);
            check("SHIFT 1: the trigs a cell later, .x..x..x; SHIFT 9 the same, round the 8", ok);
            value[DRONE_BANK_SHIFT] = 0;
            value[DRONE_BANK_TRIGS] = 16;
            for (cell = 0, trigs = 0; cell < 8u; cell++)
                trigs += drone_bank_cell(0, value, cell) == DRONE_BANK_TRIG;
            value[DRONE_BANK_TRIGS] = 1;
            for (cell = 0, ok = 1; cell < 8u; cell++)
                ok &= drone_bank_cell(0, value, cell) == (cell == 0 ? DRONE_BANK_TRIG : DRONE_BANK_EMPTY);
            check("TRIGS above STEPS: every cell; TRIGS 1: the first", trigs == 8 && ok);
            value[DRONE_BANK_TRIGS] = 3;
            value[DRONE_BANK_PAD] = 2;
            ok = drone_bank_cells(value) == 10 && drone_bank_cell(0, value, 8) == DRONE_BANK_EMPTY && drone_bank_cell(0, value, 9) == DRONE_BANK_EMPTY
                 && drone_bank_cell(0, value, 6) == DRONE_BANK_TRIG && drone_bank_cell(0, value, 10) == DRONE_BANK_EMPTY;
            check("PAD 2: two empty cells after the 8", ok);
            value[DRONE_BANK_RAND] = 2;
            for (lfo = 0, ok = 1, other_order = 0; lfo < DRONE_BANK_LFOS; lfo++) {
                for (cell = 0, trigs = 0, randoms = 0, empties = 0; cell < 10u; cell++) {
                    uint32_t got = drone_bank_cell(lfo, value, cell);
                    trigs += got == DRONE_BANK_TRIG;
                    randoms += got == DRONE_BANK_RANDOM_TRIG;
                    empties += got == DRONE_BANK_EMPTY;
                    ok &= got == drone_bank_cell(lfo, value, cell);     /* the same each time it is asked */
                    other_order |= lfo && got != drone_bank_cell(0, value, cell);
                }
                ok &= trigs == 3 && randoms == 2 && empties == 5 && drone_bank_cell(lfo, value, 0) == DRONE_BANK_TRIG;
            }
            check("RAND 2: two of the seven empty cells, pads too, are trigs; the Euclidean ones stay", ok);
            check("each LFO has its own order for them", other_order);
            value[DRONE_BANK_RAND] = 8;
            for (cell = 0, randoms = 0; cell < 10u; cell++)
                randoms += drone_bank_cell(5, value, cell) == DRONE_BANK_RANDOM_TRIG;
            check("RAND above the empty cells: all of them", randoms == 7);

            /* which steps play */
            bank_start(value);
            for (cell = 0, ok = 1; cell < 1000u; cell++)
                ok &= drone_bank_plays(0, value, cell, 0);
            check("no pattern, PROB 100, no nap: every step plays", ok && drone_bank_spec[DRONE_BANK_PROB].start == 100);
            value[DRONE_BANK_STEPS] = 8;
            value[DRONE_BANK_TRIGS] = 3;
            for (cell = 0, ok = 1; cell < 24u; cell++)
                ok &= drone_bank_plays(0, value, cell, 0) == (drone_bank_cell(0, value, cell % 8u) != DRONE_BANK_EMPTY);
            check("a pattern: a step plays on a trig, round the pattern", ok);
            value[DRONE_BANK_STEPS] = 0;
            for (k = 0; k < 3u; k++) {
                static const uint8_t prob[3] = { 0, 50, 25 };
                value[DRONE_BANK_PROB] = prob[k];
                for (cell = 0, up = 0; cell < 2000u; cell++)
                    up += (uint32_t)drone_bank_plays(3, value, cell, 0);
                check(k == 0 ? "PROB 0: no step plays" : k == 1 ? "PROB 50: about half of them" : "PROB 25: about a quarter",
                      k == 0 ? up == 0 : k == 1 ? up > 900 && up < 1100 : up > 400 && up < 600);
            }
            value[DRONE_BANK_PROB] = 100;
            value[DRONE_BANK_LOOP] = 4;
            value[DRONE_BANK_NAP] = 2;
            value[DRONE_BANK_WAKE] = 3;
            for (lfo = 0, ok = 1; lfo < 10u; lfo++)     /* lfo: the loop here */
                ok &= drone_bank_plays(0, value, 1, lfo) == (lfo % 5u < 3u);
            check("WAKE 3, NAP 2: three loops play, two rest", ok);
            value[DRONE_BANK_LSHIFT] = 1;
            for (lfo = 0, ok = 1; lfo < 10u; lfo++)
                ok &= drone_bank_plays(0, value, 1, lfo) == ((lfo + 1u) % 5u < 3u);
            check("LSHIFT 1: the rests a loop earlier", ok);
            value[DRONE_BANK_LOOP] = 0;
            for (lfo = 0, ok = 1; lfo < 10u; lfo++)
                ok &= drone_bank_plays(0, value, 1, lfo);
            check("no loop: no nap", ok);

            /* a loop's steps, and the height of a step that does not play */
            bank_start(value);
            value[DRONE_BANK_LOOP] = 3;
            n = 2, l = 7;
            drone_bank_step_after(value, &n, &l);
            k = n == 0 && l == 8;
            n = 1;
            drone_bank_step_after(value, &n, &l);
            check("a free loop of 3 cycles: 3 steps, then the first of the next loop", k && n == 2 && l == 8 && drone_bank_loop_steps(value) == 3);
            value[DRONE_BANK_SYNC] = 1;
            value[DRONE_BANK_RATIO] = 5;                /* 2 beats */
            value[DRONE_BANK_LOOP] = 5;
            k = drone_bank_loop_steps(value) == 3;
            value[DRONE_BANK_RATIO] = 9;                /* x4 */
            check("synced: 5 beats of 2-beat cycles are 3 steps, the last one half; of x4 20", k && drone_bank_loop_steps(value) == 20);
            value[DRONE_BANK_LOOP] = 0;
            n = 4000000000u;
            drone_bank_step_after(value, &n, &l);
            check("no loop: the steps count on", n == 4000000001u && drone_bank_loop_steps(value) == 0);
            bank_start(value);
            value[DRONE_BANK_SHAPE] = DRONE_BANK_GATE;
            value[DRONE_BANK_WIDTH] = 100;
            ok = drone_bank_height(0, value, 5, 0, AT(0.3), 9000) == 32767;
            value[DRONE_BANK_RANGE] = 1;
            ok &= drone_bank_height(0, value, 5, 0, AT(0.3), 9000) == 32767;
            value[DRONE_BANK_WIDTH] = 0;
            ok &= drone_bank_height(0, value, 5, 0, AT(0.3), 9000) == 0;
            value[DRONE_BANK_RANGE] = 0;
            ok &= drone_bank_height(0, value, 5, 0, AT(0.3), 9000) == -32767;
            check("RANGE: both ways, 0 .. 1 is -32767 .. 32767; up only, 0 .. 32767", ok);
            value[DRONE_BANK_PROB] = 0;
            value[DRONE_BANK_WIDTH] = 100;
            ok = drone_bank_height(0, value, 5, 0, AT(0.3), 9000) == 9000;      /* a gate */
            value[DRONE_BANK_SHAPE] = DRONE_BANK_RANDOM;
            ok &= drone_bank_height(0, value, 5, 0, AT(0.3), -9000) == -9000;
            value[DRONE_BANK_SHAPE] = DRONE_BANK_SOFT_RANDOM;
            value[DRONE_BANK_RANGE] = 1;
            ok &= drone_bank_height(0, value, 5, 0, AT(0.3), 9000) == 9000;
            check("a step that does not play: the height held, for every shape: no jump", ok);
            ok = drone_bank_held_after(0, value, 5, 0, 1234) == 1234;
            value[DRONE_BANK_PROB] = 100;
            value[DRONE_BANK_SHAPE] = DRONE_BANK_RANDOM;
            ok &= drone_bank_held_after(4, value, 5, 0, 1234)
                  == drone_bank_shape(DRONE_BANK_RANDOM, 0, 100, drone_bank_random(4, 5, DRONE_BANK_USE_HEIGHT), 0);
            value[DRONE_BANK_RANGE] = 0;
            ok &= drone_bank_held_after(4, value, 5, 0, 1234)
                  == 2 * drone_bank_shape(DRONE_BANK_RANDOM, 0, 100, drone_bank_random(4, 5, DRONE_BANK_USE_HEIGHT), 0) - 32767;
            value[DRONE_BANK_SHAPE] = DRONE_BANK_GATE;      /* all up: its end is the top */
            ok &= drone_bank_held_after(4, value, 5, 0, 1234) == 32767;
            check("the height held after a step: its own at its end (as RANGE has it) if it played, or the one before", ok);

            /* LEVEL, OFFSET and the limit */
            bank_start(value);
            ok = drone_bank_out(32767, value) == 32767 && drone_bank_out(-32767, value) == -32767 && drone_bank_out(1000, value) == 1000;
            value[DRONE_BANK_LEVEL] = 50;
            ok &= drone_bank_out(32767, value) == 16383 && drone_bank_out(-20000, value) == -10000;
            value[DRONE_BANK_LEVEL] = 100;
            value[DRONE_BANK_OFFSET] = 150;
            ok &= drone_bank_out(0, value) == 16383 && drone_bank_out(32767, value) == 32767 && drone_bank_out(-32767, value) == -16384;
            value[DRONE_BANK_OFFSET] = 0;
            ok &= drone_bank_out(0, value) == -32767 && drone_bank_out(-32767, value) == -32767;
            value[DRONE_BANK_LEVEL] = 0;
            value[DRONE_BANK_OFFSET] = 200;
            ok &= drone_bank_out(-32767, value) == 32767;
            check("the output: the height times LEVEL %, OFFSET - 100 % of a full output added, kept in -32767 .. 32767", ok);
        }

        /* random numbers: the same for one LFO, one step and one use, and spread */
        for (n = 0, ok = 1, up = 0, k = 0; n < 4096u; n++) {
            uint32_t r = drone_bank_random(n % 12u, n, DRONE_BANK_USE_HEIGHT);
            ok &= r <= 65535u && r == drone_bank_random(n % 12u, n, DRONE_BANK_USE_HEIGHT);
            up += r >= 32768u;
            k += r != drone_bank_random(n % 12u, n, DRONE_BANK_USE_CHANCE);
        }
        check("a random number: 0 .. 65535, always the same, half of them in the top half, another for another use",
              ok && up > 1900 && up < 2200 && k > 4000);

        /* a free LFO's rate: the voices' LFOs' curve */
        bank = banked();
        drone_init(&engine);
        got = bank_cycles(&bank, 0, (uint32_t)(60.0 * CALLS_A_SECOND));
        check("RATE 40, free: a cycle in 6.25 s (10 in a minute), with all the voices silent", fabs(got - 60.0 / 6.25) < 0.1);
        bank.bank[0][DRONE_BANK_RATE] = 0;
        drone_init(&engine);
        got = bank_cycles(&bank, 0, (uint32_t)(63.0 * CALLS_A_SECOND));
        check("RATE 0: a cycle in 63 s", fabs(got - 1.0) < 0.02);
        bank.bank[0][DRONE_BANK_RATE] = 127;
        drone_init(&engine);
        got = bank_cycles(&bank, 0, (uint32_t)(4.0 * CALLS_A_SECOND));
        check("RATE 127: 24 cycles a second", fabs(got / 4.0 - 24.0) < 0.6);
        check("with no loop a free LFO's step counts its cycles", engine.bank.lfo[0].step == engine.bank.lfo[0].turns
              && engine.bank.lfo[0].step > 90u && engine.bank.lfo[0].loop == 0);

        /* the clock */
        bank = banked();
        drone_init(&engine);
        check("the clock starts at 60 BPM: 10 beats in 10 s", DRONE_TEMPO_START == 60 && fabs(beats_in(&bank, (uint32_t)(10.0 * CALLS_A_SECOND)) - 10.0) < 0.02);
        bank.tempo = 120;
        drone_init(&engine);
        check("120 BPM: 20 beats in 10 s", fabs(beats_in(&bank, (uint32_t)(10.0 * CALLS_A_SECOND)) - 20.0) < 0.04);
        bank.tempo = 5;
        drone_init(&engine);
        got = beats_in(&bank, (uint32_t)(12.0 * CALLS_A_SECOND));
        bank.tempo = 0;
        drone_init(&engine);
        check("a tempo below 10 BPM: 10 BPM", fabs(got - 2.0) < 0.01 && fabs(beats_in(&bank, (uint32_t)(12.0 * CALLS_A_SECOND)) - 2.0) < 0.01
              && DRONE_TEMPO_LOW == 10 && DRONE_TEMPO_HIGH == 300);
        bank.tempo = 65535;
        drone_init(&engine);
        check("above 300 BPM: 300 BPM, 50 beats in 10 s", fabs(beats_in(&bank, (uint32_t)(10.0 * CALLS_A_SECOND)) - 50.0) < 0.1);

        /* a synced LFO is locked to the clock: its phase from the beats, never on its own */
        bank = banked();
        {
            static const uint8_t ratio[8] = { 6, 7, 8, 11, 5, 4, 3, 0 };     /* 1 beat, x2, x3, x16, 2 beats, 3 beats, 4, 32 */
            for (l = 0; l < 8u; l++) {
                bank.bank[l][DRONE_BANK_SYNC] = 1;
                bank.bank[l][DRONE_BANK_RATIO] = ratio[l];
                bank.bank[l][DRONE_BANK_RATE] = (uint8_t)(l * 15u);  /* no effect on a synced LFO */
            }
        }
        bank.bank[8][DRONE_BANK_SYNC] = 1;      /* 1 beat, a quarter of a cycle on */
        bank.bank[8][DRONE_BANK_RATIO] = 6;
        bank.bank[8][DRONE_BANK_PHASE] = 32;
        bank.bank[9][DRONE_BANK_SYNC] = 1;      /* x3 in loops of 7 beats: as x3 with no loop */
        bank.bank[9][DRONE_BANK_RATIO] = 8;
        bank.bank[9][DRONE_BANK_LOOP] = 7;
        bank.tempo = 290;
        bank.tempo_amount[BANK_UP] = 37;        /* the tempo moved on the way: the lock holds */
        bank.bank[BANK_UP][DRONE_BANK_RATE] = 90;
        drone_init(&engine);
        for (n = 0, ok = 1, near = 1; n < 6000u; n++) {
            const drone_bank_lfo_t *lfo = engine.bank.lfo;
            uint32_t clock, beats;
            bank_calls(&bank, 1);
            clock = engine.bank.clock_phase;
            beats = engine.bank.beats;
            ok &= lfo[0].phase == clock && lfo[1].phase == clock * 2u && lfo[2].phase == clock * 3u && lfo[3].phase == clock * 16u;
            ok &= lfo[8].phase == clock + 0x40000000u && lfo[9].phase == lfo[2].phase;
            near &= apart(lfo[4].phase, (beats % 2u) * 0x80000000u + clock / 2u) <= 2u;
            near &= apart(lfo[5].phase, (uint32_t)((beats % 3u) * 1431655765.333 + clock / 3.0)) <= 4u;
            near &= apart(lfo[6].phase, (beats % 4u) * 0x40000000u + clock / 4u) <= 4u;
            near &= apart(lfo[7].phase, (uint32_t)((beats % 32u) * 134217728.0 + clock / 32.0)) <= 32u;
        }
        check("synced at 1 beat, x2, x3, x16: the phase is the clock's times the ratio, to the bit, with the tempo moving",
              ok && engine.bank.beats > 120u);
        check("synced at 2, 3, 4 and 32 beats: the clock's phase and beats over the ratio, a few units of 2^32 apart at most, past 96 beats", near);
        check("PHASE: a synced LFO's place against the clock, PHASE / 128 of a cycle on", ok);
        check("two synced LFOs at x3, one in loops of 7 beats: never apart", ok);

        /* PHASE on a free LFO: turning it moves the LFO by as much, at once; the panel only changes the value */
        bank = banked();
        bank.bank[0][DRONE_BANK_RATE] = 0;
        drone_init(&engine);
        bank_calls(&bank, 3);
        {
            const drone_bank_lfo_t *lfo = &engine.bank.lfo[0];
            uint32_t was_phase = lfo->phase, free_step, jumps;
            bank_calls(&bank, 1);
            free_step = lfo->phase - was_phase;
            was_phase = lfo->phase;
            bank.bank[0][DRONE_BANK_PHASE] = 32;
            bank_calls(&bank, 1);
            jumps = lfo->phase - was_phase == free_step + 0x40000000u;
            was_phase = lfo->phase;
            bank_calls(&bank, 1);
            jumps &= lfo->phase - was_phase == free_step;
            was_phase = lfo->phase;
            bank.bank[0][DRONE_BANK_PHASE] = 0;
            bank_calls(&bank, 1);
            jumps &= lfo->phase - was_phase == free_step - 0x40000000u;
            check("PHASE on a free LFO: a quarter cycle on when it turns 32 up, back when it turns down", jumps);
            bank.bank[0][DRONE_BANK_PHASE] = 50;
            bank.bank[0][DRONE_BANK_SYNC] = 1;
            bank_calls(&bank, 2);
            was_phase = lfo->phase;
            bank.bank[0][DRONE_BANK_SYNC] = 0;
            bank_calls(&bank, 1);
            check("synced, then free again with PHASE as it was: it goes on from where the clock had it", lfo->phase - was_phase == free_step);
        }

        /* no step has played yet: the OFFSET alone; then a step that does not play holds the height before it */
        bank = banked();
        bank.bank[0][DRONE_BANK_PROB] = 0;
        bank.bank[0][DRONE_BANK_OFFSET] = 150;
        bank.bank[0][DRONE_BANK_RATE] = 100;
        bank.bank[1][DRONE_BANK_PROB] = 0;
        bank.bank[1][DRONE_BANK_OFFSET] = 60;
        bank.bank[1][DRONE_BANK_SHAPE] = DRONE_BANK_GATE;
        bank.bank[1][DRONE_BANK_WIDTH] = 100;
        bank.bank[1][DRONE_BANK_RANGE] = 1;
        bank.bank[2][DRONE_BANK_SHAPE] = DRONE_BANK_RANDOM;
        bank.bank[2][DRONE_BANK_WIDTH] = 100;
        bank.bank[2][DRONE_BANK_STEPS] = 2;     /* x.: every other step plays */
        bank.bank[2][DRONE_BANK_TRIGS] = 1;
        bank.bank[2][DRONE_BANK_RATE] = 127;
        bank.bank[3][DRONE_BANK_SHAPE] = DRONE_BANK_GATE;
        bank.bank[3][DRONE_BANK_WIDTH] = 100;
        bank.bank[3][DRONE_BANK_STEPS] = 2;
        bank.bank[3][DRONE_BANK_TRIGS] = 1;
        bank.bank[3][DRONE_BANK_RATE] = 127;
        drone_init(&engine);
        for (n = 0, ok = 1, up = 0, most = -32768, least = 32767; n < 3000u; n++) {
            const drone_bank_lfo_t *lfo = engine.bank.lfo;
            int32_t want;
            bank_calls(&bank, 1);
            ok &= lfo[0].out == 16383 && lfo[1].out == -13106;
            want = 2 * drone_bank_shape(DRONE_BANK_RANDOM, 0, 100, drone_bank_random(2, lfo[2].step & ~1u, DRONE_BANK_USE_HEIGHT), 0) - 32767;
            ok &= lfo[2].height == want;            /* on an odd step: the even one's before it */
            ok &= lfo[3].step & 1u ? lfo[3].height == 32767 : 1;       /* a gate, all up: a skipped step stays up */
            up += lfo[2].step & 1u;
            most = lfo[2].height > most ? lfo[2].height : most;
            least = lfo[2].height < least ? lfo[2].height : least;
        }
        check("no step has played: the OFFSET alone (+50 %; -40 % up only)", engine.bank.lfo[0].out == 16383);
        check("every other step, RANDOM and a GATE: a skipped step holds the height of the one before", ok && up > 500 && most - least > 40000);

        /* a loop: the same steps and the same random heights each time round */
        bank = banked();
        bank.bank[3][DRONE_BANK_SHAPE] = DRONE_BANK_RANDOM;
        bank.bank[3][DRONE_BANK_WIDTH] = 100;
        bank.bank[3][DRONE_BANK_PROB] = 60;
        bank.bank[3][DRONE_BANK_LOOP] = 5;
        bank.bank[3][DRONE_BANK_RATE] = 127;
        drone_init(&engine);
        {
            int32_t seen[4][5];
            uint32_t loops_seen = 0, highest = 0, wrong_loop = 0;
            for (n = 0; n < 4u * 5u; n++)
                seen[n / 5u][n % 5u] = 99999;
            for (n = 0; n < 1000u; n++) {
                const drone_bank_lfo_t *lfo = &engine.bank.lfo[3];
                bank_calls(&bank, 1);
                highest = lfo->step > highest ? lfo->step : highest;
                wrong_loop |= lfo->loop != lfo->turns / 5u;
                if (lfo->loop >= 1u && lfo->loop <= 4u) {
                    int32_t *mine = &seen[lfo->loop - 1u][lfo->step];
                    if (*mine == 99999)
                        *mine = lfo->height;
                    loops_seen = lfo->loop;
                }
            }
            for (n = 0, ok = 1, k = 0; n < 5u; n++) {
                ok &= seen[0][n] == seen[1][n] && seen[1][n] == seen[2][n] && seen[2][n] == seen[3][n];
                k += seen[0][n] != seen[0][(n + 1u) % 5u];
            }
            check("LOOP 5 on a free LFO: steps 0 .. 4, the loop counts the cycles", highest == 4u && !wrong_loop && loops_seen == 4u);
            check("each loop plays the same random heights and the same steps", ok && k >= 2u);
        }

        /* NAP, WAKE, LSHIFT: a synced LFO, up in the first half of each step it plays (it ends down, and a step that
         * does not play holds that), in loops of 2 beats, 1 awake and 1 asleep */
        for (k = 0; k < 2u; k++) {
            bank = banked();
            bank.tempo = 300;
            bank.bank[4][DRONE_BANK_SYNC] = 1;
            bank.bank[4][DRONE_BANK_SHAPE] = DRONE_BANK_GATE;
            bank.bank[4][DRONE_BANK_WIDTH] = 50;
            bank.bank[4][DRONE_BANK_LOOP] = 2;
            bank.bank[4][DRONE_BANK_NAP] = 1;
            bank.bank[4][DRONE_BANK_WAKE] = 0;  /* taken as 1 */
            bank.bank[4][DRONE_BANK_LSHIFT] = (uint8_t)k;
            drone_init(&engine);
            for (n = 0, ok = 1, up = 0; n < 2000u; n++) {
                uint32_t awake;
                bank_calls(&bank, 1);
                awake = (engine.bank.beats / 2u + k) % 2u == 0;
                /* asleep: down, as its last step ended; in the first loop no step has played yet: any height */
                ok &= (awake ? engine.bank.lfo[4].phase >> 28 == 7u || engine.bank.lfo[4].phase >> 28 == 8u
                               || engine.bank.lfo[4].height == (engine.bank.lfo[4].phase >> 31 ? -32767 : 32767)
                             : engine.bank.lfo[4].height == (n < 5u || engine.bank.beats < 2u ? engine.bank.lfo[4].height : -32767)) && engine.bank.lfo[4].loop == engine.bank.beats / 2u;
                up += awake;
            }
            check(k ? "LSHIFT 1: the other loops" : "NAP 1, WAKE 0 (as 1): it plays a loop, then rests a loop", ok && up > 900 && up < 1100);
        }

        /* a synced loop that is not a whole number of cycles: 5 beats of 2-beat cycles */
        bank = banked();
        bank.tempo = 300;
        bank.bank[5][DRONE_BANK_SYNC] = 1;
        bank.bank[5][DRONE_BANK_RATIO] = 5;
        bank.bank[5][DRONE_BANK_LOOP] = 5;
        bank.bank[6][DRONE_BANK_SYNC] = 1;
        bank.bank[6][DRONE_BANK_RATIO] = 5;
        bank.bank[6][DRONE_BANK_LOOP] = 5;
        bank.bank[6][DRONE_BANK_PHASE] = 64;    /* half a cycle on */
        drone_init(&engine);
        for (n = 0, ok = 1, near = 1, up = 0; n < 3000u; n++) {
            const drone_bank_lfo_t *lfo = engine.bank.lfo;
            uint32_t beat, clock;
            bank_calls(&bank, 1);
            beat = engine.bank.beats % 5u;
            clock = engine.bank.clock_phase;
            ok &= lfo[5].step == beat / 2u && lfo[5].loop == engine.bank.beats / 5u && lfo[5].loop_at == (beat << 16) + (clock >> 16);
            near &= apart(lfo[5].phase, (beat % 2u) * 0x7FFFFFFFu + clock / 2u) <= 2u;
            up += beat == 4u && lfo[5].phase >= 0x80000000u;
            /* half a cycle on: the half step is the loop's first, then steps 1 and 2 whole */
            near &= lfo[6].step == (beat + 1u) / 2u && lfo[6].loop == engine.bank.beats / 5u;
        }
        check("LOOP 5 beats of 2-beat cycles: steps 0, 1 and half of 2, then step 0 of the next loop", ok && up == 0 && engine.bank.beats > 60u);
        check("its phase is the clock's, the loop started again at its beat 0; PHASE half a cycle on: the half step is the loop's first", near);

        /* SLEW: how fast the height can move; at 127 a full swing takes a whole cycle */
        bank = banked();
        bank.bank[0][DRONE_BANK_SHAPE] = DRONE_BANK_GATE;
        bank.bank[0][DRONE_BANK_RATE] = 100;
        for (k = 0; k < 3u; k++) {
            static const uint8_t slew[3] = { 0, 32, 127 };
            bank.bank[0][DRONE_BANK_SLEW] = slew[k];
            drone_init(&engine);
            bank_calls(&bank, 400);
            for (n = 0, most = -32767, least = 32767, change = 0, ok = 1; n < 1000u; n++) {
                uint32_t was_phase = engine.bank.lfo[0].phase;
                was_height = engine.bank.lfo[0].height;
                bank_calls(&bank, 1);
                step = engine.bank.lfo[0].phase - was_phase;
                most = engine.bank.lfo[0].height > most ? engine.bank.lfo[0].height : most;
                least = engine.bank.lfo[0].height < least ? engine.bank.lfo[0].height : least;
                change = abs(engine.bank.lfo[0].height - was_height) > change ? abs(engine.bank.lfo[0].height - was_height) : change;
                ok &= abs(engine.bank.lfo[0].height) <= 32767;
            }
            if (k == 0)
                check("SLEW 0: the height is the shape's at once", most == 32767 && least == -32767 && change == 65534);
            else if (k == 1)
                fast_change = change, check("SLEW 32: it gets to the top and the bottom, a little at a time", most == 32767 && least == -32767 && change < 20000);
            else
                slow_change = change, check("SLEW 127: a full swing takes a whole cycle: never from the bottom to the top in a gate's half",
                                            ok && fabs(change * 4294967296.0 / step - 65536.0) < 700 && most - least < 65534);
        }
        check("SLEW 32 moves four times as fast as 127", fast_change > slow_change * 39 / 10 && fast_change < slow_change * 41 / 10);

        /* LEVEL and RANGE in the engine */
        bank = banked();
        bank.bank[0][DRONE_BANK_RATE] = 100;
        bank.bank[0][DRONE_BANK_LEVEL] = 50;
        bank.bank[1][DRONE_BANK_RATE] = 100;
        bank.bank[1][DRONE_BANK_RANGE] = 1;
        bank.bank[2][DRONE_BANK_RATE] = 100;
        bank.bank[2][DRONE_BANK_LEVEL] = 0;
        drone_init(&engine);
        for (n = 0, ok = 1, least = 32767, most = 32767; n < 500u; n++) {
            const drone_bank_lfo_t *lfo = engine.bank.lfo;
            bank_calls(&bank, 1);
            ok &= lfo[0].out == lfo[0].height * 50 / 100 && lfo[2].out == 0 && lfo[1].out == lfo[1].height;
            least = lfo[1].out < least ? lfo[1].out : least;
            most = lfo[0].height < most ? lfo[0].height : most;
        }
        check("LEVEL scales the output, 0 is none; SINE up only: 0 .. 32767, both ways down to -32767",
              ok && least >= 0 && least < 400 && most < -32000);

        /* what the main loop reads: asked, filled by the render, then left alone */
        bank = banked();
        bank.bank[3][DRONE_BANK_SHAPE] = DRONE_BANK_RANDOM;
        bank.bank[3][DRONE_BANK_LOOP] = 3;
        bank.bank[3][DRONE_BANK_RATE] = 127;
        bank.bank[4][DRONE_BANK_SYNC] = 1;
        bank.bank[4][DRONE_BANK_LOOP] = 4;
        bank.bank[5][DRONE_BANK_TRIGS] = 0;
        drone_init(&engine);
        bank_calls(&bank, 50);
        check("the bank's picture is not filled before it is asked", !engine.bank.view.wanted && engine.bank.view.phase[0] == 0);
        drone_bank_ask(&engine.bank.view);
        bank_calls(&bank, 1);
        ok = !engine.bank.view.wanted && engine.bank.view.clock_phase == engine.bank.clock_phase;
        for (l = 0; l < DRONE_BANK_LFOS; l++) {
            const drone_bank_lfo_t *lfo = &engine.bank.lfo[l];
            for (k = 0; k < DRONE_BANK_NPARAM; k++)
                ok &= engine.bank.view.value[l][k] == (l == 5u && k == DRONE_BANK_TRIGS ? 1u : bank.bank[l][k]);
            ok &= engine.bank.view.out[l] == lfo->out && engine.bank.view.height[l] == lfo->height && engine.bank.view.held[l] == lfo->held
                  && engine.bank.view.phase[l] == lfo->phase && engine.bank.view.step[l] == lfo->step && engine.bank.view.loop[l] == lfo->loop
                  && engine.bank.view.loop_at[l] == lfo->loop_at;
        }
        ok &= engine.bank.view.loop[3] > 0 && engine.bank.view.held[3] != 0 && engine.bank.view.loop_at[4] > 0;
        bank_calls(&bank, 3);
        check("asked: the next render fills it with every LFO's values, where it is, and the clock, then leaves it as it is",
              ok && engine.bank.view.phase[0] != engine.bank.lfo[0].phase);
        check("TRIGS 0: the engine takes it as 1", engine.bank.view.value[5][DRONE_BANK_TRIGS] == 1);

        /* the bank's cables onto its own LFOs, from the outputs of the call before */
        bank = banked();
        bank_steady(&bank);
        bank.bank[0][DRONE_BANK_RATE] = 40;
        bank.bank_amount[0][DRONE_BANK_TO_RATE][BANK_UP] = 100;     /* 24 steps up: as RATE 64 */
        bank.bank[1][DRONE_BANK_RATE] = 64;
        bank.bank[2][DRONE_BANK_RATE] = 40;
        bank.bank_amount[2][DRONE_BANK_TO_RATE][BANK_UP] = -100;    /* upside down: as RATE 16 */
        bank.bank[3][DRONE_BANK_RATE] = 16;
        bank.bank[4][DRONE_BANK_SYNC] = 1;
        bank.bank_amount[4][DRONE_BANK_TO_RATE][BANK_UP] = 100;     /* no effect on a synced LFO */
        drone_init(&engine);
        bank_calls(&bank, 2);
        {
            const drone_bank_lfo_t *lfo = engine.bank.lfo;
            uint32_t phase[5];
            for (l = 0; l < 5u; l++)
                phase[l] = lfo[l].phase;
            bank_calls(&bank, 1);
            check("an amount of 100 onto a RATE: 24 steps faster; -100: 24 slower",
                  fabs((double)(lfo[0].phase - phase[0]) / (lfo[1].phase - phase[1]) - 1.0) < 0.001
                  && fabs((double)(lfo[2].phase - phase[2]) / (lfo[3].phase - phase[3]) - 1.0) < 0.001);
            check("no effect on a synced LFO's rate", lfo[4].phase == engine.bank.clock_phase);
        }
        for (k = DRONE_BANK_TO_WIDTH; k < DRONE_BANK_NCABLED; k++) {    /* each of the others, onto LFO 0 */
            static const char *const name[DRONE_BANK_NCABLED] = {
                "RATE", "WIDTH", "LEVEL", "OFFSET", "PROB", "STEPS", "TRIGS", "SHIFT", "LOOP", "NAP", "WAKE", "LSHIFT",
            };
            uint32_t param = drone_bank_cabled_param[k], top = drone_bank_spec[param].top, low = param == DRONE_BANK_TRIGS || param == DRONE_BANK_WAKE;
            uint32_t reach = drone_bank_cable_reach[k], from = param == DRONE_BANK_OFFSET ? 60u : low, moved[4];
            static const int8_t amount[4] = { 100, 50, -100, 0 };
            for (n = 0; n < 4u; n++) {
                bank = banked();
                bank_steady(&bank);
                bank.bank[0][param] = (uint8_t)from;
                bank.bank_amount[0][k][BANK_UP] = amount[n];
                bank.bank_amount[0][k][BANK_DOWN] = (int8_t)(n == 3u ? -50 : 0);    /* from an LFO all down, upside down: up */
                drone_init(&engine);
                bank_calls(&bank, 2);
                drone_bank_ask(&engine.bank.view);
                bank_calls(&bank, 1);
                moved[n] = engine.bank.view.value[0][param];
            }
            ok = moved[0] == (from + reach < top ? from + reach : top) && moved[1] == from + reach / 2u
                 && moved[2] == (param == DRONE_BANK_OFFSET ? 0u : low) && moved[3] == moved[1];
            if (!ok) {
                printf("FAIL: a cable onto %s: %u %u %u %u from %u\n", name[k], moved[0], moved[1], moved[2], moved[3], from);
                failed = 1;
            }
        }
        bank = banked();                        /* a cable onto STEPS: the pattern plays */
        bank_steady(&bank);
        bank.bank[0][DRONE_BANK_SHAPE] = DRONE_BANK_GATE;
        bank.bank[0][DRONE_BANK_WIDTH] = 50;    /* up in the first half of a step that plays; it ends down, and the others hold that */
        bank.bank[0][DRONE_BANK_RATE] = 127;
        bank.bank[0][DRONE_BANK_TRIGS] = 1;
        bank.bank_amount[0][DRONE_BANK_TO_STEPS][BANK_UP] = 25;     /* 4 steps, 1 trig: x... */
        drone_init(&engine);
        for (n = 0, ok = 1, up = 0; n < 1000u; n++) {
            bank_calls(&bank, 1);
            ok &= engine.bank.lfo[0].step % 4u == 0 || engine.bank.lfo[0].height == -32767;
            up += engine.bank.lfo[0].height == 32767;
        }
        check("onto STEPS: the pattern it makes is the one that plays", ok && up > 75 && up < 175);

        /* and onto the tempo: an amount of 100 at full output is twice the tempo, or half */
        bank = banked();
        bank_steady(&bank);
        bank.tempo_amount[BANK_UP] = 100;
        drone_init(&engine);
        bank_calls(&bank, 2);
        check("onto the tempo: 100 is twice", fabs(beats_in(&bank, (uint32_t)(10.0 * CALLS_A_SECOND)) - 20.0) < 0.2);
        bank.tempo_amount[BANK_UP] = -100;
        drone_init(&engine);
        bank_calls(&bank, 2);
        check("-100 is half", fabs(beats_in(&bank, (uint32_t)(10.0 * CALLS_A_SECOND)) - 5.0) < 0.05);
        bank.tempo_amount[BANK_UP] = 50;
        bank.tempo_amount[BANK_DOWN] = -50;
        drone_init(&engine);
        bank_calls(&bank, 2);
        check("two cables add up", fabs(beats_in(&bank, (uint32_t)(10.0 * CALLS_A_SECOND)) - 20.0) < 0.2);

        /* values out of their range: no harm, every output in its range */
        for (n = 0, ok = 1; n < 400u; n++) {
            uint32_t r = n * 2654435761u;
            bank = banked();
            for (l = 0; l < DRONE_BANK_LFOS; l++)
                for (k = 0; k < DRONE_BANK_NPARAM; k++)
                    bank.bank[l][k] = (uint8_t)((r ^ (r >> 9)) >> ((l + k) % 24u));
            for (l = 0; l < DRONE_BANK_LFOS * DRONE_BANK_NCABLED; l++)
                bank.bank_amount[l / DRONE_BANK_NCABLED][l % DRONE_BANK_NCABLED][(r >> 3) % DRONE_BANK_LFOS] = (int8_t)(r >> (l % 24u));
            bank.tempo_amount[r % DRONE_BANK_LFOS] = (int8_t)(r >> 16);
            bank.tempo = (uint16_t)(r >> 8);
            if (n % 40u == 0)
                drone_init(&engine);
            drone_bank_ask(&engine.bank.view);
            bank_calls(&bank, 3);
            for (l = 0; l < DRONE_BANK_LFOS; l++) {
                ok &= abs(engine.bank.lfo[l].out) <= 32767 && abs(engine.bank.lfo[l].height) <= 32767
                      && abs(engine.bank.lfo[l].held) <= 32767;
                for (k = 0; k < DRONE_BANK_NPARAM; k++)
                    ok &= engine.bank.view.value[l][k] <= drone_bank_spec[k].top;
            }
        }
        check("bank values, amounts and a tempo out of their range: every output in its range, every value taken in its range", ok);
        printf("drone_test: the controls are %u bytes, the bank %u\n", (unsigned)sizeof(drone_controls_t), (unsigned)sizeof(drone_bank_t));
        check("the controls, copied each call into a place of the interrupt's own (not on its stack), stay below 4000 bytes",
              sizeof(drone_controls_t) < 4000u);
        {   /* the cables onto the MORE layer's values: PHASE, SLEW, PAD and RAND, each kept in its knob's range */
            uint8_t knob[DRONE_BANK_NPARAM], value[DRONE_BANK_NPARAM];
            int8_t amount[DRONE_BANK_NCABLED][DRONE_BANK_LFOS];
            int32_t was[DRONE_BANK_LFOS] = { 0 };
            memset(amount, 0, sizeof amount);
            for (k = 0; k < DRONE_BANK_NPARAM; k++)
                knob[k] = drone_bank_spec[k].start;
            knob[DRONE_BANK_PHASE] = 20;
            knob[DRONE_BANK_SLEW] = 10;
            knob[DRONE_BANK_PAD] = 2;
            knob[DRONE_BANK_RAND] = 1;
            was[3] = 32767;
            drone_bank_values(value, knob, (const int8_t (*)[DRONE_BANK_LFOS])amount, was);
            check("no cable onto them: as their knobs", value[DRONE_BANK_PHASE] == 20 && value[DRONE_BANK_SLEW] == 10 && value[DRONE_BANK_PAD] == 2
                  && value[DRONE_BANK_RAND] == 1);
            amount[DRONE_BANK_TO_PHASE][3] = 50;
            amount[DRONE_BANK_TO_SLEW][3] = 100;
            amount[DRONE_BANK_TO_PAD][3] = 50;
            amount[DRONE_BANK_TO_RAND][3] = -100;
            drone_bank_values(value, knob, (const int8_t (*)[DRONE_BANK_LFOS])amount, was);
            check("an LFO at its top onto each: the PHASE half its range on, the SLEW to its top and no further, the PAD 4 cells more, the RAND down to none",
                  abs(value[DRONE_BANK_PHASE] - 83) <= 1 && value[DRONE_BANK_SLEW] == 127 && value[DRONE_BANK_PAD] == 6 && value[DRONE_BANK_RAND] == 0
                  && DRONE_BANK_NCABLED == 16);
        }
        bank = plain();                         /* a voice's cables and routes from the bank, out of their range */
        for (l = 0; l < DRONE_BANK_LFOS; l++)
            for (k = 0; k < DRONE_BANK_NPARAM; k++)
                bank.bank[l][k] = drone_bank_spec[k].start;
        bank_steady(&bank);
        bank.voice[0].on = 1;
        bank.voice[0].fb[DRONE_FB_FROM] = bank.voice[0].fb[DRONE_FB_TO] = 255;
        for (n = 0, range_ok = 1; n < 8u; n++) {
            static const uint8_t place[4] = { 7, 255, DRONE_SLOTS, FILTER };
            static const int16_t reach[4] = { 32767, -32768, -32767, 300 };
            for (k = 0; k < DRONE_NBANKED * DRONE_BANK_LFOS; k++)
                bank.voice[0].bank_amount[k / DRONE_BANK_LFOS][k % DRONE_BANK_LFOS] = (int8_t)(n & 1u ? -128 : 127);
            for (k = 0; k < DRONE_BANK_ROUTES; k++)
                bank.voice[0].bank_route[k] = (drone_bank_route_t){ .block = place[(n + k) % 4u], .param = (uint8_t)(k ? 255 : 1),
                                                                    .reach = reach[(n + k) % 4u] };
            drone_init(&engine);
            render(&bank, 8u * DRONE_BLOCK_FRAMES);
        }
        check("a voice's bank amounts at -128 and 127, routes to no block and no value, reaches at the int16 ends: in range", range_ok);

        /* what the bank moves in a voice */
        bank = plain();                         /* a saw at F3, 174.6 Hz */
        bank.voice[0].on = 1;
        for (l = 0; l < DRONE_BANK_LFOS; l++)
            for (k = 0; k < DRONE_BANK_NPARAM; k++)
                bank.bank[l][k] = drone_bank_spec[k].start;
        bank.tempo = DRONE_TEMPO_START;
        bank_steady(&bank);
        bank.voice[0].bank_amount[DRONE_BANKED_PITCH][BANK_UP] = 100;
        drone_init(&engine);
        render(&bank, SECOND / 2);
        check("onto FREQ: an amount of 100 is an octave up", cycles(SECOND / 2) > 172 && cycles(SECOND / 2) < 178);
        bank.voice[0].bank_amount[DRONE_BANKED_PITCH][BANK_UP] = -100;
        drone_init(&engine);
        render(&bank, SECOND / 2);
        check("-100: an octave down", cycles(SECOND / 2) > 42 && cycles(SECOND / 2) < 46);
        bank.voice[0].bank_amount[DRONE_BANKED_PITCH][BANK_UP] = 0;
        bank.voice[0].bank_amount[DRONE_BANKED_PITCH][BANK_DOWN] = -50;
        bank.voice[0].block[3] = bank.voice[0].block[AMP];
        bank.voice[0].block[AMP] = bank.voice[0].block[OSC];      /* two oscillators: they move together */
        bank.voice[0].bank_amount[DRONE_BANKED_PITCH][4] = 50;
        bank.bank[4][DRONE_BANK_LEVEL] = 0;      /* always up */
        bank.bank[4][DRONE_BANK_OFFSET] = 200;
        drone_init(&engine);
        render(&bank, SECOND / 2);
        check("a down LFO upside down and an up one, 50 each: they add up to an octave, every oscillator",
              cycles(SECOND / 2) > 172 && cycles(SECOND / 2) < 178 && part_at(SECOND / 2, 2.0 * f3) > 20.0 * part_at(SECOND / 2, f3));
        bank.voice[0].bank_amount[DRONE_BANKED_PITCH][4] = 30;
        drone_init(&engine);
        render(&bank, SECOND);
        check("between: smoothly, not by semitones (80: 9.6 semitones up)", fabs(cycles(SECOND) - f3 * pow(2.0, 9.6 / 12.0)) < 2.0);

        bank = plain();                         /* a route: an amount of 100 moves the value `reach` steps */
        bank.voice[0].on = 1;
        for (l = 0; l < DRONE_BANK_LFOS; l++)
            for (k = 0; k < DRONE_BANK_NPARAM; k++)
                bank.bank[l][k] = drone_bank_spec[k].start;
        bank_steady(&bank);
        CUTOFF(bank, 0) = 40;
        bank.voice[0].bank_route[0] = (drone_bank_route_t){ .block = FILTER, .param = DRONE_VCF_CUTOFF, .reach = 30 };
        bank.voice[0].bank_amount[DRONE_BANKED_ROUTE][BANK_UP] = 100;
        bank.voice[0].bank_route[1] = (drone_bank_route_t){ .block = FILTER, .param = DRONE_VCF_RESONANCE, .reach = 200 };
        bank.voice[0].bank_amount[DRONE_BANKED_ROUTE + 1][BANK_DOWN] = 100;
        bank.voice[0].block[FILTER].amount[DRONE_VCF_CUTOFF][0] = 100;  /* with the block's own cable, from a still LFO */
        bank.voice[0].lfo[0][DRONE_LFO_DEPTH] = 0;
        RESONANCE(bank, 0) = 100;
        drone_init(&engine);
        bank_calls(&bank, 4);
        got = engine.voice[0].block[FILTER].cutoff.now >> 15;
        x = engine.voice[0].block[FILTER].resonance.now >> 15;
        CUTOFF(bank, 0) = 70;
        RESONANCE(bank, 0) = 0;
        memset(bank.voice[0].bank_amount, 0, sizeof bank.voice[0].bank_amount);
        drone_init(&engine);
        bank_calls(&bank, 4);
        check("a route: 100 moves its value `reach` steps, as the knob would; kept in the value's range",
              fabs(got - (engine.voice[0].block[FILTER].cutoff.now >> 15)) <= 1 && abs(x - (engine.voice[0].block[FILTER].resonance.now >> 15)) <= 1);

        {   /* a voice's limits for what the bank adds: the room its knob has left, down and up */
            int32_t up, down;
            CUTOFF(bank, 0) = 40;
            bank.voice[0].bank_low[DRONE_BANKED_ROUTE] = -5;
            bank.voice[0].bank_high[DRONE_BANKED_ROUTE] = 10;
            bank.voice[0].bank_amount[DRONE_BANKED_ROUTE][BANK_UP] = 100;       /* 30 steps up asked */
            drone_init(&engine);
            bank_calls(&bank, 4);
            up = engine.voice[0].block[FILTER].cutoff.now >> 15;
            bank.voice[0].bank_amount[DRONE_BANKED_ROUTE][BANK_UP] = -100;      /* 30 steps down asked */
            drone_init(&engine);
            bank_calls(&bank, 4);
            down = engine.voice[0].block[FILTER].cutoff.now >> 15;
            memset(bank.voice[0].bank_amount, 0, sizeof bank.voice[0].bank_amount);
            bank.voice[0].bank_low[DRONE_BANKED_ROUTE] = bank.voice[0].bank_high[DRONE_BANKED_ROUTE] = 0;
            CUTOFF(bank, 0) = 50;
            drone_init(&engine);
            bank_calls(&bank, 4);
            check("a route with limits, 10 up and 5 down: the bank moves the value up to them, no further",
                  abs(up - (engine.voice[0].block[FILTER].cutoff.now >> 15)) <= 1);
            CUTOFF(bank, 0) = 35;
            drone_init(&engine);
            bank_calls(&bank, 4);
            check("and down", abs(down - (engine.voice[0].block[FILTER].cutoff.now >> 15)) <= 1);
            CUTOFF(bank, 0) = 70;
        }
        bank.voice[0].fb[DRONE_FB_FROM] = FILTER;   /* a route to the feedback's level: the cable opens with it */
        bank.voice[0].fb[DRONE_FB_TO] = FILTER;
        bank.voice[0].bank_route[2] = (drone_bank_route_t){ .block = DRONE_SLOTS, .reach = 64 };
        bank.voice[0].bank_amount[DRONE_BANKED_ROUTE + 2][BANK_UP] = 50;
        drone_init(&engine);
        bank_calls(&bank, 4);
        check("a route to the feedback cable's level: a level of 0 opens", (engine.voice[0].fb_level.now >> 15) >= 32 * 256 - 260
              && (engine.voice[0].fb_level.now >> 15) <= 32 * 256);

        bank = plain();                         /* LINK: the second voice takes the first one's sound THROUGH, none of its own */
        for (l = 0; l < DRONE_BANK_LFOS; l++)
            for (k = 0; k < DRONE_BANK_NPARAM; k++)
                bank.bank[l][k] = drone_bank_spec[k].start;
        bank_steady(&bank);
        bank.voice[0].on = 1;
        bank.voice[0].param[DRONE_LEVEL] = 0;
        bank.voice[1] = bank.voice[0];
        bank.voice[1].param[DRONE_LEVEL] = 127;
        bank.voice[1].link_kind = DRONE_LINK_THROUGH;
        bank.voice[1].own_less = 127;
        drone_init(&engine);
        render(&bank, SECOND / 4);
        check("the link closed: nothing comes through", peak(SECOND / 4) == 0);
        bank.voice[1].bank_amount[DRONE_BANKED_LINK][BANK_UP] = 100;
        drone_init(&engine);
        render(&bank, SECOND / 4);
        least = peak(SECOND / 4);
        bank.voice[1].bank_amount[DRONE_BANKED_LINK][BANK_UP] = 0;
        bank.voice[1].link = 127;
        drone_init(&engine);
        render(&bank, SECOND / 4);
        check("onto LINK: 100 opens it all the way", least > 1000000 && least == peak(SECOND / 4));
        bank.voice[1].bank_amount[DRONE_BANKED_LINK][BANK_UP] = -100;
        drone_init(&engine);
        render(&bank, SECOND / 4);
        check("-100 closes it, kept in its range", peak(SECOND / 4) == 0);

        bank.voice[1].bank_amount[DRONE_BANKED_LINK][BANK_UP] = 0;     /* OWN: how much of a THROUGH voice's own oscillators is left */
        bank.voice[1].link = 0;
        drone_init(&engine);
        render(&bank, SECOND / 4);
        check("a THROUGH voice with none of its own and the link closed: silent", peak(SECOND / 4) == 0);
        bank.voice[1].bank_amount[DRONE_BANKED_OWN][BANK_UP] = 100;
        drone_init(&engine);
        render(&bank, SECOND / 4);
        least = peak(SECOND / 4);
        bank.voice[1].bank_amount[DRONE_BANKED_OWN][BANK_UP] = 0;
        bank.voice[1].own_less = 0;
        drone_init(&engine);
        render(&bank, SECOND / 4);
        check("onto OWN: 100 gives it all of its own oscillators", least > 1000000 && least == peak(SECOND / 4));
        bank.voice[1].bank_amount[DRONE_BANKED_OWN][BANK_UP] = -100;
        drone_init(&engine);
        render(&bank, SECOND / 4);
        check("-100 takes them all away, kept in its range", peak(SECOND / 4) == 0);
        bank.voice[1].bank_amount[DRONE_BANKED_OWN][BANK_UP] = 0;

        bank.voice[1].param[DRONE_LEVEL] = 0;   /* GAIN: the voice's level */
        drone_init(&engine);
        render(&bank, SECOND / 4);
        check("a voice with its level at 0: silent", peak(SECOND / 4) == 0);
        bank.voice[1].bank_amount[DRONE_BANKED_LEVEL][BANK_UP] = 100;
        drone_init(&engine);
        render(&bank, SECOND / 4);
        least = peak(SECOND / 4);
        bank.voice[1].bank_amount[DRONE_BANKED_LEVEL][BANK_UP] = 0;
        bank.voice[1].param[DRONE_LEVEL] = 127;
        drone_init(&engine);
        render(&bank, SECOND / 4);
        check("onto its level: 100 opens it all the way", least > 1000000 && least == peak(SECOND / 4));
        bank.voice[1].bank_amount[DRONE_BANKED_LEVEL][BANK_UP] = -100;
        drone_init(&engine);
        render(&bank, SECOND / 4);
        check("-100 closes it, kept in its range", peak(SECOND / 4) == 0);

        bank = plain();                         /* MOVE: the depth of the voice's own LFOs */
        for (l = 0; l < DRONE_BANK_LFOS; l++)
            for (k = 0; k < DRONE_BANK_NPARAM; k++)
                bank.bank[l][k] = drone_bank_spec[k].start;
        bank_steady(&bank);
        bank.voice[0].on = 1;
        bank.voice[0].lfo[0][DRONE_LFO_DEPTH] = 0;
        bank.voice[0].lfo[0][DRONE_LFO_SPEED] = 127;
        bank.voice[0].lfo[1][DRONE_LFO_SPEED] = 127;
        bank.voice[0].bank_amount[DRONE_BANKED_LFO_DEPTH][BANK_UP] = 100;
        bank.voice[0].bank_amount[DRONE_BANKED_LFO_DEPTH][BANK_DOWN] = 100;    /* 127 up, 127 down: nothing */
        drone_init(&engine);
        for (n = 0, most = 0; n < 200u; n++) {
            bank_calls(&bank, 1);
            most = abs(engine.voice[0].lfo_was[0]) > most ? abs(engine.voice[0].lfo_was[0]) : most;
        }
        check("onto MOVE: two cables, up and down, add up to nothing", most == 0);
        bank.voice[0].bank_amount[DRONE_BANKED_LFO_DEPTH][BANK_DOWN] = 0;
        drone_init(&engine);
        for (n = 0, most = 0, least = 0; n < 200u; n++) {
            bank_calls(&bank, 1);
            most = abs(engine.voice[0].lfo_was[0]) > most ? abs(engine.voice[0].lfo_was[0]) : most;
            least = abs(engine.voice[0].lfo_was[1]) > least ? abs(engine.voice[0].lfo_was[1]) : least;
        }
        check("100: an LFO at depth 0 goes all the way, one at the top stays there", most > 32000 && least > 32000);
        bank.voice[0].bank_amount[DRONE_BANKED_LFO_DEPTH][BANK_UP] = -100;
        drone_init(&engine);
        for (n = 0, most = 0; n < 200u; n++) {
            bank_calls(&bank, 1);
            most = abs(engine.voice[0].lfo_was[1]) > most ? abs(engine.voice[0].lfo_was[1]) : most;
        }
        check("-100: an LFO at the top is still", most == 0);
        {   /* SPEED: the speed of the voice's own LFOs, 48 steps each way: four times the SPEED knob's reach at an end */
            uint32_t alone, fast, slow, half, both;
            bank.voice[0].bank_amount[DRONE_BANKED_LFO_DEPTH][BANK_UP] = 0;
            bank.voice[0].lfo[0][DRONE_LFO_SPEED] = 60;
#define SPEED_STEP(to) do { \
                uint32_t before; \
                drone_init(&engine); \
                bank_calls(&bank, 10); \
                before = engine.voice[0].lfo_phase[0]; \
                bank_calls(&bank, 1); \
                (to) = engine.voice[0].lfo_phase[0] - before; \
            } while (0)
            SPEED_STEP(alone);
            bank.voice[0].bank_amount[DRONE_BANKED_LFO_SPEED][BANK_UP] = 100;
            SPEED_STEP(fast);
            bank.voice[0].bank_amount[DRONE_BANKED_LFO_SPEED][BANK_UP] = 50;
            SPEED_STEP(half);
            bank.voice[0].bank_amount[DRONE_BANKED_LFO_SPEED][BANK_UP] = -100;
            SPEED_STEP(slow);
            bank.voice[0].bank_amount[DRONE_BANKED_LFO_SPEED][BANK_UP] = 100;
            bank.voice[0].bank_amount[DRONE_BANKED_LFO_SPEED][BANK_DOWN] = 100;
            SPEED_STEP(both);
            check("onto SPEED, 100 and the bank's LFO at its top: the voice's own LFO goes 16 times faster; 50: 4 times, as the knob at its end",
                  alone > 0 && fabs((double)fast / alone - 16.0) < 0.5 && fabs((double)half / alone - 4.0) < 0.15);
            check("-100: 16 times slower", fabs((double)alone / slow - 16.0) < 0.5);
            check("two cables, up and down, add up to nothing: the LFO's own speed, to a thousandth", fabs((double)both / alone - 1.0) < 0.001);
            bank.voice[0].bank_amount[DRONE_BANKED_LFO_SPEED][BANK_DOWN] = 0;
            bank.voice[0].bank_low[DRONE_BANKED_LFO_SPEED] = -24;       /* the voice's limits: what its SPEED knob has left */
            bank.voice[0].bank_high[DRONE_BANKED_LFO_SPEED] = 24;
            SPEED_STEP(fast);
            bank.voice[0].bank_amount[DRONE_BANKED_LFO_SPEED][BANK_UP] = -100;
            SPEED_STEP(slow);
            check("with the voice's limits, 24 steps each way: 100 is 4 times faster, -100 is 4 times slower, no further",
                  fabs((double)fast / alone - 4.0) < 0.15 && fabs((double)alone / slow - 4.0) < 0.15);
            bank.voice[0].bank_amount[DRONE_BANKED_LFO_SPEED][BANK_UP] = 0;
            bank.voice[0].bank_low[DRONE_BANKED_LFO_SPEED] = bank.voice[0].bank_high[DRONE_BANKED_LFO_SPEED] = 0;
#undef SPEED_STEP
        }

        /* amounts 0: the bank running at its start values does not change V0.16's sound */
        c = three_on();
        for (l = 0; l < DRONE_BANK_LFOS; l++)
            for (k = 0; k < DRONE_BANK_NPARAM; k++)
                c.bank[l][k] = drone_bank_spec[k].start;
        bank_steady(&c);
        c.tempo = 200;
        c.tempo_amount[0] = 40;
        c.bank_amount[0][DRONE_BANK_TO_RATE][BANK_UP] = 100;
        c.voice[0].block[0] = block_of(DRONE_BLOCK_VCO, DRONE_SAW, 45, 64, 0);
        c.voice[0].block[0].amount[DRONE_VCO_PITCH][0] = 30;
        c.voice[0].block[1] = block_of(DRONE_BLOCK_VCF, DRONE_LADDER, 80, 70, 0);
        c.voice[0].block[1].amount[DRONE_VCF_CUTOFF][1] = -60;
        c.voice[0].block[1].amount[DRONE_VCF_RESONANCE][2] = 45;
        c.voice[0].block[2] = block_of(DRONE_BLOCK_VCA, 0, 100, 0, 0);
        c.voice[0].block[2].amount[DRONE_VCA_LEVEL][2] = 40;
        c.voice[0].out_amount[DRONE_OUT_LEVEL][0] = 50;
        c.voice[0].out_amount[DRONE_OUT_SEND][1] = -30;
        c.voice[0].lfo_amount[0][DRONE_LFO_TO_SPEED][1] = 50;
        c.voice[0].lfo_amount[1][DRONE_LFO_TO_DEPTH][2] = -70;
        c.voice[0].lfo_amount[2][DRONE_LFO_TO_SPEED][0] = 25;
        c.voice[0].lfo[0][DRONE_LFO_SPEED] = 96;
        c.voice[0].lfo[1][DRONE_LFO_SPEED] = 60;
        c.voice[0].lfo[1][DRONE_LFO_WAVE] = DRONE_LFO_TRIANGLE;
        c.voice[0].lfo[2][DRONE_LFO_SPEED] = 80;
        c.voice[0].lfo[2][DRONE_LFO_WAVE] = DRONE_LFO_SAW;
        c.voice[0].lfo[2][DRONE_LFO_DEPTH] = 90;
        c.voice[0].bank_route[0] = (drone_bank_route_t){ .block = 1, .param = DRONE_VCF_CUTOFF, .reach = 50 };
        c.voice[0].bank_route[1] = (drone_bank_route_t){ .block = DRONE_SLOTS, .reach = 50 };
        c.voice[1].block[0] = block_of(DRONE_BLOCK_VCO, DRONE_SQUARE, 52, 64, 0);
        c.voice[1].block[0].amount[DRONE_VCO_PITCH][1] = -100;
        c.voice[1].lfo[1][DRONE_LFO_WAVE] = DRONE_LFO_SQUARE;
        c.voice[1].lfo[1][DRONE_LFO_SPEED] = 108;
        c.voice[1].param[DRONE_LEVEL] = 20;
        c.voice[2].on = 0;
        drone_init(&engine);
        hash = heard(&c, 250, DRONE_BLOCK_FRAMES, 2166136261u);
        check_pinned("the LFOs' cables, with the bank running, its routes there and its amounts 0", hash, 0xFD64190Fu);
    }

    /* ---- the band at the end of each voice: a low cut, a high cut, both moved by POSITION, four poles each (V0.39) ---- */
    {
        static const double saw_f = 174.6;      /* plain(): a saw at F3, its harmonics up to the top */
        drone_controls_t band = plain();
        double low_part, high_part, mid_part, peak_flat, octave_part;
        int32_t kept_peak;
        band.voice[0].on = 1;
        drone_init(&engine);
        render(&band, SECOND);
        peak_flat = part_at(SECOND, 3 * saw_f);
        low_part = part_at(SECOND, saw_f);
        high_part = part_at(SECOND, 17 * saw_f);        /* 2968 Hz */
        mid_part = part_at(SECOND, 4 * saw_f);          /* 698 Hz */
        octave_part = part_at(SECOND, 12 * saw_f);      /* 2095 Hz */
        memcpy(kept, mono, sizeof kept);

        band.voice[0].band_low = 80;            /* 20 Hz * 1000^(80/127): 1.54 kHz */
        drone_init(&engine);
        render(&band, SECOND);
        check("LOW cuts below it: the saw's first harmonic far down, the one at 3 kHz as it was",
              part_at(SECOND, saw_f) < low_part / 10 && part_at(SECOND, 17 * saw_f) > high_part * 0.7);

        band.voice[0].band_low = 0;
        band.voice[0].band_high_less = 127 - 60;        /* HIGH at 60: 522 Hz */
        drone_init(&engine);
        render(&band, SECOND);
        check("HIGH cuts above it: the harmonic at 3 kHz far down, the first one as it was",
              part_at(SECOND, 17 * saw_f) < high_part / 30 && part_at(SECOND, saw_f) > low_part * 0.7);
        /* nearer the cut the harmonics read higher: the stages after the band are not quite straight and fill them in a little */
        check("four poles: two octaves above the cut, 48 dB down (two poles: 24)", part_at(SECOND, 12 * saw_f) < octave_part / 150
              && part_at(SECOND, 12 * saw_f) > octave_part / 400);
        band.voice[0].band_low = 60;            /* LOW at 60: 522 Hz, 1.6 octaves above the first harmonic */
        band.voice[0].band_high_less = 0;
        drone_init(&engine);
        render(&band, SECOND);
        check("four poles on the low cut too: 1.6 octaves below it, near 38 dB down", part_at(SECOND, saw_f) < low_part / 50
              && part_at(SECOND, 12 * saw_f) > octave_part * 0.85);

        band.voice[0].band_low = 50;            /* 305 Hz .. 870 Hz: the 4th harmonic in, the 17th out */
        band.voice[0].band_high_less = 127 - 70;
        drone_init(&engine);
        render(&band, SECOND);
        check("LOW and HIGH: a band, 305 Hz to 870 Hz; 24 dB an octave out of it", part_at(SECOND, 4 * saw_f) > mid_part * 0.7
              && part_at(SECOND, saw_f) < low_part / 2.5 && part_at(SECOND, 17 * saw_f) < high_part / 6);
        band.voice[0].band_shift = 30;          /* 30 steps up: 1.57 kHz to 4.5 kHz */
        drone_init(&engine);
        render(&band, SECOND);
        check("POSITION moves the band: up, the 17th harmonic in and the 4th out", part_at(SECOND, 17 * saw_f) > high_part * 0.5
              && part_at(SECOND, 4 * saw_f) < mid_part / 4);
        band.voice[0].band_shift = 0;

        drone_init(&engine);
        band.voice[0].band_low = 0;
        band.voice[0].band_high_less = 127 - 62;        /* 576 Hz: the 3rd harmonic near it */
        render(&band, SECOND);
        check("no Q: a cut does not ring, the harmonic near it is no louder than with no band", part_at(SECOND, 3 * saw_f) < peak_flat * 1.05 && range_ok);

        band.voice[0].band_low = 127;           /* LOW stops at HIGH */
        drone_init(&engine);
        render(&band, SECOND);
        memcpy(side, mono, sizeof kept);
        band.voice[0].band_low = 62;
        drone_init(&engine);
        render(&band, SECOND);
        check("LOW goes no higher than HIGH", !memcmp(side, mono, sizeof kept));

        band = plain();                         /* the bank moves the high cut */
        band.voice[0].on = 1;
        bank_steady(&band);
        band.tempo = DRONE_TEMPO_START;
        band.voice[0].bank_amount[DRONE_BANKED_BAND_HIGH][BANK_UP] = -50;      /* half the knob's way down: 64 steps, 610 Hz */
        drone_init(&engine);
        render(&band, SECOND);
        check("onto HIGH: an LFO cuts the highs", part_at(SECOND, 17 * saw_f) < high_part / 30 && part_at(SECOND, saw_f) > low_part * 0.7);
        band.voice[0].bank_amount[DRONE_BANKED_BAND_HIGH][BANK_UP] = 0;
        band.voice[0].bank_amount[DRONE_BANKED_BAND_SHIFT][BANK_UP] = 50;      /* POSITION up 64 steps: the low cut at 652 Hz */
        drone_init(&engine);
        render(&band, SECOND);
        kept_peak = peak(SECOND);
        check("onto POSITION: an LFO moves the open band up, its low cut too", part_at(SECOND, saw_f) < low_part / 8 && kept_peak > 0);

        band = plain();                         /* all open: no filter at all, to the bit */
        band.voice[0].on = 1;
        drone_init(&engine);
        render(&band, SECOND);
        check("the band all open: the sound as it was, to the bit", !memcmp(kept, mono, sizeof kept));
    }

    puts(failed ? "drone_test: FAILED" : "drone_test: ok");
    return failed;
}
