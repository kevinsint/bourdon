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
        drone_oscillator(&voice, wave, drone_pitch_step(pitch, 64), mono + at, DRONE_BLOCK_FRAMES);
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
        c.voice[v].send = 32;
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
    c.voice[0].send = 64;
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
    c.voice[2].send = 127;
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
        c.voice[i].send = 127;
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
    c.voice[2].send = 100;
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
    c.voice[1].send = 127;
    c.voice[2].block[0] = block_of(DRONE_BLOCK_VCO, DRONE_SQUARE, 30, 64, 0);
    c.voice[2].block[1] = block_of(DRONE_BLOCK_VCF, DRONE_WASP, 70, 127, 0);
    c.voice[2].send = 127;
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
    c.voice[0].send = 127;
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

    puts(failed ? "drone_test: FAILED" : "drone_test: ok");
    return failed;
}
