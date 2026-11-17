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
        if (i % 97 == 0)
            view_reset();
        view_show(&m);
    }
    check("nothing is ever drawn outside the screen", !outside);
    puts(failed ? "view_test: FAILED" : "view_test: ok");
    return failed;
}
