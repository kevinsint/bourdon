/* The screen: the selected voice as its chain, left to right as its knobs are on the panel (pannel-description.md):
 *
 *   OP3 LADDER                 VOICE      the voice, its filter, the open page
 *   [ oscillator ] > [ filter ] > [ level and fade ]      three pictures; the one being turned has the amber frame
 *   A#2 +0           1.3K         LVL 64
 *   SAW              RES 40       0.3 S
 *   the bottom: on the voice's page, the knob turned last, large, and the send into the reverb;
 *               on an LFO's page, the LFO's wave and its four values; on the reverb's page, the reverb's tail
 *   [1] [2] [3] [4] [5] [6]               the voices: filled is on, the amber mark is the selected one
 *
 * A voice that is a patch (drone.h) shows its own chain in place of the three pictures: a row of small pictures, one
 * for each block, then the empty place while there is room, then OUT; the place where the cursor is has the amber
 * frame and is below the row, large, with its values. While SEL is held there: the blocks to choose from.
 *
 * view_show(&model) draws only what is different from the last time; view_reset() makes it draw all again (after
 * something else wrote on the screen). It has no hardware code in it, so tests/view_test.c runs it on a computer.
 * The includer gives FONT_RECT(x, y, w, h, colour), as for font.h, and VIEW_BLIT(x, y, w, h, pixels) for w * h
 * RGB565 pixels, row by row. */
#pragma once
#include <stdint.h>
#include "app_id.h"
#include "drone.h"
#include "panel.h"

enum { VIEW_BLACK = 0x0000, VIEW_WHITE = 0xFFFF, VIEW_GREY = 0x8410, VIEW_DIM = 0x4208, VIEW_FAINT = 0x2104, VIEW_RED = 0xF800,
       VIEW_AMBER = 0xFD20 };
#define VIEW_ENCODERS 7u
_Static_assert(DRONE_VOICES <= 6, "the voices' boxes fit the screen's width");

typedef struct {                        /* all that the screen shows */
    uint8_t voices_on;                  /* bit n: voice n is on */
    uint8_t selected;                   /* the voice the knobs are for */
    uint8_t page;                       /* PANEL_VOICE .. */
    uint8_t choosing, filter;           /* SEL is down; the filter to show: the choice so far, or the voice's */
    uint8_t touched;                    /* the encoder turned last on this page */
    uint8_t param[DRONE_NPARAM];        /* the selected voice's */
    uint8_t lfo[DRONE_LFO_NPARAM];      /* its LFO, on an LFO's page */
    uint8_t send;
    uint8_t reverb[DRONE_REVERB_NPARAM];
    uint8_t info;                       /* show the version and the load in place of the filter and the page */
    uint8_t load_percent;
    uint8_t faults;                     /* VIEW_CRASHED | VIEW_NO_AUDIO */
    uint8_t patch;                      /* the selected voice is a patch: param[] has only its level and fade */
    uint8_t place;                      /* where the cursor is in its chain */
    uint8_t choice;                     /* while SEL is down (choosing): the block chosen so far for that place */
    drone_block_controls_t block[DRONE_SLOTS];
} view_model_t;
enum { VIEW_CRASHED = 1, VIEW_NO_AUDIO = 2 };

/* ---- the values as text ---- */
static char *view_put(char *to, const char *text)
{
    while (*text)
        *to++ = *text++;
    *to = 0;
    return to;
}
static char *view_put_number(char *to, uint32_t value)
{
    char digits[10];
    uint32_t n = 0;
    do {
        digits[n++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value);
    while (n)
        *to++ = digits[--n];
    *to = 0;
    return to;
}
static void view_note_text(char *to, uint32_t pitch)    /* as MIDI numbers the notes: 60 is C4 */
{
    static const char *const name[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    to = view_put(to, name[pitch % 12u]);
    if (pitch < 12u)
        view_put(to, "-1");
    else
        view_put_number(to, pitch / 12u - 1u);
}
static void view_fine_text(char *to, uint32_t fine)     /* 64 is in tune */
{
    *to++ = fine < 64u ? '-' : '+';
    view_put_number(to, fine < 64u ? 64u - fine : fine - 64u);
}
static uint32_t view_cutoff_hz(uint32_t value)          /* drone.h's: 20 Hz * 2^(value / 15) */
{
    static const uint16_t part_q10[15] = { 1024, 1072, 1123, 1176, 1232, 1290, 1351, 1415, 1482, 1552, 1625, 1702, 1783, 1867, 1956 };
    return ((20u << (value / 15u)) * part_q10[value % 15u]) >> 10;
}
static void view_hz_text(char *to, uint32_t hz)
{
    if (hz < 1000u) {
        view_put_number(to, hz);
        return;
    }
    hz = (hz + 50u) / 100u;                             /* tenths of a kilohertz */
    to = view_put_number(to, hz / 10u);
    if (hz < 100u) {
        *to++ = '.';
        to = view_put_number(to, hz % 10u);
    }
    view_put(to, "K");
}
static void view_time_text(char *to, uint32_t ms)
{
    if (ms < 100u) {
        view_put(view_put_number(to, ms), " MS");
        return;
    }
    if (ms < 9950u) {
        ms = (ms + 50u) / 100u;                         /* tenths of a second */
        to = view_put_number(to, ms / 10u);
        *to++ = '.';
        to = view_put_number(to, ms % 10u);
    } else {
        to = view_put_number(to, (ms + 500u) / 1000u);
    }
    view_put(to, " S");
}
static uint32_t view_fade_ms(uint32_t value)            /* drone_render's fade: the whole way, at 44118 frames a second */
{
    uint32_t slow = 127u - value;
    return DRONE_LEVEL_TOP / (8000u + slow * slow * slow * 5u) * 1000u / 44118u;
}
static uint32_t view_lfo_ms(uint32_t value)             /* one cycle: drone_render's speed, 62.6 s halved every 12 steps */
{
    static const uint16_t part_q10[12] = { 1024, 967, 912, 861, 813, 767, 724, 683, 645, 609, 575, 542 };
    return ((62622u >> (value / 12u)) * part_q10[value % 12u]) >> 10;
}
/* the reverb's time down to a thousandth for each decay value: 300 ms * 100^(value / 127), as drone_reverb_back_q13 */
static const uint16_t view_decay_ms[128] = {
    300, 311, 323, 334, 347, 360, 373, 387, 401, 416, 431, 447, 464, 481, 498, 517,
    536, 556, 576, 597, 620, 642, 666, 691, 716, 743, 770, 799, 828, 859, 890, 923,
    957, 993, 1029, 1067, 1107, 1148, 1190, 1234, 1280, 1327, 1376, 1427, 1479, 1534, 1590, 1649,
    1710, 1773, 1839, 1907, 1977, 2050, 2126, 2204, 2286, 2370, 2458, 2548, 2642, 2740, 2841, 2946,
    3055, 3168, 3285, 3406, 3532, 3662, 3797, 3938, 4083, 4234, 4390, 4552, 4720, 4895, 5075, 5263,
    5457, 5659, 5868, 6084, 6309, 6542, 6783, 7034, 7294, 7563, 7842, 8132, 8432, 8744, 9066, 9401,
    9748, 10108, 10482, 10869, 11270, 11686, 12118, 12565, 13029, 13510, 14009, 14526, 15063, 15619, 16196, 16794,
    17414, 18057, 18724, 19415, 20132, 20876, 21647, 22446, 23275, 24134, 25025, 25950, 26908, 27901, 28932, 30000,
};

static const char *const view_wave_name[DRONE_NWAVE] = { [DRONE_SAW] = "SAW", [DRONE_SQUARE] = "SQUARE", [DRONE_TRIANGLE] = "TRIANGLE" };
static const char *const view_wave_short[DRONE_NWAVE] = { [DRONE_SAW] = "SAW", [DRONE_SQUARE] = "SQUARE", [DRONE_TRIANGLE] = "TRI" };
static const char *const view_lfo_wave_name[DRONE_LFO_NWAVE] = {
    [DRONE_LFO_SINE] = "SINE", [DRONE_LFO_TRIANGLE] = "TRIANGLE", [DRONE_LFO_SAW] = "SAW", [DRONE_LFO_SQUARE] = "SQUARE",
};
static const char *const view_target_name[DRONE_NTARGET] = {
    [DRONE_TO_PITCH] = "TO PITCH", [DRONE_TO_CUTOFF] = "TO CUT", [DRONE_TO_RESONANCE] = "TO RES", [DRONE_TO_LEVEL] = "TO LEVEL",
};
static const char *const view_param_name[DRONE_NPARAM] = {
    [DRONE_PITCH] = "PITCH", [DRONE_WAVE] = "WAVE", [DRONE_FINE] = "FINE", [DRONE_CUTOFF] = "CUTOFF", [DRONE_RESONANCE] = "RESO",
    [DRONE_LEVEL] = "LEVEL", [DRONE_FADE] = "FADE",
};

/* ---- drawing ---- */
static void view_frame(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t thick, uint16_t colour)
{
    FONT_RECT(x, y, w, thick, colour);
    FONT_RECT(x, y + h - thick, w, thick, colour);
    FONT_RECT(x, y, thick, h, colour);
    FONT_RECT(x + w - thick, y, thick, h, colour);
}
/* text in a cell `w` wide that is cleared first; at its left, or with right != 0 at its right */
static void view_text(uint32_t x, uint32_t y, uint32_t w, uint32_t scale, const char *text, uint16_t colour, int right)
{
    uint32_t width = font_text_width(text, scale);
    FONT_RECT(x, y, w, 7u * scale, VIEW_BLACK);
    if (width <= w)
        font_draw_text(right ? x + w - width : x, y, scale, text, colour);
}

/* A picture with lines is drawn here first, then sent row by row: one pass over the screen, so it does not flicker.
 * One byte a pixel: an ink. */
enum { VIEW_INK_BLACK, VIEW_INK_WHITE, VIEW_INK_GREY, VIEW_INK_DIM, VIEW_INK_AMBER };
static const uint16_t view_ink[] = { VIEW_BLACK, VIEW_WHITE, VIEW_GREY, VIEW_DIM, VIEW_AMBER };
#define VIEW_CANVAS_W 112
#define VIEW_CANVAS_H 64
static uint8_t view_canvas[VIEW_CANVAS_W * VIEW_CANVAS_H];
static int32_t view_canvas_w, view_canvas_h;

static void view_canvas_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t ink)   /* what is outside is left out */
{
    int32_t i, j, right = x + w > view_canvas_w ? view_canvas_w : x + w, bottom = y + h > view_canvas_h ? view_canvas_h : y + h;
    for (j = y < 0 ? 0 : y; j < bottom; j++)
        for (i = x < 0 ? 0 : x; i < right; i++)
            view_canvas[j * VIEW_CANVAS_W + i] = ink;
}
static void view_canvas_start(int32_t w, int32_t h, uint32_t frame_thick, uint8_t frame_ink)
{
    view_canvas_w = w;
    view_canvas_h = h;
    view_canvas_rect(0, 0, w, h, frame_ink);
    view_canvas_rect((int32_t)frame_thick, (int32_t)frame_thick, w - 2 * (int32_t)frame_thick, h - 2 * (int32_t)frame_thick, VIEW_INK_BLACK);
}
static void view_canvas_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t ink)       /* two pixels thick; x0 <= x1 */
{
    int32_t x, before = y0;
    if (x1 <= x0) {
        view_canvas_rect(x0, y0 < y1 ? y0 : y1, 2, (y0 < y1 ? y1 - y0 : y0 - y1) + 2, ink);
        return;
    }
    for (x = x0; x <= x1; x++) {
        int32_t y = y0 + (y1 - y0) * (x - x0) / (x1 - x0);
        view_canvas_rect(x, y < before ? y : before, 2, (y < before ? before - y : y - before) + 2, ink);
        before = y;
    }
}
static void view_canvas_send(uint32_t x, uint32_t y)
{
    uint16_t row[VIEW_CANVAS_W];
    int32_t i, j;
    for (j = 0; j < view_canvas_h; j++) {
        for (i = 0; i < view_canvas_w; i++)
            row[i] = view_ink[view_canvas[j * VIEW_CANVAS_W + i]];
        VIEW_BLIT(x, y + (uint32_t)j, (uint32_t)view_canvas_w, 1u, row);
    }
}

/* ---- the chain: three boxes ---- */
enum { VIEW_OSC, VIEW_FILTER, VIEW_AMP, VIEW_NBOX, VIEW_NO_BOX = VIEW_NBOX };
#define VIEW_BOX_X(box) (8u + (box) * 78u)
#define VIEW_BOX_Y 28u
#define VIEW_BOX_W 68
#define VIEW_BOX_H 64
#define VIEW_LOWER_Y 134u               /* below the chain and its values: what the open page shows */
#define VIEW_VOICES_Y 212u

static uint32_t view_focus(const view_model_t *m)       /* the box with the amber frame */
{
    static const uint8_t box_of_param[DRONE_NPARAM] = {
        [DRONE_PITCH] = VIEW_OSC, [DRONE_WAVE] = VIEW_OSC, [DRONE_FINE] = VIEW_OSC, [DRONE_CUTOFF] = VIEW_FILTER,
        [DRONE_RESONANCE] = VIEW_FILTER, [DRONE_LEVEL] = VIEW_AMP, [DRONE_FADE] = VIEW_AMP,
    };
    static const uint8_t box_of_target[DRONE_NTARGET] = {
        [DRONE_TO_PITCH] = VIEW_OSC, [DRONE_TO_CUTOFF] = VIEW_FILTER, [DRONE_TO_RESONANCE] = VIEW_FILTER, [DRONE_TO_LEVEL] = VIEW_AMP,
    };
    if (m->choosing)
        return VIEW_FILTER;
    if (m->page == PANEL_VOICE)
        return m->touched < DRONE_NPARAM ? box_of_param[m->touched] : VIEW_NO_BOX;
    if (m->page == PANEL_REVERB)
        return VIEW_NO_BOX;
    return m->lfo[DRONE_LFO_TARGET] < DRONE_NTARGET ? box_of_target[m->lfo[DRONE_LFO_TARGET]] : VIEW_NO_BOX;
}

static void view_box(const view_model_t *m, uint32_t box)
{
    uint8_t ink = m->page == PANEL_VOICE ? VIEW_INK_WHITE : VIEW_INK_GREY;      /* on another page the chain is dimmed */
    int focus = view_focus(m) == box;
    view_canvas_start(VIEW_BOX_W, VIEW_BOX_H, focus ? 2 : 1, focus ? VIEW_INK_AMBER : VIEW_INK_DIM);
    if (box == VIEW_OSC) {                      /* one cycle of the wave */
        if (m->param[DRONE_WAVE] == DRONE_SQUARE) {
            view_canvas_line(6, 10, 6, 54, ink);
            view_canvas_line(6, 10, 33, 10, ink);
            view_canvas_line(33, 10, 33, 54, ink);
            view_canvas_line(33, 54, 60, 54, ink);
            view_canvas_line(60, 10, 60, 54, ink);
        } else if (m->param[DRONE_WAVE] == DRONE_TRIANGLE) {
            view_canvas_line(6, 54, 33, 10, ink);
            view_canvas_line(33, 10, 60, 54, ink);
        } else {
            view_canvas_line(6, 54, 60, 10, ink);
            view_canvas_line(60, 10, 60, 54, ink);
        }
    } else if (box == VIEW_FILTER) {            /* flat, the resonance's peak at the cutoff, then down: steeper with four poles */
        int32_t cut_x = 10 + m->param[DRONE_CUTOFF] * 40 / 127, peak_y = 30 - m->param[DRONE_RESONANCE] * 18 / 127;
        int32_t fall = m->filter == DRONE_LADDER ? 3 : 6, floor_x = cut_x + (56 - peak_y) * fall / 9;
        view_canvas_line(4, 30, cut_x - 8 > 4 ? cut_x - 8 : 4, 30, ink);
        view_canvas_line(cut_x - 8 > 4 ? cut_x - 8 : 4, 30, cut_x, peak_y, ink);
        if (floor_x <= 62) {
            view_canvas_line(cut_x, peak_y, floor_x, 56, ink);
            view_canvas_line(floor_x, 56, 62, 56, ink);
        } else {                                /* it leaves the box before it is down */
            view_canvas_line(cut_x, peak_y, 62, peak_y + (62 - cut_x) * 9 / fall, ink);
        }
    } else {                                    /* the fade in, as slow as it is, up to the level */
        int32_t top = 54 - m->param[DRONE_LEVEL] * 44 / 127, slope_x = 8 + m->param[DRONE_FADE] * 34 / 127, x;
        for (x = 6; x < 62; x += 4)
            view_canvas_rect(x, 58, 2, 1, VIEW_INK_DIM);
        view_canvas_line(6, 54, slope_x, top, ink);
        view_canvas_line(slope_x, top, 60, top, ink);
    }
    view_canvas_send(VIEW_BOX_X(box), VIEW_BOX_Y);
}

/* the values under a box, two rows; `hot`: the row of the knob turned last, or 2 */
static void view_box_values(const view_model_t *m, uint32_t box, uint32_t hot)
{
    char first[12], second[12], fine[6];
    uint32_t x = VIEW_BOX_X(box), row;
    if (box == VIEW_OSC) {
        view_note_text(first, m->param[DRONE_PITCH]);
        view_fine_text(fine, m->param[DRONE_FINE]);
        view_put(second, view_wave_short[m->param[DRONE_WAVE] < DRONE_NWAVE ? m->param[DRONE_WAVE] : DRONE_SAW]);
    } else if (box == VIEW_FILTER) {
        view_hz_text(first, view_cutoff_hz(m->param[DRONE_CUTOFF]));
        view_put_number(view_put(second, "RES "), m->param[DRONE_RESONANCE]);
    } else {
        view_put_number(view_put(first, "LVL "), m->param[DRONE_LEVEL]);
        view_time_text(second, view_fade_ms(m->param[DRONE_FADE]));
    }
    for (row = 0; row < 2u; row++)
        view_text(x, 98u + row * 18u, 76, 2, row ? second : first, row == hot ? VIEW_WHITE : VIEW_GREY, 0);
    if (box == VIEW_OSC && font_text_width(first, 2) + font_text_width(fine, 2) <= 68u)    /* the fine tuning, when there is room */
        font_draw_text(x + 68u - font_text_width(fine, 2), 98u, 2, fine, hot == 0 ? VIEW_WHITE : VIEW_GREY);
}

/* ---- the reverb's tail: bars that die away; a longer decay, a longer tail. A picture of the knob, not a
 * measure: the time is beside it as text ---- */
static void view_tail(uint32_t x, uint32_t bottom, uint32_t bars, uint32_t height, uint32_t decay, uint16_t colour)
{
    static const uint8_t rough[8] = { 16, 11, 14, 9, 13, 10, 15, 12 };          /* of 16: not a smooth slope */
    uint32_t last = 2u + (bars - 2u) * (decay & 127u) / 127u, i;                /* the bar where it is gone */
    for (i = 0; i < last; i++) {
        uint32_t h = height * (last - i) / last * rough[i & 7u] / 16u;
        if (h)
            FONT_RECT(x + i * 3u, bottom - h, 2, h, colour);
    }
}
static void view_send_bar(uint32_t y, uint32_t send, uint16_t colour)
{
    uint32_t w = send * 60u / 127u;
    FONT_RECT(62, y, w, 10, colour);
    FONT_RECT(62u + w, y, 60u - w, 10, VIEW_FAINT);
}

/* ---- the bottom: what the open page shows ---- */
static void view_lower_voice(const view_model_t *m, const view_model_t *shown, int all)
{
    uint32_t i;
    int values_new = all || m->touched != shown->touched || m->choosing != shown->choosing || m->filter != shown->filter;
    for (i = 0; i < DRONE_NPARAM; i++)
        values_new |= m->param[i] != shown->param[i];
    if (values_new) {                           /* the knob turned last, large; or the filter being chosen */
        char value[12];
        uint32_t touched = m->touched < DRONE_NPARAM ? m->touched : DRONE_CUTOFF, v = m->param[touched];
        const char *name = view_param_name[touched];
        if (m->choosing)
            name = "FILT", view_put(value, panel_filter_name[m->filter < DRONE_NFILTER ? m->filter : DRONE_LADDER]);
        else if (touched == DRONE_PITCH)
            view_note_text(value, v);
        else if (touched == DRONE_WAVE)
            view_put(value, view_wave_name[v < DRONE_NWAVE ? v : DRONE_SAW]);
        else if (touched == DRONE_FINE)
            view_fine_text(value, v);
        else if (touched == DRONE_CUTOFF)
            view_hz_text(value, view_cutoff_hz(v));
        else if (touched == DRONE_FADE)
            view_time_text(value, view_fade_ms(v));
        else
            view_put_number(value, v);
        FONT_RECT(8, 140, 224, 21, VIEW_BLACK);
        font_draw_text(8, 140, 3, name, VIEW_GREY);
        font_draw_text(232u - font_text_width(value, 3), 140, 3, value, m->choosing ? VIEW_AMBER : VIEW_WHITE);
    }
    if (all) {                                  /* the send into the reverb */
        FONT_RECT(8, 168, 224, 1, VIEW_DIM);
        font_draw_text(8, 182, 2, "SEND", VIEW_GREY);
        font_draw_text(126, 185, 1, ">", VIEW_GREY);
    }
    if (all || m->send != shown->send)
        view_send_bar(184, m->send, VIEW_GREY);
    if (all || m->reverb[DRONE_REVERB_DECAY] != shown->reverb[DRONE_REVERB_DECAY]) {
        view_frame(136, 174, 96, 32, 1, VIEW_DIM);
        FONT_RECT(137, 175, 94, 30, VIEW_BLACK);
        view_tail(140, 203, 30, 26, m->reverb[DRONE_REVERB_DECAY], VIEW_GREY);
    }
}

static void view_lower_lfo(const view_model_t *m, const view_model_t *shown, int all)
{
    char text[4][12];
    uint32_t wave = m->lfo[DRONE_LFO_WAVE] < DRONE_LFO_NWAVE ? m->lfo[DRONE_LFO_WAVE] : DRONE_LFO_SINE, row;
    int values_new = all || m->touched != shown->touched;
    for (row = 0; row < DRONE_LFO_NPARAM; row++)
        values_new |= m->lfo[row] != shown->lfo[row];
    if (all)
        FONT_RECT(8, 136, 224, 1, VIEW_DIM);
    if (all || m->lfo[DRONE_LFO_WAVE] != shown->lfo[DRONE_LFO_WAVE] || m->lfo[DRONE_LFO_DEPTH] != shown->lfo[DRONE_LFO_DEPTH]) {
        int32_t x, before = 31, reach = 1 + m->lfo[DRONE_LFO_DEPTH] * 23 / 127;   /* two cycles, as high as the depth */
        view_canvas_start(112, 62, 1, VIEW_INK_DIM);
        for (x = 6; x < 106; x += 4)
            view_canvas_rect(x, 31, 2, 1, VIEW_INK_DIM);
        for (x = 6; x < 106; x++) {
            int32_t y = 31 - drone_lfo_wave(wave, (uint32_t)(x - 6) * (0xFFFFFFFFu / 50u)) * reach / 32767;
            view_canvas_rect(x, y < before ? y : before, 2, (y < before ? before - y : y - before) + 2, VIEW_INK_WHITE);
            before = y;
        }
        view_canvas_send(8, 142);
    }
    if (values_new) {                           /* its speed as the time of a cycle, its wave, its depth, what it moves */
        view_time_text(text[DRONE_LFO_SPEED], view_lfo_ms(m->lfo[DRONE_LFO_SPEED]));
        view_put(text[DRONE_LFO_WAVE], view_lfo_wave_name[wave]);
        view_put_number(view_put(text[DRONE_LFO_DEPTH], "DEPTH "), m->lfo[DRONE_LFO_DEPTH]);
        view_put(text[DRONE_LFO_TARGET], view_target_name[m->lfo[DRONE_LFO_TARGET] < DRONE_NTARGET ? m->lfo[DRONE_LFO_TARGET] : DRONE_TO_PITCH]);
        for (row = 0; row < DRONE_LFO_NPARAM; row++)
            view_text(128, 142u + row * 16u, 104, 2, text[row], row == m->touched ? VIEW_WHITE : VIEW_GREY, 0);
    }
}

static void view_lower_reverb(const view_model_t *m, const view_model_t *shown, int all)
{
    char text[12];
    int touched_new = all || m->touched != shown->touched;
    int tail_new = all || m->reverb[DRONE_REVERB_DECAY] != shown->reverb[DRONE_REVERB_DECAY]
                   || m->reverb[DRONE_REVERB_TONE] != shown->reverb[DRONE_REVERB_TONE];
    if (all) {
        FONT_RECT(8, 136, 224, 1, VIEW_DIM);
        view_frame(8, 162, 224, 44, 2, VIEW_AMBER);
    }
    if (touched_new || m->send != shown->send) {        /* encoder 0: the selected voice's send */
        view_text(8, 142, 50, 2, "SEND", m->touched == 0 ? VIEW_WHITE : VIEW_GREY, 0);
        view_send_bar(144, m->send, m->touched == 0 ? VIEW_WHITE : VIEW_GREY);
    }
    if (touched_new || m->reverb[DRONE_REVERB_RETURN] != shown->reverb[DRONE_REVERB_RETURN]) {
        view_put_number(view_put(text, "RET "), m->reverb[DRONE_REVERB_RETURN]);
        view_text(140, 142, 92, 2, text, m->touched == 1u + DRONE_REVERB_RETURN ? VIEW_WHITE : VIEW_GREY, 1);
    }
    if (tail_new) {                             /* a darker tone: a greyer tail */
        FONT_RECT(10, 164, 118, 40, VIEW_BLACK);
        view_tail(14, 202, 38, 36, m->reverb[DRONE_REVERB_DECAY], m->reverb[DRONE_REVERB_TONE] >= 64 ? VIEW_WHITE : VIEW_GREY);
    }
    if (tail_new || touched_new) {
        view_time_text(text, view_decay_ms[m->reverb[DRONE_REVERB_DECAY] & 127u]);
        view_text(130, 168, 96, 2, text, m->touched == 1u + DRONE_REVERB_DECAY ? VIEW_WHITE : VIEW_GREY, 1);
        view_put_number(view_put(text, "TONE "), m->reverb[DRONE_REVERB_TONE]);
        view_text(130, 186, 96, 2, text, m->touched == 1u + DRONE_REVERB_TONE ? VIEW_WHITE : VIEW_GREY, 1);
    }
}

/* ---- a patch ---- */
#define VIEW_CELLS (DRONE_SLOTS + 1u)   /* a full chain and OUT */
#define VIEW_CELL_X(cell) (6u + (cell) * 34u)
#define VIEW_CELL_Y 28u
#define VIEW_CELL_W 28
#define VIEW_CELL_H 26
#define VIEW_PLACE_Y 72u                /* below the row: the place where the cursor is */
enum { VIEW_AT_NOTHING = PANEL_AT_OUT + 1 };
static const char *const view_block_name[DRONE_NBLOCK] = {
    [DRONE_BLOCK_NONE] = "", [DRONE_BLOCK_VCO] = "VCO", [DRONE_BLOCK_VCF] = "VCF", [DRONE_BLOCK_VCA] = "VCA",
};

typedef struct {                        /* all that one picture shows: bytes only, so that two compare byte by byte */
    uint8_t at;                         /* PANEL_AT_.., VIEW_AT_NOTHING */
    uint8_t cursor, preview, dim;
    drone_block_controls_t block;
    uint8_t level, fade, send;          /* OUT's */
} view_cell_t;

static int view_same(const void *a, const void *b, uint32_t bytes)
{
    const uint8_t *x = a, *y = b;
    while (bytes--)
        if (*x++ != *y++)
            return 0;
    return 1;
}

static view_cell_t view_cell(const view_model_t *m, uint32_t cell)
{
    uint32_t count = panel_patch_count(m->block);
    view_cell_t c = { 0 };
    c.at = (uint8_t)(cell < panel_patch_places(count) ? panel_patch_at(count, cell) : VIEW_AT_NOTHING);
    c.cursor = cell == m->place && m->page == PANEL_VOICE;
    c.dim = m->page != PANEL_VOICE;
    if (c.at == PANEL_AT_BLOCK)
        c.block = m->block[cell];
    if (c.at == PANEL_AT_OUT)
        c.level = m->param[DRONE_LEVEL], c.fade = m->param[DRONE_FADE], c.send = m->send;
    /* SEL held with another block chosen: the place shows what it gets */
    if (c.cursor && m->choosing && c.at != PANEL_AT_OUT && m->choice < DRONE_NBLOCK && m->choice != c.block.type) {
        c.preview = 1;
        c.at = m->choice == DRONE_BLOCK_NONE ? PANEL_AT_NEW : PANEL_AT_BLOCK;
        c.block = panel_block_new(m->choice);
    }
    return c;
}

/* what a block, the empty place or OUT looks like, on the canvas in the space x, y, w, h */
static void view_picture(const view_cell_t *c, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t ink)
{
    int32_t right = x + w - 2, bottom = y + h - 2, middle = y + (h - 2) / 2;
    if (c->at == PANEL_AT_NEW) {                /* a plus */
        view_canvas_rect(x + w / 2 - 4, y + h / 2 - 1, 9, 2, VIEW_INK_DIM);
        view_canvas_rect(x + w / 2 - 1, y + h / 2 - 4, 2, 9, VIEW_INK_DIM);
    } else if (c->at == PANEL_AT_OUT) {         /* the fade in, as slow as it is, up to the level */
        int32_t top = bottom - (c->level & 127) * (h - 2) / 127, slope_x = x + 2 + (c->fade & 127) * (w / 2) / 127;
        view_canvas_line(x, bottom, slope_x, top, ink);
        view_canvas_line(slope_x, top, right, top, ink);
    } else if (c->block.type == DRONE_BLOCK_VCO) {      /* one cycle of its wave */
        if (c->block.kind == DRONE_SQUARE) {
            view_canvas_line(x, y, x, bottom, ink);
            view_canvas_line(x, y, x + w / 2, y, ink);
            view_canvas_line(x + w / 2, y, x + w / 2, bottom, ink);
            view_canvas_line(x + w / 2, bottom, right, bottom, ink);
            view_canvas_line(right, y, right, bottom, ink);
        } else if (c->block.kind == DRONE_TRIANGLE) {
            view_canvas_line(x, bottom, x + w / 2, y, ink);
            view_canvas_line(x + w / 2, y, right, bottom, ink);
        } else {
            view_canvas_line(x, bottom, right, y, ink);
            view_canvas_line(right, y, right, bottom, ink);
        }
    } else if (c->block.type == DRONE_BLOCK_VCF) {      /* flat, the resonance's peak at the cutoff, then down: steeper with four poles */
        int32_t flat_y = y + h * 2 / 5, cut_x = x + w / 8 + (c->block.param[DRONE_VCF_CUTOFF] & 127) * (w * 5 / 8) / 127;
        int32_t peak_y = flat_y - (c->block.param[DRONE_VCF_RESONANCE] & 127) * (flat_y - y) / 127;
        int32_t fall = c->block.kind == DRONE_LADDER ? 3 : 6, floor_x = cut_x + 1 + (bottom - peak_y) * fall * w / (9 * h);
        int32_t rise_x = cut_x - w / 8;
        view_canvas_line(x, flat_y, rise_x, flat_y, ink);
        view_canvas_line(rise_x, flat_y, cut_x, peak_y, ink);
        if (floor_x <= right) {
            view_canvas_line(cut_x, peak_y, floor_x, bottom, ink);
            view_canvas_line(floor_x, bottom, right, bottom, ink);
        } else {                                /* it leaves the space before it is down */
            view_canvas_line(cut_x, peak_y, right, peak_y + (right - cut_x) * (bottom - peak_y) / (floor_x - cut_x), ink);
        }
    } else if (c->block.type == DRONE_BLOCK_VCA) {      /* an amplifier's triangle: as large as the level, in the outline of all of it */
        int32_t level = c->block.param[DRONE_VCA_LEVEL] & 127, half = (h - 2) / 2 * level / 127, tip = x + (w - 2) * level / 127;
        if (ink != VIEW_INK_DIM) {
            view_canvas_line(x, y, x, bottom, VIEW_INK_DIM);
            view_canvas_line(x, y, right, middle, VIEW_INK_DIM);
            view_canvas_line(x, bottom, right, middle, VIEW_INK_DIM);
        }
        view_canvas_line(x, middle - half, x, middle + half, ink);
        view_canvas_line(x, middle - half, tip, middle, ink);
        view_canvas_line(x, middle + half, tip, middle, ink);
    }
}

static const char *view_cell_name(const view_cell_t *c)
{
    return c->at == PANEL_AT_OUT ? "OUT" : c->at == PANEL_AT_BLOCK ? view_block_name[c->block.type < DRONE_NBLOCK ? c->block.type : 0] : "";
}

static void view_cell_draw(const view_cell_t *c, uint32_t cell)      /* the picture, the line from the one before, the name below */
{
    uint32_t x = VIEW_CELL_X(cell);
    const char *name = view_cell_name(c);
    if (cell)
        FONT_RECT(x - 6u, VIEW_CELL_Y + 12u, 6, 2, c->at == VIEW_AT_NOTHING ? VIEW_BLACK : VIEW_GREY);
    FONT_RECT(x, VIEW_CELL_Y + VIEW_CELL_H + 4u, VIEW_CELL_W, 7, VIEW_BLACK);
    if (c->at == VIEW_AT_NOTHING) {
        FONT_RECT(x, VIEW_CELL_Y, VIEW_CELL_W, VIEW_CELL_H, VIEW_BLACK);
        return;
    }
    view_canvas_start(VIEW_CELL_W, VIEW_CELL_H, c->cursor ? 2 : 1, c->cursor ? VIEW_INK_AMBER : VIEW_INK_DIM);
    view_picture(c, 5, 5, VIEW_CELL_W - 10, VIEW_CELL_H - 10, c->preview ? VIEW_INK_AMBER : c->dim ? VIEW_INK_GREY : VIEW_INK_WHITE);
    view_canvas_send(x, VIEW_CELL_Y);
    font_draw_text(x + (VIEW_CELL_W - font_text_width(name, 1)) / 2u, VIEW_CELL_Y + VIEW_CELL_H + 4u, 1, name,
                   c->cursor ? VIEW_AMBER : VIEW_GREY);
}

/* the place's values, one row for each of the right encoders; the one turned last in white */
static void view_place_values(const view_model_t *m, const view_cell_t *c)
{
    char text[3][16] = { "", "", "" };
    uint32_t row;
    if (c->at == PANEL_AT_OUT) {
        view_put_number(view_put(text[0], "LVL "), c->level);
        view_time_text(text[1], view_fade_ms(c->fade & 127u));
        view_put_number(view_put(text[2], "SEND "), c->send);
    } else if (c->block.type == DRONE_BLOCK_VCO) {
        view_note_text(text[0], c->block.param[DRONE_VCO_PITCH] & 127u);
        view_fine_text(view_put(text[1], "FINE "), c->block.param[DRONE_VCO_FINE] & 127u);
    } else if (c->block.type == DRONE_BLOCK_VCF) {
        view_hz_text(view_put(text[0], "CUT "), view_cutoff_hz(c->block.param[DRONE_VCF_CUTOFF] & 127u));
        view_put_number(view_put(text[1], "RES "), c->block.param[DRONE_VCF_RESONANCE] & 127u);
    } else if (c->block.type == DRONE_BLOCK_VCA) {
        view_put_number(view_put(text[0], "LVL "), c->block.param[DRONE_VCA_LEVEL] & 127u);
    }
    for (row = 0; row < 3u; row++)
        view_text(128, VIEW_PLACE_Y + 32u + row * 18u, 104, 2, text[row], row + PANEL_ENC_VALUES == m->touched ? VIEW_WHITE : VIEW_GREY, 0);
}

static const char *view_kind_name(const view_cell_t *c)
{
    if (c->at != PANEL_AT_BLOCK)
        return "";
    if (c->block.type == DRONE_BLOCK_VCO)
        return view_wave_name[c->block.kind < DRONE_NWAVE ? c->block.kind : DRONE_SAW];
    if (c->block.type == DRONE_BLOCK_VCF)
        return panel_filter_name[c->block.kind < DRONE_NFILTER ? c->block.kind : DRONE_LADDER];
    return "";
}

/* below the row: the place where the cursor is. `all`: it is drawn on a cleared screen */
static void view_place(const view_model_t *m, const view_model_t *shown, int all)
{
    view_cell_t now, before;
    view_model_t plain = *m;
    uint32_t i;
    int content_new, touched_new = all || m->touched != shown->touched;
    plain.choosing = 0;                         /* the place as it is, not what SEL chooses for it */
    now = view_cell(&plain, m->place);
    plain = *shown;
    plain.choosing = 0;
    before = view_cell(&plain, shown->place);
    if (!all && (now.at != before.at || now.block.type != before.block.type || m->choosing != shown->choosing || m->place != shown->place)) {
        FONT_RECT(0, VIEW_PLACE_Y + 2u, 240, VIEW_VOICES_Y - VIEW_PLACE_Y - 2u, VIEW_BLACK);
        all = 1;
    }
    content_new = all || !view_same(&now, &before, sizeof now);
    if (all)
        FONT_RECT(8, VIEW_PLACE_Y, 224, 1, VIEW_DIM);

    if (m->choosing && now.at != PANEL_AT_OUT) {        /* SEL held: the blocks to choose from, the chosen one amber */
        uint32_t x = 8;
        if (all) {
            font_draw_text(8, VIEW_PLACE_Y + 8u, 2, "PUT", VIEW_AMBER);
            font_draw_text(8, VIEW_PLACE_Y + 74u, 1, "LET GO OF SEL TO KEEP", VIEW_GREY);
        }
        for (i = 0; i < DRONE_NBLOCK; i++) {
            const char *name = i != DRONE_BLOCK_NONE ? view_block_name[i] : now.at == PANEL_AT_BLOCK ? "REMOVE" : "NONE";
            uint32_t w = font_text_width(name, 2);
            if (all || m->choice != shown->choice) {
                font_draw_text(x, VIEW_PLACE_Y + 40u, 2, name, i == m->choice ? VIEW_AMBER : VIEW_GREY);
                FONT_RECT(x, VIEW_PLACE_Y + 58u, w, 3, i == m->choice ? VIEW_AMBER : VIEW_BLACK);
            }
            x += w + 14u;
        }
        return;
    }
    if (now.at == PANEL_AT_NEW) {
        if (all) {
            font_draw_text(8, VIEW_PLACE_Y + 8u, 2, "EMPTY", VIEW_AMBER);
            font_draw_text(8, VIEW_PLACE_Y + 40u, 1, "HOLD SEL AND TURN SELECT", VIEW_WHITE);
            font_draw_text(8, VIEW_PLACE_Y + 54u, 1, "TO PUT A BLOCK HERE", VIEW_WHITE);
        }
        return;
    }
    if (all)
        font_draw_text(8, VIEW_PLACE_Y + 8u, 2, view_cell_name(&now), VIEW_AMBER);
    if (content_new || touched_new)
        view_text(60, VIEW_PLACE_Y + 8u, 172, 2, view_kind_name(&now), m->touched == PANEL_ENC_KIND ? VIEW_WHITE : VIEW_GREY, 1);
    if (content_new) {
        view_canvas_start(112, 62, 1, VIEW_INK_DIM);
        view_picture(&now, 10, 10, 92, 42, VIEW_INK_WHITE);
        view_canvas_send(8, VIEW_PLACE_Y + 30u);
    }
    if (content_new || touched_new)
        view_place_values(m, &now);
}

static void view_patch(const view_model_t *m, const view_model_t *shown, int all, int page_new)
{
    uint32_t cell;
    for (cell = 0; cell < VIEW_CELLS; cell++) {
        view_cell_t now = view_cell(m, cell), before = view_cell(shown, cell);
        if (all || !view_same(&now, &before, sizeof now))
            view_cell_draw(&now, cell);
    }
    if (page_new && !all)
        FONT_RECT(0, VIEW_PLACE_Y, 240, VIEW_VOICES_Y - VIEW_PLACE_Y, VIEW_BLACK);
    if (m->page == PANEL_VOICE)
        view_place(m, shown, page_new);
    else
        view_lower_reverb(m, shown, page_new);
}

/* ---- all of it ---- */
static view_model_t view_shown;
static uint8_t view_all = 1;

static void view_reset(void)                    /* the next view_show draws everything */
{
    view_all = 1;
}

static void view_show(const view_model_t *m)
{
    const view_model_t *shown = &view_shown;
    int all = view_all || m->patch != shown->patch, page_new = all || m->page != shown->page, chain_dim_new = all || (m->page == PANEL_VOICE) != (shown->page == PANEL_VOICE);
    uint32_t focus = view_focus(m), focus_before = view_focus(shown), box, i;
    /* on the voice's page, the knob turned last has its value in white: (box, row) */
    static const uint8_t hot_box[DRONE_NPARAM] = { VIEW_OSC, VIEW_OSC, VIEW_OSC, VIEW_FILTER, VIEW_FILTER, VIEW_AMP, VIEW_AMP };
    static const uint8_t hot_row[DRONE_NPARAM] = { 0, 1, 0, 0, 1, 0, 1 };
    int hot_on = m->page == PANEL_VOICE && !m->choosing && m->touched < DRONE_NPARAM;
    int hot_new = all || page_new || m->touched != shown->touched || m->choosing != shown->choosing;
    int box_new[VIEW_NBOX], values_new[VIEW_NBOX];

    if (all) {
        FONT_RECT(0, 0, 240, 240, VIEW_BLACK);
        for (box = 0; box + 1u < VIEW_NBOX && !m->patch; box++)
            font_draw_text(VIEW_BOX_X(box) + 70u, VIEW_BOX_Y + 28u, 1, ">", VIEW_GREY);
    }
    if (all || m->faults != shown->faults) {    /* a crash before this start; the audio hardware did not start */
        FONT_RECT(0, 0, 120, 2, m->faults & VIEW_CRASHED ? VIEW_AMBER : VIEW_BLACK);
        FONT_RECT(120, 0, 120, 2, m->faults & VIEW_NO_AUDIO ? VIEW_RED : VIEW_BLACK);
    }

    if (all || m->selected != shown->selected) {
        char name[4] = "OP1";
        name[2] = (char)('1' + m->selected);
        FONT_RECT(8, 4, 40, 18, VIEW_WHITE);
        font_draw_text(11, 6, 2, name, VIEW_BLACK);
    }
    if (page_new || m->filter != shown->filter || m->choosing != shown->choosing || m->info != shown->info
        || (m->info && m->load_percent != shown->load_percent)) {
        FONT_RECT(56, 4, 176, 18, VIEW_BLACK);
        if (m->info) {                          /* for the one who builds it: which firmware, and the load */
            char load[8];
            view_put(view_put_number(load, m->load_percent), "%");
            font_draw_text(56, 6, 2, APP_VERSION, VIEW_WHITE);
            font_draw_text(232u - font_text_width(load, 2), 6, 2, load, VIEW_WHITE);
        } else {
            const char *page_name = m->patch && m->page == PANEL_VOICE ? "PATCH" : panel_page_name[m->page < PANEL_NPAGE ? m->page : PANEL_VOICE];
            if (!m->patch)
                font_draw_text(56, 6, 2, panel_filter_name[m->filter < DRONE_NFILTER ? m->filter : DRONE_LADDER], m->choosing ? VIEW_AMBER : VIEW_WHITE);
            font_draw_text(232u - font_text_width(page_name, 2), 6, 2, page_name, VIEW_GREY);
        }
    }

    if (m->patch) {
        view_patch(m, shown, all, page_new);
    } else {
        for (box = 0; box < VIEW_NBOX; box++)
            box_new[box] = values_new[box] = chain_dim_new || (focus == box) != (focus_before == box);
        box_new[VIEW_OSC] |= m->param[DRONE_WAVE] != shown->param[DRONE_WAVE];
        box_new[VIEW_FILTER] |= m->param[DRONE_CUTOFF] != shown->param[DRONE_CUTOFF] || m->param[DRONE_RESONANCE] != shown->param[DRONE_RESONANCE]
                                || m->filter != shown->filter;
        box_new[VIEW_AMP] |= m->param[DRONE_LEVEL] != shown->param[DRONE_LEVEL] || m->param[DRONE_FADE] != shown->param[DRONE_FADE];
        for (i = 0; i < DRONE_NPARAM; i++)
            values_new[hot_box[i]] |= m->param[i] != shown->param[i];
        for (box = 0; box < VIEW_NBOX; box++) {
            if (box_new[box])
                view_box(m, box);
            if (values_new[box] || hot_new)
                view_box_values(m, box, hot_on && hot_box[m->touched] == box ? hot_row[m->touched] : 2u);
        }

        if (page_new)
            FONT_RECT(0, VIEW_LOWER_Y, 240, VIEW_VOICES_Y - VIEW_LOWER_Y, VIEW_BLACK);
        if (m->page == PANEL_VOICE)
            view_lower_voice(m, shown, page_new);
        else if (m->page == PANEL_REVERB)
            view_lower_reverb(m, shown, page_new);
        else
            view_lower_lfo(m, shown, page_new);
    }

    if (all || m->voices_on != shown->voices_on || m->selected != shown->selected)
        for (i = 0; i < DRONE_VOICES; i++) {    /* filled: on; an outline: off; the amber mark: the knobs are this one's */
            uint32_t x = 8u + i * 38u, on = m->voices_on >> i & 1u;
            char digit[2] = { (char)('1' + i), 0 };
            FONT_RECT(x, VIEW_VOICES_Y, 34, 18, on ? VIEW_WHITE : VIEW_BLACK);
            if (!on)
                view_frame(x, VIEW_VOICES_Y, 34, 18, 2, VIEW_DIM);
            font_draw_text(x + 12u, VIEW_VOICES_Y + 2u, 2, digit, on ? VIEW_BLACK : VIEW_GREY);
            FONT_RECT(x, VIEW_VOICES_Y + 20u, 34, 4, i == m->selected ? VIEW_AMBER : VIEW_BLACK);
        }

    if (all || m->load_percent != shown->load_percent) {        /* the load: a thin line; red when the sound is near breaking up */
        uint32_t w = (m->load_percent > 100u ? 100u : m->load_percent) * 240u / 100u;
        FONT_RECT(0, 238, w, 2, m->load_percent > 80u ? VIEW_RED : VIEW_DIM);
        FONT_RECT(w, 238, 240u - w, 2, VIEW_BLACK);
    }
    view_shown = *m;
    view_all = 0;
}
