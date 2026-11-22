/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kevin Sint */
/* The screen: the selected voice as its chain of blocks (drone.h), left to right:
 *
 *   OP1                        PATCH      the voice, the open page
 *   [VCO] [VCF] [VCA] [ + ] [OUT]         a picture for each block, then the empty place while there is room, then OUT
 *   below the row: the place where the cursor is, large, with its values; or what the open page shows
 *   [    1    ] [    2    ] [    3    ]   the voices: filled is on, the amber mark is the selected one
 *
 * The place where the cursor is has the amber frame. While a block is chosen for it (OCT- held): the blocks to
 * choose from. Above each picture, three small marks: the LFOs with a cable to it, the open LFO's brighter. On an
 * LFO's page the LFO is below the row: its wave, its speed and the list of its cables. While the LFO key is
 * held, the place's values give way to the amounts of that LFO's cables to them; on an LFO's page, to the LFO's
 * cables to the two other LFOs. The feedback cable is a green line over the row, from the output of its first block
 * back to the input of its second one, with arrows that show its direction, and has a page with its four values.
 *
 * The end of the chain (the GLO key) has the same frame, with EOC in the voice's place: its row is what the voices'
 * sends go through, the delay then the reverb, then the compressor where the voices themselves (DRY) join them, then
 * OUT. Below the row: the open effect, large, with its four values.
 *
 * A disruptor (the FX key) has DS1, DS2 or DS3 in the voice's place. Below: a picture of the disruptor as its values
 * make it, a dial for each right encoder, and over each voice's box how much of it that voice takes.
 *
 * The bank's page (the LFO key) has the shown LFO's number in the voice's place, its wave as the engine plays it, its
 * values and the row of the twelve LFOs. On the play page the bank adds marks under the dials, and its amounts while
 * the LFO key is held, in cyan.
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
       VIEW_AMBER = 0xFD20, VIEW_CYAN = 0x5EFC, VIEW_SEA = 0x1249, VIEW_GREEN = 0x5F2B };
#define VIEW_ENCODERS 7u

typedef struct {                        /* all that the screen shows */
    uint8_t voices_on;                  /* bit n: voice n is on */
    uint8_t selected;                   /* the voice the knobs are for */
    uint8_t page;                       /* PANEL_VOICE .. */
    uint8_t choosing;                   /* a block is chosen for the cursor's place (OCT- held) */
    uint8_t touched;                    /* the encoder turned last on this page */
    uint8_t param[DRONE_NPARAM];        /* the selected voice's level and fade */
    uint8_t lfo[DRONE_LFO_NPARAM];      /* its LFO, on an LFO's page */
    uint8_t send;
    uint8_t reverb[DRONE_REVERB_NPARAM];
    uint8_t delay[DRONE_DELAY_NPARAM];
    uint8_t comp[DRONE_COMP_NPARAM];
    uint8_t info;                       /* show the version and the load in place of the page */
    uint8_t load_percent;
    uint8_t faults;                     /* VIEW_CRASHED | VIEW_NO_AUDIO */
    uint8_t place;                      /* where the cursor is in its chain */
    uint8_t choice;                     /* while a block is chosen: the one chosen so far for that place */
    uint8_t lfo_held;                   /* the LFO key is down: the cables' amounts in place of the values */
    uint8_t lfo_current;                /* the LFO the cables shown are of: 0 .. */
    drone_block_controls_t block[DRONE_SLOTS];
    int8_t out_amount[DRONE_OUT_NCABLED][DRONE_LFOS];
    int8_t lfo_amount[DRONE_LFOS][DRONE_LFO_NCABLED][DRONE_LFOS];
    uint8_t fb[DRONE_FB_NPARAM];
    int8_t fb_amount[DRONE_LFOS];
    uint8_t reverb_holder;              /* the voice whose chain has the reverb, or DRONE_VOICES: the shared one */
    uint8_t fx_place;                   /* the place of the selected patch's FX block that has the reverb, or DRONE_SLOTS: none */
    int8_t scope[2][DRONE_SCOPE_POINTS];        /* the feedback's page: the sound down the chain, the sound from the cable; -28 .. 28.
                                                 * The play page: the first one is the voice's sound */
    uint8_t style;                      /* the play page: the selected voice's style, its frequency and its knobs */
    uint8_t shift;                      /* and the shift key is down: the knobs' second set */
    uint16_t freq;
    uint8_t knob[PLAY_NKNOB];
    uint8_t link[DRONE_VOICES];         /* each voice's LINK: the arrows between the voices' boxes */
    uint8_t link_kind[DRONE_VOICES];    /* and its kind: FOLLOW's line is in dashes, THROUGH's is whole */
    uint8_t mixer[DRONE_VOICES][MIXER_NVALUE];  /* the mixer's page: each voice's channel */
    uint8_t post[DRONE_VOICES];         /* its sends are taken after its level */
    uint8_t channel;                    /* the channel selected there: 0 none, 1 .. the voices' */
    uint8_t dis[DRONE_DISRUPTORS][DRONE_DIS_NPARAM];    /* the disruptors' pages: each one's values */
    uint8_t dis_send[DRONE_DISRUPTORS][DRONE_VOICES];   /* how much of it each voice takes */
    uint8_t dis_to[DRONE_DISRUPTORS][DRONE_VOICES];     /* and where: DRONE_TO_... */
    /* the bank (V0.32). On its page `touched` is what was turned last: a DRONE_BANK_... value, or PANEL_BANK_TEMPO */
    uint8_t bank_lfo;                   /* the LFO on the bank's page, and the one the amounts are of: 0 .. */
    uint8_t amount_touched;             /* the LFO key held on the play page: the encoder turned last */
    uint16_t tempo;                     /* BPM */
    uint16_t bank_used;                 /* bit l: a voice has an amount of the bank's LFO l */
    uint8_t bank[DRONE_BANK_LFOS][DRONE_BANK_NPARAM];
    int8_t bank_amount[PLAY_NBANKED][DRONE_BANK_LFOS];  /* the selected voice's */
    int16_t bank_out[DRONE_BANK_LFOS];  /* the engine's bank as it was last taken (drone_bank_view_t): each LFO's output */
    uint8_t bank_width;                 /* the shown LFO's WIDTH with what the cables add */
    uint32_t bank_phase;                /* where it is in its cycle */
    uint32_t clock_phase;               /* where the clock is in its beat: as the engine gave it last, not ten times a second */
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
static const char *const view_lfo_wave_name[DRONE_LFO_NWAVE] = {
    [DRONE_LFO_SINE] = "SINE", [DRONE_LFO_TRIANGLE] = "TRIANGLE", [DRONE_LFO_SAW] = "SAW", [DRONE_LFO_SQUARE] = "SQUARE",
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
enum { VIEW_INK_BLACK, VIEW_INK_WHITE, VIEW_INK_GREY, VIEW_INK_DIM, VIEW_INK_AMBER, VIEW_INK_CYAN, VIEW_INK_GREEN };
static const uint16_t view_ink[] = { VIEW_BLACK, VIEW_WHITE, VIEW_GREY, VIEW_DIM, VIEW_AMBER, VIEW_CYAN, VIEW_GREEN };
#define VIEW_CANVAS_W 224
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
/* the canvas's columns from .. from + w only, at their place: what is outside the canvas is left out */
static void view_canvas_send_part(uint32_t x, uint32_t y, int32_t from, int32_t w)
{
    static uint16_t row[VIEW_CANVAS_W];
    int32_t i, j;
    w = from < 0 ? w + from : w;
    from = from < 0 ? 0 : from;
    w = from + w > view_canvas_w ? view_canvas_w - from : w;
    for (j = 0; j < view_canvas_h && w > 0; j++) {
        for (i = 0; i < w; i++)
            row[i] = view_ink[view_canvas[j * VIEW_CANVAS_W + from + i]];
        VIEW_BLIT(x + (uint32_t)from, y + (uint32_t)j, (uint32_t)w, 1u, row);
    }
}
static void view_canvas_send(uint32_t x, uint32_t y)
{
    view_canvas_send_part(x, y, 0, view_canvas_w);
}

#define VIEW_VOICES_Y 212u              /* the voices' boxes: they and the gaps of 4 between them fill 8 .. 232 */
#define VIEW_VOICE_STEP (228u / DRONE_VOICES)
#define VIEW_VOICE_W (VIEW_VOICE_STEP - 4u)

/* ---- a patch ---- */
#define VIEW_CELLS (DRONE_SLOTS + 1u)   /* a full chain and OUT */
#define VIEW_CELL_X(cell) (6u + (cell) * 34u)
#define VIEW_CELL_Y 35u                 /* above the row: the LFOs' marks, and above them the feedback cable */
#define VIEW_CELL_W 28
#define VIEW_CELL_H 26
#define VIEW_PLACE_Y 72u                /* below the row: the place where the cursor is */
enum { VIEW_AT_NOTHING = PANEL_AT_OUT + 1 };
static const char *const view_block_name[DRONE_NBLOCK] = {
    [DRONE_BLOCK_NONE] = "", [DRONE_BLOCK_VCO] = "VCO", [DRONE_BLOCK_VCF] = "VCF", [DRONE_BLOCK_VCA] = "VCA", [DRONE_BLOCK_FX] = "FX",
};

typedef struct {                        /* all that one picture shows: bytes only, so that two compare byte by byte */
    uint8_t at;                         /* PANEL_AT_.., VIEW_AT_NOTHING */
    uint8_t cursor, preview;
    drone_block_controls_t block;
    uint8_t level, fade, send;          /* OUT's */
    int8_t out_amount[DRONE_OUT_NCABLED][DRONE_LFOS];
    uint8_t cabled;                     /* bit l: LFO l has a cable to it */
    uint8_t lfo_current;
    uint8_t idle;                       /* an FX block that does not have the reverb */
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
    uint32_t p, l;
    c.cursor = cell == m->place;
    c.lfo_current = m->lfo_current;
    if (c.at == PANEL_AT_BLOCK)
        c.block = m->block[cell];
    c.idle = c.at == PANEL_AT_BLOCK && c.block.type == DRONE_BLOCK_FX && cell != m->fx_place;
    if (c.at == PANEL_AT_OUT)
        c.level = m->param[DRONE_LEVEL], c.fade = m->param[DRONE_FADE], c.send = m->send;
    for (l = 0; l < DRONE_LFOS; l++) {
        for (p = 0; p < DRONE_OUT_NCABLED && c.at == PANEL_AT_OUT; p++)
            c.cabled |= (uint8_t)((m->out_amount[p][l] != 0) << l), c.out_amount[p][l] = m->out_amount[p][l];
        for (p = 0; p < DRONE_BLOCK_NPARAM && c.at == PANEL_AT_BLOCK; p++)
            c.cabled |= (uint8_t)((c.block.amount[p][l] != 0) << l);
    }
    /* OCT- held with another block chosen: the place shows what it gets */
    if (c.cursor && m->choosing && c.at != PANEL_AT_OUT && m->choice < DRONE_NBLOCK && m->choice != c.block.type) {
        c.preview = 1;
        c.cabled = c.idle = 0;
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
    } else if (c->block.type == DRONE_BLOCK_FX) {       /* the reverb's tail: bars that die away, further with a longer decay */
        static const uint8_t rough[8] = { 16, 11, 14, 9, 13, 10, 15, 12 };      /* of 16: not a smooth slope */
        int32_t bars = w / 3, last = 2 + (bars - 2) * (c->block.param[DRONE_FX_DECAY] & 127) / 127, i;
        for (i = 0; i < last; i++) {
            int32_t tall = (h - 2) * (last - i) / last * rough[i & 7] / 16;
            view_canvas_rect(x + i * 3, bottom - tall + 1, 2, tall + 1, c->idle ? VIEW_INK_DIM : ink);
        }
    }
}

static const char *view_cell_name(const view_cell_t *c)
{
    return c->at == PANEL_AT_OUT ? "OUT" : c->at == PANEL_AT_BLOCK ? view_block_name[c->block.type < DRONE_NBLOCK ? c->block.type : 0] : "";
}

static void view_cell_draw(const view_cell_t *c, uint32_t cell)      /* the picture, the line from the one before, the name below */
{
    uint32_t x = VIEW_CELL_X(cell), l;
    const char *name = view_cell_name(c);
    for (l = 0; l < DRONE_LFOS; l++)            /* the LFOs with a cable to it */
        FONT_RECT(x + 1u + l * 9u, VIEW_CELL_Y - 5u, 8, 3, !(c->cabled >> l & 1u) ? VIEW_BLACK : l == c->lfo_current ? VIEW_CYAN : VIEW_SEA);
    if (cell)
        FONT_RECT(x - 6u, VIEW_CELL_Y + 12u, 6, 2, c->at == VIEW_AT_NOTHING ? VIEW_BLACK : VIEW_GREY);
    FONT_RECT(x, VIEW_CELL_Y + VIEW_CELL_H + 3u, VIEW_CELL_W, 7, VIEW_BLACK);
    if (c->at == VIEW_AT_NOTHING) {
        FONT_RECT(x, VIEW_CELL_Y, VIEW_CELL_W, VIEW_CELL_H, VIEW_BLACK);
        return;
    }
    view_canvas_start(VIEW_CELL_W, VIEW_CELL_H, c->cursor ? 2 : 1, c->cursor ? VIEW_INK_AMBER : VIEW_INK_DIM);
    view_picture(c, 5, 5, VIEW_CELL_W - 10, VIEW_CELL_H - 10, c->preview ? VIEW_INK_AMBER : VIEW_INK_WHITE);
    view_canvas_send(x, VIEW_CELL_Y);
    font_draw_text(x + (VIEW_CELL_W - font_text_width(name, 1)) / 2u, VIEW_CELL_Y + VIEW_CELL_H + 3u, 1, name,
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
    } else if (c->block.type == DRONE_BLOCK_FX) {
        view_put_number(view_put(text[0], "MIX "), c->block.param[DRONE_FX_MIX] & 127u);
        view_time_text(text[1], view_decay_ms[c->block.param[DRONE_FX_DECAY] & 127u]);
        view_put_number(view_put(text[2], "TONE "), c->block.param[DRONE_FX_TONE] & 127u);
    }
    for (row = 0; row < 3u; row++)
        view_text(128, VIEW_PLACE_Y + 32u + row * 18u, 104, 2, text[row], row + PANEL_ENC_VALUES == m->touched ? VIEW_WHITE : VIEW_GREY, 0);
}

/* a cable's name and amount: "CUT -25" */
static char *view_amount_text(char *to, const char *name, int32_t amount)
{
    to = view_put(to, name);
    *to++ = ' ';
    *to++ = amount < 0 ? '-' : '+';
    return view_put_number(to, (uint32_t)(amount < 0 ? -amount : amount));
}
static const char *const view_cable_name[DRONE_NBLOCK][DRONE_BLOCK_NPARAM] = {
    [DRONE_BLOCK_NONE] = { "", "", "" }, [DRONE_BLOCK_VCO] = { "PIT", "", "" }, [DRONE_BLOCK_VCF] = { "CUT", "RES", "" },
    [DRONE_BLOCK_VCA] = { "LVL", "", "" }, [DRONE_BLOCK_FX] = { "MIX", "", "" },
};
static const char *const view_out_cable_name[DRONE_OUT_NCABLED] = { [DRONE_OUT_LEVEL] = "LVL", [DRONE_OUT_SEND] = "SND" };

/* the LFO key held: in the values' rows, the amount of the LFO's cable to each value; a row with no cable is grey */
static void view_place_amounts(const view_model_t *m, const view_cell_t *c)
{
    uint32_t row, lfo = m->lfo_current < DRONE_LFOS ? m->lfo_current : 0u;
    for (row = 0; row < 3u; row++) {
        char text[16] = "";
        int32_t cable = panel_patch_cable(c->at, c->block.type, PANEL_ENC_VALUES + row), amount = 0;
        if (cable >= 0) {
            amount = c->at == PANEL_AT_OUT ? c->out_amount[cable][lfo] : c->block.amount[cable][lfo];
            view_amount_text(text, c->at == PANEL_AT_OUT ? view_out_cable_name[cable] : view_cable_name[c->block.type][cable], amount);
        }
        view_text(128, VIEW_PLACE_Y + 32u + row * 18u, 104, 2, text, amount ? VIEW_CYAN : VIEW_GREY, 0);
    }
}

static const char *view_kind_name(const view_cell_t *c)
{
    if (c->at != PANEL_AT_BLOCK)
        return "";
    if (c->block.type == DRONE_BLOCK_VCO)
        return view_wave_name[c->block.kind < DRONE_NWAVE ? c->block.kind : DRONE_SAW];
    if (c->block.type == DRONE_BLOCK_VCF)
        return panel_filter_name[c->block.kind < DRONE_NFILTER ? c->block.kind : DRONE_LADDER];
    if (c->block.type == DRONE_BLOCK_FX)        /* there is one reverb: another FX block has it */
        return c->idle ? "NO REVERB" : "REVERB";
    return "";
}

static void view_lfo_name(char *to, uint32_t lfo)
{
    to = view_put(to, "LFO ");
    *to++ = (char)('1' + (lfo < DRONE_LFOS ? lfo : 0u));
    *to = 0;
}

/* a patch's LFO page, below the row: the LFO's wave, as high as its depth, its speed and depth, and its cables as a
 * list: to a block (the place in the chain, the block, the value), to OUT, to an LFO, to the feedback's level.
 * `all`: it is drawn on a cleared screen */
#define VIEW_CABLE_ROWS 6u
#define VIEW_CABLE_Y (VIEW_PLACE_Y + 72u)
static void view_cable_row(const char *text, uint32_t *row, uint32_t *more)
{
    if (*row >= VIEW_CABLE_ROWS - 1u)           /* the last row stays free for how many more there are */
        (*more)++;
    else
        view_text(128, VIEW_CABLE_Y + (*row)++ * 12u, 104, 1, text, VIEW_CYAN, 0);
}
static const char *const view_lfo_cable_name[DRONE_LFO_NCABLED] = { [DRONE_LFO_TO_SPEED] = "SPD", [DRONE_LFO_TO_DEPTH] = "DEP" };

static void view_place_lfo(const view_model_t *m, const view_model_t *shown, int all)
{
    char text[24];
    uint32_t wave = m->lfo[DRONE_LFO_WAVE] < DRONE_LFO_NWAVE ? m->lfo[DRONE_LFO_WAVE] : DRONE_LFO_SINE;
    uint32_t lfo = m->lfo_current < DRONE_LFOS ? m->lfo_current : 0u, count = panel_patch_count(m->block), cell, p, row = 0, more = 0;
    int touched_new = all || m->touched != shown->touched;
    if (all) {
        view_lfo_name(text, lfo);
        font_draw_text(8, VIEW_PLACE_Y + 8u, 2, text, VIEW_CYAN);
    }
    if (touched_new || m->lfo[DRONE_LFO_WAVE] != shown->lfo[DRONE_LFO_WAVE])
        view_text(80, VIEW_PLACE_Y + 8u, 152, 2, view_lfo_wave_name[wave], m->touched == PANEL_ENC_KIND ? VIEW_WHITE : VIEW_GREY, 1);
    if (all || m->lfo[DRONE_LFO_WAVE] != shown->lfo[DRONE_LFO_WAVE] || m->lfo[DRONE_LFO_DEPTH] != shown->lfo[DRONE_LFO_DEPTH]) {
        int32_t x, before = 31, reach = 1 + (m->lfo[DRONE_LFO_DEPTH] & 127) * 22 / 127;     /* two cycles */
        view_canvas_start(112, 62, 1, VIEW_INK_DIM);
        for (x = 6; x < 106; x += 4)
            view_canvas_rect(x, 31, 2, 1, VIEW_INK_DIM);
        for (x = 6; x < 106; x++) {
            int32_t y = 31 - drone_lfo_wave(wave, (uint32_t)(x - 6) * (0xFFFFFFFFu / 50u)) * reach / 32767;
            view_canvas_rect(x, y < before ? y : before, 2, (y < before ? before - y : y - before) + 2, VIEW_INK_CYAN);
            before = y;
        }
        view_canvas_send(8, VIEW_PLACE_Y + 30u);
    }
    if (touched_new || m->lfo[DRONE_LFO_SPEED] != shown->lfo[DRONE_LFO_SPEED]) {
        view_time_text(text, view_lfo_ms(m->lfo[DRONE_LFO_SPEED] & 127u));
        view_text(128, VIEW_PLACE_Y + 32u, 104, 2, text, m->touched == PANEL_ENC_VALUES ? VIEW_WHITE : VIEW_GREY, 0);
    }
    if (touched_new || m->lfo[DRONE_LFO_DEPTH] != shown->lfo[DRONE_LFO_DEPTH]) {
        view_put_number(view_put(text, "DEP "), m->lfo[DRONE_LFO_DEPTH] & 127u);
        view_text(128, VIEW_PLACE_Y + 50u, 104, 2, text, m->touched == PANEL_ENC_VALUES + 1u ? VIEW_WHITE : VIEW_GREY, 0);
    }
    if (!all && view_same(m->block, shown->block, sizeof m->block) && view_same(m->out_amount, shown->out_amount, sizeof m->out_amount)
        && view_same(m->lfo_amount, shown->lfo_amount, sizeof m->lfo_amount) && view_same(m->fb_amount, shown->fb_amount, sizeof m->fb_amount))
        return;
    for (cell = 0; cell <= count; cell++)       /* each block's cables, then OUT's */
        for (p = 0; p < (cell < count ? DRONE_BLOCK_NPARAM : DRONE_OUT_NCABLED); p++) {
            int32_t amount = cell < count ? m->block[cell].amount[p][lfo] : m->out_amount[p][lfo];
            char *to = text;
            if (!amount || (cell < count && !drone_block_reach[m->block[cell].type][p]))
                continue;
            if (cell < count) {
                to = view_put(view_put_number(to, cell + 1u), " ");
                to = view_put(view_put(to, view_block_name[m->block[cell].type]), " ");
                view_amount_text(to, view_cable_name[m->block[cell].type][p], amount);
            } else {
                view_amount_text(view_put(to, "OUT "), view_out_cable_name[p], amount);
            }
            view_cable_row(text, &row, &more);
        }
    for (cell = 0; cell < DRONE_LFOS; cell++)   /* to the other LFOs */
        for (p = 0; p < DRONE_LFO_NCABLED; p++)
            if (m->lfo_amount[cell][p][lfo]) {
                char *to = view_put(text, "LFO ");
                *to++ = (char)('1' + cell);
                *to++ = ' ';
                view_amount_text(to, view_lfo_cable_name[p], m->lfo_amount[cell][p][lfo]);
                view_cable_row(text, &row, &more);
            }
    if (m->fb_amount[lfo]) {
        view_amount_text(text, "FB LVL", m->fb_amount[lfo]);
        view_cable_row(text, &row, &more);
    }
    if (!row && !more)
        view_text(128, VIEW_CABLE_Y + row++ * 12u, 104, 1, "NO CABLE", VIEW_GREY, 0);
    if (more) {
        view_put(view_put_number(view_put(text, "+"), more), " MORE");
        view_text(128, VIEW_CABLE_Y + row++ * 12u, 104, 1, text, VIEW_GREY, 0);
    }
    for (; row < VIEW_CABLE_ROWS; row++)
        view_text(128, VIEW_CABLE_Y + row * 12u, 104, 1, "", VIEW_GREY, 0);
}

/* the LFO key held on an LFO's page: its cables to the two other LFOs, a row for each right encoder */
static void view_place_lfo_to(const view_model_t *m, const view_model_t *shown, int all)
{
    char text[24], *to;
    uint32_t lfo = m->lfo_current < DRONE_LFOS ? m->lfo_current : 0u, n, target, what;
    if (all) {
        view_lfo_name(text, lfo);
        view_put(text + 5, " TO");
        font_draw_text(8, VIEW_PLACE_Y + 8u, 2, text, VIEW_CYAN);
    }
    if (!all && view_same(m->lfo_amount, shown->lfo_amount, sizeof m->lfo_amount))
        return;
    for (n = 0; panel_lfo_cable(PANEL_ENC_VALUES + n, lfo, &target, &what); n++) {
        to = view_put(text, "LFO ");
        *to++ = (char)('1' + target);
        *to++ = ' ';
        view_amount_text(to, what == DRONE_LFO_TO_SPEED ? "SPEED" : "DEPTH", m->lfo_amount[target][what][lfo]);
        view_text(8, VIEW_PLACE_Y + 32u + n * 18u, 224, 2, text, m->lfo_amount[target][what][lfo] ? VIEW_CYAN : VIEW_GREY, 0);
    }
}

/* the feedback cable ---- */
typedef struct { uint8_t on, from, to; } view_fb_t;
static view_fb_t view_fb(const view_model_t *m)
{
    view_fb_t fb = { m->fb[DRONE_FB_LEVEL] != 0, m->fb[DRONE_FB_FROM] < DRONE_SLOTS ? m->fb[DRONE_FB_FROM] : DRONE_SLOTS - 1u,
                     m->fb[DRONE_FB_TO] < DRONE_SLOTS ? m->fb[DRONE_FB_TO] : DRONE_SLOTS - 1u };
    uint32_t l;
    for (l = 0; l < DRONE_LFOS; l++)
        fb.on |= m->fb_amount[l] != 0;
    return fb;
}
/* over the row, in the gaps between the pictures: up from after its first block, along, and down to before its second.
 * Its direction: an arrow head where it goes back in, and one in the middle of the line over the row */
static void view_fb_cable(const view_model_t *m, const view_model_t *shown, int all)
{
    view_fb_t now = view_fb(m), before = view_fb(shown);
    uint32_t cell, from_x = VIEW_CELL_X(now.from) + VIEW_CELL_W + 2u, to_x = VIEW_CELL_X(now.to) - 4u, middle = (from_x + to_x) / 2u + 1u;
    if (!all && view_same(&now, &before, sizeof now))
        return;
    if (!all) {
        FONT_RECT(0, VIEW_CELL_Y - 12u, 240, 6, VIEW_BLACK);
        for (cell = 0; cell < VIEW_CELLS; cell++)
            FONT_RECT(VIEW_CELL_X(cell) - 6u, VIEW_CELL_Y - 6u, 6, 18, VIEW_BLACK);
    }
    if (!now.on)
        return;
    FONT_RECT(from_x, VIEW_CELL_Y - 10u, 2, 22, VIEW_GREEN);
    FONT_RECT(to_x, VIEW_CELL_Y - 10u, 2, 22, VIEW_GREEN);
    FONT_RECT(from_x < to_x ? from_x : to_x, VIEW_CELL_Y - 10u, (from_x < to_x ? to_x - from_x : from_x - to_x) + 2u, 2, VIEW_GREEN);
    FONT_RECT(to_x - 1u, VIEW_CELL_Y + 9u, 4, 2, VIEW_GREEN);           /* the head at its input: the gap is 6 wide */
    FONT_RECT(to_x - 2u, VIEW_CELL_Y + 7u, 6, 2, VIEW_GREEN);
    if (from_x == to_x)                                                 /* to the next block: no line over the row */
        return;
    FONT_RECT(from_x < to_x ? middle - 2u : middle - 1u, VIEW_CELL_Y - 11u, 4, 4, VIEW_GREEN);
    FONT_RECT(from_x < to_x ? middle - 2u : middle + 1u, VIEW_CELL_Y - 12u, 2, 6, VIEW_GREEN);
}

/* the feedback's page, below the row. At the left the scope: the sound that comes down the chain to where the
 * cable goes back in (white) and the sound the cable brings (green), over 9.4 ms; the time is right when the two go
 * up and down together. At the right its four values, a row for each right encoder; with the LFO key held, the
 * LFO's cable to its level in place of the level */
static void view_scope_line(const int8_t *points, uint8_t ink)
{
    int32_t i, before = 31 - drone_limited(points[0], 28);
    for (i = 0; i < (int32_t)DRONE_SCOPE_POINTS; i++) {
        int32_t y = 31 - drone_limited(points[i], 28);
        view_canvas_rect(4 + i, y < before ? y : before, 1, (y < before ? before - y : y - before) + 2, ink);
        before = y;
    }
}
static void view_place_fb(const view_model_t *m, const view_model_t *shown, int all)
{
    char text[DRONE_FB_NPARAM][16], *to;
    uint32_t row, lfo = m->lfo_current < DRONE_LFOS ? m->lfo_current : 0u;
    uint32_t hundredths = (DRONE_FB_FRAMES(m->fb[DRONE_FB_TIME]) * 2267u + 500u) / 1000u;       /* of a millisecond: a frame is 22.67 us */
    view_fb_t fb = view_fb(m);
    if (all)
        font_draw_text(8, VIEW_PLACE_Y + 8u, 2, "FEEDBACK", VIEW_GREEN);
    if (all || !view_same(m->scope, shown->scope, sizeof m->scope)) {
        int32_t x;
        view_canvas_start(112, 62, 1, VIEW_INK_DIM);
        for (x = 4; x < 108; x += 4)
            view_canvas_rect(x, 31, 2, 1, VIEW_INK_DIM);
        view_scope_line(m->scope[0], VIEW_INK_WHITE);
        view_scope_line(m->scope[1], VIEW_INK_GREEN);
        view_canvas_send(8, VIEW_PLACE_Y + 30u);
    }
    if (!all && view_same(m->fb, shown->fb, sizeof m->fb) && view_same(m->fb_amount, shown->fb_amount, sizeof m->fb_amount)
        && m->touched == shown->touched && m->lfo_held == shown->lfo_held && m->lfo_current == shown->lfo_current)
        return;
    view_put_number(view_put(text[DRONE_FB_FROM], "OUTPUT "), fb.from + 1u);    /* of the block at that place in the chain, as the row shows them */
    view_put_number(view_put(text[DRONE_FB_TO], "INPUT "), fb.to + 1u);         /* of the block at that place */
    if (m->lfo_held) {
        to = view_put(text[DRONE_FB_LEVEL], "L");
        *to++ = (char)('1' + lfo);
        view_amount_text(to, "", m->fb_amount[lfo]);
    } else {
        view_put_number(view_put(text[DRONE_FB_LEVEL], "LVL "), m->fb[DRONE_FB_LEVEL] & 127u);
    }
    to = view_put(view_put_number(text[DRONE_FB_TIME], hundredths / 100u), ".");
    *to++ = (char)('0' + hundredths / 10u % 10u);
    *to++ = (char)('0' + hundredths % 10u);
    view_put(to, " MS");
    for (row = 0; row < DRONE_FB_NPARAM; row++)
        view_text(128, VIEW_PLACE_Y + 32u + row * 18u, 104, 2, text[row],
                  m->lfo_held && row == DRONE_FB_LEVEL ? VIEW_CYAN : row + PANEL_ENC_VALUES == m->touched ? VIEW_WHITE : VIEW_GREY, 0);
}

/* what is below the row; the first three are of the place where the cursor is */
enum { VIEW_SHOW_PLACE, VIEW_SHOW_CHOICE, VIEW_SHOW_AMOUNTS, VIEW_SHOW_LFO, VIEW_SHOW_LFO_TO, VIEW_SHOW_FB };
static uint32_t view_place_mode(const view_model_t *m, uint32_t at)
{
    if (m->choosing && at != PANEL_AT_OUT)
        return VIEW_SHOW_CHOICE;
    if (m->page == PANEL_FEEDBACK)
        return VIEW_SHOW_FB;
    if (panel_page_lfo(m->page) >= 0)
        return m->lfo_held ? VIEW_SHOW_LFO_TO : VIEW_SHOW_LFO;
    return m->lfo_held ? VIEW_SHOW_AMOUNTS : VIEW_SHOW_PLACE;
}

/* below the row: the place where the cursor is, or the open LFO. `all`: it is drawn on a cleared screen */
static void view_place(const view_model_t *m, const view_model_t *shown, int all)
{
    view_cell_t now, before;
    view_model_t plain = *m;
    uint32_t i, mode;
    int content_new, touched_new;
    plain.choosing = 0;                         /* the place as it is, not what is chosen for it */
    now = view_cell(&plain, m->place);
    plain = *shown;
    plain.choosing = 0;
    before = view_cell(&plain, shown->place);
    mode = view_place_mode(m, now.at);
    if (!all && (now.at != before.at || now.block.type != before.block.type || mode != view_place_mode(shown, before.at)
                 || (mode <= VIEW_SHOW_AMOUNTS && m->place != shown->place) || m->lfo_current != shown->lfo_current)) {
        FONT_RECT(0, VIEW_PLACE_Y + 2u, 240, VIEW_VOICES_Y - VIEW_PLACE_Y - 2u, VIEW_BLACK);
        all = 1;
    }
    content_new = all || !view_same(&now, &before, sizeof now);
    touched_new = all || m->touched != shown->touched;
    if (all)
        FONT_RECT(8, VIEW_PLACE_Y, 224, 1, VIEW_DIM);

    if (mode == VIEW_SHOW_CHOICE) {             /* the blocks to choose from, the chosen one amber */
        uint32_t x = 8;
        if (all) {
            font_draw_text(8, VIEW_PLACE_Y + 8u, 2, "PUT", VIEW_AMBER);
            font_draw_text(8, VIEW_PLACE_Y + 74u, 1, "LET GO OF OCT- TO KEEP", VIEW_GREY);
        }
        for (i = 0; i < DRONE_NBLOCK; i++) {
            const char *name = i != DRONE_BLOCK_NONE ? view_block_name[i] : now.at == PANEL_AT_BLOCK ? "DEL" : "NONE";
            uint32_t w = font_text_width(name, 2);
            if (all || m->choice != shown->choice) {
                font_draw_text(x, VIEW_PLACE_Y + 40u, 2, name, i == m->choice ? VIEW_AMBER : VIEW_GREY);
                FONT_RECT(x, VIEW_PLACE_Y + 58u, w, 3, i == m->choice ? VIEW_AMBER : VIEW_BLACK);
            }
            x += w + 14u;
        }
        return;
    }
    if (mode == VIEW_SHOW_LFO) {
        view_place_lfo(m, shown, all);
        return;
    }
    if (mode == VIEW_SHOW_LFO_TO) {
        view_place_lfo_to(m, shown, all);
        return;
    }
    if (mode == VIEW_SHOW_FB) {
        view_place_fb(m, shown, all);
        return;
    }
    if (now.at == PANEL_AT_NEW) {
        if (all) {
            font_draw_text(8, VIEW_PLACE_Y + 8u, 2, "EMPTY", VIEW_AMBER);
            font_draw_text(8, VIEW_PLACE_Y + 40u, 1, "HOLD OCT- AND TURN SELECT", VIEW_WHITE);
            font_draw_text(8, VIEW_PLACE_Y + 54u, 1, "TO PUT A BLOCK HERE", VIEW_WHITE);
        }
        return;
    }
    if (all)
        font_draw_text(8, VIEW_PLACE_Y + 8u, 2, view_cell_name(&now), VIEW_AMBER);
    if (mode == VIEW_SHOW_AMOUNTS && all) {     /* whose cables: the LFO's name where the kind is */
        char name[8];
        view_lfo_name(name, m->lfo_current);
        view_text(60, VIEW_PLACE_Y + 8u, 172, 2, name, VIEW_CYAN, 1);
    }
    if (mode == VIEW_SHOW_PLACE && (content_new || touched_new))
        view_text(60, VIEW_PLACE_Y + 8u, 172, 2, view_kind_name(&now), m->touched == PANEL_ENC_KIND ? VIEW_WHITE : VIEW_GREY, 1);
    if (content_new) {
        view_canvas_start(112, 62, 1, VIEW_INK_DIM);
        view_picture(&now, 10, 10, 92, 42, VIEW_INK_WHITE);
        view_canvas_send(8, VIEW_PLACE_Y + 30u);
    }
    if (mode == VIEW_SHOW_AMOUNTS && content_new)
        view_place_amounts(m, &now);
    if (mode == VIEW_SHOW_PLACE && (content_new || touched_new))
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
    view_fb_cable(m, shown, all);
    if (page_new && !all)
        FONT_RECT(0, VIEW_PLACE_Y, 240, VIEW_VOICES_Y - VIEW_PLACE_Y, VIEW_BLACK);
    view_place(m, shown, page_new);
}

/* ---- the end of the chain ---- */
enum { VIEW_EOC_DELAY, VIEW_EOC_REVERB, VIEW_EOC_COMP, VIEW_EOC_OUT, VIEW_EOC_CELLS };
#define VIEW_EOC_X(cell) (48u + (cell) * 46u)
static const char *const view_eoc_name[VIEW_EOC_CELLS] = { "DLY", "REV", "COMP", "OUT" };

static uint32_t view_delay_ms(uint32_t time)
{
    return (drone_delay_length(time) * 1000u + DRONE_DELAY_FRAMES / 2u) / DRONE_DELAY_FRAMES;
}
static uint32_t view_threshold_db(uint32_t threshold)   /* below a full sample: 3/8 dB a step */
{
    return (127u - (threshold & 127u)) * 3u / 8u;
}
static void view_ratio_text(char *to, uint32_t ratio)   /* "2.0 TO 1": of what is above the threshold, a half is left */
{
    uint32_t tenths = (ratio & 127u) < 127u ? 1270u / (127u - (ratio & 127u)) : 0u;
    if (!tenths) {
        view_put(to, "LIMIT");
        return;
    }
    to = view_put_number(to, tenths / 10u);
    if (tenths < 100u) {
        *to++ = '.';
        to = view_put_number(to, tenths % 10u);
    }
    view_put(to, " TO 1");
}

/* the delay: the sound that goes in, then its echoes: further apart with a longer time, as many as the repeats leave */
static void view_echoes(int32_t x, int32_t y, int32_t w, int32_t h, int32_t step, int32_t repeats, uint8_t ink)
{
    int32_t at, tall = h;
    for (at = x; at + 3 <= x + w && tall >= 2; at += step) {
        view_canvas_rect(at, y + h - tall, 3, tall, at == x ? VIEW_INK_WHITE : ink);
        tall = tall * repeats / 128;
        tall = tall > h ? h : tall;
    }
}
/* the compressor: what comes out (up) for what goes in (to the right). Straight up to the threshold, flatter above it
 * by the ratio, all of it higher by the gain, and flat at the top: a full sample */
static void view_knee(int32_t x, int32_t y, int32_t w, int32_t h, const uint8_t *comp, uint8_t ink, int guides)
{
    int32_t right = x + w - 2, bottom = y + h - 2, lift = (h - 2) * (comp[DRONE_COMP_GAIN] & 127) / 254;
    int32_t knee_x = x + (w - 2) * (comp[DRONE_COMP_THRESHOLD] & 127) / 127, knee_y = bottom - (knee_x - x) * (h - 2) / (w - 2);
    int32_t end_y = knee_y - (right - knee_x) * (h - 2) / (w - 2) * (127 - (comp[DRONE_COMP_RATIO] & 127)) / 127;
    int32_t from_x[2] = { x, knee_x }, from_y[2] = { bottom - lift, knee_y - lift }, to_x[2] = { knee_x, right }, to_y[2] = { knee_y - lift, end_y - lift };
    int32_t part;
    if (guides)
        for (part = x; part < x + w; part += 4)     /* the threshold */
            view_canvas_rect(part, knee_y, 2, 1, VIEW_INK_DIM);
    for (part = 0; part < 2; part++) {
        if (from_y[part] <= y) {                    /* all of it at the top */
            view_canvas_line(from_x[part], y, to_x[part], y, ink);
        } else if (to_y[part] < y) {                /* it gets to the top on its way */
            int32_t top_x = from_x[part] + (to_x[part] - from_x[part]) * (from_y[part] - y) / (from_y[part] - to_y[part]);
            view_canvas_line(from_x[part], from_y[part], top_x, y, ink);
            view_canvas_line(top_x, y, to_x[part], y, ink);
        } else {
            view_canvas_line(from_x[part], from_y[part], to_x[part], to_y[part], ink);
        }
    }
}
/* an effect's picture: in the row with values that show what it is, below the row with its own */
static void view_eoc_picture(const view_model_t *m, uint32_t cell, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t ink, int large)
{
    static const uint8_t comp_in_row[DRONE_COMP_NPARAM] = { [DRONE_COMP_THRESHOLD] = 64, [DRONE_COMP_RATIO] = 100 };
    view_cell_t as = { 0 };
    if (cell == VIEW_EOC_DELAY) {
        view_echoes(x, y, w, h, large ? 5 + (int32_t)drone_delay_length(m->delay[DRONE_DELAY_TIME]) * (w / 2) / 43773 : 5,
                    large ? (m->delay[DRONE_DELAY_REPEATS] & 127) + ((m->delay[DRONE_DELAY_REPEATS] & 127) >> 5) : 90,
                    !large || (m->delay[DRONE_DELAY_TONE] & 127) >= 64 ? ink : VIEW_INK_GREY);
    } else if (cell == VIEW_EOC_COMP) {
        view_knee(x, y, w, h, large ? m->comp : comp_in_row, ink, large);
    } else {                                    /* the reverb's tail, as an FX block's; OUT as a voice's */
        as.at = cell == VIEW_EOC_REVERB ? PANEL_AT_BLOCK : PANEL_AT_OUT;
        as.block.type = DRONE_BLOCK_FX;
        as.block.param[DRONE_FX_DECAY] = large ? m->reverb[DRONE_REVERB_DECAY] : 64;
        as.level = 100;
        as.fade = 60;
        view_picture(&as, x, y, w, h, ink);
    }
}

static void view_eoc(const view_model_t *m, const view_model_t *shown, int all, int page_new)
{
    uint32_t cell, at = m->page - PANEL_DELAY, row, in_patch = m->reverb_holder < DRONE_VOICES;
    char text[4][16];
    if (all) {                                  /* the voices' sends into the delay; the voices themselves over the row, down before COMP */
        font_draw_text(8, VIEW_CELL_Y + 3u, 1, "SEND", VIEW_GREY);
        font_draw_text(8, VIEW_CELL_Y + 13u, 1, "1 2 3", VIEW_DIM);
        FONT_RECT(38, VIEW_CELL_Y + 12u, 10, 2, VIEW_GREY);
        font_draw_text(8, VIEW_CELL_Y - 11u, 1, "DRY", VIEW_GREY);
        FONT_RECT(28, VIEW_CELL_Y - 8u, VIEW_EOC_X(VIEW_EOC_COMP) - 9u - 28u, 2, VIEW_GREY);
        FONT_RECT(VIEW_EOC_X(VIEW_EOC_COMP) - 10u, VIEW_CELL_Y - 8u, 2, 20, VIEW_GREY);
        FONT_RECT(8, VIEW_PLACE_Y, 224, 1, VIEW_DIM);
    }
    if (page_new || m->reverb_holder != shown->reverb_holder)
        for (cell = 0; cell < VIEW_EOC_CELLS; cell++) {
            uint32_t x = VIEW_EOC_X(cell);
            if (cell)
                FONT_RECT(x - 18u, VIEW_CELL_Y + 12u, 18, 2, VIEW_GREY);
            view_canvas_start(VIEW_CELL_W, VIEW_CELL_H, cell == at ? 2 : 1, cell == at ? VIEW_INK_AMBER : VIEW_INK_DIM);
            view_eoc_picture(m, cell, 5, 5, VIEW_CELL_W - 10, VIEW_CELL_H - 10, cell == VIEW_EOC_REVERB && in_patch ? VIEW_INK_DIM : VIEW_INK_WHITE, 0);
            view_canvas_send(x, VIEW_CELL_Y);
            FONT_RECT(x, VIEW_CELL_Y + VIEW_CELL_H + 3u, VIEW_CELL_W, 7, VIEW_BLACK);
            font_draw_text(x + (VIEW_CELL_W - font_text_width(view_eoc_name[cell], 1)) / 2u, VIEW_CELL_Y + VIEW_CELL_H + 3u, 1, view_eoc_name[cell],
                           cell == at ? VIEW_AMBER : VIEW_GREY);
        }
    if (!page_new && view_same(m->delay, shown->delay, sizeof m->delay) && view_same(m->comp, shown->comp, sizeof m->comp)
        && view_same(m->reverb, shown->reverb, sizeof m->reverb) && m->send == shown->send && m->reverb_holder == shown->reverb_holder
        && m->touched == shown->touched)
        return;
    if (page_new) {
        FONT_RECT(0, VIEW_PLACE_Y + 2u, 240, VIEW_VOICES_Y - VIEW_PLACE_Y - 2u, VIEW_BLACK);
        font_draw_text(8, VIEW_PLACE_Y + 8u, 2, panel_page_name[m->page], VIEW_AMBER);
    }
    if (at == VIEW_EOC_REVERB) {                /* a patch's chain has the reverb: this one is silent, the send goes into the delay only */
        char name[8] = "IN OP1";
        name[5] = (char)('1' + m->reverb_holder);
        view_text(128, VIEW_PLACE_Y + 8u, 104, 2, in_patch ? name : "", VIEW_AMBER, 1);
    }
    view_canvas_start(112, 62, 1, VIEW_INK_DIM);
    view_eoc_picture(m, at, 8, 8, 96, 46, at == VIEW_EOC_REVERB && in_patch ? VIEW_INK_DIM : VIEW_INK_WHITE, 1);
    view_canvas_send(8, VIEW_PLACE_Y + 30u);
    if (at == VIEW_EOC_DELAY) {
        view_put(view_put_number(text[DRONE_DELAY_TIME], view_delay_ms(m->delay[DRONE_DELAY_TIME])), " MS");
        view_put_number(view_put(text[DRONE_DELAY_REPEATS], "RPT "), m->delay[DRONE_DELAY_REPEATS] & 127u);
        view_put_number(view_put(text[DRONE_DELAY_TONE], "TONE "), m->delay[DRONE_DELAY_TONE] & 127u);
        view_put_number(view_put(text[DRONE_DELAY_LEVEL], "LVL "), m->delay[DRONE_DELAY_LEVEL] & 127u);
    } else if (at == VIEW_EOC_REVERB) {         /* the selected voice's send, then the reverb's own */
        text[0][0] = 0;                         /* KNOB1 has none here: a voice's send is the mixer's */
        view_time_text(text[1u + DRONE_REVERB_DECAY], view_decay_ms[m->reverb[DRONE_REVERB_DECAY] & 127u]);
        view_put_number(view_put(text[1u + DRONE_REVERB_TONE], "TONE "), m->reverb[DRONE_REVERB_TONE] & 127u);
        view_put_number(view_put(text[1u + DRONE_REVERB_RETURN], "RET "), m->reverb[DRONE_REVERB_RETURN] & 127u);
    } else {
        uint32_t below = view_threshold_db(m->comp[DRONE_COMP_THRESHOLD]);
        view_put_number(view_put(text[DRONE_COMP_THRESHOLD], below ? "THR -" : "THR "), below);
        view_ratio_text(text[DRONE_COMP_RATIO], m->comp[DRONE_COMP_RATIO]);
        view_time_text(text[DRONE_COMP_RELEASE], drone_release_ms[m->comp[DRONE_COMP_RELEASE] & 127u]);
        view_put_number(view_put(text[DRONE_COMP_GAIN], "GAIN "), (m->comp[DRONE_COMP_GAIN] & 127u) * 24u / 127u);
    }
    for (row = 0; row < 4u; row++)
        view_text(128, VIEW_PLACE_Y + 32u + row * 18u, 104, 2, text[row],
                  at == VIEW_EOC_REVERB && in_patch && row ? VIEW_DIM : row + PANEL_ENC_VALUES == m->touched ? VIEW_WHITE : VIEW_GREY, 0);
}

/* ---- the play page ----
 * The selected voice's sound as it is now, over 9.4 ms; below it a dial for each right encoder with its knob's name,
 * and the name and the value of the knob turned last. With the shift key down: the knobs' second set. Over the
 * voices' boxes, the links: an arrow into each voice from the voice before it (into the first one from the last, back
 * over the others), thicker as its LINK is more open, amber for the selected voice's; in dashes when the link is
 * FOLLOW (it carries a loudness), whole when it is THROUGH (it carries the sound), in dots when it is FM (it bends the
 * pitch). The selected voice's kind of link has its name at the right of the knob's value. */
#define VIEW_LINKS_Y 190u               /* the links' band: 22 high, down to the voices' boxes */
#define VIEW_DIAL_X(n) (36u + (n) * 56u)
#define VIEW_DIAL_Y 128u
#define VIEW_DIAL_DOTS 16u
static const char *const view_knob_name[PLAY_NKNOB] = {
    [PLAY_TONE] = "TONE", [PLAY_MOVE] = "MOVE", [PLAY_GRIT] = "GRIT", [PLAY_LINK] = "LINK", [PLAY_SPEED] = "SPEED", [PLAY_OWN] = "OWN",
};

static void view_freq_text(char *to, uint32_t freq)     /* "92.5 HZ"; from 1 kHz: "1047 HZ" */
{
    uint32_t hz10 = style_freq_hz10(freq);
    if (hz10 < 10000u) {
        to = view_put_number(to, hz10 / 10u);
        *to++ = '.';
        to = view_put_number(to, hz10 % 10u);
    } else {
        to = view_put_number(to, (hz10 + 5u) / 10u);
    }
    view_put(to, " HZ");
}

/* a frequency that is on a note: the note's name, "A4" (pitch 69, 440 Hz); 0 when it is between two notes */
static int view_freq_note_text(char *to, uint32_t freq)
{
    if (freq % 128u)
        return 0;
    view_note_text(to, freq / 128u);
    return 1;
}

/* a dial: dots round three quarters of a circle, from the bottom left over the top; the first `lit` are amber */
static void view_dial_dot(uint32_t x, uint32_t k, uint16_t colour)
{
    static const int8_t dot[VIEW_DIAL_DOTS][2] = {
        { -9, 9 }, { -12, 6 }, { -13, 2 }, { -13, -2 }, { -12, -6 }, { -9, -9 }, { -6, -12 }, { -2, -13 },
        { 2, -13 }, { 6, -12 }, { 9, -9 }, { 12, -6 }, { 13, -2 }, { 13, 2 }, { 12, 6 }, { 9, 9 },
    };
    k = k < VIEW_DIAL_DOTS ? k : VIEW_DIAL_DOTS - 1u;
    FONT_RECT((uint32_t)((int32_t)x + dot[k][0] - 1), (uint32_t)((int32_t)VIEW_DIAL_Y + dot[k][1] - 1), 3, 3, colour);
}
static void view_dial(uint32_t x, uint32_t lit)
{
    uint32_t k;
    for (k = 0; k < VIEW_DIAL_DOTS; k++)
        view_dial_dot(x, k, k < lit ? VIEW_AMBER : VIEW_FAINT);
}
/* an amount on a dial, in cyan from its centre: to the right for more, to the left for the LFO upside down */
static void view_amount_dial(uint32_t x, int32_t amount)
{
    uint32_t k, lit = ((uint32_t)(amount < 0 ? -amount : amount) * (VIEW_DIAL_DOTS / 2u) + 99u) / 100u;
    for (k = 0; k < VIEW_DIAL_DOTS; k++) {
        int on = amount > 0 ? k >= VIEW_DIAL_DOTS / 2u && k < VIEW_DIAL_DOTS / 2u + lit : amount < 0 && k < VIEW_DIAL_DOTS / 2u && k + lit >= VIEW_DIAL_DOTS / 2u;
        view_dial_dot(x, k, on ? VIEW_CYAN : VIEW_FAINT);
    }
}

/* left to right; a kind's line: FOLLOW 6 of every 9, FM 2 of every 4, THROUGH all of it */
static void view_link_line(uint32_t x, uint32_t y, uint32_t w, uint32_t thick, uint16_t colour, uint32_t kind)
{
    uint32_t every = kind == DRONE_LINK_THROUGH ? w : kind == DRONE_LINK_FM ? 4u : 9u;
    uint32_t lit = kind == DRONE_LINK_THROUGH ? w : kind == DRONE_LINK_FM ? 2u : 6u, at;
    for (at = 0; at < w; at += every)
        FONT_RECT(x + at, y, w - at < lit ? w - at : lit, thick, colour);
}

static void view_links(const view_model_t *m)
{
    uint32_t v;
    FONT_RECT(0, VIEW_LINKS_Y, 240, VIEW_VOICES_Y - VIEW_LINKS_Y, VIEW_BLACK);
    for (v = 0; v < DRONE_VOICES; v++) {
        uint32_t thick = 1u + (m->link[v] & 127u) / 32u, to = 8u + v * VIEW_VOICE_STEP + VIEW_VOICE_W / 2u;      /* the middle of its box */
        uint16_t colour = v == m->selected ? VIEW_AMBER : VIEW_GREY;
        uint32_t kind = m->link_kind[v] < DRONE_NLINK ? m->link_kind[v] : DRONE_LINK_FOLLOW;
        if (!(m->link[v] & 127u))
            continue;
        if (v) {                                /* from the box at its left: a line, and a head that points right */
            uint32_t tip = to - 10u, y = VIEW_LINKS_Y + 13u;
            view_link_line(to - VIEW_VOICE_STEP + 16u, y - thick / 2u, VIEW_VOICE_STEP - 32u, thick, colour, kind);
            FONT_RECT(tip - 6u, y - 4u, 2, 9, colour);
            FONT_RECT(tip - 4u, y - 2u, 2, 5, colour);
            FONT_RECT(tip - 2u, y, 2, 1, colour);
        } else {                                /* from the last box: up, back over the others, and down on the first one */
            uint32_t from = 8u + (DRONE_VOICES - 1u) * VIEW_VOICE_STEP + VIEW_VOICE_W / 2u, y = VIEW_LINKS_Y + 2u;
            view_link_line(to, y, from - to + thick, thick, colour, kind);
            FONT_RECT(from, y, thick, VIEW_VOICES_Y - 3u - y, colour);
            FONT_RECT(to, y, thick, 12, colour);
            FONT_RECT(to - 4u, y + 12u, 9, 2, colour);
            FONT_RECT(to - 2u, y + 14u, 5, 2, colour);
            FONT_RECT(to, y + 16u, 1, 2, colour);
        }
    }
}

/* ---- the bank (V0.32) ----
 * Its page: the shown LFO's number at the top, FREE or SYNC and the clock's tempo at the right, with a square that is
 * lit in the first quarter of each beat. Its wave: this cycle and the next one, as the engine plays them (the shape
 * with what the cables add, the random heights, the slew, the range and the depth), a dim line between the two, the
 * amber mark where the LFO is now; a synced LFO slower than a beat has a tick for each beat. Then the value turned
 * last, large, four dials, and the row of the twelve LFOs: each one's bar follows its output, the shown one has the
 * amber frame, a brighter number when a voice has an amount of it, a short line under a synced one. */
#define VIEW_BANK_POINTS 112            /* a cycle of the picture: two fill the canvas */
#define VIEW_BANK_Y 28u                 /* the picture: 64 high */
#define VIEW_BANK_MID 32                /* its middle, in the canvas */
#define VIEW_BANK_REACH 27              /* a full output, up or down from the middle */
#define VIEW_BANK_VALUE_Y 98u
#define VIEW_BANK_ROW_Y 176u            /* the row of the twelve LFOs: the play page has it in the links' band */
#define VIEW_ROW_H 21u                  /* a pixel less than that band: its frame does not touch the voices' boxes */
#define VIEW_ROW_BAR 3                  /* a bar's most, up or down from its middle: it stays clear of the number */
#define VIEW_ROW_STEP 19u
enum { VIEW_FRAME_NONE, VIEW_FRAME_AMBER, VIEW_FRAME_CYAN };
static const char *const view_bank_shape_name[DRONE_BANK_NSHAPE] = {
    "GATE", "RATCHET 2", "RATCHET 4", "SOFT GATE", "TRIANGLE", "TRAPEZOID", "SINE", "HUMP", "EXP", "SOFT EXP", "LOG", "SOFT LOG",
    "RANDOM", "SOFT RANDOM",
};

static uint32_t view_bank_lfo(const view_model_t *m)
{
    return m->bank_lfo < DRONE_BANK_LFOS ? m->bank_lfo : DRONE_BANK_LFOS - 1u;
}
/* a bank LFO's value as the engine takes it: the PHASE goes round, the others stop at their top */
static uint32_t view_bank_knob(const view_model_t *m, uint32_t lfo, uint32_t param)
{
    uint32_t value = m->bank[lfo % DRONE_BANK_LFOS][param];
    if (param == DRONE_BANK_PHASE)
        return value & 127u;
    return value > drone_bank_spec[param].top ? drone_bank_spec[param].top : value;
}
static uint32_t view_tempo(const view_model_t *m)
{
    return m->tempo < DRONE_TEMPO_LOW ? DRONE_TEMPO_LOW : m->tempo > DRONE_TEMPO_HIGH ? DRONE_TEMPO_HIGH : m->tempo;
}

/* a free LFO's cycle at that RATE, in frames (44118 a second): from the engine's step a frame */
static uint32_t view_bank_cycle_frames(uint32_t rate)
{
    uint32_t step = drone_step_q8((int32_t)(rate > 127u ? 127u : rate) * 256) >> 9;
    return 0xFFFFFFFFu / (step ? step : 1u);
}
/* "6.2 S" a cycle from a second; below it, "24.4 HZ": cycles a second. One decimal */
static void view_bank_rate_text(char *to, uint32_t rate)
{
    uint32_t frames = view_bank_cycle_frames(rate), tenths = (frames * 10u + 22059u) / 44118u;
    int seconds = tenths >= 10u;
    if (!seconds)
        tenths = (441180u + frames / 2u) / frames;
    to = view_put_number(to, tenths / 10u);
    *to++ = '.';
    view_put(view_put_number(to, tenths % 10u), seconds ? " S" : " HZ");
}
static void view_bank_ratio_text(char *to, uint32_t ratio)     /* a synced LFO's: "32 BEATS" .. "1 BEAT", then "X2" .. "X16" */
{
    uint32_t per, times;
    ratio = ratio < DRONE_BANK_RATIOS ? ratio : DRONE_BANK_RATIOS - 1u;
    per = drone_bank_beats[ratio];
    times = drone_bank_times[ratio];
    if (times > 1u)
        view_put_number(view_put(to, "X"), times);
    else
        view_put(view_put_number(to, per), per > 1u ? " BEATS" : " BEAT");
}
/* the value line: the name and the value of what was turned last */
static void view_bank_value_text(char *to, const view_model_t *m)
{
    uint32_t lfo = view_bank_lfo(m);
    *to = 0;
    switch (m->touched) {
    case DRONE_BANK_RATE:
    case DRONE_BANK_RATIO:                      /* the same knob: what it is now */
        to = view_put(to, "RATE ");
        if (view_bank_knob(m, lfo, DRONE_BANK_SYNC))
            view_bank_ratio_text(to, view_bank_knob(m, lfo, DRONE_BANK_RATIO));
        else
            view_bank_rate_text(to, view_bank_knob(m, lfo, DRONE_BANK_RATE));
        break;
    case DRONE_BANK_SHAPE:
        view_put(view_put(to, "SHAPE "), view_bank_shape_name[view_bank_knob(m, lfo, DRONE_BANK_SHAPE)]);
        break;
    case DRONE_BANK_WIDTH:
        view_put(view_put_number(view_put(to, "WIDTH "), view_bank_knob(m, lfo, DRONE_BANK_WIDTH)), "%");
        break;
    case DRONE_BANK_LEVEL:
        view_put(view_put_number(view_put(to, "LEVEL "), view_bank_knob(m, lfo, DRONE_BANK_LEVEL)), "%");
        break;
    case DRONE_BANK_PHASE:                      /* in degrees: PHASE / 128 of a cycle */
        view_put_number(view_put(to, "PHASE "), view_bank_knob(m, lfo, DRONE_BANK_PHASE) * 45u / 16u);
        break;
    case DRONE_BANK_SLEW:
        view_put_number(view_put(to, "SLEW "), view_bank_knob(m, lfo, DRONE_BANK_SLEW));
        break;
    case DRONE_BANK_RANGE:
        view_put(view_put(to, "RANGE "), view_bank_knob(m, lfo, DRONE_BANK_RANGE) ? "UP" : "BOTH");
        break;
    case DRONE_BANK_SYNC:
        view_put(view_put(to, "SYNC "), view_bank_knob(m, lfo, DRONE_BANK_SYNC) ? "ON" : "OFF");
        break;
    case PANEL_BANK_TEMPO:
        view_put(view_put_number(view_put(to, "TEMPO "), view_tempo(m)), " BPM");
        break;
    default:
        break;
    }
}

/* the top line: the LFO's number, FREE or SYNC, the tempo; then the beat's square, at the left of the tempo */
static void view_bank_top(const view_model_t *m)
{
    char text[12];
    int sync = view_bank_knob(m, view_bank_lfo(m), DRONE_BANK_SYNC) != 0;
    view_put_number(view_put(text, "LFO "), view_bank_lfo(m) + 1u);
    font_draw_text(8, 6, 2, text, VIEW_AMBER);
    font_draw_text(232u - font_text_width(sync ? "SYNC" : "FREE", 1), 5, 1, sync ? "SYNC" : "FREE", sync ? VIEW_WHITE : VIEW_GREY);
    view_put(view_put_number(text, view_tempo(m)), " BPM");
    font_draw_text(232u - font_text_width(text, 1), 14, 1, text, VIEW_GREY);
}
static int view_bank_beat_lit(const view_model_t *m)
{
    return m->clock_phase < 0x40000000u;
}
static void view_bank_beat(const view_model_t *m)
{
    char text[12];
    view_put(view_put_number(text, view_tempo(m)), " BPM");
    FONT_RECT(232u - font_text_width(text, 1) - 10u, 15, 5, 5, view_bank_beat_lit(m) ? VIEW_WHITE : VIEW_BLACK);
}
static int view_bank_top_new(const view_model_t *m, const view_model_t *shown)
{
    return view_bank_lfo(m) != view_bank_lfo(shown) || view_tempo(m) != view_tempo(shown)
        || view_bank_knob(m, view_bank_lfo(m), DRONE_BANK_SYNC) != view_bank_knob(shown, view_bank_lfo(shown), DRONE_BANK_SYNC);
}

/* all that the picture's wave depends on (not where the LFO is): bytes only, so that two compare byte by byte */
typedef struct {
    uint8_t shape, width, slew, range, level, offset, sync, ratio, phase;
} view_bank_wave_t;
_Static_assert(sizeof(view_bank_wave_t) == 9u, "no gap in it: two compare byte by byte");
static view_bank_wave_t view_bank_wave_of(const view_model_t *m)
{
    uint32_t lfo = view_bank_lfo(m);
    view_bank_wave_t w;
    w.shape = (uint8_t)view_bank_knob(m, lfo, DRONE_BANK_SHAPE);
    w.width = (uint8_t)(m->bank_width > 100u ? 100u : m->bank_width);
    w.slew = (uint8_t)view_bank_knob(m, lfo, DRONE_BANK_SLEW);
    w.range = (uint8_t)view_bank_knob(m, lfo, DRONE_BANK_RANGE);
    w.level = (uint8_t)view_bank_knob(m, lfo, DRONE_BANK_LEVEL);
    w.offset = (uint8_t)view_bank_knob(m, lfo, DRONE_BANK_OFFSET);
    w.sync = (uint8_t)view_bank_knob(m, lfo, DRONE_BANK_SYNC);
    w.ratio = (uint8_t)(w.sync ? view_bank_knob(m, lfo, DRONE_BANK_RATIO) : 0u);     /* a free LFO's beats are not drawn */
    w.phase = (uint8_t)(w.sync ? view_bank_knob(m, lfo, DRONE_BANK_PHASE) : 0u);
    return w;
}
/* the shape's height, after the range: every step as one that plays, with no random heights */
static int32_t view_bank_height(const view_bank_wave_t *w, uint32_t phase)
{
    int32_t height = drone_bank_shape(w->shape, phase, w->width, 0, 0);
    return w->range ? height : 2 * height - 32767;
}
/* a height after LEVEL and OFFSET, as drone_bank_lfo makes its output */
static int32_t view_bank_out(const view_bank_wave_t *w, int32_t height)
{
    uint8_t value[DRONE_BANK_NPARAM] = { 0 };
    value[DRONE_BANK_LEVEL] = w->level;
    value[DRONE_BANK_OFFSET] = w->offset;
    return drone_bank_out(height, value);
}
static int32_t view_bank_y(int32_t out)        /* in the canvas */
{
    return VIEW_BANK_MID - drone_limited(out, 32767) * VIEW_BANK_REACH / 32767;
}
typedef struct { uint8_t x, y; } view_bank_mark_t;     /* where the LFO is now: in its cycle, and its output */
static view_bank_mark_t view_bank_mark(const view_model_t *m)
{
    view_bank_mark_t mark = { (uint8_t)(((m->bank_phase >> 16) * VIEW_BANK_POINTS) >> 16), (uint8_t)view_bank_y(m->bank_out[view_bank_lfo(m)]) };
    return mark;
}

/* the picture. Only the mark moved: only its columns are sent, where it was and where it is */
static void view_bank_picture(const view_model_t *m, const view_model_t *shown, int all)
{
    view_bank_wave_t now = view_bank_wave_of(m), before = view_bank_wave_of(shown);
    view_bank_mark_t mark = view_bank_mark(m), was = view_bank_mark(shown);
    int wave_new = all || !view_same(&now, &before, sizeof now);
    int32_t i, height, y, before_y = VIEW_BANK_MID, most = drone_bank_slew_limit(0xFFFFFFFFu / VIEW_BANK_POINTS, now.slew);
    if (!wave_new && mark.x == was.x && mark.y == was.y)
        return;
    view_canvas_start(2 * VIEW_BANK_POINTS, 64, 0, VIEW_INK_BLACK);
    for (i = 0; i < 2 * VIEW_BANK_POINTS; i += 8)
        view_canvas_rect(i, VIEW_BANK_MID, 4, 1, VIEW_INK_DIM);
    view_canvas_rect(VIEW_BANK_POINTS, 0, 1, 64, VIEW_INK_DIM);
    if (now.sync && drone_bank_beats[now.ratio] > 1u) {        /* a tick for each beat; PHASE moves the cycle against them */
        int32_t per = drone_bank_beats[now.ratio], offset = now.phase * VIEW_BANK_POINTS / 128, k;
        for (k = -per; k < 2 * per; k++)
            view_canvas_rect(offset + k * VIEW_BANK_POINTS / per, 61, 1, 3, VIEW_INK_GREY);
    }
    /* the cycle before first, so that the slew comes into this one from where it was */
    height = view_bank_height(&now, 0);
    for (i = -VIEW_BANK_POINTS; i < 2 * VIEW_BANK_POINTS; i++) {
        uint32_t phase = (uint32_t)((i + VIEW_BANK_POINTS) % VIEW_BANK_POINTS) * (0xFFFFFFFFu / VIEW_BANK_POINTS);
        height += drone_limited(view_bank_height(&now, phase) - height, most);
        y = view_bank_y(view_bank_out(&now, height));
        if (i >= 0)
            view_canvas_rect(i, y < before_y ? y : before_y, 1, (y < before_y ? before_y - y : y - before_y) + 2, VIEW_INK_WHITE);
        before_y = y;
    }
    view_canvas_rect(mark.x, 0, 1, 64, VIEW_INK_AMBER);
    view_canvas_rect(mark.x - 2, mark.y - 2, 5, 5, VIEW_INK_AMBER);
    if (wave_new) {
        view_canvas_send(8, VIEW_BANK_Y);
        return;
    }
    view_canvas_send_part(8, VIEW_BANK_Y, was.x - 2, 5);
    view_canvas_send_part(8, VIEW_BANK_Y, mark.x - 2, 5);
}

static void view_bank_value_line(const view_model_t *m)
{
    char text[24];
    const char *wave = view_bank_shape_name[view_bank_knob(m, view_bank_lfo(m), DRONE_BANK_SHAPE)];
    view_bank_value_text(text, m);
    view_text(8, VIEW_BANK_VALUE_Y, 224, 2, text, VIEW_WHITE, 0);
    if (m->touched != DRONE_BANK_SHAPE && font_text_width(text, 2) + 12u + font_text_width(wave, 2) <= 224u)  /* there is room */
        font_draw_text(232u - font_text_width(wave, 2), VIEW_BANK_VALUE_Y, 2, wave, VIEW_GREY);
}

typedef struct { uint8_t lit, bright, param; } view_bank_dial_t;     /* what a dial shows: bytes only */
static view_bank_dial_t view_bank_dial(const view_model_t *m, uint32_t n)
{
    uint32_t lfo = view_bank_lfo(m), param = (uint32_t)panel_bank_param(PANEL_ENC_VALUES + n, m->shift, view_bank_knob(m, lfo, DRONE_BANK_SYNC) != 0);
    uint32_t top = drone_bank_spec[param].top;
    view_bank_dial_t d;
    d.lit = (uint8_t)((view_bank_knob(m, lfo, param) * VIEW_DIAL_DOTS + top / 2u) / top);
    d.bright = m->touched == param || (!n && (m->touched == DRONE_BANK_RATE || m->touched == DRONE_BANK_RATIO));
    d.param = (uint8_t)param;
    return d;
}
static void view_bank_dials(const view_model_t *m, const view_model_t *shown, int all)
{
    uint32_t n;
    for (n = 0; n < 4u; n++) {
        view_bank_dial_t now = view_bank_dial(m, n), before = view_bank_dial(shown, n);
        const char *name = now.param == DRONE_BANK_WIDTH ? "WIDTH" : now.param == DRONE_BANK_PHASE ? "PHASE"
                         : now.param == DRONE_BANK_SLEW ? "SLEW" : now.param == DRONE_BANK_LEVEL ? "LEVEL" : "RATE";
        if (!all && view_same(&now, &before, sizeof now))
            continue;
        view_dial(VIEW_DIAL_X(n), now.lit);
        FONT_RECT(VIEW_DIAL_X(n) - 26u, VIEW_DIAL_Y + 22u, 52, 7, VIEW_BLACK);
        font_draw_text(VIEW_DIAL_X(n) - font_text_width(name, 1) / 2u, VIEW_DIAL_Y + 22u, 1, name, now.bright ? VIEW_WHITE : VIEW_GREY);
    }
}

/* the row of the twelve LFOs, at `y`; `frame`: the one the frame is on (bank_lfo) has it in amber or in cyan */
typedef struct { uint8_t frame, used, sync; int8_t bar; } view_row_cell_t;
static view_row_cell_t view_row_cell(const view_model_t *m, uint32_t n, uint8_t frame)
{
    view_row_cell_t c;
    c.frame = n == view_bank_lfo(m) ? frame : (uint8_t)VIEW_FRAME_NONE;
    c.used = (uint8_t)(m->bank_used >> n & 1u);
    c.sync = m->bank[n][DRONE_BANK_SYNC] != 0;
    c.bar = (int8_t)(drone_limited(m->bank_out[n], 32767) * VIEW_ROW_BAR / 32767);
    return c;
}
static void view_bank_row(const view_model_t *m, const view_model_t *shown, uint32_t y, uint8_t frame, int all)
{
    uint32_t n;
    for (n = 0; n < DRONE_BANK_LFOS; n++) {
        view_row_cell_t now = view_row_cell(m, n, frame), before = view_row_cell(shown, n, frame);
        uint32_t x = 8u + n * VIEW_ROW_STEP, bar = (uint32_t)(now.bar < 0 ? -now.bar : now.bar);
        uint16_t colour = now.used ? VIEW_WHITE : VIEW_GREY;
        char number[4];
        if (!all && view_same(&now, &before, sizeof now))
            continue;
        view_put_number(number, n + 1u);
        FONT_RECT(x - 1u, y, VIEW_ROW_STEP, VIEW_ROW_H, VIEW_BLACK);
        if (now.frame)
            view_frame(x - 1u, y, VIEW_ROW_STEP, VIEW_ROW_H, 1, now.frame == VIEW_FRAME_CYAN ? VIEW_CYAN : VIEW_AMBER);
        font_draw_text(x + 8u - font_text_width(number, 1) / 2u, y + 2u, 1, number, colour);
        FONT_RECT(x + 3u, y + 13u, 11, 1, VIEW_DIM);                    /* the middle; the bar up or down from it */
        FONT_RECT(x + 5u, now.bar > 0 ? y + 13u - bar : y + 13u, 7, bar + 1u, colour);
        if (now.sync)
            FONT_RECT(x + 6u, y + 18u, 5, 2, VIEW_WHITE);
    }
}

static void view_bank(const view_model_t *m, const view_model_t *shown, int all)
{
    uint32_t lfo = view_bank_lfo(m);
    view_bank_picture(m, shown, all);
    if (all || m->touched != shown->touched || lfo != view_bank_lfo(shown) || view_tempo(m) != view_tempo(shown)
        || !view_same(m->bank[lfo], shown->bank[lfo], sizeof m->bank[lfo]))
        view_bank_value_line(m);
    view_bank_dials(m, shown, all);
    view_bank_row(m, shown, VIEW_BANK_ROW_Y, VIEW_FRAME_AMBER, all);
}

/* the bank on the play page. Under each dial, twelve marks: the LFOs with an amount on that knob (KNOB1 to KNOB4:
 * TONE, MOVE, GRIT, LINK, the knobs the LFO key's amounts are on), the current one's cyan while the LFO key is held.
 * A white dot on a dial: its value now, with what the bank adds, when it is not on the knob's own dots */
#define VIEW_MARKS_Y 168u               /* their bottom: between the dials' names and the value line */
static const uint8_t view_dial_banked[4] = { PLAY_BANKED_TONE, PLAY_BANKED_MOVE, PLAY_BANKED_GRIT, PLAY_BANKED_LINK };
static const char *const view_banked_name[PLAY_NBANKED] = {
    [PLAY_BANKED_FREQ] = "FREQ", [PLAY_BANKED_TONE] = "TONE", [PLAY_BANKED_MOVE] = "MOVE", [PLAY_BANKED_GRIT] = "GRIT", [PLAY_BANKED_LINK] = "LINK",
};
static void view_marks(const view_model_t *m, uint32_t n)
{
    const int8_t *amount = m->bank_amount[view_dial_banked[n]];
    uint32_t l, x = VIEW_DIAL_X(n) - 23u, lfo = view_bank_lfo(m);
    FONT_RECT(x, VIEW_MARKS_Y - 5u, 4u * DRONE_BANK_LFOS - 1u, 5, VIEW_BLACK);
    for (l = 0; l < DRONE_BANK_LFOS; l++) {
        uint32_t high = amount[l] ? 5u : 2u;
        FONT_RECT(x + 4u * l, VIEW_MARKS_Y - high, 3, high, m->lfo_held && l == lfo ? VIEW_CYAN : amount[l] ? VIEW_GREY : VIEW_FAINT);
    }
}
/* what the bank adds to a knob now, in 1/256 of its steps: as if the knob went its whole range (127) each way */
static int32_t view_banked_q8(const view_model_t *m, uint32_t banked)
{
    int32_t out[DRONE_BANK_LFOS];
    uint32_t l;
    for (l = 0; l < DRONE_BANK_LFOS; l++)
        out[l] = m->bank_out[l];
    return drone_bank_cabled_q8(out, m->bank_amount[banked], 127);
}
static uint32_t view_lit_q8(int32_t value_q8)   /* a dial's dots for a value in 1/256, 0 .. 127 * 256 */
{
    value_q8 = value_q8 < 0 ? 0 : value_q8 > 127 * 256 ? 127 * 256 : value_q8;
    return ((uint32_t)value_q8 * VIEW_DIAL_DOTS + 63u * 256u) / (127u * 256u);
}
#define VIEW_NO_DOT 0xFFu
typedef struct { uint8_t layer, lit, dot; int8_t amount; } view_play_dial_t;   /* what a dial shows: bytes only */
static view_play_dial_t view_play_dial(const view_model_t *m, uint32_t n, int through)
{
    int32_t knob = panel_play_knob(PANEL_ENC_VALUES + n, m->shift, through);
    view_play_dial_t d = { 0, 0, VIEW_NO_DOT, 0 };
    if (m->lfo_held) {                          /* the current LFO's amount on the knob */
        d.layer = 1;
        d.amount = (int8_t)drone_limited(m->bank_amount[view_dial_banked[n]][view_bank_lfo(m)], DRONE_AMOUNT_TOP);
        return d;
    }
    if (knob < 0)
        return d;
    d.lit = (uint8_t)view_lit_q8((m->knob[knob] & 127) * 256);
    if (knob <= PLAY_LINK) {                    /* TONE, MOVE, GRIT, LINK: the knobs the bank moves */
        uint32_t lit = view_lit_q8((m->knob[knob] & 127) * 256 + view_banked_q8(m, view_dial_banked[knob]));
        if (lit != d.lit)
            d.dot = (uint8_t)(lit ? lit - 1u : 0u);
    }
    return d;
}
/* the LFO key held: the value line is the knob's name and the current LFO's amount on it, "TONE +38" */
static void view_amount_line_text(char *to, const view_model_t *m)
{
    int32_t banked = panel_banked(m->amount_touched);
    *to = 0;
    if (banked >= 0)
        view_amount_text(to, view_banked_name[banked], drone_limited(m->bank_amount[banked][view_bank_lfo(m)], DRONE_AMOUNT_TOP));
}

static void view_play(const view_model_t *m, const view_model_t *shown, int all)
{
    int through = m->shift && m->link_kind[m->selected % DRONE_VOICES] == DRONE_LINK_THROUGH;      /* OWN is there: the dials change with it */
    int was_through = shown->shift && shown->link_kind[shown->selected % DRONE_VOICES] == DRONE_LINK_THROUGH;
    int32_t touched = panel_play_knob(m->touched, m->shift, through);
    int held = m->lfo_held != 0, held_new = all || held != (shown->lfo_held != 0);
    int amounts_new = held_new || view_bank_lfo(m) != view_bank_lfo(shown) || !view_same(m->bank_amount, shown->bank_amount, sizeof m->bank_amount);
    uint32_t n;
    if (all || !view_same(m->scope[0], shown->scope[0], sizeof m->scope[0])) {
        int32_t i, before = 31 - drone_limited(m->scope[0][0], 28);
        view_canvas_start(224, 62, 1, VIEW_INK_DIM);
        for (i = 8; i < 216; i += 8)
            view_canvas_rect(i, 31, 4, 1, VIEW_INK_DIM);
        for (i = 0; i < (int32_t)DRONE_SCOPE_POINTS; i++) {
            int32_t y = 31 - drone_limited(m->scope[0][i], 28);
            view_canvas_rect(8 + 2 * i, y < before ? y : before, 2, (y < before ? before - y : y - before) + 2, VIEW_INK_AMBER);
            before = y;
        }
        view_canvas_send(8, 30);
    }
    for (n = 0; n < 4u; n++) {
        int32_t knob = panel_play_knob(PANEL_ENC_VALUES + n, m->shift, through);
        view_play_dial_t now = view_play_dial(m, n, through), before = view_play_dial(shown, n, was_through);
        /* the LFO key held: the knobs its amounts are on, whatever the shift key is */
        const char *name = held ? view_banked_name[view_dial_banked[n]] : knob < 0 ? "" : view_knob_name[knob];
        int bright = held ? m->amount_touched == PANEL_ENC_VALUES + n : knob >= 0 && knob == touched;
        if (amounts_new || all)
            view_marks(m, n);
        if (!all && !held_new && m->shift == shown->shift && m->touched == shown->touched && through == was_through
            && m->amount_touched == shown->amount_touched && view_same(&now, &before, sizeof now))
            continue;
        if (now.layer)
            view_amount_dial(VIEW_DIAL_X(n), now.amount);
        else
            view_dial(VIEW_DIAL_X(n), now.lit);
        if (now.dot != VIEW_NO_DOT)
            view_dial_dot(VIEW_DIAL_X(n), now.dot, VIEW_WHITE);
        FONT_RECT(VIEW_DIAL_X(n) - 26u, VIEW_DIAL_Y + 22u, 52, 7, VIEW_BLACK);
        font_draw_text(VIEW_DIAL_X(n) - font_text_width(name, 1) / 2u, VIEW_DIAL_Y + 22u, 1, name, bright ? VIEW_WHITE : VIEW_GREY);
    }
    if (all || held_new || m->shift != shown->shift || m->touched != shown->touched || through != was_through
        || (touched >= 0 && m->knob[touched] != shown->knob[touched]) || (held && (amounts_new || m->amount_touched != shown->amount_touched))) {
        char text[16] = "";
        if (held)
            view_amount_line_text(text, m);
        else if (touched >= 0)
            view_put_number(view_put(view_put(text, view_knob_name[touched]), " "), m->knob[touched] & 127u);
        view_text(8, VIEW_DIAL_Y + 44u, 120, 2, text, VIEW_WHITE, 0);
    }
    if (all || m->selected != shown->selected || !view_same(m->link_kind, shown->link_kind, sizeof m->link_kind))
        view_text(136, VIEW_DIAL_Y + 44u, 96, 2, panel_link_name[m->link_kind[m->selected % DRONE_VOICES] < DRONE_NLINK
                                                                 ? m->link_kind[m->selected % DRONE_VOICES] : DRONE_LINK_FOLLOW], VIEW_GREY, 1);
    if (held) {                                 /* the row of the twelve LFOs in the links' place */
        if (held_new)
            FONT_RECT(0, VIEW_LINKS_Y, 240, VIEW_VOICES_Y - VIEW_LINKS_Y, VIEW_BLACK);
        view_bank_row(m, shown, VIEW_LINKS_Y, VIEW_FRAME_CYAN, held_new);
    } else if (held_new || m->selected != shown->selected || !view_same(m->link, shown->link, sizeof m->link)
               || !view_same(m->link_kind, shown->link_kind, sizeof m->link_kind)) {
        view_links(m);
    }
}

/* ---- the mixer's page ----
 * A strip for each voice, over its box: its pan as a mark on a line, its level as a fader, its two sends as short
 * bars, and PRE or POST for where its sends are taken. The selected channel's strip has the amber frame. Below the
 * strips, a row for the four right encoders: the selected channel's four values, or the three voices' levels. */
#define VIEW_STRIP_Y 28u
#define VIEW_STRIP_H 128u
#define VIEW_MIXER_ROW_Y 165u

static void view_pan_text(char *to, uint32_t pan)       /* "L 64" .. "0" .. "R 64" */
{
    if (pan == MIXER_PAN_CENTRE)
        view_put(to, "0");
    else
        view_put_number(view_put(to, pan < MIXER_PAN_CENTRE ? "L " : "R "), pan < MIXER_PAN_CENTRE ? MIXER_PAN_CENTRE - pan : pan - MIXER_PAN_CENTRE);
}

static void view_level_text(char *to, uint32_t level)   /* the fader in dB: "0" at the top, "-12", "OFF" at the bottom */
{
    if (!level)
        view_put(to, "OFF");
    else
        view_put_number(view_put(to, mixer_level_db(level) ? "-" : ""), mixer_level_db(level));
}

static void view_strip(const view_model_t *m, uint32_t v)
{
    uint32_t pan = m->mixer[v][MIXER_PAN] > mixer_spec[MIXER_PAN].top ? mixer_spec[MIXER_PAN].top : m->mixer[v][MIXER_PAN];
    uint32_t x = 8u + v * VIEW_VOICE_STEP, on = m->channel == v + 1u, send;
    uint16_t ink = on ? VIEW_AMBER : VIEW_WHITE;
    const uint8_t *value = m->mixer[v];
    FONT_RECT(x, VIEW_STRIP_Y, VIEW_VOICE_W, VIEW_STRIP_H, VIEW_BLACK);
    view_frame(x, VIEW_STRIP_Y, VIEW_VOICE_W, VIEW_STRIP_H, on ? 2 : 1, on ? VIEW_AMBER : VIEW_DIM);
    FONT_RECT(x + 8u, VIEW_STRIP_Y + 12u, 56, 1, VIEW_GREY);                    /* the pan: a line, left to right */
    FONT_RECT(x + 8u + pan * 51u / mixer_spec[MIXER_PAN].top, VIEW_STRIP_Y + 8u, 5, 9, ink);
    FONT_RECT(x + 20u, VIEW_STRIP_Y + 26u, 1, 92, VIEW_GREY);                   /* the level: a fader, up is louder */
    FONT_RECT(x + 12u, VIEW_STRIP_Y + 112u - (value[MIXER_LEVEL] & 127u) * 86u / 127u, 17, 6, ink);
    for (send = 0; send < 2u; send++) {                                         /* the sends: a bar each */
        uint32_t y = VIEW_STRIP_Y + 34u + send * 28u;
        font_draw_text(x + 38u, y, 1, send ? "S2" : "S1", VIEW_GREY);
        FONT_RECT(x + 38u, y + 11u, 26, 4, VIEW_DIM);
        FONT_RECT(x + 38u, y + 11u, (value[MIXER_SEND_DELAY + send] & 127u) * 26u / 127u, 4, ink);
    }
    font_draw_text(x + 38u, VIEW_STRIP_Y + 108u, 1, m->post[v] ? "POST" : "PRE", on ? VIEW_AMBER : VIEW_GREY);
}

static void view_mixer(const view_model_t *m, const view_model_t *shown, int all)
{
    static const char *const value_name[MIXER_NVALUE] = { "LEVEL", "PAN", "SEND 1", "SEND 2" };
    uint32_t v, n, channel = m->channel <= DRONE_VOICES ? m->channel : 0u;
    for (v = 0; v < DRONE_VOICES; v++)
        if (all || m->channel != shown->channel || m->post[v] != shown->post[v] || !view_same(m->mixer[v], shown->mixer[v], sizeof m->mixer[v]))
            view_strip(m, v);
    if (!all && m->channel == shown->channel && view_same(m->mixer, shown->mixer, sizeof m->mixer))
        return;
    for (n = 0; n < MIXER_NVALUE; n++) {        /* a name and a value for each right encoder that holds one */
        char name[8] = "OP1", text[8] = "";
        uint32_t voice, value, x = 8u + n * 58u;
        int held = panel_mixer_value(channel, PANEL_ENC_VALUES + n, &voice, &value);
        name[2] = (char)('1' + n);
        if (held && value == MIXER_PAN)
            view_pan_text(text, m->mixer[voice][value] > mixer_spec[MIXER_PAN].top ? mixer_spec[MIXER_PAN].top : m->mixer[voice][value]);
        else if (held && value == MIXER_LEVEL)
            view_level_text(text, m->mixer[voice][value] & 127u);
        else if (held)
            view_put_number(text, m->mixer[voice][value] & 127u);
        view_text(x, VIEW_MIXER_ROW_Y, 56, 1, !held ? "" : channel ? value_name[value] : name, VIEW_GREY, 0);
        view_text(x, VIEW_MIXER_ROW_Y + 11u, 56, 2, text, VIEW_WHITE, 0);
    }
}

/* ---- the disruptors' pages ----
 * The disruptor as its values make it (not to a scale of time), amber while a voice that is on takes some, grey while
 * it is off:
 *   the noise   a step for each new value, longer as the RATE is lower, its heights by the BITS, its gaps by the DUST
 *   the pulse   closer as the RATE is higher, as wide as the WIDTH; a pulse SKIP leaves out is a dim outline
 *   the sine    more cycles as the FREQ is higher, bent over by FOLD, and with DRIFT its cycles are not all as long
 * Below it a dial for each right encoder, and the name and the value of the one turned last, with where the selected
 * voice takes the disruptor at the right. Over each voice's box: a bar as long as its amount, and an arrow down into
 * the box when it has some; amber for the selected voice's. */
static void view_hz10_text(char *to, uint32_t hz10)     /* tenths of a hertz: "0.7 HZ", "172 HZ", "44K" */
{
    if (hz10 >= 10000u) {
        view_hz_text(to, (hz10 + 5u) / 10u);
    } else if (hz10 >= 1000u) {
        view_put(view_put_number(to, (hz10 + 5u) / 10u), " HZ");
    } else {
        to = view_put_number(to, hz10 / 10u);
        *to++ = '.';
        view_put(view_put_number(to, hz10 % 10u), " HZ");
    }
}
static void view_noise_rate_text(char *to, uint32_t rate)       /* new values a second */
{
    uint32_t step = drone_noise_step(rate);
    /* 43 Hz and more: 16 bits of the step times 44118 stays in 32; below: the step times 1000 does */
    view_hz10_text(to, step >> 22 ? (((step >> 16) * 44118u + 32768u) >> 16) * 10u : (((step * 1000u) >> 16) * 441u + 32768u) >> 16);
}
/* a disruptor's value as the screen says it: a rate or a frequency in hertz, the noise's bits, the pulse's width in
 * percent of its cycle, the others as they are. 9735 of a phase step is a tenth of a hertz */
static void view_dis_text(char *to, uint32_t dis, uint32_t param, uint32_t value)
{
    const param_spec_t *spec = &drone_dis_spec[dis % DRONE_DISRUPTORS][param % DRONE_DIS_NPARAM];
    value = value > spec->top ? spec->top : value;
    if (dis == DRONE_DIS_NOISE && param == DRONE_NOISE_RATE)
        view_noise_rate_text(to, value);
    else if (dis == DRONE_DIS_CHOP && param == DRONE_CHOP_RATE)
        view_hz10_text(to, (drone_chop_step(value) + 4867u) / 9735u);
    else if (dis == DRONE_DIS_RING && param == DRONE_RING_FREQ)
        view_hz10_text(to, (drone_step_q8((int32_t)value * 256) + 4867u) / 9735u);
    else if (dis == DRONE_DIS_CHOP && param == DRONE_CHOP_WIDTH)
        view_put(view_put_number(to, ((value + 1u) * 100u + 64u) / 128u), "%");
    else
        view_put_number(to, dis == DRONE_DIS_NOISE && param == DRONE_NOISE_BITS ? value + 1u : value);
}

static int view_dis_on(const view_model_t *m, uint32_t dis)     /* a voice that is on takes some: the disruptor runs */
{
    uint32_t v, on = 0;
    for (v = 0; v < DRONE_VOICES; v++)
        on |= (m->voices_on >> v & 1u) && (m->dis_send[dis % DRONE_DISRUPTORS][v] & 127u);
    return on != 0;
}

/* the picture: 104 points, two pixels each, as the play page's */
static void view_dis_picture(uint32_t dis, const uint8_t *value, uint8_t ink)
{
    uint32_t seed = 12345u, left = 0, phase = 0;
    int32_t i, y = 31, before = 31;
    view_canvas_start(224, 62, 1, VIEW_INK_DIM);
    for (i = 8; i < 216; i += 8)
        view_canvas_rect(i, 31, 4, 1, VIEW_INK_DIM);
    for (i = 0; i < (int32_t)DRONE_SCOPE_POINTS; i++) {
        if (dis == DRONE_DIS_NOISE) {
            uint32_t down = 127u - (value[DRONE_NOISE_RATE] & 127u);
            if (!left) {
                y = 31 - drone_noise_value(&seed, value[DRONE_NOISE_BITS], value[DRONE_NOISE_DUST]) * 27 / 32767;
                left = 1u + down * down / 400u;
            }
            left--;
        } else if (dis == DRONE_DIS_CHOP) {
            uint32_t down = 127u - (value[DRONE_CHOP_RATE] & 127u), cycle = 3u + down * down / 160u;
            uint32_t open = cycle * ((value[DRONE_CHOP_WIDTH] & 127u) + 1u) / 128u, at = (uint32_t)i % cycle;
            open = open ? open : 1u;
            if (!at) {                          /* a new cycle: a pulse left out is a dim outline */
                seed = seed * 1664525u + 1013904223u;
                left = (seed >> 8 & 127u) < (value[DRONE_CHOP_SKIP] & 127u);
                if (left) {
                    view_canvas_rect(8 + 2 * i, 8, 2, 46, VIEW_INK_DIM);
                    view_canvas_rect(8 + 2 * i, 8, 2 * (int32_t)open, 2, VIEW_INK_DIM);
                    view_canvas_rect(8 + 2 * (i + (int32_t)open), 8, 2, 46, VIEW_INK_DIM);
                }
            }
            y = !left && at < open ? 8 : 52;
            if (!i)
                before = y;
        } else {
            uint32_t step = 5162220u * (8u + (value[DRONE_RING_FREQ] & 127u));  /* 1 to 17 cycles across the picture */
            int32_t wander = (drone_lfo_wave(DRONE_LFO_SINE, (uint32_t)i * 82595524u) * (int32_t)(value[DRONE_RING_DRIFT] & 127u)) >> 15;
            y = 31 - drone_ring_fold(drone_lfo_wave(DRONE_LFO_SINE, phase), value[DRONE_RING_FOLD]) * 27 / 32767;
            phase += step + (uint32_t)((int32_t)(step >> 8) * wander);
            if (!i)
                before = y;
        }
        view_canvas_rect(8 + 2 * i, y < before ? y : before, 2, (y < before ? before - y : y - before) + 2, ink);
        before = y;
    }
    view_canvas_send(8, 30);
}

static void view_dis(const view_model_t *m, const view_model_t *shown, int all, uint32_t dis)
{
    const uint8_t *value = m->dis[dis], *send = m->dis_send[dis], *to = m->dis_to[dis];
    uint32_t selected = m->selected % DRONE_VOICES, touched = m->touched - PANEL_ENC_VALUES, n, v;
    int on = view_dis_on(m, dis), values_new = all || !view_same(value, shown->dis[dis], sizeof m->dis[dis]);
    int sends_new = all || m->selected != shown->selected || !view_same(send, shown->dis_send[dis], sizeof m->dis_send[dis]);
    char to_voice[8] = "TO OP1";
    to_voice[5] = (char)('1' + selected);
    if (values_new || on != view_dis_on(shown, dis))
        view_dis_picture(dis, value, on ? VIEW_INK_AMBER : VIEW_INK_GREY);
    for (n = 0; n < 4u; n++) {
        const param_spec_t *spec = panel_dis_spec(dis, PANEL_ENC_VALUES + n);
        uint32_t now = n < DRONE_DIS_NPARAM ? value[n] : send[selected] & 127u;
        const char *knob = n < DRONE_DIS_NPARAM ? panel_dis_knob_name[dis][n] : to_voice;
        if (!values_new && !sends_new && m->touched == shown->touched)
            continue;
        now = now > spec->top ? spec->top : now;
        view_dial(VIEW_DIAL_X(n), (now * VIEW_DIAL_DOTS + spec->top / 2u) / spec->top);
        FONT_RECT(VIEW_DIAL_X(n) - 26u, VIEW_DIAL_Y + 22u, 52, 7, VIEW_BLACK);
        font_draw_text(VIEW_DIAL_X(n) - font_text_width(knob, 1) / 2u, VIEW_DIAL_Y + 22u, 1, knob, n == touched ? VIEW_WHITE : VIEW_GREY);
    }
    if (values_new || sends_new || m->touched != shown->touched) {
        char text[24] = "";
        if (touched < DRONE_DIS_NPARAM)
            view_dis_text(view_put(view_put(text, panel_dis_knob_name[dis][touched]), " "), dis, touched, value[touched]);
        else if (touched == DRONE_DIS_NPARAM)
            view_put_number(view_put(view_put(text, to_voice), " "), send[selected] & 127u);
        view_text(8, VIEW_DIAL_Y + 44u, 136, 2, text, VIEW_WHITE, 0);
    }
    if (all || m->selected != shown->selected || !view_same(to, shown->dis_to[dis], sizeof m->dis_to[dis]))
        view_text(148, VIEW_DIAL_Y + 44u, 84, 2, panel_to_name(dis, to[selected]), VIEW_GREY, 1);
    if (sends_new) {
        FONT_RECT(0, VIEW_LINKS_Y, 240, VIEW_VOICES_Y - VIEW_LINKS_Y, VIEW_BLACK);
        for (v = 0; v < DRONE_VOICES; v++) {
            uint32_t x = 8u + v * VIEW_VOICE_STEP, amount = send[v] & 127u, w = amount * (VIEW_VOICE_W - 16u) / 127u;
            uint32_t middle = x + VIEW_VOICE_W - 6u;
            uint16_t colour = v == m->selected ? VIEW_AMBER : VIEW_GREY;
            FONT_RECT(x, VIEW_LINKS_Y + 6u, VIEW_VOICE_W - 16u, 5, VIEW_FAINT);
            if (!amount)
                continue;
            FONT_RECT(x, VIEW_LINKS_Y + 6u, w ? w : 1u, 5, colour);
            FONT_RECT(middle - 1u, VIEW_LINKS_Y + 2u, 3, 12, colour);
            FONT_RECT(middle - 5u, VIEW_LINKS_Y + 14u, 11, 2, colour);
            FONT_RECT(middle - 3u, VIEW_LINKS_Y + 16u, 7, 2, colour);
            FONT_RECT(middle - 1u, VIEW_LINKS_Y + 18u, 3, 2, colour);
        }
    }
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
    int all = view_all, page_new = all || m->page != shown->page;
    int eoc = panel_page_eoc(m->page), play = m->page == PANEL_PLAY, mixer = m->page == PANEL_MIXER, dis = panel_page_dis(m->page);
    int bank = m->page == PANEL_BANK;
    int row_new = all || eoc != panel_page_eoc(shown->page) || play != (shown->page == PANEL_PLAY) || mixer != (shown->page == PANEL_MIXER)
               || dis != panel_page_dis(shown->page) || bank != (shown->page == PANEL_BANK);
    int top_new = page_new || m->info != shown->info || (m->info && m->load_percent != shown->load_percent)
               || (play && (m->style != shown->style || m->freq != shown->freq || m->lfo_held != shown->lfo_held))
               || (play && m->lfo_held && (view_bank_lfo(m) != view_bank_lfo(shown)
                                           || view_bank_knob(m, view_bank_lfo(m), DRONE_BANK_SHAPE) != view_bank_knob(shown, view_bank_lfo(shown), DRONE_BANK_SHAPE)))
               || (mixer && (m->channel != shown->channel || !view_same(m->post, shown->post, sizeof m->post)))
               || (dis >= 0 && view_dis_on(m, (uint32_t)dis) != view_dis_on(shown, (uint32_t)dis))
               || (bank && view_bank_top_new(m, shown));
    uint32_t i;

    if (all)
        FONT_RECT(0, 0, 240, 240, VIEW_BLACK);
    if (all || m->faults != shown->faults) {    /* a crash before this start; the audio hardware did not start */
        FONT_RECT(0, 0, 120, 2, m->faults & VIEW_CRASHED ? VIEW_AMBER : VIEW_BLACK);
        FONT_RECT(120, 0, 120, 2, m->faults & VIEW_NO_AUDIO ? VIEW_RED : VIEW_BLACK);
    }

    if (!bank && (row_new || m->selected != shown->selected)) {        /* the voice, or the end of the chain in its place */
        char name[4] = "OP1", dis_name[4] = "DS1";
        name[2] = (char)('1' + m->selected);
        dis_name[2] = (char)('1' + dis);
        FONT_RECT(8, 4, 40, 18, VIEW_WHITE);
        font_draw_text(11, 6, 2, eoc ? "EOC" : mixer ? "MIX" : dis >= 0 ? dis_name : name, VIEW_BLACK);
    }
    if (row_new && !all)                        /* a patch gives way to the end of the chain, or the other way */
        FONT_RECT(0, 23, 240, VIEW_VOICES_Y - 23u, VIEW_BLACK);
    if (top_new) {
        FONT_RECT(bank ? 8 : 56, 4, bank ? 224 : 176, 18, VIEW_BLACK);      /* the bank's page has no voice's box */
        if (m->info) {                          /* for the one who builds it: which firmware, and the load */
            char load[8];
            view_put(view_put_number(load, m->load_percent), "%");
            font_draw_text(56, 6, 2, APP_VERSION, VIEW_WHITE);
            font_draw_text(232u - font_text_width(load, 2), 6, 2, load, VIEW_WHITE);
        } else if (mixer) {                     /* the selected channel, and where its sends are taken */
            char channel[12] = "OP1 ";
            channel[2] = (char)('0' + m->channel);
            view_put(channel + 4, m->post[(m->channel - 1u) % DRONE_VOICES] ? "POST" : "PRE");
            if (m->channel && m->channel <= DRONE_VOICES)
                font_draw_text(232u - font_text_width(channel, 2), 6, 2, channel, VIEW_GREY);
        } else if (dis >= 0) {                  /* which disruptor, and if it runs */
            const char *state = view_dis_on(m, (uint32_t)dis) ? "ON" : "OFF";
            font_draw_text(56, 6, 2, panel_dis_name[dis], VIEW_WHITE);
            font_draw_text(232u - font_text_width(state, 2), 6, 2, state, view_dis_on(m, (uint32_t)dis) ? VIEW_GREEN : VIEW_GREY);
        } else if (bank) {
            view_bank_top(m);
        } else if (play && m->lfo_held) {       /* the LFO key held: the current LFO, and its wave */
            char lfo[8];
            const char *wave = view_bank_shape_name[view_bank_knob(m, view_bank_lfo(m), DRONE_BANK_SHAPE)];
            view_put_number(view_put(lfo, "LFO "), view_bank_lfo(m) + 1u);
            font_draw_text(56, 6, 2, lfo, VIEW_CYAN);
            font_draw_text(232u - font_text_width(wave, 2), 6, 2, wave, VIEW_GREY);
        } else if (play) {                      /* the voice's style and its frequency */
            char hz[12];
            int on_note = view_freq_note_text(hz, m->freq);  /* on a note: its name, in white, in the place of the hertz */
            if (!on_note)
                view_freq_text(hz, m->freq);
            font_draw_text(56, 6, 2, style_list[m->style < STYLE_NSTYLE ? m->style : 0u].name, VIEW_WHITE);
            font_draw_text(232u - font_text_width(hz, 2), 6, 2, hz, on_note ? VIEW_WHITE : VIEW_GREY);
        } else {
            const char *page_name = m->page == PANEL_VOICE ? "PATCH" : panel_page_name[m->page < PANEL_NPAGE ? m->page : PANEL_VOICE];
            font_draw_text(232u - font_text_width(page_name, 2), 6, 2, page_name, VIEW_GREY);
        }
    }

    if (bank && !m->info && (top_new || view_bank_beat_lit(m) != view_bank_beat_lit(shown)))
        view_bank_beat(m);

    if (eoc)
        view_eoc(m, shown, row_new, page_new);
    else if (play)
        view_play(m, shown, row_new);
    else if (mixer)
        view_mixer(m, shown, row_new);
    else if (dis >= 0)
        view_dis(m, shown, row_new, (uint32_t)dis);
    else if (bank)
        view_bank(m, shown, row_new);
    else
        view_patch(m, shown, row_new, page_new || row_new);

    if (all || m->voices_on != shown->voices_on || m->selected != shown->selected)
        for (i = 0; i < DRONE_VOICES; i++) {    /* filled: on; an outline: off; the amber mark: the knobs are this one's */
            uint32_t x = 8u + i * VIEW_VOICE_STEP, on = m->voices_on >> i & 1u;
            char digit[2] = { (char)('1' + i), 0 };
            FONT_RECT(x, VIEW_VOICES_Y, VIEW_VOICE_W, 18, on ? VIEW_WHITE : VIEW_BLACK);
            if (!on)
                view_frame(x, VIEW_VOICES_Y, VIEW_VOICE_W, 18, 2, VIEW_DIM);
            font_draw_text(x + (VIEW_VOICE_W - font_text_width(digit, 2)) / 2u, VIEW_VOICES_Y + 2u, 2, digit, on ? VIEW_BLACK : VIEW_GREY);
            FONT_RECT(x, VIEW_VOICES_Y + 20u, VIEW_VOICE_W, 4, i == m->selected ? VIEW_AMBER : VIEW_BLACK);
        }

    if (all || m->load_percent != shown->load_percent) {        /* the load: a thin line; red when the sound is near breaking up */
        uint32_t w = (m->load_percent > 100u ? 100u : m->load_percent) * 240u / 100u;
        FONT_RECT(0, 238, w, 2, m->load_percent > 80u ? VIEW_RED : VIEW_DIM);
        FONT_RECT(w, 238, 240u - w, 2, VIEW_BLACK);
    }
    view_shown = *m;
    view_all = 0;
}
