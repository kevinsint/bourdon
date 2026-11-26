/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kevin Sint */
/* The screen on this computer: what it writes for each value, that it draws only what changed, that nothing goes
 * outside the 240 by 240 pixels, that the pictures follow the controls, and the three voices' boxes.
 *   build/host/view_test [directory]     with a directory: also writes the screens there as .ppm pictures, to look at */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static uint16_t screen[240][240];
static int outside, draws, top_drawn, bottom_drawn;
static void screen_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t colour)
{
    uint32_t i, j;
    if (!w || !h)
        return;
    if (x >= 240 || y >= 240 || w > 240 - x || h > 240 - y) {
        outside = 1;
        return;
    }
    draws++;
    if ((int)y < top_drawn)
        top_drawn = (int)y;
    if ((int)(y + h) > bottom_drawn)
        bottom_drawn = (int)(y + h);
    for (j = y; j < y + h; j++)
        for (i = x; i < x + w; i++)
            screen[j][i] = colour;
}
static void screen_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *pixels)
{
    uint32_t i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            screen_rect(x + i, y + j, 1, 1, pixels[j * w + i]);
}
#define FONT_RECT(x, y, w, h, colour) screen_rect(x, y, w, h, colour)
#define VIEW_BLIT(x, y, w, h, pixels) screen_blit(x, y, w, h, pixels)
#include "font.h"
#include "view.h"

static int failed;
static void check(const char *what, int ok)
{
    if (!ok) {
        printf("FAIL: %s\n", what);
        failed = 1;
    }
}
static int same(const char *got, const char *want)
{
    if (strcmp(got, want))
        printf("  got \"%s\", want \"%s\"\n", got, want);
    return !strcmp(got, want);
}
static void watch(void)                         /* count the drawing from here */
{
    draws = 0;
    top_drawn = 240;
    bottom_drawn = 0;
}
static int pixels_of(uint16_t colour, int x, int y, int w, int h)
{
    int i, j, n = 0;
    for (j = y; j < y + h; j++)
        for (i = x; i < x + w; i++)
            n += screen[j][i] == colour;
    return n;
}
static int last_column_of(uint16_t colour, int x, int y, int w, int h)   /* the rightmost column with that colour */
{
    int i;
    for (i = x + w - 1; i >= x; i--)
        if (pixels_of(colour, i, y, 1, h))
            return i;
    return -1;
}
static void save(const char *directory, const char *name)
{
    char path[512];
    FILE *file;
    int i, j;
    if (!directory)
        return;
    snprintf(path, sizeof path, "%s/%s.ppm", directory, name);
    file = fopen(path, "wb");
    if (!file)
        return;
    fprintf(file, "P6\n240 240\n255\n");
    for (j = 0; j < 240; j++)
        for (i = 0; i < 240; i++) {
            uint16_t c = screen[j][i];
            fputc((c >> 11) * 255 / 31, file);
            fputc((c >> 5 & 63) * 255 / 63, file);
            fputc((c & 31) * 255 / 31, file);
        }
    fclose(file);
}

/* ---- a dial as the screen shows it ----
 * Read from the pixels: where the knob's line points, where the lit arc goes and where the lit dot is, each as a
 * position on the dial (0 .. VIEW_DIAL_STEPS). The tests take them within a few steps: the pixels are coarse. */
#define NEAR 5                                  /* a position read from the pixels: within this many steps */
#define RING_IN (VIEW_RING_R / 16 - 2)          /* the ring's band, in pixels from the centre */
#define RING_OUT (VIEW_RING_R / 16 + 2)
#define KNOB_OUT (VIEW_KNOB_R / 16 - 1)         /* inside the knob: its line */
#define DIALS_TOP (int)(VIEW_DIAL_Y - VIEW_DIAL_HALF)   /* the dials' band, across the screen */
#define DIALS_H (int)VIEW_DIAL_SIZE
typedef struct { int count, from, to, at; } seen_t;    /* the pixels seen, the first and the last position, their middle */
static seen_t seen_on_dial(uint32_t n, uint16_t colour, int r_from, int r_to)  /* `colour` from r_from to r_to pixels out */
{
    seen_t s = { 0, 1000, -1000, -1 };
    int x, y, sum_x = 0, sum_y = 0;
    for (y = -VIEW_DIAL_HALF; y <= VIEW_DIAL_HALF; y++)
        for (x = -VIEW_DIAL_HALF; x <= VIEW_DIAL_HALF; x++) {
            int r2 = x * x + y * y, at;
            if (r2 < r_from * r_from || r2 > r_to * r_to || screen[VIEW_DIAL_Y + y][VIEW_DIAL_X(n) + x] != colour
                || VIEW_DIAL_Y + y >= VIEW_DIAL_NAME_Y)     /* the name, in the ring's open bottom, is not the dial */
                continue;
            at = ((int)view_angle(x * 16, y * 16) - VIEW_TURN / 8) * (int)VIEW_DIAL_STEPS / (VIEW_TURN * 3 / 4);
            s.count++;
            s.from = at < s.from ? at : s.from;
            s.to = at > s.to ? at : s.to;
            sum_x += x;
            sum_y += y;
        }
    if (s.count)
        s.at = ((int)view_angle(sum_x * 16 / s.count, sum_y * 16 / s.count) - VIEW_TURN / 8) * (int)VIEW_DIAL_STEPS / (VIEW_TURN * 3 / 4);
    return s;
}
static int near(int got, int want)
{
    return got >= want - NEAR && got <= want + NEAR;
}
static int knob_line(uint32_t n)                /* where dial n's knob points; -1: it has no line */
{
    return seen_on_dial(n, VIEW_WHITE, 0, KNOB_OUT).at;
}
static int ring_dot(uint32_t n)                 /* where dial n's lit dot is; -1: none */
{
    return seen_on_dial(n, VIEW_WHITE, RING_IN - 2, RING_OUT + 2).at;
}
/* dial n's lit arc, in `ink`, goes from .. to; the dot and its glow are drawn on it: where the dot is, the arc is too */
static int arc_is(uint32_t n, uint16_t ink, int from, int to)
{
    seen_t s = seen_on_dial(n, ink, RING_IN, RING_OUT), dot = seen_on_dial(n, VIEW_WHITE, RING_IN, RING_OUT);
    if (dot.count && s.count && dot.from <= s.to + 2 * NEAR && dot.to >= s.from - 2 * NEAR) {
        s.from = dot.from < s.from ? dot.from : s.from;
        s.to = dot.to > s.to ? dot.to : s.to;
    }
    return s.count > 0 && near(s.from, from) && near(s.to, to);
}
static int no_arc(uint32_t n)                   /* no lit arc on dial n, amber or cyan */
{
    return !seen_on_dial(n, VIEW_AMBER, 0, VIEW_DIAL_HALF + 1).count && !seen_on_dial(n, VIEW_CYAN, 0, VIEW_DIAL_HALF + 1).count;
}
static int contour_of(uint32_t n)               /* dial n's ring is there: its contour's pixels */
{
    return seen_on_dial(n, view_grey(92), RING_IN, RING_OUT).count;
}

/* the first voice: oscillator, filter, level; the cursor on the filter, its cutoff turned last */
static view_model_t plain(void)
{
    view_model_t m;
    uint32_t i;
    memset(&m, 0, sizeof m);
    m.voices_on = 1u << 0 | 1u << 2;
    m.page = PANEL_VOICE;
    m.place = 1;
    m.touched = PANEL_ENC_VALUES;
    m.block[0] = panel_block_new(DRONE_BLOCK_VCO);
    m.block[1] = panel_block_new(DRONE_BLOCK_VCF);
    m.block[1].param[DRONE_VCF_RESONANCE] = 40;
    m.block[2] = panel_block_new(DRONE_BLOCK_VCA);
    for (i = 0; i < DRONE_NPARAM; i++)
        m.param[i] = drone_param_spec[i].start;
    for (i = 0; i < DRONE_LFO_NPARAM; i++)
        m.lfo[i] = drone_lfo_spec[i].start;
    m.lfo[DRONE_LFO_DEPTH] = 50;
    m.send = drone_send_spec.start;
    for (i = 0; i < DRONE_REVERB_NPARAM; i++)
        m.reverb[i] = drone_reverb_spec[i].start;
    for (i = 0; i < DRONE_DELAY_NPARAM; i++)
        m.delay[i] = drone_delay_spec[i].start;
    for (i = 0; i < DRONE_COMP_NPARAM; i++)
        m.comp[i] = drone_comp_spec[i].start;
    m.reverb_holder = DRONE_VOICES;
    m.fx_place = DRONE_SLOTS;
    m.load_percent = 31;
    return m;
}
static int cell_x(int cell) { return (int)VIEW_CELL_X(cell); }
static int eoc_x(int cell) { return (int)VIEW_EOC_X(cell); }
static int picture_ink(void)                    /* what is drawn in the large picture below the row, without its frame */
{
    return pixels_of(VIEW_WHITE, 10, 104, 108, 58) + pixels_of(VIEW_GREY, 10, 104, 108, 58);
}

/* the bank's page: the first LFO, a free sine at its start, the clock at the start of a beat */
/* LFO l gives what an LFO with that RANGE (1: up only), LEVEL (%) and OFFSET (%, 0: none) gives */
static void swing(view_model_t *m, uint32_t l, int up_only, int level, int offset)
{
    uint8_t value[DRONE_BANK_NPARAM] = { 0 };
    value[DRONE_BANK_RANGE] = (uint8_t)up_only;
    value[DRONE_BANK_LEVEL] = (uint8_t)level;
    value[DRONE_BANK_OFFSET] = (uint8_t)((int)DRONE_BANK_OFFSET_ZERO + offset);
    m->bank_low[l] = (int16_t)drone_bank_out(up_only ? 0 : -32767, value);
    m->bank_high[l] = (int16_t)drone_bank_out(32767, value);
}
static view_model_t bank_model(void)
{
    view_model_t m = plain();
    uint32_t p;
    for (p = 0; p < DRONE_BANK_LFOS; p++)
        swing(&m, p, 0, 100, 0);
    m.page = PANEL_BANK;
    m.touched = DRONE_BANK_RATE;
    m.tempo = DRONE_TEMPO_START;
    for (p = 0; p < DRONE_BANK_NPARAM; p++)
        m.bank_knob[p] = m.bank_value[p] = drone_bank_spec[p].start;
    for (p = 0; p < PLAY_NKNOB; p++)
        m.knob[p] = play_knob_spec[p].start;
    m.amount_touched = PANEL_ENC_VALUES;
    return m;
}
/* the engine's frames for one cycle of a free bank LFO at that RATE: counted, render call by render call */
static drone_t bank_engine;
static uint32_t engine_cycle_frames(uint32_t rate)
{
    static drone_controls_t controls;
    uint32_t p, calls = 0, cycles = 0, before;
    for (p = 0; p < DRONE_BANK_NPARAM; p++)
        controls.bank[0][p] = drone_bank_spec[p].start;
    controls.bank[0][DRONE_BANK_RATE] = (uint8_t)rate;
    controls.tempo = DRONE_TEMPO_START;
    drone_init(&bank_engine);
    drone_bank_run(&bank_engine.bank, &controls, DRONE_BLOCK_FRAMES);
    while (cycles < 4u) {
        before = bank_engine.bank.lfo[0].phase;
        drone_bank_run(&bank_engine.bank, &controls, DRONE_BLOCK_FRAMES);
        cycles += bank_engine.bank.lfo[0].phase < before;
        calls++;
    }
    return calls * DRONE_BLOCK_FRAMES / cycles;
}
static void bank_value(view_model_t *m, uint32_t param, uint32_t value)     /* a value of the shown LFO, with no cable on it */
{
    m->bank_knob[param] = m->bank_value[param] = (uint8_t)value;
}

/* the engine's bank from its start, with these controls (every LFO at its start, then what a test sets), as main.c
 * runs it: until LFO `lfo` is at `step` of loop `loop`, `phase` (of 2^32) along it */
static drone_controls_t engine_controls;
static void engine_start(void)
{
    uint32_t l, p;
    memset(&engine_controls, 0, sizeof engine_controls);
    for (l = 0; l < DRONE_BANK_LFOS; l++)
        for (p = 0; p < DRONE_BANK_NPARAM; p++)
            engine_controls.bank[l][p] = drone_bank_spec[p].start;
    engine_controls.tempo = DRONE_TEMPO_START;
}
static int engine_run_to(uint32_t lfo, uint32_t step, uint32_t loop, uint32_t phase)
{
    const drone_bank_lfo_t *at = &bank_engine.bank.lfo[lfo];
    uint32_t calls;
    drone_init(&bank_engine);
    for (calls = 0; calls < 2000000u; calls++) {
        drone_bank_run(&bank_engine.bank, &engine_controls, DRONE_BLOCK_FRAMES);
        if (at->step == step && at->loop == loop && at->phase >= phase)
            return 1;
    }
    return 0;
}
/* the engine's picture of the bank, taken into the model as main.c takes it */
static void engine_take(view_model_t *m, uint32_t lfo, uint32_t source, uint32_t set)
{
    drone_bank_ask(&bank_engine.bank.view);
    drone_bank_run(&bank_engine.bank, &engine_controls, DRONE_BLOCK_FRAMES);
    view_bank_take(m, &engine_controls, &bank_engine.bank.view, lfo, source, set);
}
/* the picture of LFO `lfo` on the screen against the engine run on from where it is: in each column of this step
 * and the next one, the engine's output is white at its height. Columns at the amber mark are left out. Gives the
 * columns checked, or -1 when one is not white. For shapes that are flat along a step: the screen samples a column at
 * its start, the engine anywhere in it */
static int picture_follows_engine(const view_model_t *m, uint32_t lfo)
{
    const drone_bank_lfo_t *at = &bank_engine.bank.lfo[lfo];
    uint32_t step = m->bank_step, loop = m->bank_loop, next = step, next_loop = loop, calls, mark = (((m->bank_phase >> 16) * 112u) >> 16);
    int checked = 0, wrong = 0, seen_next = 0;
    drone_bank_step_after(bank_engine.bank.view.value[lfo], &next, &next_loop);
    for (calls = 0; calls < 2000000u; calls++) {
        int k = at->step == step && at->loop == loop ? 0 : at->step == next && at->loop == next_loop ? 1 : -1;
        uint32_t col = (uint32_t)k * 112u + (((at->phase >> 16) * 112u) >> 16);
        if (k < 0 && seen_next)
            break;
        seen_next |= k == 1;
        if (k >= 0 && (col + 3u < mark || col > mark + 3u)) {
            checked++;
            wrong += screen[VIEW_BANK_Y + view_bank_y(at->out)][8 + col] != VIEW_WHITE;
        }
        drone_bank_run(&bank_engine.bank, &engine_controls, DRONE_BLOCK_FRAMES);
    }
    return wrong ? -1 : checked;
}
static uint16_t screen_drawn[240][240];
static int as_if_drawn_again(const view_model_t *m)    /* the screen, drawn bit by bit, is the one drawn all at once */
{
    memcpy(screen_drawn, screen, sizeof screen);
    view_reset();
    view_show(m);
    return !memcmp(screen_drawn, screen, sizeof screen);
}

int main(int argc, char **argv)
{
    const char *directory = argc > 1 ? argv[1] : 0;
    view_model_t m = plain();
    char text[24];                              /* as view_bank_value_line's: "SHAPE SOFT RANDOM" */
    uint32_t i, r = 12345;
    int low_edge;

    view_note_text(text, 42);
    check("pitch 42 is F#2", same(text, "F#2"));
    view_note_text(text, 60);
    check("pitch 60 is C4", same(text, "C4"));
    view_note_text(text, 1);
    check("the lowest octave is -1", same(text, "C#-1"));
    view_fine_text(text, 64);
    check("fine in tune is +0", same(text, "+0"));
    view_fine_text(text, 0);
    check("fine at the bottom is -64", same(text, "-64"));
    check("the cutoff's frequency is the engine's: 20 Hz, an octave every 15 steps",
          view_cutoff_hz(0) == 20 && view_cutoff_hz(15) == 40 && view_cutoff_hz(90) == 1280 && view_cutoff_hz(127) > 7000);
    view_hz_text(text, 640);
    check("below 1 kHz: the hertz", same(text, "640"));
    view_hz_text(text, 1280);
    check("above: kilohertz with one decimal", same(text, "1.3K"));
    view_time_text(text, 5);
    check("a short time in milliseconds", same(text, "5 MS"));
    view_time_text(text, 310);
    check("a time in seconds with one decimal", same(text, "0.3 S"));
    view_time_text(text, 30000);
    check("a long time in whole seconds", same(text, "30 S"));
    check("the fade's time is the engine's: 5 ms .. 6 s", view_fade_ms(0) <= 5 && view_fade_ms(127) > 5900 && view_fade_ms(127) < 6200);
    check("an LFO's time is the engine's: 40 is about 6 s", view_lfo_ms(40) > 6000 && view_lfo_ms(40) < 6500 && view_lfo_ms(0) > 62000);
    check("the decay's time is the engine's: 0.3 s .. 30 s, 64 is 3 s",
          view_decay_ms[0] == 300 && view_decay_ms[127] == 30000 && view_decay_ms[64] > 3000 && view_decay_ms[64] < 3100);

    view_reset();
    watch();
    view_show(&m);
    check("the first time, it draws", draws > 50 && top_drawn == 0);
    check("the selected voice's name is in a white box at the top left", pixels_of(VIEW_WHITE, 8, 4, 40, 18) > 300);
    save(directory, "1_patch");
    watch();
    view_show(&m);
    check("nothing changed: nothing is drawn", draws == 0);

    m.voices_on = 0;
    view_show(&m);
    check("no voice on: the voices' boxes are outlines", pixels_of(VIEW_WHITE, 8, 212, 224, 18) == 0 && pixels_of(VIEW_DIM, 8, 212, 224, 18) > 300);
    m.voices_on = 1;
    view_show(&m);
    check("the first voice on: its box is filled", pixels_of(VIEW_WHITE, 8, 212, 72, 18) > 1200 && pixels_of(VIEW_WHITE, 84, 212, 148, 18) == 0);
    m.voices_on = 7;
    m.selected = 2;
    view_show(&m);
    check("three voices: three boxes of one width across the row, with a gap between them",
          pixels_of(VIEW_WHITE, 8, 212, 72, 18) > 1200 && pixels_of(VIEW_WHITE, 84, 212, 72, 18) > 1200 && pixels_of(VIEW_WHITE, 160, 212, 72, 18) > 1200
          && pixels_of(VIEW_BLACK, 80, 212, 4, 18) == 4 * 18 && pixels_of(VIEW_BLACK, 156, 212, 4, 18) == 4 * 18
          && pixels_of(VIEW_WHITE, 0, 212, 8, 18) == 0 && pixels_of(VIEW_WHITE, 232, 212, 8, 18) == 0);
    check("the selected voice has the amber mark under its box", pixels_of(VIEW_AMBER, 160, 232, 72, 4) == 72 * 4
          && pixels_of(VIEW_AMBER, 0, 232, 160, 4) == 0);
    save(directory, "2_voices");
    m.voices_on = 0x38;
    view_show(&m);
    check("no box for a fourth voice", pixels_of(VIEW_WHITE, 0, 212, 240, 18) == 0);
    m = plain();

    /* ---- the end of the chain ---- */
    m.page = PANEL_REVERB;
    m.touched = PANEL_ENC_VALUES + 1;
    view_show(&m);
    check("the end of the chain: its three effects and OUT in a row, the cursor on the open one",
          pixels_of(VIEW_AMBER, eoc_x(1), 35, 28, 2) == 28 * 2 && pixels_of(VIEW_AMBER, eoc_x(0), 35, 28, 26) == 0
          && pixels_of(VIEW_AMBER, eoc_x(2), 35, 28, 26) == 0 && pixels_of(VIEW_WHITE, eoc_x(3), 35, 28, 26) > 10);
    check("EOC in the voice's place", pixels_of(VIEW_BLACK, 8, 4, 40, 18) > 60 && pixels_of(VIEW_WHITE, 8, 4, 40, 18) > 400);
    i = (uint32_t)picture_ink();
    check("the reverb's page: its tail, and the value turned last in white", i > 100 && pixels_of(VIEW_WHITE, 128, 104 + 18, 104, 14) > 50
          && pixels_of(VIEW_WHITE, 128, 104, 104, 14) == 0);
    save(directory, "5_eoc_reverb");
    m.reverb[DRONE_REVERB_DECAY] = 127;
    view_show(&m);
    check("a longer decay: a longer tail", (uint32_t)picture_ink() > i + i / 2);
    m.reverb_holder = 0;
    view_show(&m);
    check("a patch has the reverb: the page says which voice, and its own values are dim",
          pixels_of(VIEW_AMBER, 128, 80, 104, 14) > 60 && pixels_of(VIEW_WHITE, 128, 104 + 18, 104, 50) == 0 && pixels_of(VIEW_GREY, 128, 104 + 18, 104, 50) == 0
          && pixels_of(VIEW_GREY, 128, 104, 104, 14) == 0 && pixels_of(VIEW_WHITE, 128, 104, 104, 14) == 0);   /* no send there: it is the mixer's */
    save(directory, "27_eoc_reverb_in_a_patch");
    m.reverb_holder = DRONE_VOICES;
    view_show(&m);
    check("the shared reverb again", pixels_of(VIEW_AMBER, 128, 80, 104, 14) == 0 && pixels_of(VIEW_WHITE, 128, 104 + 18, 104, 14) > 50);

    m.page = PANEL_DELAY;
    m.touched = PANEL_ENC_VALUES;
    m.delay[DRONE_DELAY_REPEATS] = 0;
    view_show(&m);
    i = (uint32_t)picture_ink();
    check("the delay's page: the cursor on it, the sound that goes in as one bar", pixels_of(VIEW_AMBER, eoc_x(0), 35, 28, 2) == 28 * 2
          && pixels_of(VIEW_AMBER, eoc_x(1), 35, 28, 26) == 0 && i > 50);
    m.delay[DRONE_DELAY_REPEATS] = 100;
    view_show(&m);
    check("repeats: more bars", (uint32_t)picture_ink() > 2 * i);
    save(directory, "14_eoc_delay");
    i = (uint32_t)picture_ink();
    m.delay[DRONE_DELAY_TIME] = 255;
    view_show(&m);
    check("a longer time: the bars further apart", (uint32_t)picture_ink() < i);

    m.page = PANEL_COMP;
    m.comp[DRONE_COMP_THRESHOLD] = 64;
    m.comp[DRONE_COMP_RATIO] = 0;
    view_show(&m);
    check("the compressor's page: ratio 1, the line goes straight to the top", pixels_of(VIEW_AMBER, eoc_x(2), 35, 28, 2) == 28 * 2
          && pixels_of(VIEW_WHITE, 10, 104, 108, 16) > 20);
    m.comp[DRONE_COMP_RATIO] = 127;
    view_show(&m);
    check("the ratio at the top: flat above the threshold", pixels_of(VIEW_WHITE, 10, 104, 108, 16) == 0 && pixels_of(VIEW_WHITE, 10, 104, 108, 58) > 100);
    save(directory, "28_eoc_comp");
    m.comp[DRONE_COMP_GAIN] = 127;
    view_show(&m);
    check("with gain: the line is higher", pixels_of(VIEW_WHITE, 10, 104, 108, 16) > 20);

    m = plain();
    view_show(&m);
    check("back on the patch: the chain again, nothing left of the end of the chain", pixels_of(VIEW_AMBER, cell_x(1), 35, 28, 2) == 28 * 2
          && pixels_of(VIEW_GREY, 0, 24, 46, 9) == 0 && pixels_of(VIEW_WHITE, 8, 4, 40, 18) > 400);

    view_ratio_text(text, 0);
    check("a ratio of 1", same(text, "1.0 TO 1"));
    view_ratio_text(text, 64);
    check("a ratio of 2", same(text, "2.0 TO 1"));
    view_ratio_text(text, 120);
    check("a high ratio has no tenths", same(text, "18 TO 1"));
    view_ratio_text(text, 127);
    check("the ratio at the top is a limiter", same(text, "LIMIT"));
    check("the delay's time in ms", view_delay_ms(0) == 1 && view_delay_ms(152) == 353 && view_delay_ms(255) == 992);
    check("the threshold in dB below a full sample", view_threshold_db(127) == 0 && view_threshold_db(95) == 12 && view_threshold_db(0) == 47);

    m = plain();
    m.info = 1;
    view_show(&m);
    check("SEL held: the version and the load, where the page's name was", pixels_of(VIEW_WHITE, 56, 4, 176, 18) > 100);
    save(directory, "6_info");
    m.info = 0;
    m.load_percent = 95;
    view_show(&m);
    check("a load above 80 %: the line at the bottom is red", pixels_of(VIEW_RED, 0, 238, 240, 2) > 400);

    /* ---- a patch ---- */
    m = plain();
    view_reset();
    view_show(&m);
    check("a patch: a picture for each block, the empty place and OUT, and no more",
          pixels_of(VIEW_WHITE, cell_x(0), 35, 28, 26) > 20 && pixels_of(VIEW_WHITE, cell_x(2), 35, 28, 26) > 20
          && pixels_of(VIEW_WHITE, cell_x(4), 35, 28, 26) > 10 && pixels_of(VIEW_BLACK, cell_x(5), 35, 28, 26) == 28 * 26);
    check("the cursor's block has the amber frame, the others have not",
          pixels_of(VIEW_AMBER, cell_x(1), 35, 28, 2) == 28 * 2 && pixels_of(VIEW_AMBER, cell_x(0), 35, 28, 2) == 0);
    check("and is below the row, large, with its values", pixels_of(VIEW_WHITE, 8, 102, 112, 62) > 100 && pixels_of(VIEW_WHITE, 128, 104, 104, 14) > 50);
    save(directory, "7_patch_filter");
    watch();
    view_show(&m);
    check("a patch, nothing changed: nothing is drawn", draws == 0);
    low_edge = last_column_of(VIEW_WHITE, 8, 104, 112, 20);
    m.block[1].param[DRONE_VCF_CUTOFF] = 20;
    watch();
    view_show(&m);
    check("the block's cutoff turned: its two pictures and its values are drawn again, no more", draws > 0 && top_drawn >= 23 && bottom_drawn <= 180
          && last_column_of(VIEW_WHITE, 8, 104, 112, 20) < low_edge - 15);
    m.place = 0;
    view_show(&m);
    check("the cursor moved: the frame is on the first block", pixels_of(VIEW_AMBER, cell_x(0), 35, 28, 2) == 28 * 2
          && pixels_of(VIEW_AMBER, cell_x(1), 35, 28, 2) == 0);
    save(directory, "8_patch_oscillator");
    m.place = 2;
    m.block[2].param[DRONE_VCA_LEVEL] = 127;
    view_show(&m);
    i = (uint32_t)pixels_of(VIEW_WHITE, 8, 102, 112, 62);
    m.block[2].param[DRONE_VCA_LEVEL] = 30;
    view_show(&m);
    check("a level block: its triangle is as large as its level", (uint32_t)pixels_of(VIEW_WHITE, 8, 102, 112, 62) < i / 2);
    save(directory, "9_patch_level");
    m.place = 3;
    view_show(&m);
    check("the cursor on the empty place: it has the frame, and no picture is below",
          pixels_of(VIEW_AMBER, cell_x(3), 35, 28, 2) == 28 * 2 && pixels_of(VIEW_DIM, 8, 102, 112, 1) == 0);
    save(directory, "10_patch_empty");
    m.choosing = 1;
    m.choice = DRONE_BLOCK_VCF;
    view_show(&m);
    check("a block chosen there: the place shows it in amber", pixels_of(VIEW_AMBER, cell_x(3) + 3, 38, 22, 20) > 20);
    save(directory, "11_patch_put");
    i = (uint32_t)pixels_of(VIEW_AMBER, 0, 100, 240, 40);
    m.choice = DRONE_BLOCK_VCA;
    watch();
    view_show(&m);
    check("another block chosen: the choice moves, with no more amber than before",
          draws > 0 && (uint32_t)pixels_of(VIEW_AMBER, 0, 100, 240, 40) < i + i / 4 && (uint32_t)pixels_of(VIEW_AMBER, 0, 100, 240, 40) > i - i / 4);
    m.choosing = 0;
    m.block[3] = panel_block_new(DRONE_BLOCK_VCA);
    view_show(&m);
    check("the choice kept: the block is in the row, and OUT has moved right", pixels_of(VIEW_WHITE, cell_x(3), 35, 28, 26) > 20
          && pixels_of(VIEW_WHITE, cell_x(5), 35, 28, 26) > 10);
    m.place = 5;
    m.touched = PANEL_ENC_VALUES + 2;
    view_show(&m);
    check("the cursor on OUT: the voice's level, fade and send", pixels_of(VIEW_WHITE, 128, 104 + 36, 104, 14) > 50);
    save(directory, "12_patch_out");
    m.block[4] = m.block[5] = panel_block_new(DRONE_BLOCK_VCO);
    m.place = 6;
    view_show(&m);
    check("a full chain: six blocks and OUT fit the screen", pixels_of(VIEW_AMBER, cell_x(6), 35, 28, 2) == 28 * 2 && !outside);
    save(directory, "13_patch_full");
    /* ---- a patch's LFOs and cables ---- */
    m = plain();
    view_show(&m);
    check("no cable: no mark above the blocks", pixels_of(VIEW_CYAN, 0, 30, 240, 3) == 0 && pixels_of(VIEW_SEA, 0, 30, 240, 3) == 0);
    m.block[1].amount[DRONE_VCF_CUTOFF][0] = -25;
    m.block[2].amount[DRONE_VCA_LEVEL][1] = 40;
    m.out_amount[DRONE_OUT_SEND][2] = 10;
    view_show(&m);
    check("a cable from the open LFO: a bright mark above the block, in the LFO's place; from another LFO: a dim one",
          pixels_of(VIEW_CYAN, cell_x(1) + 1, 30, 8, 3) == 24 && pixels_of(VIEW_SEA, cell_x(2) + 10, 30, 8, 3) == 24
          && pixels_of(VIEW_SEA, cell_x(4) + 19, 30, 8, 3) == 24 && pixels_of(VIEW_CYAN, cell_x(0), 30, 28, 3) == 0);
    save(directory, "15_patch_cables");
    m.lfo_held = 1;
    watch();
    view_show(&m);
    check("the LFO key held on the filter: the amounts of its cables in place of the values, and the chain is not drawn again",
          pixels_of(VIEW_CYAN, 128, 104, 104, 14) > 50 && pixels_of(VIEW_CYAN, 128, 122, 104, 14) == 0 && top_drawn >= 72);
    save(directory, "16_patch_amounts");
    m.lfo_held = 0;
    m.page = 1;
    m.lfo_current = 0;
    m.touched = PANEL_ENC_VALUES;
    view_show(&m);
    check("a patch's LFO page: the LFO's wave in cyan, its cable in the list, the cursor's frame still on the chain",
          pixels_of(VIEW_CYAN, 8, 102, 112, 62) > 100 && pixels_of(VIEW_CYAN, 128, 144, 104, 7) > 20 && pixels_of(VIEW_CYAN, 128, 156, 104, 7) == 0
          && pixels_of(VIEW_AMBER, cell_x(1), 35, 28, 2) == 28 * 2);
    save(directory, "17_patch_lfo");
    m.page = 2;
    m.lfo_current = 1;
    watch();
    view_show(&m);
    check("the next LFO: its own cable, and its mark is the bright one", pixels_of(VIEW_CYAN, cell_x(2) + 10, 30, 8, 3) == 24
          && pixels_of(VIEW_SEA, cell_x(1) + 1, 30, 8, 3) == 24 && pixels_of(VIEW_CYAN, 128, 144, 104, 7) > 20);
    for (i = 0; i < DRONE_SLOTS; i++) {         /* more cables than the list has rows */
        m.block[i] = panel_block_new(DRONE_BLOCK_VCF);
        m.block[i].amount[0][1] = 50;
        m.block[i].amount[1][1] = -50;
    }
    view_show(&m);
    check("more cables than rows: the last row says how many more", pixels_of(VIEW_GREY, 128, 144 + 5 * 12, 104, 7) > 20 && !outside);
    save(directory, "18_patch_lfo_many");

    /* ---- an LFO's depth and its cables to the other LFOs; the feedback cable ---- */
    m = plain();
    m.page = 1;
    m.lfo[DRONE_LFO_DEPTH] = 127;
    view_show(&m);
    i = (uint32_t)pixels_of(VIEW_CYAN, 8, 102, 112, 62);
    m.lfo[DRONE_LFO_DEPTH] = 20;
    view_show(&m);
    check("an LFO with less depth: a lower wave", (uint32_t)pixels_of(VIEW_CYAN, 8, 102, 112, 62) < i * 2 / 3);
    m.lfo_amount[1][DRONE_LFO_TO_SPEED][0] = 40;
    m.fb_amount[0] = -20;
    view_show(&m);
    check("its cables to an LFO and to the feedback are in its list", pixels_of(VIEW_CYAN, 128, 144, 104, 7) > 20 && pixels_of(VIEW_CYAN, 128, 156, 104, 7) > 20
          && pixels_of(VIEW_CYAN, 128, 168, 104, 7) == 0);
    save(directory, "19_patch_lfo_depth");
    m.lfo_held = 1;
    view_show(&m);
    check("the LFO key held on an LFO's page: its cables to the two other LFOs, the one with an amount in cyan",
          pixels_of(VIEW_CYAN, 8, 104, 224, 14) > 60 && pixels_of(VIEW_CYAN, 8, 122, 224, 14) == 0 && pixels_of(VIEW_GREY, 8, 158, 224, 14) > 60);
    save(directory, "20_patch_lfo_to");
    m.lfo_held = 0;
    m.fb_amount[0] = 0;
    m.page = PANEL_FEEDBACK;
    m.touched = PANEL_ENC_VALUES + 2;
    view_show(&m);
    check("the feedback's page with no level: no cable over the chain", pixels_of(VIEW_GREEN, 0, 22, 240, 26) == 0
          && pixels_of(VIEW_GREEN, 8, 80, 224, 14) > 60);
    m.fb[DRONE_FB_FROM] = 2;
    m.fb[DRONE_FB_TO] = 1;
    m.fb[DRONE_FB_LEVEL] = 64;
    watch();
    view_show(&m);
    check("a level: the cable from after the third block back to before the second, over the chain",
          pixels_of(VIEW_GREEN, cell_x(2) + 30, 25, 2, 22) == 44 && pixels_of(VIEW_GREEN, cell_x(1) - 4, 25, 2, 22) == 44
          && pixels_of(VIEW_GREEN, cell_x(1) - 4, 25, cell_x(2) + 36 - cell_x(1), 2) == (cell_x(2) + 36 - cell_x(1)) * 2
          && pixels_of(VIEW_GREEN, cell_x(0) - 4, 27, 2, 18) == 0);
    check("the cable has an arrow head where it goes back in, and none where it leaves",
          pixels_of(VIEW_GREEN, cell_x(1) - 6, 42, 6, 5) == 22 && pixels_of(VIEW_GREEN, cell_x(2) + 28, 42, 6, 5) == 10);
    check("an arrow on its line over the chain points to where it goes back in: here to the left",
          pixels_of(VIEW_GREEN, cell_x(1), 23, 64, 1) == 2 && pixels_of(VIEW_GREEN, cell_x(1), 28, 64, 1) == 2
          && pixels_of(VIEW_GREEN, cell_x(1), 24, 64, 1) == 4
          && last_column_of(VIEW_GREEN, cell_x(1), 24, 64, 1) == last_column_of(VIEW_GREEN, cell_x(1), 23, 64, 1));
    check("its two ends have their names: the output of a block, the input of a block",
          last_column_of(VIEW_GREY, 128, 104, 104, 14) == 128 + 93 && last_column_of(VIEW_GREY, 128, 122, 104, 14) == 128 + 81);
    check("its values are at the right, the time in milliseconds", pixels_of(VIEW_WHITE, 128, 140, 104, 14) > 40 && pixels_of(VIEW_GREY, 128, 158, 104, 14) > 40);
    check("no picture yet: the scope has two flat lines", pixels_of(VIEW_GREEN, 8, 102, 112, 62) > 100 && pixels_of(VIEW_GREEN, 8, 102, 112, 20) == 0);
    for (i = 0; i < DRONE_SCOPE_POINTS; i++) {  /* the sound down the chain goes up, the sound from the cable goes down */
        m.scope[0][i] = (int8_t)(i < 52 ? 25 : -25);
        m.scope[1][i] = (int8_t)(i < 52 ? -12 : 12);
    }
    watch();
    view_show(&m);
    check("a picture: the two sounds in the scope, each in its colour, and nothing else is drawn",
          pixels_of(VIEW_WHITE, 12, 104, 50, 10) > 40 && pixels_of(VIEW_GREEN, 12, 142, 50, 10) > 40 && pixels_of(VIEW_GREEN, 12, 104, 50, 20) == 0
          && top_drawn >= 102 && bottom_drawn <= 164);
    save(directory, "21_patch_feedback");
    m.fb[DRONE_FB_TIME] = 237;
    view_show(&m);
    m.fb[DRONE_FB_TO] = 0;
    view_show(&m);
    check("its end moved: the old line is gone", pixels_of(VIEW_GREEN, cell_x(1) - 4, 27, 2, 18) == 0 && pixels_of(VIEW_GREEN, cell_x(0) - 4, 25, 2, 22) == 44);
    check("its arrow moved with it", pixels_of(VIEW_GREEN, cell_x(1) - 6, 42, 6, 5) == 0 && pixels_of(VIEW_GREEN, cell_x(0) - 6, 42, 6, 5) == 22
          && pixels_of(VIEW_GREEN, cell_x(0), 23, 98, 1) == 2);
    m.fb[DRONE_FB_FROM] = 0;
    m.fb[DRONE_FB_TO] = 2;
    view_show(&m);
    check("a cable that goes on down the chain: its arrow points to the right",
          pixels_of(VIEW_GREEN, cell_x(1), 23, 30, 1) == 2 && pixels_of(VIEW_GREEN, cell_x(1), 24, 30, 1) == 4
          && last_column_of(VIEW_GREEN, cell_x(1), 24, 30, 1) == last_column_of(VIEW_GREEN, cell_x(1), 23, 30, 1) + 2);
    m.fb[DRONE_FB_TO] = 1;
    view_show(&m);
    check("a cable from a block to the next one has no line over the chain, so no arrow there",
          pixels_of(VIEW_GREEN, 0, 25, 240, 2) == 4 && pixels_of(VIEW_GREEN, 0, 23, 240, 2) == 0 && pixels_of(VIEW_GREEN, cell_x(1) - 6, 42, 6, 5) == 22);
    m.fb[DRONE_FB_FROM] = 2;
    m.fb[DRONE_FB_TO] = 0;
    view_show(&m);
    m.lfo_held = 1;
    m.fb_amount[0] = 40;
    view_show(&m);
    check("the LFO key held there: the LFO's cable to the level, in cyan", pixels_of(VIEW_CYAN, 8, 140, 224, 14) > 60);
    save(directory, "22_patch_feedback_lfo");
    m.lfo_held = 0;
    m.page = PANEL_VOICE;
    m.block[1].type = DRONE_BLOCK_VCA;
    view_show(&m);
    check("on the chain's page the cable stays over the chain when a block changes", pixels_of(VIEW_GREEN, cell_x(2) + 30, 25, 2, 22) == 44);
    save(directory, "23_patch_with_feedback");

    /* ---- the reverb as a block ---- */
    m = plain();
    m.block[3] = panel_block_new(DRONE_BLOCK_FX);
    m.place = 3;
    m.fx_place = 3;
    m.reverb_holder = 0;
    view_show(&m);
    check("an FX block with the reverb: its tail in white, large below the row, and its three values",
          pixels_of(VIEW_WHITE, cell_x(3), 35, 28, 26) > 15 && pixels_of(VIEW_WHITE, 8, 102, 112, 62) > 100 && pixels_of(VIEW_GREY, 128, 140, 104, 14) > 40);
    i = (uint32_t)pixels_of(VIEW_WHITE, 8, 102, 112, 62);
    save(directory, "24_patch_fx");
    m.block[3].param[DRONE_FX_DECAY] = 127;
    view_show(&m);
    check("a longer decay: a longer tail", (uint32_t)pixels_of(VIEW_WHITE, 8, 102, 112, 62) > i + i / 3);
    m.fx_place = DRONE_SLOTS;
    m.reverb_holder = 1;
    view_show(&m);
    check("an FX block that does not have the reverb: its tail is dim", pixels_of(VIEW_WHITE, cell_x(3) + 3, 38, 22, 20) == 0);
    save(directory, "25_patch_fx_idle");
    m.choosing = 1;
    m.choice = DRONE_BLOCK_FX;
    m.place = 4;
    view_show(&m);
    check("the blocks to choose from, FX with them, fit the screen", !outside && pixels_of(VIEW_AMBER, 180, 110, 60, 20) > 30);
    save(directory, "26_patch_put_fx");
    m.choosing = 0;

    m = plain();
    for (i = 0; i < 20000; i++) {               /* every page with any values: nothing outside the screen */
        uint32_t p;
        r = r * 1664525u + 1013904223u;
        m.voices_on = (uint8_t)(r >> 3 & 7u);
        m.selected = (uint8_t)((r >> 9) % DRONE_VOICES);
        m.page = (uint8_t)((r >> 12) % PANEL_NPAGE);
        m.choosing = (r >> 15 & 7u) == 0;
        m.touched = (uint8_t)((r >> 20) % VIEW_ENCODERS);
        m.info = (r >> 23 & 15u) == 0;
        m.load_percent = (uint8_t)(r >> 24);
        m.faults = (uint8_t)(r >> 30);
        for (p = 0; p < DRONE_NPARAM; p++)
            m.param[p] = (uint8_t)((r >> p ^ r >> (p + 7)) % (drone_param_spec[p].top + 1u));
        for (p = 0; p < DRONE_LFO_NPARAM; p++)
            m.lfo[p] = (uint8_t)((r >> (p + 2) ^ r >> 11) % (drone_lfo_spec[p].top + 1u));
        for (p = 0; p < DRONE_REVERB_NPARAM; p++)
            m.reverb[p] = (uint8_t)((r >> (p + 5) ^ r >> 13) % 128u);
        for (p = 0; p < DRONE_DELAY_NPARAM; p++) {
            m.delay[p] = (uint8_t)(r >> (p + 3) ^ r >> 15);
            m.comp[p] = (uint8_t)(r >> (p + 6) ^ r >> 18);
        }
        m.send = (uint8_t)((r >> 4 ^ r >> 17) % 128u);
        m.place = (uint8_t)((r >> 6) % 9u);
        m.choice = (uint8_t)((r >> 16) % 6u);
        m.lfo_held = (r >> 19 & 3u) == 0;
        m.lfo_current = (uint8_t)((r >> 26) % 4u);
        m.out_amount[r >> 5 & 1u][(r >> 7) % DRONE_LFOS] = (int8_t)(r >> 10);
        m.lfo_amount[(r >> 2) % DRONE_LFOS][r >> 9 & 1u][(r >> 12) % DRONE_LFOS] = (int8_t)(r >> 14);
        m.fb[(r >> 4) % DRONE_FB_NPARAM] = (uint8_t)(r >> 8);
        m.fb_amount[(r >> 6) % DRONE_LFOS] = (int8_t)(r >> 16 & 3u ? 0 : r >> 20);
        m.scope[r >> 3 & 1u][(r >> 5) % DRONE_SCOPE_POINTS] = (int8_t)(r >> 11);
        m.reverb_holder = (uint8_t)((r >> 13) % 9u);
        m.fx_place = (uint8_t)((r >> 17) % 8u);
        m.bank_lfo = (uint8_t)(r >> 22 & 15u);
        m.amount_touched = (uint8_t)(r >> 25 & 7u);
        m.tempo = (uint16_t)(r >> 9);
        m.bank_phase = r * 2654435761u;
        m.clock_phase = r * 40503u;
        m.bank_knob[(r >> 8) % DRONE_BANK_NPARAM] = (uint8_t)(r >> 12);
        m.bank_value[(r >> 4) % DRONE_BANK_NPARAM] = (uint8_t)(r >> 14);
        m.bank_amount[(r >> 5) % PLAY_NBANKED][(r >> 9) % DRONE_BANK_LFOS] = (int8_t)(r >> 15);
        m.bank_out[(r >> 6) % DRONE_BANK_LFOS] = (int16_t)(r >> 13);
        for (p = 0; p < DRONE_SLOTS; p++) {     /* any blocks, also with gaps and values out of range */
            m.block[p].type = (uint8_t)((r >> (p + 1) ^ r >> 14) % 6u);
            m.block[p].kind = (uint8_t)(r >> (p + 3) & 7u);
            m.block[p].param[0] = (uint8_t)(r >> (p + 8));
            m.block[p].param[1] = (uint8_t)(r >> (p + 13));
            m.block[p].param[2] = (uint8_t)(r >> (p + 17));
            m.block[p].amount[p & 1u][(r >> p) % DRONE_LFOS] = (int8_t)(r >> (p + 2));
        }
        if (i % 97 == 0)
            view_reset();
        view_show(&m);
    }
    check("nothing is ever drawn outside the screen", !outside);

    /* ---- a dial: a round knob with a line where it is, in a ring with a lit arc and a lit dot ---- */
    check("the square root: whole, rounded down", view_root(0) == 0 && view_root(1) == 1 && view_root(15) == 3 && view_root(16) == 4
          && view_root(131072) == 362 && view_root(0xFFFFFFFFu) == 65535);
    check("the sine, in 1/4096: at 0, a quarter, a half, three quarters of a turn; at an eighth within 0.2 %",
          view_sin(0) == 0 && view_sin(1024) == 4096 && view_sin(2048) == 0 && view_sin(3072) == -4096 && view_sin(4096) == 0
          && view_sin(512) > 2890 && view_sin(512) < 2902 && view_sin(2560) < -2890);
    check("the angle of a point: clockwise from straight down, a quarter turn each way round",
          view_angle(0, 16) == 0 && view_angle(-16, 0) == 1024 && view_angle(0, -16) == 2048 && view_angle(16, 0) == 3072
          && view_angle(-16, 16) >= 510 && view_angle(-16, 16) <= 514 && view_angle(16, -16) >= 2558 && view_angle(16, -16) <= 2562);
    check("a value at its bottom, middle and top: the dial's first, middle and last position",
          view_dial_at(0, 127) == 0 && view_dial_at(127 * 128, 127) == VIEW_DIAL_STEPS / 2u && view_dial_at(127 * 256, 127) == VIEW_DIAL_STEPS
          && view_dial_at(-500, 127) == 0 && view_dial_at(200 * 256, 127) == VIEW_DIAL_STEPS);
    {
        view_moved_t moved = { -32 * 256, 32 * 256, -16 * 256, 1 };
        view_dial_t d = view_dial_of(64 * 256, 0, 127, VIEW_AMBER);
        check("no LFO on a value: the knob only, no arc, no dot", d.knob == 97 && d.from > d.to && d.dot == VIEW_NO_DOT);
        d = view_dial_of(64 * 256, &moved, 127, VIEW_AMBER);
        check("an LFO on it: the arc its range down and up, the dot where it has it now", d.knob == 97 && d.from == 48 && d.to == 145 && d.dot == 73
              && d.ink == VIEW_AMBER);
        moved = (view_moved_t){ 0, 32 * 256, 16 * 256, 1 };
        d = view_dial_of(64 * 256, &moved, 127, VIEW_CYAN);
        check("an LFO that goes up only: the arc from the knob's place up, not below it; in the ink asked for", d.from == 97 && d.to == 145
              && d.dot == 121 && d.ink == VIEW_CYAN);
        moved = (view_moved_t){ -32 * 256, 0, -32 * 256, 1 };
        d = view_dial_of(64 * 256, &moved, 127, VIEW_AMBER);
        check("an LFO that goes from -1 to 0: the arc from below the knob's place up to it, not above it", d.from == 48 && d.to == 97 && d.dot == 48);
        moved = (view_moved_t){ 16 * 256, 32 * 256, 16 * 256, 1 };
        d = view_dial_of(64 * 256, &moved, 127, VIEW_AMBER);
        check("an LFO that never comes to 0: the arc is away from the knob's place", d.knob == 97 && d.from == 121 && d.to == 145);
        moved = (view_moved_t){ -200 * 256, 50 * 256, -200 * 256, 1 };
        d = view_dial_of(120 * 256, &moved, 127, VIEW_AMBER);
        check("the range and the dot stay on the dial", d.from == 0 && d.to == VIEW_DIAL_STEPS && d.dot == 0);
        moved = (view_moved_t){ -32 * 256, 32 * 256, 0, 0 };
        d = view_dial_of(64 * 256, &moved, 127, VIEW_AMBER);
        check("LFOs, but none with an amount on it: no arc, no dot", d.from > d.to && d.dot == VIEW_NO_DOT);
    }
    {   /* what the LFOs add: LFO 1 goes -1 .. 1, amount +50; LFO 2 goes 0 .. 1, amount -100; both at their top now */
        static view_model_t lfos;
        int8_t amount[DRONE_BANK_LFOS] = { 50, -100 };
        view_moved_t moved;
        swing(&lfos, 0, 0, 100, 0);
        swing(&lfos, 1, 1, 100, 0);
        lfos.bank_out[0] = lfos.bank_out[1] = 32767;
        moved = view_moved(&lfos, amount, 127, DRONE_BANK_LFOS);
        check("what LFOs add to a value: the least and the most over what each one gives, an amount below 0 turned over; and now",
              moved.any && near(moved.low / 256, -63 - 127) && near(moved.high / 256, 63) && near(moved.now / 256, 63 - 127));
        moved = view_moved(&lfos, amount, 127, 1);
        check("one LFO alone: up only, upside down: from all the way down to 0", moved.any && near(moved.low / 256, -127) && moved.high == 0
              && near(moved.now / 256, -127));
        moved = view_moved(&lfos, amount, 127, 5);
        check("an LFO without an amount: none", !moved.any && !moved.low && !moved.high && !moved.now);
        swing(&lfos, 0, 0, 50, -50);
        check("LEVEL 50 % and OFFSET -50 %: the LFO goes from -1 to 0", lfos.bank_low[0] < -32700 && lfos.bank_high[0] >= -1 && lfos.bank_high[0] <= 1);
        swing(&lfos, 0, 1, 60, 20);
        check("up only, LEVEL 60 %, OFFSET +20 %: from 0.2 to 0.8", near(lfos.bank_low[0] * 100 / 32767, 20) && near(lfos.bank_high[0] * 100 / 32767, 80)
              && lfos.bank_low[0] > 6000 && lfos.bank_high[0] < 26500);
        swing(&lfos, 0, 0, 100, 60);
        check("an OFFSET that takes it over the top: kept in a full output", lfos.bank_high[0] == 32767 && near(lfos.bank_low[0] * 100 / 32767, -40));
    }
    {
        static const uint8_t at[] = { 0, 24, 48, 96, 144, 168, 192 };
        int ok = 1;
        for (i = 0; i < sizeof at; i++) {
            view_dial_t d = { at[i], 1, 0, VIEW_NO_DOT, VIEW_AMBER };
            memset(screen, 0, sizeof screen);
            outside = 0;
            view_dial(VIEW_DIAL_X(1), &d);
            ok &= near(knob_line(1), at[i]) && ring_dot(1) == -1 && no_arc(1) && contour_of(1) > 0 && !outside;
        }
        check("the knob's line on the screen where the dial says, round its three quarters; the contour; no arc, no dot", ok);
        for (i = 0, ok = 1; i < sizeof at; i++) {
            view_dial_t d = { VIEW_NO_DOT, 20, 150, at[i], VIEW_AMBER };
            memset(screen, 0, sizeof screen);
            view_dial(VIEW_DIAL_X(2), &d);
            ok &= near(ring_dot(2), at[i]) && knob_line(2) == -1;
        }
        check("the lit dot on the ring where the dial says; a knob without its line", ok);
        {
            view_dial_t d = { 96, 20, 150, VIEW_NO_DOT, VIEW_AMBER };
            memset(screen, 0, sizeof screen);
            view_dial(VIEW_DIAL_X(0), &d);
            check("the lit arc in amber from its start to its end", arc_is(0, VIEW_AMBER, 20, 150));
            check("the dial stays in its block of 33 x 33", pixels_of(VIEW_BLACK, 0, 0, 240, VIEW_DIAL_Y - VIEW_DIAL_HALF) == 240 * (VIEW_DIAL_Y - VIEW_DIAL_HALF)
                  && pixels_of(VIEW_BLACK, 0, VIEW_DIAL_Y + VIEW_DIAL_HALF + 1, 240, 240 - VIEW_DIAL_Y - VIEW_DIAL_HALF - 1)
                         == 240 * (240 - VIEW_DIAL_Y - VIEW_DIAL_HALF - 1)
                  && pixels_of(VIEW_BLACK, VIEW_DIAL_X(0) + VIEW_DIAL_HALF + 1, 0, 240 - VIEW_DIAL_X(0) - VIEW_DIAL_HALF - 1, 240)
                         == 240 * (240 - VIEW_DIAL_X(0) - VIEW_DIAL_HALF - 1));
        }
        {   /* the dot at the top, where its glow is nearest the block's edge: it fades out before the edge */
            view_dial_t d = { 96, 1, 0, 96, VIEW_AMBER };
            memset(screen, 0, sizeof screen);
            view_dial(VIEW_DIAL_X(0), &d);
            check("the dot's glow ends inside the block: its top row is black", pixels_of(VIEW_BLACK, VIEW_DIAL_X(0) - VIEW_DIAL_HALF,
                  VIEW_DIAL_Y - VIEW_DIAL_HALF, VIEW_DIAL_SIZE, 1) == VIEW_DIAL_SIZE);
        }
    }

    /* ---- the play page ---- */
    view_freq_text(text, 42u * 128u);
    check("a frequency below 1 kHz: tenths of a hertz", same(text, "92.5 HZ"));
    view_freq_text(text, 84u * 128u);
    check("from 1 kHz: whole hertz", same(text, "1047 HZ"));
    check("a frequency on a note has the note's name: pitch 69 is A4, pitch 42 is F#2, pitch 0 is C-1",
          view_freq_note_text(text, 69u * 128u) && same(text, "A4") && view_freq_note_text(text, 42u * 128u) && same(text, "F#2")
          && view_freq_note_text(text, 0) && same(text, "C-1") && view_freq_note_text(text, PLAY_FREQ_TOP) && same(text, "G9"));
    check("a frequency between two notes has no name", !view_freq_note_text(text, 69u * 128u + 1u) && !view_freq_note_text(text, 69u * 128u - 1u));
    m = plain();
    m.page = PANEL_PLAY;
    m.style = STYLE_REED;
    m.freq = 42u * 128u;
    m.touched = PANEL_ENC_VALUES;
    view_reset();
    view_show(&m);
    check("the play page, the frequency on a note: the note's name in white at the right of the top line, and no hertz",
          pixels_of(VIEW_WHITE, 196, 4, 36, 18) > 40 && pixels_of(VIEW_GREY, 140, 4, 92, 18) == 0);
    m.freq = 42u * 128u + 3u;
    m.touched = PANEL_ENC_VALUES;
    for (i = 0; i < PLAY_NKNOB; i++)
        m.knob[i] = play_knob_spec[i].start;
    view_reset();
    view_show(&m);
    check("the play page: the voice in its white box, its style and its frequency in the top line",
          pixels_of(VIEW_WHITE, 8, 4, 40, 18) > 300 && pixels_of(VIEW_WHITE, 56, 4, 60, 18) > 60 && pixels_of(VIEW_GREY, 140, 4, 92, 18) > 60);
    check("the voice's sound has a picture across the screen, a flat line when there is none",
          pixels_of(VIEW_AMBER, 8, 30, 224, 62) > 300 && pixels_of(VIEW_AMBER, 8, 30, 224, 20) == 0);
    check("a knob in its middle: its line straight up; no LFO on it: no arc, no dot", near(knob_line(0), 96) && no_arc(0)
          && ring_dot(0) == -1 && contour_of(0) > 0);
    check("KNOB4 is LINK, closed at the start: its name, its line at the start, and no link drawn between the voices",
          near(knob_line(3), 0) && no_arc(3)
          && pixels_of(VIEW_GREY, 178, 150, 52, 7) > 20 && pixels_of(VIEW_BLACK, 0, 190, 240, 22) == 240 * 22);
    check("the knob turned last has its name in white, the others in grey; its name and value are below, large",
          pixels_of(VIEW_WHITE, 10, 150, 52, 7) > 20 && pixels_of(VIEW_GREY, 66, 150, 52, 7) > 20 && pixels_of(VIEW_WHITE, 66, 150, 52, 7) == 0
          && pixels_of(VIEW_WHITE, 8, 172, 224, 14) > 100);
    check("the voices' boxes are below, as on the other pages", pixels_of(VIEW_WHITE, 8, 212, 72, 18) > 1200);
    save(directory, "8_play");
    watch();
    view_show(&m);
    check("the play page, nothing changed: nothing is drawn", draws == 0);
    m.knob[PLAY_TONE] = 127;
    watch();
    view_show(&m);
    check("TONE at the top: its line at the end, and only the dials and the value are drawn again",
          near(knob_line(0), 192) && top_drawn >= DIALS_TOP && bottom_drawn <= 190);
    m.knob[PLAY_TONE] = 0;
    view_show(&m);
    check("TONE at 0: its line at the start", near(knob_line(0), 0));
    m.freq += 50;
    watch();
    view_show(&m);
    check("another frequency: only the top line is drawn again", draws > 0 && bottom_drawn <= 23);
    for (i = 0; i < DRONE_SCOPE_POINTS; i++)
        m.scope[0][i] = (int8_t)(i % 26u < 13u ? 25 : -25);
    watch();
    view_show(&m);
    check("a sound: its picture goes up and down, and only the picture is drawn again",
          pixels_of(VIEW_AMBER, 8, 30, 224, 20) > 100 && pixels_of(VIEW_AMBER, 8, 72, 224, 20) > 100 && top_drawn >= 30 && bottom_drawn <= 92);
    /* the links: each voice listens to the voice before it; an arrow into it, thicker as its LINK is more open */
    m.selected = 1;
    m.link[1] = 20;
    watch();
    view_show(&m);
    i = (uint32_t)pixels_of(VIEW_AMBER, 56, 190, 56, 22);
    check("the selected voice's link: an amber arrow from the voice before it, between their boxes",
          i > 40 && pixels_of(VIEW_AMBER, 112, 190, 128, 22) == 0 && pixels_of(VIEW_AMBER, 0, 190, 56, 22) == 0);
    m.link[1] = 127;
    view_show(&m);
    check("LINK more open: a thicker arrow", (uint32_t)pixels_of(VIEW_AMBER, 56, 190, 56, 22) > i + 60);
    check("the selected voice's kind of link has its name at the right of the knob's value", pixels_of(VIEW_GREY, 136, 172, 96, 14) > 80);
    i = (uint32_t)pixels_of(VIEW_AMBER, 56, 190, 56, 22);
    m.link_kind[1] = DRONE_LINK_THROUGH;
    watch();
    view_show(&m);
    check("THROUGH: the arrow's line is whole, FOLLOW's is in dashes", (uint32_t)pixels_of(VIEW_AMBER, 56, 190, 56, 22) > i + 30
          && pixels_of(VIEW_AMBER, 60, 201, 44, 4) == 44 * 4);
    check("and only the links and the kind's name are drawn again", top_drawn >= 172);
    m.link_kind[1] = DRONE_LINK_FOLLOW;
    m.link[2] = 64;
    view_show(&m);
    check("another voice's link: a grey arrow", pixels_of(VIEW_GREY, 132, 190, 56, 22) > 40 && pixels_of(VIEW_AMBER, 132, 190, 56, 22) == 0);
    m.link[0] = 64;
    view_show(&m);
    check("the first voice listens to the third: the arrow goes back over the two others, and comes down on its box",
          pixels_of(VIEW_GREY, 60, 190, 120, 8) > 200 && pixels_of(VIEW_GREY, 36, 198, 17, 13) > 20);
    save(directory, "9_play_links");
    m.selected = 0;
    view_show(&m);
    check("that voice selected: its arrow is the amber one", pixels_of(VIEW_AMBER, 60, 190, 120, 8) > 200 && pixels_of(VIEW_AMBER, 56, 198, 56, 14) == 0);
    m.link[0] = m.link[1] = m.link[2] = 0;
    view_show(&m);
    check("the links closed again: no arrow left", pixels_of(VIEW_BLACK, 0, 190, 240, 22) == 240 * 22);
    m.layer = 1;
    m.knob[PLAY_SPEED] = 127;
    view_show(&m);
    check("the second layer: KNOB1 is the FREQ, its line where the frequency is of its ten octaves; KNOB2 is SPEED; KNOB4 has no knob: no line",
          near(knob_line(0), (int)(m.freq * VIEW_DIAL_STEPS / PLAY_FREQ_TOP)) && no_arc(0) && ring_dot(0) == -1
          && near(knob_line(1), 192) && pixels_of(VIEW_GREY, 66, 150, 52, 7) > 20
          && knob_line(3) == -1 && pixels_of(VIEW_GREY, 178, 150, 52, 7) == 0);
    m.mixer[0][MIXER_LEVEL] = 96;
    m.mixer[1][MIXER_LEVEL] = 20;
    view_show(&m);
    check("KNOB3 is the GAIN: the selected voice's level in the mixer, 96 of 127, its name under the dial; no LFO moves it: no arc, no marks",
          near(knob_line(2), 145) && pixels_of(VIEW_GREY, 122, 150, 52, 7) > 15 && no_arc(2) && ring_dot(2) == -1
          && pixels_of(VIEW_GREY, 125, 163, 48, 5) == 0 && pixels_of(VIEW_FAINT, 125, 163, 48, 5) == 0 && as_if_drawn_again(&m));
    m.touched = PANEL_ENC_VALUES + 2;
    view_show(&m);
    view_play_value_text(text, &m, PANEL_PLAY_GAIN);
    check("turned last: GAIN in white, and the value line has the level as the mixer shows it, in dB", same(text, "GAIN -12")
          && pixels_of(VIEW_WHITE, 122, 150, 52, 7) > 15 && pixels_of(VIEW_WHITE, 8, 172, 120, 14) > 100 && as_if_drawn_again(&m));
    m.mixer[0][MIXER_LEVEL] = 127;
    watch();
    view_show(&m);
    view_play_value_text(text, &m, PANEL_PLAY_GAIN);
    check("the level all up: its line at the end, GAIN 0, and only the dials and the value are drawn again", near(knob_line(2), 192)
          && same(text, "GAIN 0") && draws > 0 && top_drawn >= DIALS_TOP && bottom_drawn <= 186 && as_if_drawn_again(&m));
    m.mixer[0][MIXER_LEVEL] = 0;
    view_show(&m);
    view_play_value_text(text, &m, PANEL_PLAY_GAIN);
    check("at the bottom: OFF", near(knob_line(2), 0) && same(text, "GAIN OFF"));
    m.selected = 1;
    view_show(&m);
    check("another voice selected: its own level", near(knob_line(2), 30) && as_if_drawn_again(&m));
    m.selected = 0;
    m.mixer[0][MIXER_LEVEL] = 96;
    m.touched = PANEL_ENC_VALUES;
    view_show(&m);
    check("KNOB1 was turned last: FREQ in white under its dial, and FREQ and the hertz below, large", pixels_of(VIEW_WHITE, 10, 150, 52, 7) > 15
          && pixels_of(VIEW_WHITE, 8, 172, 120, 14) > 100 && pixels_of(VIEW_WHITE, 128, 172, 104, 14) == 0);
    view_play_value_text(text, &m, PANEL_PLAY_FREQ);
    check("the value line: FREQ and the hertz, without the unit", !strncmp(text, "FREQ ", 5) && !strchr(text + 5, ' ') && strlen(text) <= 10);
    m.freq += 128u;
    watch();
    view_show(&m);
    check("the frequency a semitone up: the top line, the FREQ dial and the value line are drawn again, no more", draws > 0 && bottom_drawn <= 186
          && as_if_drawn_again(&m));
    m.freq -= 128u;
    view_show(&m);
    save(directory, "9_play_shift");
    m.link_kind[0] = DRONE_LINK_THROUGH;
    view_show(&m);
    check("the shift key down on a THROUGH voice: KNOB4 is OWN, all of it at the start", near(knob_line(3), 192)
          && pixels_of(VIEW_GREY, 178, 150, 52, 7) > 10);
    m.knob[PLAY_OWN] = 0;
    m.touched = PANEL_ENC_VALUES + 3;
    view_show(&m);
    check("OWN at 0: its line at the start, and its name and value are below", near(knob_line(3), 0)
          && pixels_of(VIEW_WHITE, 8, 172, 120, 14) > 60);
    save(directory, "9_play_own");
    m.link_kind[0] = DRONE_LINK_FM;
    view_show(&m);
    check("another kind of link: KNOB4 has no knob with shift", pixels_of(VIEW_GREY, 178, 150, 52, 7) == 0 && pixels_of(VIEW_WHITE, 178, 150, 52, 7) == 0
          && pixels_of(VIEW_WHITE, 8, 172, 120, 14) == 0);
    m.layer = 0;
    m.touched = PANEL_ENC_VALUES;
    m.knob[PLAY_OWN] = 127;
    m.selected = 1;
    m.link[1] = 127;
    m.link[0] = 64;
    m.link_kind[1] = DRONE_LINK_FOLLOW;
    view_show(&m);
    i = (uint32_t)pixels_of(VIEW_AMBER, 60, 201, 44, 4);
    m.link_kind[1] = DRONE_LINK_FM;
    view_show(&m);
    check("FM: the arrow's line is in dots, shorter than FOLLOW's dashes", pixels_of(VIEW_AMBER, 60, 201, 44, 4) >= 80
          && pixels_of(VIEW_AMBER, 60, 201, 44, 4) <= 96 && i > 100 && i < 44 * 4);
    check("and its name is at the right of the knob's value", pixels_of(VIEW_GREY, 136, 172, 96, 14) > 30);
    save(directory, "9_play_fm");
    m.selected = 0;
    m.link[0] = m.link[1] = 0;
    m.link_kind[0] = m.link_kind[1] = DRONE_LINK_FOLLOW;
    view_show(&m);
    m.page = PANEL_DELAY;
    view_show(&m);
    check("the GLO key: the end of the chain in the play page's place, no dial left", pixels_of(VIEW_AMBER, 20, 112, 32, 32) == 0
          && pixels_of(view_grey(92), 0, DIALS_TOP, 240, DIALS_H) == 0);
    m.page = PANEL_PLAY;
    view_show(&m);
    check("and back: the play page again", contour_of(3) > 0 && near(knob_line(3), 0) && pixels_of(VIEW_WHITE, 56, 4, 60, 18) > 60);
    /* ---- the mixer ---- */
    view_pan_text(text, MIXER_PAN_CENTRE);
    check("the pan in the centre is 0", same(text, "0"));
    view_pan_text(text, 0);
    check("all to the left: L 64", same(text, "L 64"));
    view_pan_text(text, 128);
    check("all to the right: R 64, as far", same(text, "R 64"));
    view_level_text(text, 127);
    check("the fader at the top: 0 dB", same(text, "0"));
    view_level_text(text, 96);
    check("at its start: 12 dB down", same(text, "-12"));
    view_level_text(text, 0);
    check("at the bottom: off", same(text, "OFF"));
    m = plain();
    m.page = PANEL_MIXER;
    m.selected = DRONE_VOICES;                  /* no channel selected */
    for (i = 0; i < DRONE_VOICES; i++) {
        uint32_t p;
        for (p = 0; p < MIXER_NVALUE; p++)
            m.mixer[i][p] = mixer_spec[p].start;
    }
    view_reset();
    view_show(&m);
    check("the mixer: MIX in the voice's place, a strip over each voice's box, none with the amber frame, no amber mark below",
          pixels_of(VIEW_WHITE, 8, 4, 40, 18) > 300 && pixels_of(VIEW_DIM, 8, 28, 72, 1) == 72 && pixels_of(VIEW_DIM, 160, 28, 72, 1) == 72
          && pixels_of(VIEW_AMBER, 0, 24, 240, 216) == 0 && pixels_of(VIEW_WHITE, 8, 212, 72, 18) > 1200);
    check("no channel selected: the three levels in the row below the strips", pixels_of(VIEW_WHITE, 8, 176, 56, 14) > 30
          && pixels_of(VIEW_WHITE, 124, 176, 56, 14) > 30 && pixels_of(VIEW_WHITE, 182, 176, 56, 14) == 0);
    save(directory, "10_mixer");
    watch();
    view_show(&m);
    check("the mixer, nothing changed: nothing is drawn", draws == 0);
    low_edge = last_column_of(VIEW_WHITE, 16, 34, 60, 12);
    m.mixer[0][MIXER_PAN] = 128;
    watch();
    view_show(&m);
    check("a voice's pan to the right: its mark goes right; only its strip and the row of values are drawn again",
          last_column_of(VIEW_WHITE, 16, 34, 60, 12) > low_edge + 15 && top_drawn >= 28 && bottom_drawn <= 192
          && pixels_of(VIEW_DIM, 84, 28, 72, 1) == 72);
    m.mixer[0][MIXER_LEVEL] = 0;
    view_show(&m);
    i = (uint32_t)pixels_of(VIEW_WHITE, 16, 54, 26, 50);
    m.mixer[0][MIXER_LEVEL] = 127;
    view_show(&m);
    check("its level up: the fader's knob is in the top half of its line", (uint32_t)pixels_of(VIEW_WHITE, 16, 54, 26, 50) > i + 60);
    m.selected = 1;
    m.channel = 2;
    m.post[1] = 1;
    m.mixer[1][MIXER_SEND_DELAY] = 127;
    view_show(&m);
    check("a channel selected: its strip has the amber frame, the others not, and the amber mark is under its box",
          pixels_of(VIEW_AMBER, 84, 28, 72, 2) == 72 * 2 && pixels_of(VIEW_AMBER, 8, 28, 72, 128) == 0 && pixels_of(VIEW_AMBER, 160, 28, 72, 128) == 0
          && pixels_of(VIEW_AMBER, 84, 232, 72, 4) == 72 * 4);
    check("its four values in the row below: LEVEL, PAN, SEND 1, SEND 2", pixels_of(VIEW_WHITE, 8, 176, 56, 14) > 30
          && pixels_of(VIEW_WHITE, 66, 176, 56, 14) > 20 && pixels_of(VIEW_WHITE, 124, 176, 56, 14) > 30 && pixels_of(VIEW_WHITE, 182, 176, 56, 14) > 30);
    check("the top line says which channel, and that its sends are taken after the level", pixels_of(VIEW_GREY, 130, 4, 102, 18) > 80);
    save(directory, "11_mixer_channel");
    m.page = PANEL_DELAY;
    view_show(&m);
    check("the GLO key: the end of the chain, no strip left", pixels_of(VIEW_AMBER, 84, 100, 72, 56) == 0 && pixels_of(VIEW_BLACK, 8, 4, 40, 18) > 60);
    m.page = PANEL_MIXER;
    view_show(&m);
    check("and the mixer again", pixels_of(VIEW_AMBER, 84, 28, 72, 2) == 72 * 2);
    r = 7;
    for (i = 0; i < 3000; i++) {                /* any values on the mixer: nothing outside the screen */
        uint32_t p;
        r = r * 1664525u + 1013904223u;
        m.selected = (uint8_t)(r >> 3 & 3u);
        m.channel = (uint8_t)(r >> 6 & 7u);
        for (p = 0; p < DRONE_VOICES * MIXER_NVALUE; p++)
            m.mixer[p / MIXER_NVALUE][p % MIXER_NVALUE] = (uint8_t)(r >> (p + 8));
        for (p = 0; p < DRONE_VOICES; p++)
            m.post[p] = (uint8_t)(r >> (p + 1));
        view_show(&m);
    }
    check("the mixer with any values: nothing drawn outside the screen", !outside);
    m = plain();
    m.page = PANEL_PLAY;
    m.touched = PANEL_ENC_VALUES;
    view_show(&m);

    r = 99;
    for (i = 0; i < 3000; i++) {                /* any values on the play page: nothing outside the screen, no knob out of its list */
        uint32_t p;
        r = r * 1664525u + 1013904223u;
        m.style = (uint8_t)(r >> 8);
        m.freq = (uint16_t)(r >> 12);
        m.layer = (uint8_t)(r >> 5 & 1u);
        m.touched = (uint8_t)(r >> 20 & 15u);
        m.selected = (uint8_t)(r >> 3 & 3u);
        for (p = 0; p < PLAY_NKNOB; p++)
            m.knob[p] = (uint8_t)(r >> (p + 9));
        for (p = 0; p < DRONE_VOICES; p++) {
            m.link[p] = (uint8_t)(r >> (p + 2));
            m.link_kind[p] = (uint8_t)(r >> (p + 15));
        }
        for (p = 0; p < DRONE_SCOPE_POINTS; p++)
            m.scope[0][p] = (int8_t)(r >> (p % 24u));
        view_show(&m);
    }
    check("the play page with any values: nothing drawn outside the screen", !outside);
    /* ---- the first disruptor: the noise ---- */
    view_noise_rate_text(text, 127);
    check("the noise's RATE at the top: every frame", same(text, "44K"));
    view_noise_rate_text(text, 127 - 64);
    check("eight octaves down: 172 Hz", same(text, "172 HZ"));
    view_noise_rate_text(text, 0);
    check("at the bottom: less than one a second, in tenths", same(text, "0.7 HZ"));
    m = plain();
    m.page = PANEL_NOISE;
    m.selected = 1;
    m.voices_on = 1u << 0 | 1u << 2;
    m.touched = PANEL_ENC_VALUES + 3u;
    for (i = 0; i < DRONE_DISRUPTORS * DRONE_DIS_NPARAM; i++)
        m.dis[i / DRONE_DIS_NPARAM][i % DRONE_DIS_NPARAM] = drone_dis_spec[i / DRONE_DIS_NPARAM][i % DRONE_DIS_NPARAM].start;
    view_reset();
    view_show(&m);
    check("the noise's page: DS1 in the voice's place, NOISE, and OFF in grey while no voice takes any",
          pixels_of(VIEW_WHITE, 8, 4, 40, 18) > 300 && pixels_of(VIEW_WHITE, 56, 4, 60, 18) > 60 && pixels_of(VIEW_GREY, 190, 4, 42, 18) > 40
          && pixels_of(VIEW_GREEN, 0, 0, 240, 24) == 0);
    check("the noise has a picture, grey while it is off", pixels_of(VIEW_GREY, 8, 30, 224, 62) > 300 && pixels_of(VIEW_AMBER, 8, 30, 224, 62) == 0);
    check("RATE and BITS at the top: their lines at the end; DUST and the amount at 0: at the start; no LFO: no arc",
          near(knob_line(0), 192) && near(knob_line(1), 192) && near(knob_line(2), 0) && near(knob_line(3), 0)
          && no_arc(0) && no_arc(1) && no_arc(2) && no_arc(3));
    check("no amount: a faint line over each voice's box, no bar and no arrow", pixels_of(VIEW_FAINT, 8, 190, 224, 22) == 3 * 56 * 5
          && pixels_of(VIEW_AMBER, 0, 190, 240, 22) == 0 && pixels_of(VIEW_GREY, 0, 190, 240, 22) == 0);
    check("the amount's name and value below, large, and where the voice takes it at the right",
          pixels_of(VIEW_WHITE, 8, 172, 136, 14) > 100 && pixels_of(VIEW_GREY, 148, 172, 84, 14) > 60);
    save(directory, "12_noise_off");
    watch();
    view_show(&m);
    check("the noise's page, nothing changed: nothing is drawn", draws == 0);
    m.dis_send[DRONE_DIS_NOISE][1] = 46;
    view_show(&m);
    check("an amount into a voice that is off: still OFF, but its bar and its arrow are there, in amber: it is the selected one",
          pixels_of(VIEW_GREEN, 0, 0, 240, 24) == 0 && pixels_of(VIEW_AMBER, 84, 190, 72, 22) > 100 && near(knob_line(3), 70));
    m.voices_on |= 1u << 1;
    view_show(&m);
    check("that voice on: ON in green, and the picture in amber", pixels_of(VIEW_GREEN, 190, 4, 42, 18) > 30
          && pixels_of(VIEW_AMBER, 8, 30, 224, 62) > 300 && pixels_of(VIEW_GREY, 8, 31, 224, 60) == 0);
    save(directory, "12_noise_soft");
    low_edge = pixels_of(VIEW_AMBER, 8, 30, 224, 62);
    m.dis[DRONE_DIS_NOISE][DRONE_NOISE_RATE] = 70;
    m.dis[DRONE_DIS_NOISE][DRONE_NOISE_BITS] = 1;
    m.dis[DRONE_DIS_NOISE][DRONE_NOISE_DUST] = 70;
    m.dis_send[DRONE_DIS_NOISE][0] = 30;
    m.dis_send[DRONE_DIS_NOISE][2] = 110;
    m.dis_to[DRONE_DIS_NOISE][1] = DRONE_TO_PITCH;
    m.touched = PANEL_ENC_VALUES + 1u;
    watch();
    view_show(&m);
    check("a slow RATE: the picture has long steps, so less ink", pixels_of(VIEW_AMBER, 8, 30, 224, 62) < low_edge / 2);
    check("the other voices' amounts: bars and arrows in grey, longer for more", pixels_of(VIEW_GREY, 160, 196, 56, 5) > 2 * pixels_of(VIEW_GREY, 8, 196, 56, 5)
          && pixels_of(VIEW_GREY, 8, 196, 56, 5) > 0);
    check("the voices' boxes and the top line are not drawn again", top_drawn >= 30 && bottom_drawn <= 212);
    save(directory, "12_noise_gritty");
    m.page = PANEL_PLAY;
    view_show(&m);
    check("the FX key again: the play page, no bar left over the voices", pixels_of(VIEW_FAINT, 8, 196, 56, 5) == 0 && pixels_of(VIEW_GREEN, 0, 0, 240, 24) == 0);
    m.page = PANEL_NOISE;
    view_show(&m);
    check("and the noise again", pixels_of(VIEW_GREEN, 190, 4, 42, 18) > 30 && pixels_of(VIEW_FAINT, 8, 196, 56, 5) > 0);
    /* ---- the second and the third disruptor: the pulse and the sine ---- */
    view_dis_text(text, DRONE_DIS_CHOP, DRONE_CHOP_RATE, 0);
    check("the pulse's RATE at the bottom: one in four seconds", same(text, "0.3 HZ"));
    view_dis_text(text, DRONE_DIS_CHOP, DRONE_CHOP_RATE, 60);
    check("at 60: 8 a second", same(text, "8.2 HZ"));
    view_dis_text(text, DRONE_DIS_CHOP, DRONE_CHOP_RATE, 127);
    check("at the top: a buzz", same(text, "392 HZ"));
    view_dis_text(text, DRONE_DIS_CHOP, DRONE_CHOP_WIDTH, 63);
    check("its WIDTH in percent of the cycle", same(text, "50%"));
    view_dis_text(text, DRONE_DIS_CHOP, DRONE_CHOP_WIDTH, 127);
    check("all open: 100%", same(text, "100%"));
    view_dis_text(text, DRONE_DIS_RING, DRONE_RING_FREQ, 57);
    check("the sine's FREQ at its start: 220 Hz", same(text, "220 HZ"));
    view_dis_text(text, DRONE_DIS_RING, DRONE_RING_FREQ, 0);
    check("at the bottom: tenths", same(text, "8.2 HZ"));
    view_dis_text(text, DRONE_DIS_NOISE, DRONE_NOISE_BITS, 11);
    check("the noise's BITS count from 1", same(text, "12"));
    m = plain();
    m.page = PANEL_CHOP;
    m.selected = 1;
    m.voices_on = 7;
    m.touched = PANEL_ENC_VALUES;
    for (i = 0; i < DRONE_DISRUPTORS * DRONE_DIS_NPARAM; i++)
        m.dis[i / DRONE_DIS_NPARAM][i % DRONE_DIS_NPARAM] = drone_dis_spec[i / DRONE_DIS_NPARAM][i % DRONE_DIS_NPARAM].start;
    m.dis_send[DRONE_DIS_NOISE][0] = 80;        /* another disruptor's amount: not this page's */
    m.dis_send[DRONE_DIS_CHOP][1] = 90;
    view_reset();
    view_show(&m);
    check("the pulse's page: DS2, CHOP, ON; only its own amounts over the voices' boxes", pixels_of(VIEW_WHITE, 8, 4, 40, 18) > 300
          && pixels_of(VIEW_GREEN, 190, 4, 42, 18) > 30 && pixels_of(VIEW_AMBER, 84, 190, 72, 22) > 100 && pixels_of(VIEW_GREY, 8, 190, 72, 22) == 0);
    check("its picture: pulses, open half of the time: the top line and the bottom line have as much ink",
          pixels_of(VIEW_AMBER, 16, 38, 208, 2) > 60 && abs(pixels_of(VIEW_AMBER, 16, 38, 208, 2) - pixels_of(VIEW_AMBER, 16, 82, 208, 2)) < 40
          && pixels_of(VIEW_DIM, 16, 45, 208, 10) == 0);
    save(directory, "13_chop");
    m.dis[DRONE_DIS_CHOP][DRONE_CHOP_RATE] = 112;
    m.dis[DRONE_DIS_CHOP][DRONE_CHOP_WIDTH] = 30;
    m.dis[DRONE_DIS_CHOP][DRONE_CHOP_SKIP] = 50;
    m.dis_to[DRONE_DIS_CHOP][1] = DRONE_TO_CUTOFF;
    m.touched = PANEL_ENC_VALUES + 2u;
    view_show(&m);
    check("SKIP: the pulses left out are dim outlines", pixels_of(VIEW_DIM, 16, 45, 208, 10) > 20 && pixels_of(VIEW_AMBER, 16, 45, 208, 10) > 20);
    save(directory, "13_chop_skips");
    m.page = PANEL_RING;
    m.touched = PANEL_ENC_VALUES;
    watch();
    view_show(&m);
    check("the FX key: the sine's page, all of it drawn again but the voices' boxes; no voice takes any: OFF, the picture grey",
          top_drawn <= 6 && bottom_drawn <= 212 && pixels_of(VIEW_GREEN, 0, 0, 240, 24) == 0 && pixels_of(VIEW_GREY, 8, 31, 224, 60) > 300
          && pixels_of(VIEW_AMBER, 8, 30, 224, 62) == 0 && pixels_of(VIEW_AMBER, 0, 190, 240, 22) == 0);
    m.dis_send[DRONE_DIS_RING][2] = 100;
    m.selected = 2;
    view_show(&m);
    low_edge = pixels_of(VIEW_AMBER, 8, 31, 224, 60);
    check("an amount: ON, the sine in amber", pixels_of(VIEW_GREEN, 190, 4, 42, 18) > 30 && low_edge > 300);
    save(directory, "14_ring");
    m.dis[DRONE_DIS_RING][DRONE_RING_FREQ] = 70;
    m.dis[DRONE_DIS_RING][DRONE_RING_FOLD] = 96;
    m.dis[DRONE_DIS_RING][DRONE_RING_DRIFT] = 90;
    m.dis_to[DRONE_DIS_RING][2] = DRONE_TO_PITCH;
    m.touched = PANEL_ENC_VALUES + 1u;
    view_show(&m);
    check("FOLD: the sine goes up and down more, so more ink", pixels_of(VIEW_AMBER, 8, 31, 224, 60) > low_edge * 3 / 2);
    save(directory, "14_ring_folded");
    outside = 0;
    r = 11;
    for (i = 0; i < 3000; i++) {                /* any values on the noise's page: nothing outside the screen */
        uint32_t p;
        r = r * 1664525u + 1013904223u;
        m.selected = (uint8_t)(r >> 3 & 3u);
        m.voices_on = (uint8_t)(r >> 5);
        m.touched = (uint8_t)(r >> 9 & 7u);
        m.page = (uint8_t)(PANEL_NOISE + i % DRONE_DISRUPTORS);
        for (p = 0; p < DRONE_DISRUPTORS * DRONE_DIS_NPARAM; p++)
            m.dis[p / DRONE_DIS_NPARAM][p % DRONE_DIS_NPARAM] = (uint8_t)(r >> (p + 8));
        for (p = 0; p < DRONE_DISRUPTORS * DRONE_VOICES; p++) {
            m.dis_send[p / DRONE_VOICES][p % DRONE_VOICES] = (uint8_t)(r >> (p + 12));
            m.dis_to[p / DRONE_VOICES][p % DRONE_VOICES] = (uint8_t)(r >> (p + 17));
        }
        view_show(&m);
    }
    check("the noise's page with any values: nothing drawn outside the screen", !outside);

    /* ---- the bank's page (V0.32; its sets, pattern and loop V0.33) ---- */
    {
        uint32_t frames = engine_cycle_frames(40), fast = engine_cycle_frames(127);
        check("a free bank LFO's cycle, as the engine runs it: 6 s at 40, 1/24 s at 127",
              frames / 100u == view_bank_cycle_frames(40) / 100u && fast * 100u / view_bank_cycle_frames(127) >= 99u
              && fast * 100u / view_bank_cycle_frames(127) <= 101u);
    }
    view_bank_rate_text(text, 40);
    check("RATE 40: seconds a cycle, one decimal", same(text, "6.2 S"));
    view_bank_rate_text(text, 0);
    check("RATE 0: the slowest", same(text, "62.6 S"));
    view_bank_rate_text(text, 127);
    check("RATE 127: cycles a second", same(text, "24.5 HZ"));
    view_bank_rate_text(text, 72);
    check("from a second a cycle: seconds", same(text, "1.0 S"));
    view_bank_rate_text(text, 73);
    check("below a second a cycle: cycles a second", same(text, "1.1 HZ"));
    view_bank_ratio_text(text, 0);
    check("a synced LFO's ratio: 32 beats a cycle", same(text, "32 BEATS"));
    view_bank_ratio_text(text, 5);
    check("2 beats", same(text, "2 BEATS"));
    view_bank_ratio_text(text, 6);
    check("1 beat", same(text, "1 BEAT"));
    view_bank_ratio_text(text, 7);
    check("then cycles a beat", same(text, "X2"));
    view_bank_ratio_text(text, DRONE_BANK_RATIOS - 1u);
    check("16 a beat", same(text, "X16"));
    m = bank_model();
    view_bank_value_text(text, &m);
    check("the value line: RATE and its time", same(text, "RATE 6.2 S"));
    m.touched = DRONE_BANK_PHASE;
    bank_value(&m, DRONE_BANK_PHASE, 32);
    view_bank_value_text(text, &m);
    check("PHASE in degrees", same(text, "PHASE 90"));
    bank_value(&m, DRONE_BANK_PHASE, 127);
    view_bank_value_text(text, &m);
    check("PHASE at its top: 357, with no sign", same(text, "PHASE 357"));
    m.touched = PANEL_BANK_TEMPO;
    view_bank_value_text(text, &m);
    check("the tempo", same(text, "TEMPO 60 BPM"));
    m.touched = DRONE_BANK_RANGE;
    bank_value(&m, DRONE_BANK_RANGE, 1);
    view_bank_value_text(text, &m);
    check("RANGE UP", same(text, "RANGE UP"));
    m.touched = DRONE_BANK_SYNC;
    bank_value(&m, DRONE_BANK_SYNC, 1);
    view_bank_value_text(text, &m);
    check("SYNC ON", same(text, "SYNC ON"));
    m.touched = DRONE_BANK_RATIO;
    view_bank_value_text(text, &m);
    check("a synced LFO's RATE is its ratio", same(text, "RATE 1 BEAT"));
    m.touched = DRONE_BANK_WIDTH;
    view_bank_value_text(text, &m);
    check("the WIDTH in percent", same(text, "WIDTH 50%"));
    m.touched = DRONE_BANK_SHAPE;
    bank_value(&m, DRONE_BANK_SHAPE, DRONE_BANK_SOFT_RANDOM);
    view_bank_value_text(text, &m);
    check("the shape", same(text, "SHAPE SOFT RANDOM"));
    m.touched = DRONE_BANK_LEVEL;
    bank_value(&m, DRONE_BANK_LEVEL, 100);
    view_bank_value_text(text, &m);
    check("the level", same(text, "LEVEL 100%"));

    m = bank_model();
    m.bank_phase = 0x40000000u;                 /* a quarter of its cycle */
    m.bank_out[0] = 32767;
    m.bank_used = 1u << 3;
    view_reset();
    view_show(&m);
    check("the bank's page: LFO 1 in amber in the top line, no voice's box; FREE and the tempo, small, at the right",
          pixels_of(VIEW_AMBER, 8, 4, 80, 18) > 60 && pixels_of(VIEW_WHITE, 8, 4, 40, 18) == 0 && pixels_of(VIEW_GREY, 196, 4, 36, 8) > 10
          && pixels_of(VIEW_GREY, 180, 14, 52, 8) > 20);
    check("the clock at the start of its beat: the small square is lit", pixels_of(VIEW_WHITE, 150, 14, 50, 8) == 25);
    check("the shape: SINE, from the bottom up to the top at the middle of each step and down again, two steps",
          pixels_of(VIEW_WHITE, 8, VIEW_BANK_Y + 52, 8, 12) > 0 && pixels_of(VIEW_WHITE, 56, VIEW_BANK_Y, 16, 8) > 0 && pixels_of(VIEW_WHITE, 8, VIEW_BANK_Y, 24, 20) == 0
          && pixels_of(VIEW_WHITE, 168, VIEW_BANK_Y, 16, 8) > 0 && pixels_of(VIEW_WHITE, 116, VIEW_BANK_Y + 52, 8, 12) > 0);
    check("a dim line between the two cycles", pixels_of(VIEW_DIM, 120, VIEW_BANK_Y, 1, 64) > 30);
    check("the amber mark at a quarter of the cycle, and its square at the output", pixels_of(VIEW_AMBER, 36, VIEW_BANK_Y, 1, 64) >= 60
          && pixels_of(VIEW_AMBER, 34, VIEW_BANK_Y + 3, 5, 5) == 25 && pixels_of(VIEW_AMBER, 40, VIEW_BANK_Y, 192, 64) == 0);
    check("the RATE, large, below it, and the wave's name in grey at the right", pixels_of(VIEW_WHITE, 8, VIEW_BANK_VALUE_Y, 150, 14) > 100
          && pixels_of(VIEW_GREY, 170, VIEW_BANK_VALUE_Y, 62, 14) > 40);
    check("four dials, the WAVE set: the RATE at 40 of 127, the WIDTH at half, the LEVEL all up, the OFFSET at its middle",
          near(knob_line(0), 60) && near(knob_line(1), 96) && near(knob_line(2), 192) && near(knob_line(3), 96));
    check("their names: RATE in white, it was turned last; the others in grey", pixels_of(VIEW_WHITE, 10, 150, 52, 7) > 15
          && pixels_of(VIEW_GREY, 66, 150, 52, 7) > 15 && pixels_of(VIEW_GREY, 178, 150, 52, 7) > 15 && pixels_of(VIEW_WHITE, 66, 150, 166, 7) == 0);
    check("no LFO has an amount on them: no arc, no dot, twelve low marks under each", no_arc(0) && no_arc(1) && no_arc(2) && no_arc(3)
          && ring_dot(0) == -1 && ring_dot(1) == -1 && ring_dot(2) == -1 && ring_dot(3) == -1
          && pixels_of(VIEW_FAINT, 13, 166, 48, 2) == 12 * 6 && pixels_of(VIEW_GREY, 0, 163, 240, 5) == 0);
    check("the top line: the three sets, WAVE in hand, white with an amber line under it; STEPS and LOOP grey",
          pixels_of(VIEW_WHITE, 84, 8, 23, 7) > 20 && pixels_of(VIEW_AMBER, 84, 18, 23, 2) == 46 && pixels_of(VIEW_AMBER, 108, 18, 80, 2) == 0
          && pixels_of(VIEW_GREY, 112, 8, 29, 7) > 20 && pixels_of(VIEW_GREY, 146, 8, 23, 7) > 20 && pixels_of(VIEW_WHITE, 110, 8, 70, 7) == 0);
    check("the row of the twelve LFOs: the shown one in the amber frame", pixels_of(VIEW_AMBER, 7, VIEW_BANK_ROW_Y, 19, 1) == 19
          && pixels_of(VIEW_AMBER, 26, VIEW_BANK_ROW_Y, 214, 22) == 0);
    check("its bar at the top: the output all up, clear of the number", pixels_of(VIEW_GREY, 13, VIEW_BANK_ROW_Y + 10, 7, 4) == 28
          && pixels_of(VIEW_GREY, 13, VIEW_BANK_ROW_Y + 9, 7, 1) == 0);
    check("an LFO a voice has an amount of: its number brighter", pixels_of(VIEW_WHITE, 8 + 3 * 19, VIEW_BANK_ROW_Y + 2, 18, 7) > 10
          && pixels_of(VIEW_WHITE, 8 + 4 * 19, VIEW_BANK_ROW_Y + 2, 18, 7) == 0 && pixels_of(VIEW_GREY, 8 + 4 * 19, VIEW_BANK_ROW_Y + 2, 18, 7) > 10);
    check("a free LFO: no line under it", pixels_of(VIEW_WHITE, 8, VIEW_BANK_ROW_Y + 18, 224, 2) == 0);
    check("the voices' boxes below, as on every page", pixels_of(VIEW_WHITE, 8, 212, 72, 18) > 1200 && !outside);
    save(directory, "10_bank");
    watch();
    view_show(&m);
    check("the bank's page, nothing changed: nothing is drawn", draws == 0);
    m.bank_phase = 0x60000000u;
    watch();
    view_show(&m);
    check("the LFO moved on: only its mark is drawn again, where it was and where it is",
          draws > 0 && draws <= 2 * 5 * 64 && top_drawn >= (int)VIEW_BANK_Y && bottom_drawn <= (int)VIEW_BANK_Y + 64 && pixels_of(VIEW_AMBER, 36, VIEW_BANK_Y, 1, 64) == 0
          && pixels_of(VIEW_AMBER, 50, VIEW_BANK_Y, 1, 64) >= 50);
    m.bank_out[0] = 0;
    m.bank_out[5] = 20000;
    watch();
    view_show(&m);
    check("the outputs moved: the square and the row's bars, no more", draws > 0 && top_drawn >= (int)VIEW_BANK_Y && bottom_drawn <= (int)VIEW_BANK_ROW_Y + 22
          && pixels_of(VIEW_GREY, 8 + 5 * 19 + 5, VIEW_BANK_ROW_Y + 12, 7, 2) == 14 && pixels_of(VIEW_GREY, 13, VIEW_BANK_ROW_Y + 9, 7, 4) == 0);
    m.clock_phase = 0x80000000u;
    watch();
    view_show(&m);
    check("the clock in the second half of its beat: the square is off, nothing else is drawn",
          pixels_of(VIEW_WHITE, 150, 14, 50, 8) == 0 && draws > 0 && bottom_drawn <= 23);
    m.tempo = 120;
    m.touched = PANEL_BANK_TEMPO;
    view_show(&m);
    check("PRESETS turned: the tempo at the right of the top line and on the value line", pixels_of(VIEW_GREY, 174, 14, 58, 8) > 30
          && pixels_of(VIEW_WHITE, 8, VIEW_BANK_VALUE_Y, 224, 14) > 100);
    m.touched = DRONE_BANK_SLEW;
    watch();
    view_show(&m);
    check("another value turned last: its line and the dials' names, no more", draws > 0 && top_drawn >= (int)VIEW_BANK_VALUE_Y - 2 && bottom_drawn <= 158);
    m.touched = DRONE_BANK_SHAPE;
    view_show(&m);
    check("the SHAPE turned last: its name is not twice on the line", pixels_of(VIEW_GREY, 170, VIEW_BANK_VALUE_Y, 62, 14) == 0);
    m.touched = DRONE_BANK_PHASE;
    m.bank_set = PANEL_BANK_MORE;
    bank_value(&m, DRONE_BANK_PHASE, 32);
    view_show(&m);
    check("the MORE layer: the first dial is the PHASE, in white; its short name in hand in the top line; the row of the twelve LFOs stays",
          near(knob_line(0), 48) && pixels_of(VIEW_WHITE, 10, 150, 52, 7) > 15 && pixels_of(VIEW_AMBER, 174, 18, 5, 2) == 10
          && pixels_of(VIEW_AMBER, 84, 18, 88, 2) == 0 && pixels_of(VIEW_AMBER, 7, VIEW_BANK_ROW_Y, 19, 1) == 19 && as_if_drawn_again(&m));
    check("its values take no LFO: no marks under its dials", pixels_of(VIEW_FAINT, 0, 163, 240, 5) == 0 && pixels_of(VIEW_GREY, 0, 163, 240, 5) == 0);
    save(directory, "10_bank_more");
    m.bank_set = PANEL_BANK_WAVE;
    m.bank_lfo = 11;
    view_show(&m);
    check("the twelfth LFO: its number in the top line, the amber frame on the last cell, and no more", pixels_of(VIEW_AMBER, 8, 4, 80, 18) > 80
          && pixels_of(VIEW_AMBER, 7 + 11 * 19, VIEW_BANK_ROW_Y, 19, 1) == 19 && pixels_of(VIEW_AMBER, 7, VIEW_BANK_ROW_Y, 19, 1) == 0 && !outside);

    m = bank_model();                           /* a synced GATE with a slew: soft edges */
    bank_value(&m, DRONE_BANK_SYNC, 1);
    bank_value(&m, DRONE_BANK_RATIO, 3);            /* 4 beats a cycle */
    bank_value(&m, DRONE_BANK_SHAPE, DRONE_BANK_GATE);
    bank_value(&m, DRONE_BANK_SLEW, 60);
    m.bank_synced = 1u << 0 | 1u << 2;
    m.bank_phase = 0x20000000u;
    m.bank_out[0] = 9000;
    m.bank_out[2] = -32767;
    m.touched = DRONE_BANK_RATIO;
    view_show(&m);
    check("a synced LFO: SYNC in white at the right of the top line", pixels_of(VIEW_WHITE, 196, 4, 36, 8) > 10);
    check("slower than a beat: a small tick for each beat, four a cycle", pixels_of(VIEW_GREY, 8, VIEW_BANK_Y + 61, 224, 3) >= 8 * 3
          && pixels_of(VIEW_GREY, 8, VIEW_BANK_Y + 61, 224, 3) <= 9 * 3);
    check("the slew: the gate's rise at the next cycle is soft, not at once", pixels_of(VIEW_WHITE, 125, VIEW_BANK_Y + 48, 1, 8) > 0
          && pixels_of(VIEW_WHITE, 125, VIEW_BANK_Y + 2, 1, 6) == 0);
    check("a short line under each synced LFO in the row", pixels_of(VIEW_WHITE, 14, VIEW_BANK_ROW_Y + 18, 5, 2) == 10
          && pixels_of(VIEW_WHITE, 14 + 2 * 19, VIEW_BANK_ROW_Y + 18, 5, 2) == 10 && pixels_of(VIEW_WHITE, 14 + 19, VIEW_BANK_ROW_Y + 18, 5, 2) == 0);
    check("an output all down: its bar below the middle", pixels_of(VIEW_GREY, 13 + 2 * 19, VIEW_BANK_ROW_Y + 13, 7, 4) == 28
          && pixels_of(VIEW_GREY, 13 + 2 * 19, VIEW_BANK_ROW_Y + 17, 7, 1) == 0);
    check("its RATE dial: the ratio, 3 of 11", near(knob_line(0), 52));
    save(directory, "10_bank_sync_gate");
    bank_value(&m, DRONE_BANK_SLEW, 0);
    view_show(&m);
    check("no slew: the rise is at once", pixels_of(VIEW_WHITE, 125, VIEW_BANK_Y + 48, 1, 8) == 0 && pixels_of(VIEW_WHITE, 125, VIEW_BANK_Y + 2, 1, 6) > 0);

    m = bank_model();                           /* a GATE up only, not at its full level */
    bank_value(&m, DRONE_BANK_SHAPE, DRONE_BANK_GATE);
    bank_value(&m, DRONE_BANK_RANGE, 1);
    bank_value(&m, DRONE_BANK_LEVEL, 60);
    bank_value(&m, DRONE_BANK_WIDTH, 40);
    m.bank_phase = 0xA0000000u;
    m.bank_out[0] = 0;
    m.touched = DRONE_BANK_WIDTH;
    view_show(&m);
    check("RANGE UP: nothing below the middle", pixels_of(VIEW_WHITE, 8, VIEW_BANK_Y + 35, 224, 29) == 0 && pixels_of(VIEW_WHITE, 8, VIEW_BANK_Y + 30, 224, 4) > 60);
    check("less level: it does not go to the top", pixels_of(VIEW_WHITE, 8, VIEW_BANK_Y, 224, 14) == 0 && pixels_of(VIEW_WHITE, 8, VIEW_BANK_Y + 14, 224, 6) > 40);
    check("its WIDTH: up for less than half of the cycle", pixels_of(VIEW_WHITE, 8, VIEW_BANK_Y + 14, 50, 6) > 40 && pixels_of(VIEW_WHITE, 70, VIEW_BANK_Y + 14, 50, 6) == 0);
    check("WIDTH under the second dial, in white: it was turned last", pixels_of(VIEW_WHITE, 66, 150, 52, 7) > 15);
    save(directory, "10_bank_gate_up");
    m.page = PANEL_PLAY;
    view_show(&m);
    check("back on the play page: no LFO's name or row left, the voice's box again", pixels_of(VIEW_AMBER, 0, 4, 240, 18) == 0
          && pixels_of(VIEW_WHITE, 8, 4, 40, 18) > 300 && pixels_of(VIEW_GREY, 0, 186, 240, 12) == 0 && pixels_of(VIEW_DIM, 120, VIEW_BANK_Y, 1, 64) < 20);

    /* ---- the values' texts (V0.33) ---- */
    {
        static const char *const shapes[DRONE_BANK_NSHAPE] = { "GATE", "RATCHET 2", "RATCHET 4", "SOFT GATE", "TRIANGLE", "TRAPEZOID",
                                                               "SINE", "HUMP", "EXP", "SOFT EXP", "LOG", "SOFT LOG", "RANDOM", "SOFT RANDOM" };
        static const struct { uint8_t param, value, sync; const char *text; } line[] = {
            { DRONE_BANK_OFFSET, 120, 0, "OFFSET +20%" }, { DRONE_BANK_OFFSET, 100, 0, "OFFSET 0%" }, { DRONE_BANK_OFFSET, 70, 0, "OFFSET -30%" },
            { DRONE_BANK_OFFSET, 255, 0, "OFFSET +100%" }, { DRONE_BANK_PROB, 60, 0, "PROB 60%" }, { DRONE_BANK_SLEW, 40, 0, "SLEW 40" },
            { DRONE_BANK_STEPS, 0, 0, "STEPS OFF" }, { DRONE_BANK_STEPS, 8, 0, "STEPS 8" }, { DRONE_BANK_TRIGS, 3, 0, "TRIGS 3" },
            { DRONE_BANK_TRIGS, 0, 0, "TRIGS 1" }, { DRONE_BANK_PAD, 1, 0, "PAD 1" }, { DRONE_BANK_RAND, 2, 0, "RAND 2" },
            { DRONE_BANK_SHIFT, 5, 0, "SHIFT 5" }, { DRONE_BANK_LOOP, 0, 1, "LOOP OFF" }, { DRONE_BANK_LOOP, 4, 1, "LOOP 4 BEATS" },
            { DRONE_BANK_LOOP, 1, 1, "LOOP 1 BEAT" }, { DRONE_BANK_LOOP, 4, 0, "LOOP 4 CYCLES" }, { DRONE_BANK_LOOP, 1, 0, "LOOP 1 CYCLE" },
            { DRONE_BANK_LOOP, 99, 0, "LOOP 32 CYCLES" }, { DRONE_BANK_NAP, 0, 0, "NAP OFF" }, { DRONE_BANK_NAP, 1, 0, "NAP 1 LOOP" },
            { DRONE_BANK_NAP, 3, 0, "NAP 3 LOOPS" }, { DRONE_BANK_WAKE, 2, 0, "WAKE 2 LOOPS" }, { DRONE_BANK_WAKE, 1, 0, "WAKE 1 LOOP" },
            { DRONE_BANK_WAKE, 0, 0, "WAKE 1 LOOP" }, { DRONE_BANK_LSHIFT, 2, 0, "L SHIFT 2" }, { DRONE_BANK_RANGE, 0, 0, "RANGE BOTH" },
            { DRONE_BANK_RANGE, 1, 0, "RANGE UP" }, { DRONE_BANK_SYNC, 0, 0, "SYNC OFF" }, { DRONE_BANK_SHAPE, DRONE_BANK_EXP, 0, "SHAPE EXP" },
            { DRONE_BANK_SHAPE, 200, 0, "SHAPE SOFT RANDOM" },
        };
        uint32_t k, names = 1;
        for (k = 0; k < DRONE_BANK_NSHAPE; k++)
            names &= !strcmp(view_bank_shape_name[k], shapes[k]);
        check("the fourteen shapes' names", names);
        for (k = 0; k < sizeof line / sizeof line[0]; k++) {
            m = bank_model();
            m.touched = line[k].param;
            bank_value(&m, DRONE_BANK_SYNC, line[k].sync);
            bank_value(&m, line[k].param, line[k].value);
            view_bank_value_text(text, &m);
            check(line[k].text, same(text, line[k].text));
        }
        m = bank_model();
        m.lfo_held = 1;
        m.bank_source = 3;
        m.bank_cabled[1][3] = 40;
        m.bank_cabled[1][4] = -70;
        m.bank_tempo_amount = -20;
        m.amount_touched = PANEL_ENC_VALUES + 1;
        view_bank_value_text(text, &m);
        check("the LFO key held: the value KNOB2 holds and the source's amount on it", same(text, "WIDTH +40"));
        m.bank_source = 4;
        view_bank_value_text(text, &m);
        check("another source: its own", same(text, "WIDTH -70"));
        m.bank_set = PANEL_BANK_STEPS;
        view_bank_value_text(text, &m);
        check("the STEPS set: KNOB2 is onto STEPS", same(text, "STEPS -70"));
        m.bank_set = PANEL_BANK_LOOP;
        m.amount_touched = PANEL_ENC_VALUES + 3;
        m.bank_cabled[3][4] = 100;
        view_bank_value_text(text, &m);
        check("the LOOP set's KNOB4: onto L SHIFT", same(text, "L SHIFT +100"));
        m.bank_set = PANEL_BANK_MORE;
        view_bank_value_text(text, &m);
        check("the MORE layer's knobs hold no amount: nothing on the line", same(text, ""));
        m.bank_set = PANEL_BANK_LOOP;
        m.amount_touched = PANEL_ENC_MOVE;
        view_bank_value_text(text, &m);
        check("PRESETS: onto the tempo", same(text, "TEMPO -20"));
    }

    /* ---- the WAVE set: a synced RANDOM in a loop of 4 beats, from the engine (V0.33) ---- */
    engine_start();
    engine_controls.bank[0][DRONE_BANK_SYNC] = 1;
    engine_controls.bank[0][DRONE_BANK_SHAPE] = DRONE_BANK_RANDOM;
    engine_controls.bank[0][DRONE_BANK_WIDTH] = 60;
    engine_controls.bank[0][DRONE_BANK_LOOP] = 4;
    engine_controls.bank[0][DRONE_BANK_STEPS] = 4;  /* cells 0 and 2 are trigs: step 3 does not play, it holds step 2's height */
    engine_controls.bank[0][DRONE_BANK_TRIGS] = 2;
    check("the engine gets to the last step of the second loop", engine_run_to(0, 3, 1, 0x50000000u));
    m = bank_model();
    engine_take(&m, 0, 0, PANEL_BANK_WAVE);
    check("the model takes the shown LFO from the engine: its values, where it is", m.bank_value[DRONE_BANK_SHAPE] == DRONE_BANK_RANDOM
          && m.bank_step == 3 && m.bank_loop == 1 && m.bank_knob[DRONE_BANK_LOOP] == 4 && m.bank_synced == 1u && m.tempo == DRONE_TEMPO_START);
    view_reset();
    view_show(&m);
    {
        int columns = picture_follows_engine(&m, 0);
        check("the wave: a step that does not play holds the height of the last one that did; then the loop starts again with "
              "its first step's random height, as the engine plays them", columns > 150);
        if (columns <= 150)
            printf("  %d columns\n", columns);
    }
    {
        uint8_t *v = bank_engine.bank.view.value[0];
        int32_t held = drone_bank_held_after(0, v, 2, 1, 0), first = drone_bank_shape(DRONE_BANK_RANDOM, 0, 60, drone_bank_random(0, 0, DRONE_BANK_USE_HEIGHT), 0);
        check("step 3 does not play, step 0 does", !drone_bank_plays(0, v, 3, 1) && drone_bank_plays(0, v, 0, 2) && held == m.bank_held);
        check("the two heights on the screen: step 2's, held, then step 0's", pixels_of(VIEW_WHITE, 8 + 45, VIEW_BANK_Y + view_bank_y(2 * held - 32767), 60, 1) == 60
              && pixels_of(VIEW_WHITE, 8 + 112 + 8, VIEW_BANK_Y + view_bank_y(2 * first - 32767), 100, 1) == 100);
    }
    check("a step before: step 2 plays, step 3 after it holds step 2's height at its end", engine_run_to(0, 2, 1, 0xA0000000u));
    engine_take(&m, 0, 0, PANEL_BANK_WAVE);
    view_show(&m);
    check("the wave, as the engine plays it", picture_follows_engine(&m, 0) > 150 && as_if_drawn_again(&m));
    engine_controls.bank[0][DRONE_BANK_STEPS] = 0;
    engine_controls.bank[0][DRONE_BANK_SLEW] = 24;
    check("then every step plays, with a slew", engine_run_to(0, 3, 1, 0x50000000u));
    m = bank_model();
    engine_take(&m, 0, 0, PANEL_BANK_WAVE);
    m.touched = DRONE_BANK_RATIO;
    m.bank_used = 1u << 0;
    view_reset();
    view_show(&m);
    check("its RATE: one beat a step, the shape's name at the right", pixels_of(VIEW_WHITE, 8, VIEW_BANK_VALUE_Y, 150, 14) > 100 && pixels_of(VIEW_GREY, 150, VIEW_BANK_VALUE_Y, 82, 14) > 60);
    check("the loop goes round: the next step's height is step 0's", pixels_of(VIEW_WHITE, 8 + 112 + 60, VIEW_BANK_Y + view_bank_y(2 * drone_bank_shape(DRONE_BANK_RANDOM, 0, 60,
          drone_bank_random(0, 0, DRONE_BANK_USE_HEIGHT), 0) - 32767), 52, 1) == 52);
    save(directory, "10_bank_wave");
    {
        int32_t h2 = 2 * drone_bank_shape(DRONE_BANK_RANDOM, 0, 60, drone_bank_random(0, 2, DRONE_BANK_USE_HEIGHT), 0) - 32767;
        int32_t h3 = 2 * drone_bank_shape(DRONE_BANK_RANDOM, 0, 60, drone_bank_random(0, 3, DRONE_BANK_USE_HEIGHT), 0) - 32767;
        check("the slew: this step starts at the height of the step before, and goes to its own", (h2 - h3 > 8000 || h3 - h2 > 8000)
              && screen[VIEW_BANK_Y + view_bank_y(h2)][8] == VIEW_WHITE && pixels_of(VIEW_WHITE, 8 + 60, VIEW_BANK_Y + view_bank_y(h3), 40, 1) == 40);
    }

    /* ---- the STEPS set: an 8-step pattern of 3 trigs with a pad and a random trig, on a GATE (V0.33) ---- */
    engine_start();
    engine_controls.bank[2][DRONE_BANK_SYNC] = 1;
    engine_controls.bank[2][DRONE_BANK_RATIO] = 7;  /* two steps a beat */
    engine_controls.bank[2][DRONE_BANK_SHAPE] = DRONE_BANK_GATE;
    engine_controls.bank[2][DRONE_BANK_RANGE] = 0;
    engine_controls.bank[2][DRONE_BANK_STEPS] = 8;
    engine_controls.bank[2][DRONE_BANK_TRIGS] = 3;
    engine_controls.bank[2][DRONE_BANK_PAD] = 1;
    engine_controls.bank[2][DRONE_BANK_RAND] = 1;
    check("the engine gets to step 11: the pattern's third cell, after one round of its nine", engine_run_to(2, 11, 0, 0x30000000u));
    m = bank_model();
    engine_take(&m, 2, 0, PANEL_BANK_STEPS);
    m.bank_set = PANEL_BANK_STEPS;
    m.touched = DRONE_BANK_TRIGS;
    view_reset();
    view_show(&m);
    {
        uint32_t i, kinds = 0, ok = 1;
        for (i = 0; i < 9u; i++) {              /* a cell is 13 wide here */
            int x = 8 + 13 * (int)i;
            uint32_t cell = drone_bank_cell(2, bank_engine.bank.view.value[2], i);
            kinds |= 1u << (cell == DRONE_BANK_EMPTY && i == 8u ? 3u : cell);
            if (cell == DRONE_BANK_TRIG)
                ok &= pixels_of(VIEW_WHITE, x + 1, 180, 10, 16) == 160;
            else if (cell == DRONE_BANK_RANDOM_TRIG)
                ok &= pixels_of(VIEW_GREY, x + 1, 180, 10, 16) == 160;
            else if (i == 8u)
                ok &= pixels_of(VIEW_GREY, x + 5, 187, 2, 2) == 4 && pixels_of(VIEW_GREY, x + 1, 180, 10, 16) == 4;
            else
                ok &= pixels_of(VIEW_DIM, x + 1, 180, 10, 16) == 48 && pixels_of(VIEW_WHITE, x + 1, 180, 10, 16) == 0;
            ok &= i == 2u ? pixels_of(VIEW_AMBER, x - 1, 178, 14, 1) == 14 : pixels_of(VIEW_AMBER, x, 178, 11, 20) == 0;
        }
        check("the pattern row: white trigs, a grey random trig, a dot for the pad, dim frames for the empty cells; the amber frame on the "
              "cell it is on", ok && kinds == 0xFu);
    }
    check("the STEPS set's dials: PROB all up, STEPS 8 of 16, TRIGS 3 of 16, SHIFT 0; TRIGS in white", near(knob_line(0), 192)
          && near(knob_line(1), 96) && near(knob_line(2), 36) && near(knob_line(3), 0) && pixels_of(VIEW_WHITE, 122, 150, 52, 7) > 15 && pixels_of(VIEW_WHITE, 10, 150, 108, 7) == 0);
    check("the top line: STEPS in hand", pixels_of(VIEW_AMBER, 112, 18, 29, 2) == 58 && pixels_of(VIEW_AMBER, 84, 18, 23, 2) == 0
          && pixels_of(VIEW_WHITE, 112, 8, 29, 7) > 20);
    check("the picture: the gate up for half of the random trig's step, and of the trig's after it", pixels_of(VIEW_WHITE, 8 + 40, VIEW_BANK_Y + view_bank_y(32767), 14, 1) == 14
          && pixels_of(VIEW_WHITE, 8 + 60, VIEW_BANK_Y + view_bank_y(-32767), 50, 1) == 50 && pixels_of(VIEW_WHITE, 8 + 112 + 2, VIEW_BANK_Y + view_bank_y(32767), 52, 1) == 52
          && pixels_of(VIEW_WHITE, 8 + 172, VIEW_BANK_Y + view_bank_y(-32767), 50, 1) == 50);
    save(directory, "10_bank_steps");
    m.bank_set = PANEL_BANK_MORE;
    view_show(&m);
    check("the MORE layer: PAD 1 of 8 and RAND 1 of 8 in the third and fourth dials", near(knob_line(2), 24)
          && near(knob_line(3), 24) && as_if_drawn_again(&m));
    m.bank_set = PANEL_BANK_STEPS;
    view_show(&m);
    check("and back: the pattern row again, as if drawn at once", as_if_drawn_again(&m));
    m.bank_step = 12;
    watch();
    view_show(&m);
    check("the next step: only the picture and the row are drawn", draws > 0 && top_drawn >= (int)VIEW_BANK_Y && pixels_of(VIEW_AMBER, 8 + 3 * 13 - 1, 178, 14, 1) == 14
          && pixels_of(VIEW_AMBER, 8 + 2 * 13 - 1, 178, 13, 1) == 0 && as_if_drawn_again(&m));
    bank_value(&m, DRONE_BANK_STEPS, 0);
    view_show(&m);
    check("STEPS off: every step plays, the row says so", pixels_of(VIEW_GREY, 8, VIEW_BANK_ROW_Y + 8, 162, 7) > 100
          && pixels_of(VIEW_WHITE, 0, 176, 240, 30) == 0 && as_if_drawn_again(&m));

    /* ---- the LOOP set: a loop of 4 beats that plays 2 loops and rests 1 (V0.33) ---- */
    engine_start();
    engine_controls.bank[0][DRONE_BANK_SYNC] = 1;
    engine_controls.bank[0][DRONE_BANK_SHAPE] = DRONE_BANK_TRIANGLE;
    engine_controls.bank[0][DRONE_BANK_LOOP] = 4;
    engine_controls.bank[0][DRONE_BANK_NAP] = 1;
    engine_controls.bank[0][DRONE_BANK_WAKE] = 2;
    check("the engine gets to the third step of the second loop", engine_run_to(0, 2, 1, 0x80000000u));
    m = bank_model();
    engine_take(&m, 0, 0, PANEL_BANK_LOOP);
    m.bank_set = PANEL_BANK_LOOP;
    m.touched = DRONE_BANK_WAKE;
    view_reset();
    view_show(&m);
    check("the loop row: the bar with a grey mark for each beat, the amber place two and a half beats in",
          pixels_of(VIEW_DIM, 8, 178, 224, 1) == 224 - 3 - 2 && pixels_of(VIEW_GREY, 64, 178, 1, 10) == 10 && pixels_of(VIEW_GREY, 120, 178, 1, 10) == 10
          && pixels_of(VIEW_GREY, 176, 178, 1, 10) == 10 && pixels_of(VIEW_AMBER, 8 + 138, 176, 4, 14) == 28 && pixels_of(VIEW_AMBER, 8, 176, 130, 14) == 0);
    check("under it, the two loops it plays in white, the one it rests framed, the amber frame on the second loop",
          pixels_of(VIEW_WHITE, 9, 194, 21, 8) == 168 && pixels_of(VIEW_WHITE, 33, 194, 21, 8) == 168 && pixels_of(VIEW_GREY, 57, 194, 21, 8) == 54
          && pixels_of(VIEW_AMBER, 31, 192, 25, 1) == 25 && pixels_of(VIEW_AMBER, 7, 192, 24, 12) == 0);
    check("the LOOP set's dials: LOOP 4 of 32, NAP 1 of 8, WAKE 2 of 8, L SHIFT 0; WAKE in white", near(knob_line(0), 24)
          && near(knob_line(1), 24) && near(knob_line(2), 48) && near(knob_line(3), 0)
          && pixels_of(VIEW_WHITE, 122, 150, 52, 7) > 15);
    view_bank_value_text(text, &m);
    check("the value line: WAKE 2 LOOPS", same(text, "WAKE 2 LOOPS"));
    save(directory, "10_bank_loop");
    check("the engine gets to the third loop, the one it rests", engine_run_to(0, 1, 2, 0x80000000u));
    engine_take(&m, 0, 0, PANEL_BANK_LOOP);
    view_show(&m);
    check("there: the amber frame on the third loop, the wave flat", pixels_of(VIEW_AMBER, 55, 192, 25, 1) == 25
          && pixels_of(VIEW_WHITE, 8, VIEW_BANK_Y, 224, 30) == 0 && as_if_drawn_again(&m));
    m.bank_set = PANEL_BANK_WAVE;
    view_show(&m);
    check("the WAVE set: the row of the twelve LFOs, nothing of the loop's row left", pixels_of(VIEW_AMBER, 7, VIEW_BANK_ROW_Y, 19, 1) == 19
          && as_if_drawn_again(&m));
    m.bank_set = PANEL_BANK_LOOP;
    view_show(&m);
    check("and the LOOP set again", pixels_of(VIEW_AMBER, 55, 192, 25, 1) == 25 && as_if_drawn_again(&m));
    bank_value(&m, DRONE_BANK_NAP, 0);
    view_show(&m);
    check("NAP off: the row says so", pixels_of(VIEW_GREY, 8, VIEW_BANK_ROW_Y + 19, 41, 7) > 20 && pixels_of(VIEW_WHITE, 0, 192, 240, 14) == 0
          && as_if_drawn_again(&m));
    bank_value(&m, DRONE_BANK_LOOP, 0);
    view_show(&m);
    check("LOOP off: it never starts again", pixels_of(VIEW_GREY, 8, VIEW_BANK_ROW_Y + 8, 185, 7) > 100 && pixels_of(VIEW_DIM, 0, 176, 240, 30) == 0
          && as_if_drawn_again(&m));
    view_bank_value_text(text, &m);
    check("WAKE as it was", same(text, "WAKE 2 LOOPS"));

    /* ---- a free SOFT RANDOM; an EXP that plays 60 % of its steps (V0.33) ---- */
    engine_start();
    engine_controls.bank[5][DRONE_BANK_SHAPE] = DRONE_BANK_SOFT_RANDOM;
    engine_controls.bank[5][DRONE_BANK_RATE] = 76;
    engine_controls.bank[5][DRONE_BANK_WIDTH] = 70;
    check("a free LFO gets to its fifth step", engine_run_to(5, 4, 0, 0x60000000u));
    m = bank_model();
    engine_take(&m, 5, 0, PANEL_BANK_WAVE);
    m.touched = DRONE_BANK_WIDTH;
    view_reset();
    view_show(&m);
    check("FREE at the right; the soft random from one height to the next, no step flat", pixels_of(VIEW_GREY, 196, 4, 36, 8) > 10
          && pixels_of(VIEW_WHITE, 8, VIEW_BANK_Y, 224, 64) > 224 && pixels_of(VIEW_AMBER, 7 + 5 * 19, VIEW_BANK_ROW_Y, 19, 1) == 19);
    save(directory, "10_bank_soft_random");
    engine_start();
    engine_controls.bank[7][DRONE_BANK_SYNC] = 1;
    engine_controls.bank[7][DRONE_BANK_RATIO] = 7;
    engine_controls.bank[7][DRONE_BANK_SHAPE] = DRONE_BANK_EXP;
    engine_controls.bank[7][DRONE_BANK_RANGE] = 1;
    engine_controls.bank[7][DRONE_BANK_WIDTH] = 30;
    engine_controls.bank[7][DRONE_BANK_PROB] = 60;
    {
        uint32_t step;
        for (step = 0; step < 64u && !(drone_bank_plays(7, engine_controls.bank[7], step, 0) && !drone_bank_plays(7, engine_controls.bank[7], step + 1u, 0)); step++)
            ;
        check("PROB 60: a step that plays with one that does not after it", step < 64u && engine_run_to(7, step, 0, 0x20000000u));
    }
    m = bank_model();
    engine_take(&m, 7, 0, PANEL_BANK_STEPS);
    m.bank_set = PANEL_BANK_STEPS;
    m.touched = DRONE_BANK_PROB;
    view_reset();
    view_show(&m);
    check("the EXP from the top, falling; the next step does not play: flat at 0", pixels_of(VIEW_WHITE, 8, VIEW_BANK_Y + view_bank_y(32767), 4, 2) > 0
          && pixels_of(VIEW_WHITE, 8 + 112 + 4, VIEW_BANK_Y + view_bank_y(0), 104, 1) == 104 && pixels_of(VIEW_WHITE, 8 + 112 + 4, VIEW_BANK_Y, 104, view_bank_y(0) - 1) == 0);
    check("PROB 60 of 100 on the first dial, in white", near(knob_line(0), 115) && pixels_of(VIEW_WHITE, 10, 150, 52, 7) > 15);
    view_bank_value_text(text, &m);
    check("PROB 60%", same(text, "PROB 60%"));
    save(directory, "10_bank_exp_prob");

    /* ---- what each LFO gives, from its own RANGE, LEVEL and OFFSET: the dials' ranges are made of it ---- */
    {
        static view_model_t taken;
        uint8_t was[DRONE_BANK_NPARAM];
        memcpy(was, engine_controls.bank[5], sizeof was);
        for (i = 0; i < DRONE_BANK_NPARAM; i++)
            engine_controls.bank[5][i] = drone_bank_spec[i].start;
        engine_take(&taken, 0, 0, PANEL_BANK_WAVE);
        check("an LFO as it starts: all of its output, down and up", taken.bank_low[5] == -32767 && taken.bank_high[5] == 32767);
        engine_controls.bank[5][DRONE_BANK_RANGE] = 1;
        engine_controls.bank[5][DRONE_BANK_LEVEL] = 60;
        engine_controls.bank[5][DRONE_BANK_OFFSET] = DRONE_BANK_OFFSET_ZERO + 20;
        engine_take(&taken, 0, 0, PANEL_BANK_WAVE);
        check("up only, LEVEL 60 %, OFFSET +20 %: the model has it from 0.2 to 0.8, whatever LFO is shown",
              near(taken.bank_low[5] * 100 / 32767, 20) && near(taken.bank_high[5] * 100 / 32767, 80) && taken.bank_low[5] > 6000 && taken.bank_high[5] < 26500);
        memcpy(engine_controls.bank[5], was, sizeof was);
    }

    /* ---- the cables onto the shown LFO, and the LFO key held on its page (V0.33) ---- */
    m = bank_model();
    m.bank_used = 1u << 3;
    m.bank_out[3] = 32767;
    m.bank_out[4] = 16384;
    m.bank_cabled[1][3] = 40;                   /* LFO 4 at its top: WIDTH 50 + 40 */
    m.bank_cabled[2][9] = -20;                  /* LFO 10 at 0: LEVEL as it is */
    view_reset();
    view_show(&m);
    check("a cable moves WIDTH: its line at half, the arc its range 10 % .. 90 %, the dot at 90 %; a tall grey mark for LFO 4",
          near(knob_line(1), 96) && arc_is(1, VIEW_AMBER, 19, 173) && near(ring_dot(1), 173)
          && pixels_of(VIEW_GREY, 92 - 23 + 3 * 4, 163, 3, 5) == 15 && pixels_of(VIEW_GREY, 148 - 23 + 9 * 4, 163, 3, 5) == 15);
    check("and LEVEL, all up, from LFO 10 at 0 now: its arc 20 % down from the top, the dot at the top",
          arc_is(2, VIEW_AMBER, 154, 192) && near(ring_dot(2), 192) && no_arc(0) && ring_dot(0) == -1);
    m.bank_set = PANEL_BANK_MORE;
    view_show(&m);
    check("the MORE layer: no cable moves its values, no arc, no dot, and no marks",
          no_arc(0) && no_arc(1) && ring_dot(1) == -1 && pixels_of(VIEW_GREY, 0, 163, 240, 5) == 0 && as_if_drawn_again(&m));
    m.bank_set = PANEL_BANK_WAVE;
    view_show(&m);
    check("and back to WAVE: the marks again", pixels_of(VIEW_GREY, 92 - 23 + 3 * 4, 163, 3, 5) == 15 && arc_is(1, VIEW_AMBER, 19, 173) && as_if_drawn_again(&m));
    m.lfo_held = 1;
    m.bank_source = 3;
    m.amount_touched = PANEL_ENC_VALUES + 1;
    m.bank_tempo_amount = 25;
    view_show(&m);
    check("the LFO key held: the path, LFO 4 in cyan, then LFO 1 in amber; no sets, no FREE, no tempo, no beat",
          pixels_of(VIEW_CYAN, 8, 4, 70, 18) > 60 && pixels_of(VIEW_GREY, 80, 4, 20, 18) > 10 && pixels_of(VIEW_AMBER, 104, 4, 80, 18) > 60
          && pixels_of(VIEW_WHITE, 0, 4, 240, 18) == 0 && pixels_of(VIEW_GREY, 190, 4, 42, 18) == 0);
    check("the dials: the range the source alone moves each value over, in cyan, the knobs' lines on their values, its marks cyan; no amber",
          arc_is(1, VIEW_CYAN, 19, 173) && near(knob_line(1), 96) && near(ring_dot(1), 173) && no_arc(0) && near(knob_line(0), 60)
          && no_arc(2) && ring_dot(2) == -1 && pixels_of(VIEW_AMBER, 0, DIALS_TOP, 240, DIALS_H) == 0
          && pixels_of(VIEW_CYAN, 92 - 23 + 3 * 4, 163, 3, 5) == 15 && pixels_of(VIEW_CYAN, 36 - 23 + 3 * 4, 166, 3, 2) == 6);
    check("the names the knobs' amounts are on: WIDTH, turned last, in white", pixels_of(VIEW_WHITE, 66, 150, 52, 7) > 15 && pixels_of(VIEW_GREY, 10, 150, 52, 7) > 15);
    check("the value line: WIDTH +40, no shape's name", pixels_of(VIEW_WHITE, 8, VIEW_BANK_VALUE_Y, 120, 14) > 60 && pixels_of(VIEW_GREY, 120, VIEW_BANK_VALUE_Y, 112, 14) == 0);
    check("the row of the twelve LFOs: the source in the cyan frame, the shown one in the amber one", pixels_of(VIEW_CYAN, 7 + 3 * 19, VIEW_BANK_ROW_Y, 19, 1) == 19
          && pixels_of(VIEW_AMBER, 7, VIEW_BANK_ROW_Y, 19, 1) == 19 && as_if_drawn_again(&m));
    save(directory, "10_bank_layer");
    swing(&m, 3, 1, 100, 0);
    view_show(&m);
    check("the source goes up only: WIDTH's range is from its value up, 50 % .. 90 %", arc_is(1, VIEW_CYAN, 96, 173) && as_if_drawn_again(&m));
    m.lfo_held = 0;
    view_show(&m);
    check("the key let go: the same range in amber", arc_is(1, VIEW_AMBER, 96, 173) && near(ring_dot(1), 173) && as_if_drawn_again(&m));
    swing(&m, 3, 0, 100, 0);
    m.lfo_held = 1;
    m.bank_source = 0;
    view_show(&m);
    check("the shown LFO as its own source: the cyan frame on it", pixels_of(VIEW_CYAN, 7, VIEW_BANK_ROW_Y, 19, 1) == 19
          && pixels_of(VIEW_AMBER, 0, VIEW_BANK_ROW_Y, 240, 22) == 0 && pixels_of(VIEW_CYAN, 0, DIALS_TOP, 240, DIALS_H) == 0 && as_if_drawn_again(&m));
    m.bank_set = PANEL_BANK_LOOP;
    m.amount_touched = PANEL_ENC_MOVE;
    view_show(&m);
    check("held in the LOOP set: still the row of the twelve LFOs; the dials' names the LOOP set's", pixels_of(VIEW_CYAN, 7, VIEW_BANK_ROW_Y, 19, 1) == 19
          && pixels_of(VIEW_WHITE, 66, 150, 166, 7) == 0 && as_if_drawn_again(&m));
    view_bank_value_text(text, &m);
    check("PRESETS turned last: TEMPO +25", same(text, "TEMPO +25"));
    m.lfo_held = 0;
    view_show(&m);
    check("the key let go: the LOOP row, the top line with its sets, no cyan", pixels_of(VIEW_CYAN, 0, 0, 240, 240) == 0
          && pixels_of(VIEW_AMBER, 146, 18, 23, 2) == 46 && as_if_drawn_again(&m));
    m.bank_set = PANEL_BANK_WAVE;
    view_show(&m);
    check("and the WAVE set again: the row of the twelve LFOs", pixels_of(VIEW_AMBER, 7, VIEW_BANK_ROW_Y, 19, 1) == 19 && as_if_drawn_again(&m));

    /* ---- the bank on the play page: the marks, the white dot, and the amounts while the LFO key is held ---- */
    m = bank_model();
    m.page = PANEL_PLAY;
    m.touched = PANEL_ENC_VALUES;
    m.style = STYLE_GLASS;
    m.freq = 50u * 128u + 40u;
    m.bank_lfo = 2;
    m.bank_amount[PLAY_BANKED_TONE][2] = 40;
    m.bank_amount[PLAY_BANKED_GRIT][2] = -60;
    m.bank_amount[PLAY_BANKED_GRIT][7] = 20;
    m.bank_amount[PLAY_BANKED_FREQ][2] = -12;
    view_reset();
    view_show(&m);
    check("under each dial, twelve marks: taller and grey, an LFO with an amount on that knob; low and faint, none",
          pixels_of(VIEW_GREY, 21, 163, 3, 5) == 15 && pixels_of(VIEW_FAINT, 13, 166, 3, 2) == 6 && pixels_of(VIEW_FAINT, 13, 163, 3, 3) == 0
          && pixels_of(VIEW_GREY, 69, 163, 48, 5) == 0 && pixels_of(VIEW_FAINT, 69, 166, 48, 2) == 12 * 6
          && pixels_of(VIEW_GREY, 148 - 23 + 2 * 4, 163, 3, 5) == 15 && pixels_of(VIEW_GREY, 148 - 23 + 7 * 4, 163, 3, 5) == 15);
    check("LFOs on TONE and GRIT: their ranges in amber; all at 0 now: the dots on the knobs' places; MOVE and LINK: none",
          arc_is(0, VIEW_AMBER, 20, 174) && near(ring_dot(0), 97) && arc_is(2, VIEW_AMBER, 0, 192) && near(ring_dot(2), 97)
          && no_arc(1) && ring_dot(1) == -1 && no_arc(3) && ring_dot(3) == -1);
    m.bank_out[2] = 32767;
    watch();
    view_show(&m);
    check("an LFO moves TONE up: the dot at the value now, the line where the knob is, and no more is drawn",
          near(ring_dot(0), 174) && near(knob_line(0), 97) && top_drawn >= DIALS_TOP && bottom_drawn <= 168);
    check("and GRIT down to 0: its dot at the start", near(ring_dot(2), 0));
    save(directory, "11_play_marks");
    watch();
    view_show(&m);
    check("the play page with the bank, nothing changed: nothing is drawn", draws == 0);
    m.bank_out[2] = 200;
    view_show(&m);
    check("the LFO near its middle: the dots on the knobs' places", near(ring_dot(0), 97) && near(ring_dot(2), 97));
    m.bank_out[2] = 32767;
    m.lfo_held = 1;
    view_show(&m);
    check("the LFO key held: LFO 3 in cyan in the place of the style, its wave's name in grey in the place of the frequency",
          pixels_of(VIEW_CYAN, 56, 4, 80, 18) > 60 && pixels_of(VIEW_WHITE, 56, 4, 176, 18) == 0 && pixels_of(VIEW_GREY, 160, 4, 72, 18) > 40
          && pixels_of(VIEW_WHITE, 8, 4, 40, 18) > 300);
    check("the dials: the range this LFO alone moves each knob over, in cyan, with its dot; the knobs' lines stay; no amber",
          arc_is(0, VIEW_CYAN, 20, 174) && near(knob_line(0), 97) && near(ring_dot(0), 174) && arc_is(2, VIEW_CYAN, 0, 192) && near(knob_line(2), 97)
          && near(ring_dot(2), 0) && no_arc(1) && near(knob_line(1), 97) && ring_dot(1) == -1 && no_arc(3) && near(knob_line(3), 0)
          && pixels_of(VIEW_AMBER, 0, DIALS_TOP, 240, DIALS_H) == 0);
    check("its marks are cyan under each dial", pixels_of(VIEW_CYAN, 21, 163, 3, 5) == 15 && pixels_of(VIEW_CYAN, 77, 166, 3, 2) == 6
          && pixels_of(VIEW_CYAN, 69, 163, 48, 3) == 0);
    check("the value line: the knob's name and the signed amount", pixels_of(VIEW_WHITE, 8, 172, 120, 14) > 60);
    check("the row of the twelve LFOs in the links' band, the cyan frame on the current one", pixels_of(VIEW_CYAN, 7 + 2 * 19, 190, 19, 1) == 19
          && pixels_of(VIEW_CYAN, 7 + 2 * 19, 210, 19, 1) == 19 && pixels_of(VIEW_CYAN, 0, 211, 240, 1) == 0 && pixels_of(VIEW_GREY, 8, 192, 224, 7) > 60
          && pixels_of(VIEW_WHITE, 8, 212, 72, 18) > 1200);
    save(directory, "11_play_amounts");
    view_amount_line_text(text, &m);
    check("TONE +40", same(text, "TONE +40"));
    m.amount_touched = PANEL_ENC_MOVE;
    view_amount_line_text(text, &m);
    check("PRESETS: FREQ -12", same(text, "FREQ -12"));
    m.amount_touched = PANEL_ENC_VALUES + 1;
    view_amount_line_text(text, &m);
    check("no amount: +0", same(text, "MOVE +0"));
    m.bank_lfo = 7;
    view_show(&m);
    check("another LFO: its range on GRIT only, 20 each way; its marks", pixels_of(VIEW_CYAN, 148 - 23 + 7 * 4, 163, 3, 5) == 15 && no_arc(0)
          && arc_is(2, VIEW_CYAN, 58, 135) && near(ring_dot(2), 97) && pixels_of(VIEW_CYAN, 7 + 7 * 19, 190, 19, 1) == 19);
    m.lfo_held = 0;
    view_show(&m);
    check("the key let go: the page as before, no cyan left", pixels_of(VIEW_CYAN, 0, 0, 240, 240) == 0 && pixels_of(VIEW_WHITE, 56, 4, 60, 18) > 60
          && pixels_of(VIEW_BLACK, 0, 190, 240, 22) == 240 * 22 && near(ring_dot(0), 174) && arc_is(0, VIEW_AMBER, 20, 174));
    swing(&m, 2, 1, 100, 0);
    watch();
    view_show(&m);
    check("LFO 3 goes up only (0 .. 1): TONE's range is from the knob's place up, not below it; GRIT's, upside down, from it down, "
          "with LFO 8's 20 each way; only the dials are drawn again",
          arc_is(0, VIEW_AMBER, 97, 174) && near(ring_dot(0), 174) && arc_is(2, VIEW_AMBER, 0, 135)
          && draws > 0 && top_drawn >= DIALS_TOP && bottom_drawn <= DIALS_TOP + DIALS_H && as_if_drawn_again(&m));
    swing(&m, 2, 0, 50, -50);
    m.bank_out[2] = -32767;
    view_show(&m);
    check("LFO 3 goes from -1 to 0 (LEVEL 50 %, OFFSET -50 %): TONE's range is from below the knob's place up to it, not above it",
          arc_is(0, VIEW_AMBER, 20, 97) && near(ring_dot(0), 20) && near(knob_line(0), 97) && as_if_drawn_again(&m));
    m.lfo_held = 1;
    m.bank_lfo = 2;
    view_show(&m);
    check("the LFO key held: the same range in cyan", arc_is(0, VIEW_CYAN, 20, 97) && near(ring_dot(0), 20) && as_if_drawn_again(&m));
    save(directory, "11_play_range");
    m.lfo_held = 0;
    m.layer = 1;
    m.bank_amount[PLAY_BANKED_FREQ][2] = 100;   /* LFO 3, -1 .. 0 here, onto the FREQ: an octave down at its bottom */
    m.freq = 60u * 128u;
    m.bank_out[2] = -32767;
    view_show(&m);
    check("the second layer: the FREQ's dial has the range the LFO moves the frequency over, an octave down from the knob's place, and the dot at "
          "its bottom; SPEED takes no LFO: no arc, no marks under it",
          near(knob_line(0), 91) && arc_is(0, VIEW_AMBER, 73, 91) && near(ring_dot(0), 73) && no_arc(1) && ring_dot(1) == -1
          && pixels_of(VIEW_GREY, 69, 163, 48, 5) == 0 && pixels_of(VIEW_FAINT, 69, 163, 48, 5) == 0
          && pixels_of(VIEW_GREY, 21, 163, 3, 5) == 15 && as_if_drawn_again(&m));
    m.lfo_held = 1;
    m.amount_touched = PANEL_ENC_VALUES;
    view_show(&m);
    view_amount_line_text(text, &m);
    check("the LFO key held there: KNOB1 is the amount onto the FREQ, its range in cyan", same(text, "FREQ +100") && arc_is(0, VIEW_CYAN, 73, 91)
          && pixels_of(VIEW_WHITE, 10, 150, 52, 7) > 15 && as_if_drawn_again(&m));
    m.amount_touched = PANEL_ENC_VALUES + 1;
    view_amount_line_text(text, &m);
    check("KNOB2 (SPEED) holds no amount there: nothing on the line", same(text, ""));
    save(directory, "11_play_freq");
    m.layer = 0;
    m.bank_amount[PLAY_BANKED_FREQ][2] = -12;
    m.amount_touched = PANEL_ENC_VALUES;
    m.lfo_held = 0;
    view_show(&m);

    outside = 0;
    r = 23;
    for (i = 0; i < 4000; i++) {                /* any values on the bank's page and the play page: nothing outside the screen */
        uint32_t p;
        r = r * 1664525u + 1013904223u;
        m.page = (uint8_t)((r >> 3 & 1u) ? PANEL_BANK : PANEL_PLAY);
        m.layer = (uint8_t)(r >> 4 & 1u);
        m.bank_set = (uint8_t)(r >> 9 & 3u);
        m.lfo_held = (uint8_t)(r >> 5 & 1u);
        m.info = (r >> 6 & 15u) == 0;
        m.touched = (uint8_t)(r >> 10 & 31u);
        m.amount_touched = (uint8_t)(r >> 14 & 7u);
        m.bank_lfo = (uint8_t)(r >> 17 & 15u);
        m.selected = (uint8_t)(r >> 21 & 3u);
        m.tempo = (uint16_t)(r >> 8);
        m.bank_used = (uint16_t)(r >> 12);
        m.bank_source = (uint8_t)(r >> 13 & 15u);
        m.bank_set = (uint8_t)(r >> 23 & 3u);
        m.bank_synced = (uint16_t)(r >> 7);
        m.bank_phase = r * 2654435761u;
        m.bank_step = r >> (r & 31u);
        m.bank_loop = r * 69069u >> (r >> 27);
        m.bank_loop_at = r * 1103515245u >> (r >> 26 & 15u);
        m.bank_held = (int16_t)(r >> 9);
        m.bank_tempo_amount = (int8_t)(r >> 3);
        m.clock_phase = r * 40503u;
        for (p = 0; p < DRONE_BANK_LFOS; p++)
            m.bank_out[p] = (int16_t)(r >> (p % 16u));
        for (p = 0; p < DRONE_BANK_NPARAM; p++) {     /* any values, also out of their ranges: the screen keeps them in */
            m.bank_knob[p] = (uint8_t)(r >> (p % 24u));
            m.bank_value[p] = (uint8_t)(r >> ((p + 5u) % 24u) ^ r >> 20);
        }
        for (p = 0; p < 4u * DRONE_BANK_LFOS; p++)
            m.bank_cabled[p / DRONE_BANK_LFOS][p % DRONE_BANK_LFOS] = (int8_t)(r >> (p % 23u));
        for (p = 0; p < PLAY_NBANKED * DRONE_BANK_LFOS; p++)
            m.bank_amount[p / DRONE_BANK_LFOS][p % DRONE_BANK_LFOS] = (int8_t)(r >> (p % 24u));
        if (i % 89 == 0)
            view_reset();
        view_show(&m);
    }
    check("the bank's page and the play page with any values, in any set: nothing drawn outside the screen", !outside);
    puts(failed ? "view_test: FAILED" : "view_test: ok");
    return failed;
}
