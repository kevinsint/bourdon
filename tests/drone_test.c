/* The voice on this computer: pitch, the three waves and their smooth edges, the four filters, the LFOs, the glide
 * of a changed value, the fade to silence, the output range. */
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
static int32_t mono[SECOND + DRONE_BLOCK_FRAMES], kept[SECOND];

static drone_controls_t plain(void)             /* a saw at F3, the filter open, no resonance, loud, a fast fade */
{
    drone_controls_t c;
    memset(&c, 0, sizeof c);
    c.voice[0].param[DRONE_PITCH] = 53;
    c.voice[0].param[DRONE_FINE] = 64;
    c.voice[0].param[DRONE_CUTOFF] = 127;
    c.voice[0].param[DRONE_LEVEL] = 127;
    c.master = DRONE_MASTER_MAX;
    return c;
}

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
    static drone_voice_t voice;
    uint32_t at;
    memset(&voice, 0, sizeof voice);
    voice.phase_step = drone_pitch_step(pitch, 64);
    for (at = 0; at < SECOND; at += DRONE_BLOCK_FRAMES)
        drone_oscillator(&voice, wave, drone_pitch_step(pitch, 64), mono + at, DRONE_BLOCK_FRAMES);
    return part_at(SECOND, false_hz) / part_at(SECOND, 440.0 * pow(2.0, ((double)pitch - 69.0) / 12.0));
}

static const char *const filter_called[DRONE_NFILTER] = { "ladder", "MS-20", "Polivoks", "Wasp" };
static void check_filter(uint32_t filter, const char *what, int ok)
{
    if (!ok) {
        printf("FAIL: %s: %s\n", filter_called[filter], what);
        failed = 1;
    }
}

int main(void)
{
    drone_controls_t c;
    uint32_t p, i, filter;
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

    c.voice[0].param[DRONE_PITCH] = 41;         /* an octave down */
    render(&c, SECOND);
    check("an octave down is 87 or 88", cycles(SECOND) >= 87 && cycles(SECOND) <= 88);
    c.voice[0].param[DRONE_PITCH] = 53;

    c.voice[0].param[DRONE_WAVE] = DRONE_SQUARE;
    render(&c, SECOND);
    second_square = part_at(SECOND, 2 * f3) / part_at(SECOND, f3);
    third_square = part_at(SECOND, 3 * f3) / part_at(SECOND, f3);
    check("a square has no second harmonic, and its third", second_square < 0.02 && third_square > 0.25);
    c.voice[0].param[DRONE_WAVE] = DRONE_TRIANGLE;
    render(&c, SECOND);
    third_tri = part_at(SECOND, 3 * f3) / part_at(SECOND, f3);
    check("a triangle is the softest", third_tri < 0.15 && third_tri < third_saw && peak(SECOND) > 4000000);
    c.voice[0].param[DRONE_WAVE] = DRONE_SAW;

    c.voice[0].param[DRONE_CUTOFF] = 30;        /* the filter at 80 Hz takes the harmonics away */
    render(&c, SECOND);
    render(&c, SECOND);
    check("the filter low: the third harmonic is far down", part_at(SECOND, 3 * f3) / part_at(SECOND, f3) < third_saw / 4);
    c.voice[0].param[DRONE_CUTOFF] = 0;
    render(&c, SECOND);
    render(&c, SECOND);
    low = peak(SECOND);
    check("the filter at the bottom: nearly nothing left", low < 8388607 / 20);

    c.voice[0].param[DRONE_CUTOFF] = 61;        /* the filter near the third harmonic (335 of 524 Hz), with resonance */
    c.voice[0].param[DRONE_LEVEL] = 16;         /* low, so the peak is not clipped */
    render(&c, SECOND);
    render(&c, SECOND);
    low = part_at(SECOND, 2 * f3);
    c.voice[0].param[DRONE_RESONANCE] = 127;
    render(&c, SECOND);
    render(&c, SECOND);
    peaked = part_at(SECOND, 2 * f3);
    check("resonance lifts what is at the filter's frequency", peaked > 3.0 * low);
    c.voice[0].param[DRONE_RESONANCE] = 0;
    c.voice[0].param[DRONE_CUTOFF] = 127;

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
        c.voice[0].filter = (uint8_t)filter;
        c.voice[0].on = 1;
        render(&c, SECOND);
        check_filter(filter, "open: loud, at the note's pitch",
                     peak(SECOND) > 4000000 && cycles(SECOND) >= 174 && cycles(SECOND) <= 175);
        peaked = part_at(SECOND, 3 * f3) / part_at(SECOND, f3);
        c.voice[0].param[DRONE_CUTOFF] = 30;
        render(&c, SECOND);
        render(&c, SECOND);
        check_filter(filter, "low: the third harmonic is far down", part_at(SECOND, 3 * f3) / part_at(SECOND, f3) < peaked / 4);
        c.voice[0].param[DRONE_CUTOFF] = 0;
        render(&c, SECOND);
        render(&c, SECOND);
        check_filter(filter, "at the bottom: nearly nothing left", peak(SECOND) < 8388607 / 20);

        c.voice[0].param[DRONE_CUTOFF] = 47;    /* at the note: how steep is it eight harmonics up? */
        render(&c, SECOND);
        render(&c, SECOND);
        eighth[filter] = part_at(SECOND, 8 * f3) / part_at(SECOND, f3);

        c.voice[0].param[DRONE_CUTOFF] = 61;
        c.voice[0].param[DRONE_LEVEL] = 16;
        render(&c, SECOND);
        render(&c, SECOND);
        low = part_at(SECOND, 2 * f3);
        c.voice[0].param[DRONE_RESONANCE] = 100;            /* a strong peak; at the top some filters sing their own note */
        render(&c, SECOND);
        render(&c, SECOND);
        check_filter(filter, "resonance lifts what is at the filter's frequency", part_at(SECOND, 2 * f3) > 2.0 * low);
        c.voice[0].param[DRONE_WAVE] = DRONE_TRIANGLE;      /* a triangle has no second harmonic of its own */
        c.voice[0].param[DRONE_CUTOFF] = 47;                /* the peak on the note: the filter works hard */
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
        c.voice[0].filter = (uint8_t)(i / 7 % DRONE_NFILTER);
        c.voice[0].param[DRONE_RESONANCE] = 127;
        render(&c, DRONE_BLOCK_FRAMES);
    }
    render(&c, SECOND);
    check("after many changes of filter it still sounds", peak(SECOND) > 1000000);

    /* the LFOs. A square LFO at its slowest is high for the first half minute */
    drone_init(&engine);
    c = plain();
    c.voice[0].on = 1;
    c.voice[0].lfo[0][DRONE_LFO_WAVE] = DRONE_LFO_SQUARE;
    c.voice[0].lfo[0][DRONE_LFO_TARGET] = DRONE_TO_PITCH;
    render(&c, SECOND);
    check("an LFO with no depth changes nothing", cycles(SECOND) >= 174 && cycles(SECOND) <= 175);
    c.voice[0].lfo[0][DRONE_LFO_DEPTH] = 127;
    render(&c, SECOND);
    check("LFO to pitch, full depth: an octave up", cycles(SECOND) >= 348 && cycles(SECOND) <= 350);
    c.voice[0].lfo[0][DRONE_LFO_DEPTH] = 0;
    c.voice[0].lfo[1][DRONE_LFO_WAVE] = c.voice[0].lfo[2][DRONE_LFO_WAVE] = DRONE_LFO_SQUARE;
    c.voice[0].lfo[1][DRONE_LFO_DEPTH] = c.voice[0].lfo[2][DRONE_LFO_DEPTH] = 32;
    c.voice[0].lfo[1][DRONE_LFO_TARGET] = c.voice[0].lfo[2][DRONE_LFO_TARGET] = DRONE_TO_PITCH;
    render(&c, SECOND);
    check("two LFOs on one target add: 3 and 3 semitones up", cycles(SECOND) >= 246 && cycles(SECOND) <= 248);
    c.voice[0].lfo[1][DRONE_LFO_DEPTH] = c.voice[0].lfo[2][DRONE_LFO_DEPTH] = 0;

    c.voice[0].param[DRONE_CUTOFF] = 30;
    render(&c, SECOND);
    render(&c, SECOND);
    low = part_at(SECOND, 3 * f3) / part_at(SECOND, f3);
    c.voice[0].lfo[2][DRONE_LFO_TARGET] = DRONE_TO_CUTOFF;
    c.voice[0].lfo[2][DRONE_LFO_DEPTH] = 127;
    render(&c, SECOND);
    render(&c, SECOND);
    check("LFO 3 to the cutoff opens the filter", part_at(SECOND, 3 * f3) / part_at(SECOND, f3) > 4.0 * low);
    c.voice[0].lfo[2][DRONE_LFO_DEPTH] = 0;
    c.voice[0].param[DRONE_CUTOFF] = 61;
    c.voice[0].param[DRONE_LEVEL] = 16;
    render(&c, SECOND);
    render(&c, SECOND);
    low = part_at(SECOND, 2 * f3);
    c.voice[0].lfo[0][DRONE_LFO_TARGET] = DRONE_TO_RESONANCE;
    c.voice[0].lfo[0][DRONE_LFO_DEPTH] = 127;
    render(&c, SECOND);
    render(&c, SECOND);
    check("LFO to the resonance lifts the peak", part_at(SECOND, 2 * f3) > 2.0 * low);

    c = plain();                                /* speed 96 is 4.09 Hz: the level swells four times a second */
    c.voice[0].on = 1;
    c.voice[0].param[DRONE_LEVEL] = 64;
    c.voice[0].lfo[1][DRONE_LFO_SPEED] = 96;
    c.voice[0].lfo[1][DRONE_LFO_DEPTH] = 127;
    c.voice[0].lfo[1][DRONE_LFO_TARGET] = DRONE_TO_LEVEL;
    render(&c, SECOND);
    render(&c, SECOND);
    check("LFO to the level at 4 Hz: four swells a second", swells(SECOND) >= 3 && swells(SECOND) <= 5);
    c.voice[0].lfo[1][DRONE_LFO_SPEED] = 108;
    render(&c, SECOND);
    render(&c, SECOND);
    check("an octave faster: eight", swells(SECOND) >= 7 && swells(SECOND) <= 9);

    /* a changed value glides over one block: no step in the sound */
    drone_init(&engine);
    c = plain();
    c.voice[0].on = 1;
    c.voice[0].param[DRONE_WAVE] = DRONE_SQUARE;
    c.voice[0].param[DRONE_PITCH] = 77;         /* 698 Hz: a few cycles in every part of a block */
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
        c.voice[i].param[DRONE_WAVE] = DRONE_TRIANGLE;
        c.voice[i].param[DRONE_LEVEL] = 32;     /* a quarter each: the sum stays below the limit */
    }
    c.voice[1].param[DRONE_PITCH] = 60;         /* C4 */
    c.voice[2].param[DRONE_PITCH] = 67;         /* G4 */
    c.voice[0].on = c.voice[2].on = 1;
    render(&c, SECOND);
    check("three voices: two on, two sound", DRONE_VOICES == 3 && drone_voices_sounding(&engine) == 2);
    check("the first and the third are in the sound, the second is not",
          part_at(SECOND, f3) > 100000 && part_at(SECOND, g4) > 100000 && part_at(SECOND, c4) < 1000);
    c.voice[1].on = 1;
    render(&c, SECOND);
    check("the second on: all three sound", drone_voices_sounding(&engine) == 3 && part_at(SECOND, c4) > 100000);

    /* the sum of the voices is bent near the top, not cut flat: three saws at half level go above a full sample */
    drone_init(&engine);
    c = plain();
    for (i = 0; i < DRONE_VOICES; i++) {
        c.voice[i] = c.voice[0];
        c.voice[i].param[DRONE_PITCH] = (uint8_t)(53 + 2 * i);
        c.voice[i].param[DRONE_LEVEL] = 64;
        c.voice[i].on = 1;
    }
    render(&c, SECOND);
    for (x = 0, i = 0; i < SECOND; i++)
        x += abs(mono[i]) >= 32767 * (int32_t)DRONE_MASTER_MAX;
    check("three voices at half level: loud, and no frame flat at the limit", x == 0 && peak(SECOND) > 6000000);

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
    c.voice[0].send = 127;                      /* a send, RETURN down: the same */
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
    c.voice[1].send = 0;
    c.voice[1].on = 1;
    for (i = 0; i < 25; i++)                    /* the first voice's tail goes */
        render(&c, SECOND);
    stereo_same = 1;
    render(&c, SECOND);
    check("a voice's send is its own: another voice with none stays dry", stereo_same && peak(SECOND) > 4000000);

    for (p = 0; p < 2; p++) {                   /* a short decay and a long one: what is left a second after the voice */
        drone_init(&engine);
        c = plain();
        c.voice[0].on = 1;
        c.voice[0].send = 127;
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
        c.voice[0].send = 127;
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

    drone_init(&engine);
    c = plain();
    for (i = 0; i < DRONE_VOICES; i++)
        c.voice[i].send = 127;
    for (i = 0; i < DRONE_REVERB_NPARAM; i++)   /* the longest decay, all open, every send up: the stress below feeds it */
        c.reverb[i] = 127;
    for (i = 1; i < DRONE_VOICES; i++) {        /* the other voices loud all the time, each at its own note */
        c.voice[i] = c.voice[0];
        c.voice[i].param[DRONE_PITCH] = (uint8_t)(53 + 7 * i);
        c.voice[i].on = 1;
    }
    for (i = 0; i < 4000; i++) {                /* the controls jumping, with full resonance: never out of range */
        uint32_t r = i * 2654435761u;
        for (p = 0; p < DRONE_NPARAM; p++)
            c.voice[0].param[p] = (uint8_t)((r >> (p * 4)) % (drone_param_spec[p].top + 1u));
        for (p = 0; p < DRONE_LFOS * DRONE_LFO_NPARAM; p++)
            c.voice[0].lfo[p / DRONE_LFO_NPARAM][p % DRONE_LFO_NPARAM] =
                (uint8_t)(((r ^ r >> 7) >> p) % (drone_lfo_spec[p % DRONE_LFO_NPARAM].top + 1u));
        c.voice[0].filter = (uint8_t)(i / 50 % DRONE_NFILTER);
        if (i % 3 == 0)
            c.voice[0].param[DRONE_RESONANCE] = 127;
        c.voice[0].on = (r >> 29) != 0;
        render(&c, DRONE_BLOCK_FRAMES);
    }
    check("the output is always in the 24-bit range", range_ok);
    puts(failed ? "drone_test: FAILED" : "drone_test: ok");
    return failed;
}
