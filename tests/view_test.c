/* The screen on this computer: what it writes for each value, that it draws only what changed, that nothing goes
 * outside the 240 by 240 pixels, and that the pictures follow the controls.
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

static view_model_t plain(void)                 /* the third voice selected, on its page, the cutoff turned last */
{
    view_model_t m;
    uint32_t i;
    memset(&m, 0, sizeof m);
    m.voices_on = 1u << 0 | 1u << 2 | 1u << 3;
    m.selected = 2;
    m.page = PANEL_VOICE;
    m.touched = DRONE_CUTOFF;
    for (i = 0; i < DRONE_NPARAM; i++)
        m.param[i] = drone_param_spec[i].start;
    m.param[DRONE_PITCH] = 46;
    m.param[DRONE_RESONANCE] = 40;
    for (i = 0; i < DRONE_LFO_NPARAM; i++)
        m.lfo[i] = drone_lfo_spec[i].start;
    m.lfo[DRONE_LFO_DEPTH] = 50;
    m.send = drone_send_spec.start;
    for (i = 0; i < DRONE_REVERB_NPARAM; i++)
        m.reverb[i] = drone_reverb_spec[i].start;
    m.load_percent = 31;
    return m;
}

static view_model_t patch(void)                 /* the first voice, a patch: oscillator, filter, level; the cursor on the filter */
{
    view_model_t m = plain();
    m.selected = 0;
    m.patch = 1;
    m.place = 1;
    m.touched = PANEL_ENC_VALUES;
    m.block[0] = panel_block_new(DRONE_BLOCK_VCO);
    m.block[1] = panel_block_new(DRONE_BLOCK_VCF);
    m.block[1].param[DRONE_VCF_RESONANCE] = 40;
    m.block[2] = panel_block_new(DRONE_BLOCK_VCA);
    return m;
}
static int cell_x(int cell) { return (int)VIEW_CELL_X(cell); }

int main(int argc, char **argv)
{
    const char *directory = argc > 1 ? argv[1] : 0;
    view_model_t m = plain();
    char text[16];
    uint32_t i, r = 12345;
    int low_edge, high_edge, steep_edge;

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
    check("the first time, it draws", draws > 50);
    check("the selected voice's name is in a white box at the top left", pixels_of(VIEW_WHITE, 8, 4, 40, 18) > 300);
    save(directory, "1_voice_cutoff");
    watch();
    view_show(&m);
    check("nothing changed: nothing is drawn", draws == 0);

    m.param[DRONE_CUTOFF] = 30;
    watch();
    view_show(&m);
    check("the cutoff turned: only the middle of the screen is drawn again", draws > 0 && top_drawn >= 26 && bottom_drawn <= 212);
    low_edge = last_column_of(VIEW_WHITE, 88, 30, 64, 30);
    m.param[DRONE_CUTOFF] = 110;
    view_show(&m);
    high_edge = last_column_of(VIEW_WHITE, 88, 30, 64, 30);
    check("a higher cutoff: the filter's curve goes down further right", low_edge > 0 && high_edge > low_edge + 15);
    m.filter = DRONE_MS20;
    view_show(&m);
    steep_edge = last_column_of(VIEW_WHITE, 88, 30, 64, 60);
    m.filter = DRONE_LADDER;
    view_show(&m);
    check("the ladder (four poles) falls faster than a two-pole filter", last_column_of(VIEW_WHITE, 88, 30, 64, 60) <= steep_edge);
    m = plain();

    m.voices_on = 0;
    view_show(&m);
    check("no voice on: the voices' boxes are outlines", pixels_of(VIEW_WHITE, 8, 212, 34, 18) == 0);
    m.voices_on = 1;
    view_show(&m);
    check("the first voice on: its box is filled", pixels_of(VIEW_WHITE, 8, 212, 34, 18) > 400 && pixels_of(VIEW_WHITE, 46, 212, 34, 18) == 0);
    check("the selected voice has the amber mark under its box", pixels_of(VIEW_AMBER, 8 + 2 * 38, 232, 34, 4) == 34 * 4
          && pixels_of(VIEW_AMBER, 8, 232, 34, 4) == 0);
    m = plain();

    m.touched = DRONE_WAVE;
    m.param[DRONE_WAVE] = DRONE_SQUARE;
    view_show(&m);
    check("the wave turned: the oscillator's box has the amber frame, the filter's has not",
          pixels_of(VIEW_AMBER, 8, 28, 68, 2) == 68 * 2 && pixels_of(VIEW_AMBER, 86, 28, 68, 2) == 0);
    save(directory, "2_voice_wave");
    m = plain();
    m.choosing = 1;
    m.filter = DRONE_MS20;
    view_show(&m);
    check("SEL held: the filter's name is amber, and its box has the frame",
          pixels_of(VIEW_AMBER, 56, 4, 100, 18) > 50 && pixels_of(VIEW_AMBER, 86, 28, 68, 2) == 68 * 2);
    save(directory, "3_sel_held");

    m = plain();
    m.page = 1;
    m.touched = DRONE_LFO_DEPTH;
    view_show(&m);
    check("an LFO's page: the wave's picture is at the bottom left", pixels_of(VIEW_WHITE, 8, 142, 112, 62) > 100);
    check("and the box of what the LFO moves has the amber frame", pixels_of(VIEW_AMBER, 86, 28, 68, 2) == 68 * 2);
    save(directory, "4_lfo");
    watch();
    m.lfo[DRONE_LFO_DEPTH] = 127;
    view_show(&m);
    check("the LFO's depth turned: the chain above is not drawn again", draws > 0 && top_drawn >= 134);

    m = plain();
    m.page = PANEL_REVERB;
    m.touched = 1;
    view_show(&m);
    i = (uint32_t)pixels_of(VIEW_WHITE, 12, 164, 116, 40) + (uint32_t)pixels_of(VIEW_GREY, 12, 164, 116, 40);
    check("the reverb's page: its tail is drawn in the amber box", pixels_of(VIEW_AMBER, 8, 162, 224, 2) == 224 * 2 && i > 100);
    save(directory, "5_reverb");
    m.reverb[DRONE_REVERB_DECAY] = 127;
    view_show(&m);
    check("a longer decay: a longer tail",
          (uint32_t)pixels_of(VIEW_WHITE, 12, 164, 116, 40) + (uint32_t)pixels_of(VIEW_GREY, 12, 164, 116, 40) > i + i / 2);

    m = plain();
    m.info = 1;
    view_show(&m);
    check("the two octave buttons held: the version and the load, where the filter's name was", pixels_of(VIEW_WHITE, 56, 4, 176, 18) > 100);
    save(directory, "6_info");
    m.info = 0;
    m.load_percent = 95;
    view_show(&m);
    check("a load above 80 %: the line at the bottom is red", pixels_of(VIEW_RED, 0, 238, 240, 2) > 400);

    /* ---- a patch ---- */
    m = patch();
    watch();
    view_show(&m);
    check("a patch after a voice that is not one: all is drawn again", draws > 50 && top_drawn == 0);
    check("a patch: a picture for each block, the empty place and OUT, and no more",
          pixels_of(VIEW_WHITE, cell_x(0), 28, 28, 26) > 20 && pixels_of(VIEW_WHITE, cell_x(2), 28, 28, 26) > 20
          && pixels_of(VIEW_WHITE, cell_x(4), 28, 28, 26) > 10 && pixels_of(VIEW_BLACK, cell_x(5), 28, 28, 26) == 28 * 26);
    check("the cursor's block has the amber frame, the others have not",
          pixels_of(VIEW_AMBER, cell_x(1), 28, 28, 2) == 28 * 2 && pixels_of(VIEW_AMBER, cell_x(0), 28, 28, 2) == 0);
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
    check("the cursor moved: the frame is on the first block", pixels_of(VIEW_AMBER, cell_x(0), 28, 28, 2) == 28 * 2
          && pixels_of(VIEW_AMBER, cell_x(1), 28, 28, 2) == 0);
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
          pixels_of(VIEW_AMBER, cell_x(3), 28, 28, 2) == 28 * 2 && pixels_of(VIEW_DIM, 8, 102, 112, 1) == 0);
    save(directory, "10_patch_empty");
    m.choosing = 1;
    m.choice = DRONE_BLOCK_VCF;
    view_show(&m);
    check("a block chosen there: the place shows it in amber", pixels_of(VIEW_AMBER, cell_x(3) + 3, 31, 22, 20) > 20);
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
    check("the choice kept: the block is in the row, and OUT has moved right", pixels_of(VIEW_WHITE, cell_x(3), 28, 28, 26) > 20
          && pixels_of(VIEW_WHITE, cell_x(5), 28, 28, 26) > 10);
    m.place = 5;
    m.touched = PANEL_ENC_VALUES + 2;
    view_show(&m);
    check("the cursor on OUT: the voice's level, fade and send", pixels_of(VIEW_WHITE, 128, 104 + 36, 104, 14) > 50);
    save(directory, "12_patch_out");
    m.block[4] = m.block[5] = panel_block_new(DRONE_BLOCK_VCO);
    m.place = 6;
    view_show(&m);
    check("a full chain: six blocks and OUT fit the screen", pixels_of(VIEW_AMBER, cell_x(6), 28, 28, 2) == 28 * 2 && !outside);
    save(directory, "13_patch_full");
    m.page = PANEL_REVERB;
    view_show(&m);
    check("a patch's reverb page: the reverb's tail under the chain, no frame on the chain",
          pixels_of(VIEW_AMBER, 8, 162, 224, 2) == 224 * 2 && pixels_of(VIEW_AMBER, 0, 28, 240, 26) == 0);
    save(directory, "14_patch_reverb");
    /* ---- a patch's LFOs and cables ---- */
    m = patch();
    view_show(&m);
    check("no cable: no mark above the blocks", pixels_of(VIEW_CYAN, 0, 23, 240, 3) == 0 && pixels_of(VIEW_SEA, 0, 23, 240, 3) == 0);
    m.block[1].amount[DRONE_VCF_CUTOFF][0] = -25;
    m.block[2].amount[DRONE_VCA_LEVEL][1] = 40;
    m.out_amount[DRONE_OUT_SEND][2] = 10;
    view_show(&m);
    check("a cable from the open LFO: a bright mark above the block, in the LFO's place; from another LFO: a dim one",
          pixels_of(VIEW_CYAN, cell_x(1) + 1, 23, 8, 3) == 24 && pixels_of(VIEW_SEA, cell_x(2) + 10, 23, 8, 3) == 24
          && pixels_of(VIEW_SEA, cell_x(4) + 19, 23, 8, 3) == 24 && pixels_of(VIEW_CYAN, cell_x(0), 23, 28, 3) == 0);
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
          pixels_of(VIEW_CYAN, 8, 102, 112, 62) > 100 && pixels_of(VIEW_CYAN, 128, 126, 104, 7) > 20 && pixels_of(VIEW_CYAN, 128, 138, 104, 7) == 0
          && pixels_of(VIEW_AMBER, cell_x(1), 28, 28, 2) == 28 * 2);
    save(directory, "17_patch_lfo");
    m.page = 2;
    m.lfo_current = 1;
    watch();
    view_show(&m);
    check("the next LFO: its own cable, and its mark is the bright one", pixels_of(VIEW_CYAN, cell_x(2) + 10, 23, 8, 3) == 24
          && pixels_of(VIEW_SEA, cell_x(1) + 1, 23, 8, 3) == 24 && pixels_of(VIEW_CYAN, 128, 126, 104, 7) > 20);
    for (i = 0; i < DRONE_SLOTS; i++) {         /* more cables than the list has rows */
        m.block[i] = panel_block_new(DRONE_BLOCK_VCF);
        m.block[i].amount[0][1] = 50;
        m.block[i].amount[1][1] = -50;
    }
    view_show(&m);
    check("more cables than rows: the last row says how many more", pixels_of(VIEW_GREY, 128, 126 + 6 * 12, 104, 7) > 20 && !outside);
    save(directory, "18_patch_lfo_many");

    m = plain();
    watch();
    view_show(&m);
    check("back to a voice that is not a patch: all is drawn again", draws > 50 && pixels_of(VIEW_WHITE, 8, 28, 68, 64) > 50);

    for (i = 0; i < 20000; i++) {               /* every page with any values: nothing outside the screen */
        uint32_t p;
        r = r * 1664525u + 1013904223u;
        m.voices_on = (uint8_t)(r >> 3 & 63u);
        m.selected = (uint8_t)((r >> 9) % DRONE_VOICES);
        m.page = (uint8_t)((r >> 12) % PANEL_NPAGE);
        m.choosing = (r >> 15 & 7u) == 0;
        m.filter = (uint8_t)((r >> 18) % DRONE_NFILTER);
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
        m.send = (uint8_t)((r >> 4 ^ r >> 17) % 128u);
        m.patch = (r >> 21 & 3u) == 0;
        m.place = (uint8_t)((r >> 6) % 9u);
        m.choice = (uint8_t)((r >> 16) % 5u);
        m.lfo_held = (r >> 19 & 3u) == 0;
        m.lfo_current = (uint8_t)((r >> 26) % 4u);
        m.out_amount[r >> 5 & 1u][(r >> 7) % DRONE_LFOS] = (int8_t)(r >> 10);
        for (p = 0; p < DRONE_SLOTS; p++) {     /* any blocks, also with gaps and values out of range */
            m.block[p].type = (uint8_t)((r >> (p + 1) ^ r >> 14) % 5u);
            m.block[p].kind = (uint8_t)(r >> (p + 3) & 7u);
            m.block[p].param[0] = (uint8_t)(r >> (p + 8));
            m.block[p].param[1] = (uint8_t)(r >> (p + 13));
            m.block[p].amount[p & 1u][(r >> p) % DRONE_LFOS] = (int8_t)(r >> (p + 2));
        }
        if (i % 97 == 0)
            view_reset();
        view_show(&m);
    }
    check("nothing is ever drawn outside the screen", !outside);
    puts(failed ? "view_test: FAILED" : "view_test: ok");
    return failed;
}
