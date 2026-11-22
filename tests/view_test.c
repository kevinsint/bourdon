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

int main(int argc, char **argv)
{
    const char *directory = argc > 1 ? argv[1] : 0;
    view_model_t m = plain();
    char text[16];
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
    check("a knob in its middle: half of its dial's dots are amber", pixels_of(VIEW_AMBER, 20, 112, 32, 32) == 8 * 9
          && pixels_of(VIEW_FAINT, 20, 112, 32, 32) == 8 * 9);
    check("KNOB4 is LINK, closed at the start: its name, no dot lit, and no link drawn between the voices",
          pixels_of(VIEW_AMBER, 188, 112, 32, 32) == 0 && pixels_of(VIEW_FAINT, 188, 112, 32, 32) == 16 * 9
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
    check("TONE at the top: all its dial's dots are amber, and only the dials and the value are drawn again",
          pixels_of(VIEW_AMBER, 20, 112, 32, 32) == 16 * 9 && top_drawn >= 112 && bottom_drawn <= 190);
    m.knob[PLAY_TONE] = 0;
    view_show(&m);
    check("TONE at 0: no dot is amber", pixels_of(VIEW_AMBER, 20, 112, 32, 32) == 0);
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
    m.shift = 1;
    m.knob[PLAY_SPEED] = 127;
    view_show(&m);
    check("the shift key down: KNOB2 is SPEED, the others have no knob", pixels_of(VIEW_AMBER, 20, 112, 32, 32) == 0
          && pixels_of(VIEW_AMBER, 76, 112, 32, 32) == 16 * 9 && pixels_of(VIEW_GREY, 66, 150, 52, 7) > 20
          && pixels_of(VIEW_AMBER, 188, 112, 32, 32) == 0 && pixels_of(VIEW_GREY, 178, 150, 52, 7) == 0);
    check("the knob turned last is not one of this set: no name below", pixels_of(VIEW_WHITE, 8, 172, 224, 14) == 0);
    save(directory, "9_play_shift");
    m.link_kind[0] = DRONE_LINK_THROUGH;
    view_show(&m);
    check("the shift key down on a THROUGH voice: KNOB4 is OWN, all of it at the start", pixels_of(VIEW_AMBER, 188, 112, 32, 32) == 16 * 9
          && pixels_of(VIEW_GREY, 178, 150, 52, 7) > 10);
    m.knob[PLAY_OWN] = 0;
    m.touched = PANEL_ENC_VALUES + 3;
    view_show(&m);
    check("OWN at 0: no dot is amber, and its name and value are below", pixels_of(VIEW_AMBER, 188, 112, 32, 32) == 0
          && pixels_of(VIEW_WHITE, 8, 172, 120, 14) > 60);
    save(directory, "9_play_own");
    m.link_kind[0] = DRONE_LINK_FM;
    view_show(&m);
    check("another kind of link: KNOB4 has no knob with shift", pixels_of(VIEW_GREY, 178, 150, 52, 7) == 0 && pixels_of(VIEW_WHITE, 178, 150, 52, 7) == 0
          && pixels_of(VIEW_WHITE, 8, 172, 120, 14) == 0);
    m.shift = 0;
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
          && pixels_of(VIEW_FAINT, 0, 112, 240, 32) == 0);
    m.page = PANEL_PLAY;
    view_show(&m);
    check("and back: the play page again", pixels_of(VIEW_FAINT, 188, 112, 32, 32) == 16 * 9 && pixels_of(VIEW_WHITE, 56, 4, 60, 18) > 60);
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
        m.shift = (uint8_t)(r >> 5 & 1u);
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
    check("RATE and BITS at the top: all their dials' dots; DUST and the amount at 0: none",
          pixels_of(VIEW_AMBER, 20, 112, 32, 32) == 16 * 9 && pixels_of(VIEW_AMBER, 76, 112, 32, 32) == 16 * 9
          && pixels_of(VIEW_AMBER, 132, 112, 32, 32) == 0 && pixels_of(VIEW_AMBER, 188, 112, 32, 32) == 0);
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
          pixels_of(VIEW_GREEN, 0, 0, 240, 24) == 0 && pixels_of(VIEW_AMBER, 84, 190, 72, 22) > 100 && pixels_of(VIEW_AMBER, 188, 112, 32, 32) > 0);
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
    puts(failed ? "view_test: FAILED" : "view_test: ok");
    return failed;
}
