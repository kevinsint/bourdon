/* The drone engine on this computer: pitch, the latch, the fade to silence, the output range, the glitch hold. */
#include <math.h>
#include <stdio.h>
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
static int32_t mono[4u * SECOND];

static drone_controls_t plain(void)             /* one clean tone: no detune, noise, movement or glitch; a fast fade */
{
    drone_controls_t c;
    memset(&c, 0, sizeof c);
    c.param[DRONE_CUTOFF] = 127;
    c.param[DRONE_FADE] = 0;
    c.master = DRONE_MASTER_MAX;
    return c;
}

/* render n frames into mono[]; every frame is checked: in range, and the same left and right */
static int range_ok, stereo_same;
static void render(const drone_controls_t *c, uint32_t n)
{
    uint32_t at, i;
    for (at = 0; at < n; at += DRONE_BLOCK_FRAMES) {
        drone_render(&engine, c, out, DRONE_BLOCK_FRAMES);
        for (i = 0; i < DRONE_BLOCK_FRAMES; i++) {
            if (out[2 * i] < -8388608 || out[2 * i] > 8388607)
                range_ok = 0;
            if (out[2 * i] != out[2 * i + 1])
                stereo_same = 0;
            if (at + i < n)
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
static uint32_t cycles(uint32_t n)              /* a saw has one jump down a cycle */
{
    uint32_t i, jumps = 0;
    for (i = 1; i < n; i++)
        if (mono[i - 1] - mono[i] > 2000000)
            jumps++;
    return jumps;
}
static uint32_t longest_run_min(uint32_t n)     /* the shortest run of equal samples */
{
    uint32_t i, run = 1, shortest = n, whole = 0;   /* the first run starts before mono[]: not counted */
    for (i = 1; i < n; i++) {
        if (mono[i] == mono[i - 1]) {
            run++;
        } else {
            if (whole && run < shortest)
                shortest = run;
            whole = 1;
            run = 1;
        }
    }
    return shortest;
}

int main(void)
{
    drone_controls_t c;
    uint32_t n, i;
    range_ok = stereo_same = 1;

    for (n = 0; n < 12; n++) {                  /* the pitch table against the formula */
        double want = 174.614115717 * pow(2.0, n / 12.0) * 4294967296.0 / RATE;
        if (fabs((double)drone_note_step(n, 0) - want) > 1.0) {
            printf("FAIL: step of note %u\n", n);
            failed = 1;
        }
    }
    check("an octave up is twice the step", drone_note_step(12, 0) == 2 * drone_note_step(0, 0)
          && drone_note_step(26, 0) == 4 * drone_note_step(2, 0));
    check("the octave control shifts it", drone_note_step(0, -1) == drone_note_step(0, 0) / 2
          && drone_note_step(0, 2) == 4 * drone_note_step(0, 0));

    drone_init(&engine);
    c = plain();
    render(&c, SECOND);
    check("no voice: silence", silent(SECOND) && drone_voices_sounding(&engine) == 0);

    c.notes_on = 1u << 0;                       /* F3, 174.6 Hz */
    render(&c, SECOND);
    check("a voice sounds", peak(SECOND) > 1000000 && drone_voices_sounding(&engine) == 1);
    render(&c, SECOND);
    check("F3 is 174 or 175 cycles a second", cycles(SECOND) >= 174 && cycles(SECOND) <= 175);
    c.octave = -1;
    render(&c, SECOND);
    check("an octave down is 87 or 88", cycles(SECOND) >= 87 && cycles(SECOND) <= 88);
    c.octave = 0;

    c.master = 0;
    render(&c, SECOND);
    check("master at zero: silence", silent(SECOND));
    c.master = DRONE_MASTER_MAX;

    c.notes_on = 0;                             /* the latch released: it fades, then it is exactly silent */
    render(&c, SECOND);
    render(&c, SECOND);
    check("off: it ends in silence and the voice is free", silent(SECOND) && drone_voices_sounding(&engine) == 0);

    c.param[DRONE_FADE] = 127;                  /* the slowest fade: still rising after a second */
    c.notes_on = 1u << 5;
    render(&c, SECOND);
    i = (uint32_t)peak(SECOND);
    render(&c, SECOND);
    check("a slow fade is still rising after one second", (uint32_t)peak(SECOND) > i + i / 4);
    c.notes_on = 0;
    c.param[DRONE_FADE] = 0;
    render(&c, SECOND);

    c.notes_on = 1u << 0;                       /* the glitch: the output holds each value for many samples */
    c.param[DRONE_CRUSH] = 127;
    render(&c, SECOND);
    render(&c, SECOND);
    check("crush at the top holds each value 64 samples", longest_run_min(SECOND) >= 64);
    c.param[DRONE_CRUSH] = 0;
    render(&c, SECOND);
    check("crush at zero holds nothing", longest_run_min(SECOND) == 1);

    c.param[DRONE_CUTOFF] = 0;                  /* the filter closed takes nearly all of it away */
    render(&c, SECOND);
    render(&c, SECOND);
    i = (uint32_t)peak(SECOND);
    c.param[DRONE_CUTOFF] = 127;
    render(&c, SECOND);
    check("the filter closed is much quieter than open", i * 8 < (uint32_t)peak(SECOND));

    c.notes_on = 0;                             /* noise alone has no pitch: far more jumps than a tone has cycles */
    render(&c, SECOND);
    c.notes_on = 1u << 0;
    c.param[DRONE_NOISE] = 127;
    render(&c, SECOND);
    check("noise at the top is noise", cycles(SECOND) > 2000);

    for (i = 0; i < 2000; i++) {                /* everything at once, the controls jumping: never out of range */
        uint32_t r = i * 2654435761u, p;
        c.notes_on = i % 7 ? (1u << DRONE_NOTES) - 1u : r & ((1u << DRONE_NOTES) - 1u);
        for (p = 0; p < DRONE_NPARAM; p++)
            c.param[p] = (uint8_t)((r >> (p * 4)) % 128u);
        c.octave = (int8_t)((int)(r % 6u) - 3);
        render(&c, DRONE_BLOCK_FRAMES);
    }
    check("27 voices sound together", drone_voices_sounding(&engine) > 0);
    check("the output is always in the 24-bit range", range_ok);
    check("left and right are the same", stereo_same);
    puts(failed ? "drone_test: FAILED" : "drone_test: ok");
    return failed;
}
