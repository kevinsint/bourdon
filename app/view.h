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
 * The bank's page (the LFO key) has the shown LFO's number in the voice's place and the set of values the knobs hold,
 * its wave as the engine plays it, those values, and below them the row of the twelve LFOs, its pattern or its loop;
 * the LFO key held, the range a source LFO moves each of them over, in cyan. On the play page the bank adds marks
 * under the dials, and while the LFO key is held, in cyan, the range the current LFO moves each knob over.
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
    uint8_t layer;                      /* and the layer in hand: 1, the knobs' second set (FREQ, none, GAIN, OWN) */
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
    /* the bank (V0.32). On its page `touched` is what was turned last: a DRONE_BANK_... value, or PANEL_BANK_TEMPO.
     * view_bank_take fills the shown LFO's part from the engine's picture of the bank */
    uint8_t bank_lfo;                   /* the LFO on the bank's page, and the one the play page's amounts are of: 0 .. */
    uint8_t bank_source;                /* the LFO key held on the bank's page: the LFO whose amounts the knobs are */
    uint8_t bank_set;                   /* the set of values the knobs hold there: PANEL_BANK_WAVE .. */
    uint8_t amount_touched;             /* the LFO key held: the encoder turned last */
    uint16_t tempo;                     /* BPM */
    uint16_t bank_used;                 /* bit l: a voice has an amount of the bank's LFO l */
    uint16_t bank_synced;               /* bit l: the bank's LFO l is synced */
    uint8_t bank_knob[DRONE_BANK_NPARAM];       /* the shown LFO's own values, as its knobs set them */
    uint8_t bank_value[DRONE_BANK_NPARAM];      /* and as the engine takes them, with what the cables add */
    int8_t bank_cabled[4][DRONE_BANK_LFOS];     /* the amounts onto the values KNOB1 .. KNOB4 hold in the set without shift: [knob][from] */
    int8_t bank_tempo_amount;           /* the source's amount onto the tempo */
    int16_t bank_held;                  /* the shown LFO in the engine (drone_bank_view_t): what a random shape holds, */
    uint32_t bank_phase, bank_step, bank_loop, bank_loop_at;    /* where it is in its step, its step, its loop, where it is in its loop */
    int8_t bank_amount[PLAY_NBANKED][DRONE_BANK_LFOS];  /* the selected voice's */
    int16_t bank_out[DRONE_BANK_LFOS];  /* the engine's bank as it was last taken (drone_bank_view_t): each LFO's output */
    int16_t bank_low[DRONE_BANK_LFOS], bank_high[DRONE_BANK_LFOS];     /* the least and the most each LFO gives, as its RANGE, LEVEL and OFFSET are set */
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
#define VIEW_CANVAS_W 226              /* the bank's rows have a frame a pixel each side of the 224 */
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
 * and the name and the value of the knob turned last. In the second layer: the knobs' second set. Over the
 * voices' boxes, the links: an arrow into each voice from the voice before it (into the first one from the last, back
 * over the others), thicker as its LINK is more open, amber for the selected voice's; in dashes when the link is
 * FOLLOW (it carries a loudness), whole when it is THROUGH (it carries the sound), in dots when it is FM (it bends the
 * pitch). The selected voice's kind of link has its name at the right of the knob's value. */
#define VIEW_LINKS_Y 190u               /* the links' band: 22 high, down to the voices' boxes */
#define VIEW_DIAL_X(n) (36u + (n) * 56u)
#define VIEW_DIAL_Y 132u               /* the dials' centre: as big as four fit across, with a gap */
#define VIEW_DIAL_NAME_Y 150u          /* their names, in the open bottom of their rings */
#define VIEW_DIAL_VALUE_Y 172u         /* the line under them: the name and the value turned last */
static const char *const view_knob_name[PLAY_NKNOB] = {
    [PLAY_TONE] = "TONE", [PLAY_STYLE] = "STYLE", [PLAY_GRIT] = "GRIT", [PLAY_LINK] = "LINK", [PLAY_SPEED] = "SPEED", [PLAY_OWN] = "OWN",
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

/* ---- a dial: a round knob with a line where it is, in a ring ----
 * The ring goes three quarters of a circle, from the bottom left over the top to the bottom right. On it: a lit arc,
 * the range LFOs move the value over (as each one's RANGE, LEVEL and OFFSET are set: an LFO that goes up only moves it
 * to one side of the knob's line only), and a lit dot, the value as the LFOs have it now. A position on the dial is one of VIEW_DIAL_STEPS + 1, 0 at the bottom left. Both are drawn in a block of
 * pixels that is sent at once: smooth edges (each edge pixel takes its share of the colour), and no flicker.
 * The sizes are in 1/16 of a pixel. */
#define VIEW_DIAL_STEPS 192u
#define VIEW_NO_DOT 0xFFu               /* a dial without a dot, or a knob without its line */
#define VIEW_DIAL_HALF 25               /* the block: 51 x 51 pixels round the dial's centre */
#define VIEW_DIAL_SIZE (2u * VIEW_DIAL_HALF + 1u)
#define VIEW_RING_R 336                 /* 21 pixels: the dot and its glow end in the block */
#define VIEW_RING_LINE 12               /* half its line: the contour */
#define VIEW_RING_ARC 28                /* half the lit arc */
#define VIEW_RING_DOT 40                /* the dot's radius; its glow goes 1.5 pixels more */
#define VIEW_RING_GLOW 24
#define VIEW_KNOB_R 240                 /* the knob: 15 pixels */
#define VIEW_KNOB_LINE_FROM 64          /* its line, from 4 pixels out of the centre to 12 */
#define VIEW_KNOB_LINE_TO 192
#define VIEW_KNOB_LINE 18               /* half the line */
#define VIEW_TURN 4096                  /* an angle: 1/4096 of a turn, clockwise from straight down */
typedef struct { uint8_t knob, from, to, dot; uint16_t ink; } view_dial_t;     /* from > to: no arc */
typedef uint16_t view_dial_block_t[VIEW_DIAL_SIZE * VIEW_DIAL_SIZE];

static uint32_t view_root(uint32_t n)           /* the whole square root */
{
    uint32_t root = 0, bit = 1u << 30;
    while (bit > n)
        bit >>= 2;
    for (; bit; bit >>= 2) {
        if (n >= root + bit) {
            n -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
    }
    return root;
}
static int32_t view_sin(uint32_t angle)         /* in 1/4096; within 0.2 % (Bhaskara's) */
{
    int32_t half = (int32_t)(angle % VIEW_TURN) & (VIEW_TURN / 2 - 1), q = half * (VIEW_TURN / 2 - half);
    int32_t s = q * 1024 / (327680 - q / 16);
    return angle % VIEW_TURN >= VIEW_TURN / 2u ? -s : s;
}
/* the angle of a point (x, y) from the centre, y down the screen: clockwise from straight down; within 0.1 degree */
static uint32_t view_angle(int32_t x, int32_t y)
{
    int32_t across = -x, along = y, a = across < 0 ? -across : across, b = along < 0 ? -along : along, t, base;
    if (!a && !b)
        return 0;
    t = (a < b ? a : b) * 4096 / (a > b ? a : b);
    base = (512 * t + (int32_t)(178u * (uint32_t)t * (uint32_t)(4096 - t) / 4096u)) / 4096;  /* atan, to 1/8 of a turn */
    base = a > b ? VIEW_TURN / 4 - base : base;
    if (along < 0)
        base = VIEW_TURN / 2 - base;
    return (uint32_t)(across < 0 ? VIEW_TURN - base : base) % VIEW_TURN;
}
static uint32_t view_dial_angle(uint32_t at)    /* a position on the dial: 45 degrees from straight down, then 270 */
{
    return VIEW_TURN / 8u + at * (VIEW_TURN * 3u / 4u) / VIEW_DIAL_STEPS;
}
static uint32_t view_cover(int32_t inside)      /* how much of a pixel a shape covers, 0 .. 256: `inside` its edge */
{
    inside += 8;
    return inside <= 0 ? 0u : inside >= 16 ? 256u : (uint32_t)inside * 16u;
}
static uint16_t view_mix(uint16_t under, uint16_t over, uint32_t cover)        /* `over` on `under`, cover 0 .. 256 */
{
    int32_t r = under >> 11, g = under >> 5 & 63, b = under & 31;
    r += ((over >> 11) - r) * (int32_t)cover / 256;
    g += ((over >> 5 & 63) - g) * (int32_t)cover / 256;
    b += ((over & 31) - b) * (int32_t)cover / 256;
    return (uint16_t)(r << 11 | g << 5 | b);
}
static uint16_t view_glow(uint16_t under, uint16_t light, uint32_t much)       /* light added, much 0 .. 256 */
{
    uint32_t r = (under >> 11) + (light >> 11) * much / 256u, g = (under >> 5 & 63) + (light >> 5 & 63) * much / 256u;
    uint32_t b = (under & 31) + (light & 31) * much / 256u;
    return (uint16_t)((r > 31 ? 31 : r) << 11 | (g > 63 ? 63 : g) << 5 | (b > 31 ? 31 : b));
}
static uint16_t view_grey(int32_t level)        /* 0 .. 255 */
{
    level = level < 0 ? 0 : level > 255 ? 255 : level;
    return (uint16_t)((level >> 3) << 11 | (level >> 2) << 5 | (level >> 3));
}

/* the ring: its contour, the lit arc from `from` to `to` in `ink`, and the lit dot at `dot` */
static void view_ring(view_dial_block_t block, uint32_t from, uint32_t to, uint32_t dot, uint16_t ink)
{
    int32_t dot_x = 0, dot_y = 0, i, j;
    uint32_t arc_from = view_dial_angle(from), arc_to = view_dial_angle(to);
    if (dot != VIEW_NO_DOT) {
        uint32_t angle = view_dial_angle(dot);
        dot_x = -view_sin(angle) * VIEW_RING_R / 4096;
        dot_y = view_sin(angle + VIEW_TURN / 4u) * VIEW_RING_R / 4096;
    }
    for (j = 0; j < (int32_t)VIEW_DIAL_SIZE; j++)
        for (i = 0; i < (int32_t)VIEW_DIAL_SIZE; i++) {
            int32_t x = (i - VIEW_DIAL_HALF) * 16, y = (j - VIEW_DIAL_HALF) * 16, r = (int32_t)view_root((uint32_t)(x * x + y * y));
            int32_t off = r > VIEW_RING_R ? r - VIEW_RING_R : VIEW_RING_R - r;
            uint32_t angle = view_angle(x, y);
            uint16_t *pixel = &block[j * (int32_t)VIEW_DIAL_SIZE + i];
            if (angle >= VIEW_TURN / 8u && angle <= VIEW_TURN * 7u / 8u) {
                int32_t to_end = (int32_t)(angle - VIEW_TURN / 8u < VIEW_TURN * 7u / 8u - angle ? angle - VIEW_TURN / 8u : VIEW_TURN * 7u / 8u - angle);
                uint32_t contour = view_cover(VIEW_RING_LINE - off), end = view_cover(to_end * r * 201 / 131072);   /* 2 pi / 4096 */
                *pixel = view_mix(*pixel, view_grey(92), contour < end ? contour : end);
                if (from <= to && angle + 1u >= arc_from && angle <= arc_to + 1u) {
                    int32_t in = (int32_t)(angle - arc_from < arc_to - angle ? angle - arc_from : arc_to - angle);
                    uint32_t arc = view_cover(VIEW_RING_ARC - off);
                    end = view_cover(in * r * 201 / 131072 + 8);
                    *pixel = view_mix(*pixel, ink, arc < end ? arc : end);
                }
            }
            if (dot != VIEW_NO_DOT) {
                int32_t far = (int32_t)view_root((uint32_t)((x - dot_x) * (x - dot_x) + (y - dot_y) * (y - dot_y)));
                if (far < VIEW_RING_DOT + VIEW_RING_GLOW) {
                    int32_t fade = VIEW_RING_DOT + VIEW_RING_GLOW - far;   /* the glow: brighter to the dot */
                    *pixel = view_glow(*pixel, ink, (uint32_t)(fade * fade * 400 / ((VIEW_RING_DOT + VIEW_RING_GLOW) * (VIEW_RING_DOT + VIEW_RING_GLOW))));
                    *pixel = view_mix(*pixel, VIEW_WHITE, view_cover(VIEW_RING_DOT - far));
                }
            }
        }
}

/* the knob: round, lit from the top left, with a line to `at`; VIEW_NO_DOT: no line */
static void view_knob(view_dial_block_t block, uint32_t at)
{
    int32_t line_x = 0, line_y = 0, i, j;
    if (at != VIEW_NO_DOT) {
        uint32_t angle = view_dial_angle(at);
        line_x = -view_sin(angle);
        line_y = view_sin(angle + VIEW_TURN / 4u);
    }
    for (j = 0; j < (int32_t)VIEW_DIAL_SIZE; j++)
        for (i = 0; i < (int32_t)VIEW_DIAL_SIZE; i++) {
            int32_t x = (i - VIEW_DIAL_HALF) * 16, y = (j - VIEW_DIAL_HALF) * 16, r = (int32_t)view_root((uint32_t)(x * x + y * y));
            uint16_t *pixel = &block[j * (int32_t)VIEW_DIAL_SIZE + i];
            int32_t light, rim;
            if (r > VIEW_KNOB_R + 8)
                continue;
            /* the face: lighter to the top left, darker to its edge; the rim: a light edge at the top left */
            light = 58 + (-x - y) * 22 / (2 * VIEW_KNOB_R) - r * r * 18 / (VIEW_KNOB_R * VIEW_KNOB_R);
            rim = view_cover(r - (VIEW_KNOB_R - 20));
            light += (int32_t)rim * ((-x - y) * 40 / (2 * VIEW_KNOB_R)) / 256;
            *pixel = view_mix(*pixel, view_grey(light), view_cover(VIEW_KNOB_R - r));
            if (at != VIEW_NO_DOT) {
                int32_t along = (x * line_x + y * line_y) / 4096, far;
                along = along < VIEW_KNOB_LINE_FROM ? VIEW_KNOB_LINE_FROM : along > VIEW_KNOB_LINE_TO ? VIEW_KNOB_LINE_TO : along;
                far = (int32_t)view_root((uint32_t)((x - along * line_x / 4096) * (x - along * line_x / 4096)
                                                    + (y - along * line_y / 4096) * (y - along * line_y / 4096)));
                *pixel = view_mix(*pixel, VIEW_WHITE, view_cover(VIEW_KNOB_LINE - far));
            }
        }
}

static void view_dial(uint32_t x, const view_dial_t *d)
{
    static view_dial_block_t block;
    uint32_t k;
    for (k = 0; k < VIEW_DIAL_SIZE * VIEW_DIAL_SIZE; k++)
        block[k] = VIEW_BLACK;
    view_knob(block, d->knob);
    view_ring(block, d->from, d->to, d->dot, d->ink);
    VIEW_BLIT(x - VIEW_DIAL_HALF, VIEW_DIAL_Y - VIEW_DIAL_HALF, VIEW_DIAL_SIZE, VIEW_DIAL_SIZE, block);
}
/* a dial's position for a value in 1/256 of a step, 0 .. top * 256 */
static uint8_t view_dial_at(int32_t value_q8, uint32_t top)
{
    int32_t full = (int32_t)top * 256;
    value_q8 = value_q8 < 0 ? 0 : value_q8 > full ? full : value_q8;
    return (uint8_t)(((uint32_t)value_q8 * VIEW_DIAL_STEPS + (uint32_t)full / 2u) / (uint32_t)full);
}
/* what LFOs add to a value, in 1/256 of its step: the least and the most they can, and what they add now; `any`: one
 * of them has an amount on it */
typedef struct { int32_t low, high, now; int any; } view_moved_t;
/* a value set by its knob (q8, of `top`): the knob's line there. With LFOs on it (`moved`; 0: none): an arc in `ink`
 * over the range they move it, and the dot where they have it now */
static view_dial_t view_dial_of(int32_t value_q8, const view_moved_t *moved, uint32_t top, uint16_t ink)
{
    view_dial_t d = { view_dial_at(value_q8, top), 1, 0, VIEW_NO_DOT, ink };
    if (moved && moved->any) {
        d.from = view_dial_at(value_q8 + moved->low, top);
        d.to = view_dial_at(value_q8 + moved->high, top);
        d.dot = view_dial_at(value_q8 + moved->now, top);
    }
    return d;
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

/* ---- the bank (V0.33) ----
 * Its page. The top line: the shown LFO's number, the three sets of values the knobs hold (WAVE, STEPS, LOOP) with a
 * line under the one in hand, FREE or SYNC and the clock's tempo at the right, with a square that is lit in the
 * first quarter of each beat. Its wave: this step and the next one, drawn with the engine's own functions from what
 * the engine gave of the LFO, so as it plays them (a step that plays or not by its pattern, its chance and its loop,
 * the random heights and the one a random shape holds, its loop starting again, the slew, the range, LEVEL and
 * OFFSET), a dim line between the two, the amber mark where the LFO is now; a synced LFO slower than a beat has a tick
 * for each beat. Then the name and the value turned last, large, with the shape's name at the right when there is
 * room; a dial for each value the knobs hold now, with the range the cables move it over and its value now, and under it
 * twelve marks: the LFOs with an amount on that value. The bottom row, with the WAVE set: the twelve LFOs, each one's
 * bar follows its output, the shown one has the amber frame, a brighter number when a voice has an amount of it, a
 * short line under a synced one. With STEPS: the pattern, a cell for each step (white a trig, grey a random trig, a
 * dot a pad, a dim frame an empty cell), the amber frame on the cell it is on. With LOOP: the loop as a bar with a
 * mark for each beat (a free LFO: each cycle) and the amber place where it is; under it the loops it plays (white)
 * and rests (framed), the amber frame on the one it is in.
 * The LFO key held there: the top line is the path from the source LFO (cyan) to the shown one (amber), the dials
 * have in cyan the range the source alone moves each value over, its marks cyan, and the bottom row the twelve LFOs with the
 * source in the cyan frame. */
#define VIEW_BANK_POINTS 112            /* a step of the picture: two fill the canvas */
#define VIEW_BANK_Y 26u                 /* the picture: 64 high */
#define VIEW_BANK_MID 32                /* its middle, in the canvas */
#define VIEW_BANK_REACH 27              /* a full output, up or down from the middle */
#define VIEW_BANK_VALUE_Y 92u           /* down to 105: the dials start at 107 */
#define VIEW_BANK_ROW_Y 176u            /* the row of the twelve LFOs: the play page has it in the links' band */
#define VIEW_ROW_H 21u                  /* a pixel less than that band: its frame does not touch the voices' boxes */
#define VIEW_ROW_BAR 3                  /* a bar's most, up or down from its middle: it stays clear of the number */
#define VIEW_ROW_STEP 19u
#define VIEW_BANK_ROW_X 7u              /* the pattern's and the loop's rows, in the canvas: their frames go a pixel left of 8 */
#define VIEW_BANK_ROW_W 226
#define VIEW_BANK_ROW_H 30
#define VIEW_BANK_CELLS 24u             /* the most cells a pattern has: 16 steps and 8 pads */
enum { VIEW_FRAME_NONE, VIEW_FRAME_AMBER, VIEW_FRAME_CYAN };
static const char *const view_bank_shape_name[DRONE_BANK_NSHAPE] = {
    "GATE", "RATCHET 2", "RATCHET 4", "SOFT GATE", "TRIANGLE", "TRAPEZOID", "SINE", "HUMP", "EXP", "SOFT EXP", "LOG", "SOFT LOG",
    "RANDOM", "SOFT RANDOM",
};
static const char *const view_bank_param_name[DRONE_BANK_NPARAM] = {
    [DRONE_BANK_RATE] = "RATE", [DRONE_BANK_RATIO] = "RATE", [DRONE_BANK_SHAPE] = "SHAPE", [DRONE_BANK_WIDTH] = "WIDTH",
    [DRONE_BANK_LEVEL] = "LEVEL", [DRONE_BANK_OFFSET] = "OFFSET", [DRONE_BANK_PHASE] = "PHASE", [DRONE_BANK_SLEW] = "SLEW",
    [DRONE_BANK_RANGE] = "RANGE", [DRONE_BANK_SYNC] = "SYNC", [DRONE_BANK_PROB] = "PROB", [DRONE_BANK_STEPS] = "STEPS",
    [DRONE_BANK_TRIGS] = "TRIGS", [DRONE_BANK_PAD] = "PAD", [DRONE_BANK_RAND] = "RAND", [DRONE_BANK_SHIFT] = "SHIFT",
    [DRONE_BANK_LOOP] = "LOOP", [DRONE_BANK_NAP] = "NAP", [DRONE_BANK_WAKE] = "WAKE", [DRONE_BANK_LSHIFT] = "L SHIFT",
};

static uint32_t view_bank_lfo(const view_model_t *m)
{
    return m->bank_lfo < DRONE_BANK_LFOS ? m->bank_lfo : DRONE_BANK_LFOS - 1u;
}
static uint32_t view_bank_source(const view_model_t *m)
{
    return m->bank_source < DRONE_BANK_LFOS ? m->bank_source : DRONE_BANK_LFOS - 1u;
}
static uint32_t view_bank_set(const view_model_t *m)
{
    return m->bank_set < PANEL_BANK_NSET ? m->bank_set : PANEL_BANK_WAVE;
}
/* a bank LFO's value as the engine keeps it: the PHASE goes round, the others stop at their top, TRIGS and WAKE are
 * 1 or more. The engine's functions index tables with them */
static uint32_t view_bank_kept(uint32_t param, uint32_t value)
{
    if (param == DRONE_BANK_PHASE)
        return value & 127u;
    value = value > drone_bank_spec[param].top ? drone_bank_spec[param].top : value;
    return !value && (param == DRONE_BANK_TRIGS || param == DRONE_BANK_WAKE) ? 1u : value;
}
static uint32_t view_bank_own(const view_model_t *m, uint32_t param)   /* the shown LFO's own value, as its knob sets it */
{
    return view_bank_kept(param, m->bank_knob[param]);
}
static void view_bank_values(const view_model_t *m, uint8_t *value)    /* its values as the engine takes them */
{
    uint32_t p;
    for (p = 0; p < DRONE_BANK_NPARAM; p++)
        value[p] = (uint8_t)view_bank_kept(p, m->bank_value[p]);
}
static int view_bank_sync(const view_model_t *m)
{
    return view_bank_own(m, DRONE_BANK_SYNC) != 0;
}
static uint32_t view_tempo(const view_model_t *m)
{
    return m->tempo < DRONE_TEMPO_LOW ? DRONE_TEMPO_LOW : m->tempo > DRONE_TEMPO_HIGH ? DRONE_TEMPO_HIGH : m->tempo;
}

/* the bank in the model: LFO `lfo`'s own values (`c`, the controls) and what the engine gave of it (`engine`, its
 * picture of the bank: its values with the cables, where it is), each LFO's output and SYNC, the tempo, and the
 * cables onto the values the knobs of `set` hold: from which LFOs, and LFO `source`'s amount onto the tempo */
static void view_bank_take(view_model_t *m, const volatile drone_controls_t *c, const drone_bank_view_t *engine, uint32_t lfo, uint32_t source,
                           uint32_t set)
{
    uint32_t p, l, n;
    lfo %= DRONE_BANK_LFOS;
    source %= DRONE_BANK_LFOS;
    m->bank_lfo = (uint8_t)lfo;
    m->bank_source = (uint8_t)source;
    m->bank_set = (uint8_t)set;
    m->tempo = c->tempo;
    m->bank_synced = 0;
    for (l = 0; l < DRONE_BANK_LFOS; l++) {
        uint8_t value[DRONE_BANK_NPARAM];
        for (p = 0; p < DRONE_BANK_NPARAM; p++)
            value[p] = (uint8_t)view_bank_kept(p, c->bank[l][p]);
        m->bank_out[l] = engine->out[l];
        m->bank_low[l] = (int16_t)drone_bank_out(value[DRONE_BANK_RANGE] ? 0 : -32767, value);
        m->bank_high[l] = (int16_t)drone_bank_out(32767, value);
        m->bank_synced |= (uint16_t)((c->bank[l][DRONE_BANK_SYNC] != 0) << l);
    }
    for (p = 0; p < DRONE_BANK_NPARAM; p++) {
        m->bank_knob[p] = c->bank[lfo][p];
        m->bank_value[p] = engine->value[lfo][p];
    }
    for (n = 0; n < 4u; n++)
        for (l = 0; l < DRONE_BANK_LFOS; l++)
            m->bank_cabled[n][l] = (int8_t)(panel_bank_cable(set, n) < DRONE_BANK_NCABLED ? c->bank_amount[lfo][panel_bank_cable(set, n)][l] : 0);
    m->bank_tempo_amount = c->tempo_amount[source];
    m->bank_held = engine->held[lfo];
    m->bank_phase = engine->phase[lfo];
    m->bank_step = engine->step[lfo];
    m->bank_loop = engine->loop[lfo];
    m->bank_loop_at = engine->loop_at[lfo];
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
static char *view_bank_counted(char *to, uint32_t count, const char *unit)     /* "1 LOOP", "4 LOOPS" */
{
    to = view_put(view_put(view_put_number(to, count), " "), unit);
    return count > 1u ? view_put(to, "S") : to;
}
/* a value of the shown LFO as the value line says it: "50%", "+20%", "OFF", "4 BEATS" */
static void view_bank_param_text(char *to, const view_model_t *m, uint32_t param)
{
    uint32_t value = view_bank_own(m, param);
    switch (param) {
    case DRONE_BANK_RATE:
    case DRONE_BANK_RATIO:                      /* the same knob: what it is now */
        if (view_bank_sync(m))
            view_bank_ratio_text(to, view_bank_own(m, DRONE_BANK_RATIO));
        else
            view_bank_rate_text(to, view_bank_own(m, DRONE_BANK_RATE));
        break;
    case DRONE_BANK_SHAPE:
        view_put(to, view_bank_shape_name[value]);
        break;
    case DRONE_BANK_WIDTH:
    case DRONE_BANK_LEVEL:
    case DRONE_BANK_PROB:
        view_put(view_put_number(to, value), "%");
        break;
    case DRONE_BANK_OFFSET:                     /* 0 .. 200: -100 % .. +100 % */
        if (value != DRONE_BANK_OFFSET_ZERO)
            *to++ = value < DRONE_BANK_OFFSET_ZERO ? '-' : '+';
        view_put(view_put_number(to, value < DRONE_BANK_OFFSET_ZERO ? DRONE_BANK_OFFSET_ZERO - value : value - DRONE_BANK_OFFSET_ZERO), "%");
        break;
    case DRONE_BANK_PHASE:                      /* in degrees: PHASE / 128 of a cycle */
        view_put_number(to, value * 45u / 16u);
        break;
    case DRONE_BANK_RANGE:
        view_put(to, value ? "UP" : "BOTH");
        break;
    case DRONE_BANK_SYNC:
        view_put(to, value ? "ON" : "OFF");
        break;
    case DRONE_BANK_STEPS:
    case DRONE_BANK_NAP:
    case DRONE_BANK_LOOP:                       /* 0: none */
        if (!value)
            view_put(to, "OFF");
        else if (param == DRONE_BANK_STEPS)
            view_put_number(to, value);
        else
            view_bank_counted(to, value, param == DRONE_BANK_NAP ? "LOOP" : view_bank_sync(m) ? "BEAT" : "CYCLE");
        break;
    case DRONE_BANK_WAKE:
        view_bank_counted(to, value, "LOOP");
        break;
    default:
        view_put_number(to, value);
        break;
    }
}
/* the LFO key held: the name of the value the knob turned last holds, and the source's amount on it, "WIDTH +40";
 * PRESETS: "TEMPO -20" */
static void view_bank_amount_text(char *to, const view_model_t *m)
{
    panel_bank_holds_t holds = panel_bank_holds(view_bank_set(m), m->amount_touched, 1, 0);
    *to = 0;
    if (holds.kind == PANEL_HOLDS_AMOUNT)
        view_amount_text(to, view_bank_param_name[drone_bank_cabled_param[holds.which]],
                         drone_limited(m->bank_cabled[m->amount_touched - PANEL_ENC_VALUES][view_bank_source(m)], DRONE_AMOUNT_TOP));
    else if (holds.kind == PANEL_HOLDS_TEMPO_AMOUNT)
        view_amount_text(to, "TEMPO", drone_limited(m->bank_tempo_amount, DRONE_AMOUNT_TOP));
}
/* the value line: the name and the value of what was turned last; the LFO key held, the amount turned last */
static void view_bank_value_text(char *to, const view_model_t *m)
{
    *to = 0;
    if (m->lfo_held)
        view_bank_amount_text(to, m);
    else if (m->touched == PANEL_BANK_TEMPO)
        view_put(view_put_number(view_put(to, "TEMPO "), view_tempo(m)), " BPM");
    else if (m->touched < DRONE_BANK_NPARAM)
        view_bank_param_text(view_put(view_put(to, view_bank_param_name[m->touched]), " "), m, m->touched);
}
/* the shape's name at the right of the value line, when there is room: not the SHAPE's line, not the amounts'; 0: none */
static const char *view_bank_value_right(const view_model_t *m, const char *text)
{
    const char *shape = view_bank_shape_name[view_bank_own(m, DRONE_BANK_SHAPE)];
    return m->lfo_held || m->touched == DRONE_BANK_SHAPE || font_text_width(text, 2) + 12u + font_text_width(shape, 2) > 224u ? 0 : shape;
}

/* the top line: the LFO's number, FREE or SYNC, the tempo; then the beat's square, at the left of the tempo. The
 * layer in hand is not here: view_layers */
static void view_bank_top(const view_model_t *m)
{
    char text[12];
    int sync = view_bank_sync(m);
    view_put_number(view_put(text, "LFO "), view_bank_lfo(m) + 1u);
    font_draw_text(8, 6, 2, text, VIEW_AMBER);
    font_draw_text(232u - font_text_width(sync ? "SYNC" : "FREE", 1), 5, 1, sync ? "SYNC" : "FREE", sync ? VIEW_WHITE : VIEW_GREY);
    view_put(view_put_number(text, view_tempo(m)), " BPM");
    font_draw_text(232u - font_text_width(text, 1), 14, 1, text, VIEW_GREY);
}
/* the LFO key held: the path from the source LFO to the shown one, "LFO 2 > LFO 5" */
static void view_bank_path(const view_model_t *m)
{
    char from[8], to[8];
    uint32_t x;
    view_put_number(view_put(from, "LFO "), view_bank_source(m) + 1u);
    view_put_number(view_put(to, "LFO "), view_bank_lfo(m) + 1u);
    x = 8u + font_text_width(from, 2) + 14u;    /* a space, and 2 */
    font_draw_text(8, 6, 2, from, VIEW_CYAN);
    font_draw_text(x, 6, 2, ">", VIEW_GREY);
    font_draw_text(x + 24u, 6, 2, to, VIEW_AMBER);
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
    int held = m->lfo_held != 0;
    return view_bank_lfo(m) != view_bank_lfo(shown) || view_tempo(m) != view_tempo(shown) || view_bank_sync(m) != view_bank_sync(shown)
        || held != (shown->lfo_held != 0) || (held && view_bank_source(m) != view_bank_source(shown));
}

/* ---- the picture: the engine's functions, step by step ---- */
typedef struct { uint32_t step, loop; int32_t held; } view_bank_at_t;  /* a step, its loop, the height a random shape holds in it */
/* the step before: in a loop, at its first step, the last one of the loop before. Only the slew comes from it, so the
 * very first step is its own. Its `held` is this one's: a random shape holds a new height only after a step that
 * plays, and then that step draws its own shape, not what it holds */
static view_bank_at_t view_bank_before(const uint8_t *value, view_bank_at_t at)
{
    uint32_t steps = drone_bank_loop_steps(value);
    if (at.step) {
        at.step--;
    } else if (steps && at.loop) {
        at.step = steps - 1u;
        at.loop--;
    }
    return at;
}
static view_bank_at_t view_bank_after(uint32_t lfo, const uint8_t *value, view_bank_at_t at)   /* the step after: a loop starts again */
{
    view_bank_at_t next = at;
    drone_bank_step_after(value, &next.step, &next.loop);
    next.held = drone_bank_held_after(lfo, value, at.step, at.loop, at.held);
    return next;
}
static int32_t view_bank_y(int32_t out)        /* in the canvas */
{
    return VIEW_BANK_MID - drone_limited(out, 32767) * VIEW_BANK_REACH / 32767;
}
typedef struct { uint8_t x, y; } view_bank_mark_t;     /* where the LFO is now: in its step, and its output */
static view_bank_mark_t view_bank_mark(const view_model_t *m)
{
    view_bank_mark_t mark = { (uint8_t)(((m->bank_phase >> 16) * VIEW_BANK_POINTS) >> 16), (uint8_t)view_bank_y(m->bank_out[view_bank_lfo(m)]) };
    return mark;
}
static int view_bank_wave_new(const view_model_t *m, const view_model_t *shown)     /* what the wave is drawn from has changed */
{
    return view_bank_lfo(m) != view_bank_lfo(shown) || !view_same(m->bank_value, shown->bank_value, sizeof m->bank_value)
        || m->bank_step != shown->bank_step || m->bank_loop != shown->bank_loop || m->bank_held != shown->bank_held;
}

/* the picture. Only the mark moved: only its columns are sent, where it was and where it is */
static void view_bank_picture(const view_model_t *m, const view_model_t *shown, int all)
{
    view_bank_mark_t mark = view_bank_mark(m), was = view_bank_mark(shown);
    int wave_new = all || view_bank_wave_new(m, shown);
    uint32_t lfo = view_bank_lfo(m);
    uint8_t value[DRONE_BANK_NPARAM];
    view_bank_at_t at[3];
    int32_t i, height, y, before_y = VIEW_BANK_MID, most;
    if (!wave_new && mark.x == was.x && mark.y == was.y)
        return;
    view_bank_values(m, value);
    most = drone_bank_slew_limit(0xFFFFFFFFu / VIEW_BANK_POINTS, value[DRONE_BANK_SLEW]);
    at[1] = (view_bank_at_t){ m->bank_step, m->bank_loop, m->bank_held };
    at[0] = view_bank_before(value, at[1]);
    at[2] = view_bank_after(lfo, value, at[1]);
    view_canvas_start(2 * VIEW_BANK_POINTS, 64, 0, VIEW_INK_BLACK);
    for (i = 0; i < 2 * VIEW_BANK_POINTS; i += 8)
        view_canvas_rect(i, VIEW_BANK_MID, 4, 1, VIEW_INK_DIM);
    view_canvas_rect(VIEW_BANK_POINTS, 0, 1, 64, VIEW_INK_DIM);
    if (value[DRONE_BANK_SYNC] && drone_bank_beats[value[DRONE_BANK_RATIO]] > 1u) {   /* a tick for each beat; PHASE moves the step against them */
        int32_t per = drone_bank_beats[value[DRONE_BANK_RATIO]], offset = value[DRONE_BANK_PHASE] * VIEW_BANK_POINTS / 128, k;
        for (k = -per; k < 2 * per; k++)
            view_canvas_rect(offset + k * VIEW_BANK_POINTS / per, 61, 1, 3, VIEW_INK_GREY);
    }
    /* the step before first, so that the slew comes into this one from where it was */
    height = drone_bank_height(lfo, value, at[0].step, at[0].loop, 0, at[0].held);
    for (i = -VIEW_BANK_POINTS; i < 2 * VIEW_BANK_POINTS; i++) {
        const view_bank_at_t *in = &at[(i + VIEW_BANK_POINTS) / VIEW_BANK_POINTS];
        uint32_t phase = (uint32_t)((i + VIEW_BANK_POINTS) % VIEW_BANK_POINTS) * (0xFFFFFFFFu / VIEW_BANK_POINTS);
        height += drone_limited(drone_bank_height(lfo, value, in->step, in->loop, phase, in->held) - height, most);
        y = view_bank_y(drone_bank_out(height, value));
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

static void view_bank_value_line(const view_model_t *m, const view_model_t *shown, int all)
{
    char text[24] = "", was[24] = "";
    const char *right;
    view_bank_value_text(text, m);
    view_bank_value_text(was, shown);
    right = view_bank_value_right(m, text);
    if (!all && view_same(text, was, sizeof text) && right == view_bank_value_right(shown, was))
        return;
    view_text(8, VIEW_BANK_VALUE_Y, 224, 2, text, VIEW_WHITE, 0);
    if (right)
        font_draw_text(232u - font_text_width(right, 2), VIEW_BANK_VALUE_Y, 2, right, VIEW_GREY);
}

/* ---- the dials ---- */
/* under a dial, twelve marks: the LFOs with an amount (`amount`, one for each) on its value; `cyan`: the one in cyan,
 * or DRONE_BANK_LFOS: none. `amount` 0: the value takes no LFO, no marks */
#define VIEW_MARKS_Y 168u               /* their bottom: between the dials' names and the play page's value line */
/* a page's layers, at the left of the dials: a square for each, the first at the top as the layers' keys go from
 * the left, and the one in hand in amber. No names: a page with fewer layers has its squares at the same places */
#define VIEW_LAYER_X 4u
#define VIEW_LAYER_Y 117u
static void view_layers(uint32_t layers, uint32_t in_hand)
{
    uint32_t l;
    for (l = 0; l < layers; l++) {
        FONT_RECT(VIEW_LAYER_X, VIEW_LAYER_Y + 8u * l, 5, 5, l == in_hand ? VIEW_AMBER : VIEW_GREY);
        if (l != in_hand)
            FONT_RECT(VIEW_LAYER_X + 1u, VIEW_LAYER_Y + 8u * l + 1u, 3, 3, VIEW_BLACK);
    }
}

static void view_marks(uint32_t n, const int8_t *amount, uint32_t cyan)
{
    uint32_t l, x = VIEW_DIAL_X(n) - 23u;
    FONT_RECT(x, VIEW_MARKS_Y - 5u, 4u * DRONE_BANK_LFOS - 1u, 5, VIEW_BLACK);
    for (l = 0; amount && l < DRONE_BANK_LFOS; l++) {
        uint32_t high = amount[l] ? 5u : 2u;
        FONT_RECT(x + 4u * l, VIEW_MARKS_Y - high, 3, high, l == cyan ? VIEW_CYAN : amount[l] ? VIEW_GREY : VIEW_FAINT);
    }
}
static uint32_t view_bank_cyan(const view_model_t *m)  /* the LFO key held: the source's mark is cyan */
{
    return m->lfo_held ? view_bank_source(m) : DRONE_BANK_LFOS;
}
/* what the bank's LFOs add to a value through their amounts (`reach`: as drone_bank_cabled_q8's): the least and the
 * most, each LFO over what it gives (bank_low .. bank_high; an amount below 0 turns it over), and now. `only`: that
 * LFO alone, or DRONE_BANK_LFOS: all of them */
static view_moved_t view_moved(const view_model_t *m, const int8_t *amount, int32_t reach, uint32_t only)
{
    int32_t low[DRONE_BANK_LFOS], high[DRONE_BANK_LFOS], now[DRONE_BANK_LFOS];
    int8_t taken[DRONE_BANK_LFOS];
    view_moved_t moved = { 0, 0, 0, 0 };
    uint32_t l;
    for (l = 0; l < DRONE_BANK_LFOS; l++) {
        taken[l] = only == DRONE_BANK_LFOS || only == l ? amount[l] : 0;
        low[l] = taken[l] < 0 ? m->bank_high[l] : m->bank_low[l];
        high[l] = taken[l] < 0 ? m->bank_low[l] : m->bank_high[l];
        now[l] = m->bank_out[l];
        moved.any |= taken[l] != 0;
    }
    moved.low = drone_bank_cabled_q8(low, taken, reach);
    moved.high = drone_bank_cabled_q8(high, taken, reach);
    moved.now = drone_bank_cabled_q8(now, taken, reach);
    return moved;
}
typedef struct { view_dial_t dial; uint8_t bright, param; } view_bank_dial_t;  /* what a dial shows: no gap between its bytes */
static view_bank_dial_t view_bank_dial(const view_model_t *m, uint32_t n)
{
    int held = m->lfo_held != 0;
    uint32_t cable = panel_bank_cable(view_bank_set(m), n);
    int cabled;
    view_bank_dial_t d;
    view_moved_t moved;
    /* the value the knob holds; the LFO key held: the value the source's amount is on (a synced LFO: its RATE, not
     * its RATIO), and in cyan the range the source alone moves it over */
    d.param = held && cable < DRONE_BANK_NCABLED ? (uint8_t)drone_bank_cabled_param[cable]
                                                 : panel_bank_holds(view_bank_set(m), PANEL_ENC_VALUES + n, 0, view_bank_sync(m)).which;
    d.bright = held ? m->amount_touched == PANEL_ENC_VALUES + n
                    : m->touched == d.param || (m->touched <= DRONE_BANK_RATIO && d.param <= DRONE_BANK_RATIO);    /* RATE and RATIO: one knob */
    /* no cable moves it: a value of the MORE layer, or a synced LFO's RATIO */
    cabled = cable < DRONE_BANK_NCABLED && d.param == drone_bank_cabled_param[cable];
    moved = view_moved(m, m->bank_cabled[n], cabled ? drone_bank_cable_reach[cable] : 0, held ? view_bank_source(m) : DRONE_BANK_LFOS);
    moved.any = moved.any && cabled;
    d.dial = view_dial_of((int32_t)view_bank_own(m, d.param) * 256, &moved, drone_bank_spec[d.param].top, held ? VIEW_CYAN : VIEW_AMBER);
    return d;
}
static void view_bank_dials(const view_model_t *m, const view_model_t *shown, int all)
{
    uint32_t n;
    if (all || view_bank_set(m) != view_bank_set(shown))
        view_layers(PANEL_BANK_NSET, view_bank_set(m));
    for (n = 0; n < 4u; n++) {
        view_bank_dial_t now = view_bank_dial(m, n), before = view_bank_dial(shown, n);
        if (all || view_bank_cyan(m) != view_bank_cyan(shown) || view_bank_set(m) != view_bank_set(shown)
            || !view_same(m->bank_cabled[n], shown->bank_cabled[n], sizeof m->bank_cabled[n]))
            view_marks(n, panel_bank_cable(view_bank_set(m), n) < DRONE_BANK_NCABLED ? m->bank_cabled[n] : 0, view_bank_cyan(m));
        if (!all && view_same(&now, &before, sizeof now))
            continue;
        view_dial(VIEW_DIAL_X(n), &now.dial);
        FONT_RECT(VIEW_DIAL_X(n) - 26u, VIEW_DIAL_NAME_Y, 52, 7, VIEW_BLACK);
        font_draw_text(VIEW_DIAL_X(n) - font_text_width(view_bank_param_name[now.param], 1) / 2u, VIEW_DIAL_NAME_Y, 1,
                       view_bank_param_name[now.param], now.bright ? VIEW_WHITE : VIEW_GREY);
    }
}

/* ---- the bottom row ---- */
/* the row of the twelve LFOs, at `y`. The bank's page: the shown LFO has the amber frame, the LFO key held the source
 * the cyan one; the play page (the LFO key held): the LFO the amounts are of has the cyan frame */
typedef struct { uint8_t frame, used, sync; int8_t bar; } view_row_cell_t;
static view_row_cell_t view_row_cell(const view_model_t *m, uint32_t n)
{
    view_row_cell_t c;
    int bank = m->page == PANEL_BANK;
    c.frame = (uint8_t)(bank && m->lfo_held && n == view_bank_source(m) ? VIEW_FRAME_CYAN : n != view_bank_lfo(m) ? VIEW_FRAME_NONE
                        : bank ? VIEW_FRAME_AMBER : VIEW_FRAME_CYAN);
    c.used = (uint8_t)(m->bank_used >> n & 1u);
    c.sync = (uint8_t)(m->bank_synced >> n & 1u);
    c.bar = (int8_t)(drone_limited(m->bank_out[n], 32767) * VIEW_ROW_BAR / 32767);
    return c;
}
static void view_bank_row(const view_model_t *m, const view_model_t *shown, uint32_t y, int all)
{
    uint32_t n;
    for (n = 0; n < DRONE_BANK_LFOS; n++) {
        view_row_cell_t now = view_row_cell(m, n), before = view_row_cell(shown, n);
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

/* the STEPS row and the LOOP row: all that one shows, bytes only, so that two compare byte by byte */
enum { VIEW_ROW_PATTERN = 1, VIEW_ROW_LOOP };
#define VIEW_BANK_PAD 3u                /* a cell of the pattern: after DRONE_BANK_EMPTY .. DRONE_BANK_RANDOM_TRIG, an empty pad */
typedef struct {
    uint8_t row;                        /* VIEW_ROW_PATTERN or VIEW_ROW_LOOP */
    uint8_t count, here;                /* the pattern's cells and the one it is on; the loop's beats or cycles (0: none) */
    uint8_t place;                      /* the loop: where it is in it, 0 .. 222 */
    uint8_t wake, nap, round;           /* the loops it plays and rests, and the one it is in, from 0 */
    uint8_t cell[VIEW_BANK_CELLS];      /* each cell of the pattern: DRONE_BANK_EMPTY .. or VIEW_BANK_PAD */
} view_bank_steps_t;
static view_bank_steps_t view_bank_steps_of(const view_model_t *m)
{
    view_bank_steps_t s = { 0 };
    uint8_t value[DRONE_BANK_NPARAM];
    uint32_t lfo = view_bank_lfo(m), i;
    view_bank_values(m, value);
    if (view_bank_set(m) == PANEL_BANK_STEPS) {
        s.row = VIEW_ROW_PATTERN;
        s.count = (uint8_t)drone_bank_cells(value);
        s.here = (uint8_t)(s.count ? m->bank_step % s.count : 0u);
        for (i = 0; i < s.count; i++) {
            uint32_t cell = drone_bank_cell(lfo, value, i);
            s.cell[i] = (uint8_t)(cell == DRONE_BANK_EMPTY && i >= value[DRONE_BANK_STEPS] ? VIEW_BANK_PAD : cell);
        }
        return s;
    }
    s.row = VIEW_ROW_LOOP;
    s.count = value[DRONE_BANK_LOOP];
    if (s.count) {                              /* loop_at: beats or cycles, 65536 each */
        uint32_t whole = (uint32_t)s.count << 16, at = m->bank_loop_at < whole ? m->bank_loop_at : whole;
        s.place = (uint8_t)((at >> 8) * 222u / (whole >> 8));
        s.wake = value[DRONE_BANK_WAKE];
        s.nap = value[DRONE_BANK_NAP];
        s.round = (uint8_t)(s.nap ? (m->bank_loop + value[DRONE_BANK_LSHIFT]) % (uint32_t)(s.wake + s.nap) : 0u);
    }
    return s;
}
static void view_canvas_frame(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t ink)
{
    view_canvas_rect(x, y, w, 1, ink);
    view_canvas_rect(x, y + h - 1, w, 1, ink);
    view_canvas_rect(x, y, 1, h, ink);
    view_canvas_rect(x + w - 1, y, 1, h, ink);
}
/* the row from what it shows, in the canvas from (VIEW_BANK_ROW_X, VIEW_BANK_ROW_Y): one pass, so it does not flicker */
static void view_bank_steps_draw(const view_bank_steps_t *s)
{
    int32_t i, x0 = 8 - (int32_t)VIEW_BANK_ROW_X;              /* the screen's x 8 */
    view_canvas_start(VIEW_BANK_ROW_W, VIEW_BANK_ROW_H, 0, VIEW_INK_BLACK);
    if (s->row == VIEW_ROW_PATTERN && s->count) {
        int32_t cell = 224 / s->count < 13 ? 224 / s->count : 13;
        for (i = 0; i < s->count; i++) {
            int32_t x = x0 + i * cell;
            if (s->cell[i] == DRONE_BANK_TRIG || s->cell[i] == DRONE_BANK_RANDOM_TRIG)
                view_canvas_rect(x + 1, 4, cell - 3, 16, s->cell[i] == DRONE_BANK_TRIG ? VIEW_INK_WHITE : VIEW_INK_GREY);
            else if (s->cell[i] == VIEW_BANK_PAD)
                view_canvas_rect(x + cell / 2 - 1, 11, 2, 2, VIEW_INK_GREY);
            else
                view_canvas_frame(x + 1, 4, cell - 3, 16, VIEW_INK_DIM);
            if (i == s->here)
                view_canvas_frame(x - 1, 2, cell + 1, 20, VIEW_INK_AMBER);
        }
    } else if (s->row == VIEW_ROW_LOOP && s->count) {
        int32_t total = s->wake + s->nap, cell = total && 224 / total < 24 ? 224 / total : 24;
        view_canvas_frame(x0, 2, 224, 10, VIEW_INK_DIM);
        for (i = 1; i < s->count; i++)          /* a mark for each beat, or cycle */
            view_canvas_rect(x0 + (i * 224 + s->count / 2) / s->count, 2, 1, 10, VIEW_INK_GREY);
        view_canvas_rect(x0 + s->place, 0, 2, 14, VIEW_INK_AMBER);
        for (i = 0; i < total && s->nap; i++) {
            if (i < s->wake)
                view_canvas_rect(x0 + i * cell + 1, 18, cell - 3, 8, VIEW_INK_WHITE);
            else
                view_canvas_frame(x0 + i * cell + 1, 18, cell - 3, 8, VIEW_INK_GREY);
            if (i == s->round)
                view_canvas_frame(x0 + i * cell - 1, 16, cell + 1, 12, VIEW_INK_AMBER);
        }
    }
    view_canvas_send(VIEW_BANK_ROW_X, VIEW_BANK_ROW_Y);
    if (!s->count)
        font_draw_text(8, VIEW_BANK_ROW_Y + 8u, 1, s->row == VIEW_ROW_PATTERN ? "STEPS OFF. EVERY STEP PLAYS" : "LOOP OFF. IT NEVER STARTS AGAIN", VIEW_GREY);
    else if (s->row == VIEW_ROW_LOOP && !s->nap)
        font_draw_text(8, VIEW_BANK_ROW_Y + 19u, 1, "NAP OFF", VIEW_GREY);
}
/* drawn again only when what it shows has changed; it is worked out only when what it is made of has changed */
static void view_bank_steps(const view_model_t *m, const view_model_t *shown, int all)
{
    static view_bank_steps_t drawn;
    view_bank_steps_t now;
    if (!all && view_bank_lfo(m) == view_bank_lfo(shown) && view_bank_set(m) == view_bank_set(shown)
        && view_same(m->bank_value, shown->bank_value, sizeof m->bank_value) && m->bank_step == shown->bank_step
        && m->bank_loop == shown->bank_loop && m->bank_loop_at == shown->bank_loop_at)
        return;
    now = view_bank_steps_of(m);
    if (!all && view_same(&now, &drawn, sizeof now))
        return;
    drawn = now;
    view_bank_steps_draw(&now);
}

static void view_bank(const view_model_t *m, const view_model_t *shown, int all)
{
    /* the row of the twelve LFOs: with the WAVE and the MORE layers, and while the LFO key is held */
    int strip = m->lfo_held || view_bank_set(m) == PANEL_BANK_WAVE || view_bank_set(m) == PANEL_BANK_MORE;
    int was_strip = shown->lfo_held || view_bank_set(shown) == PANEL_BANK_WAVE || view_bank_set(shown) == PANEL_BANK_MORE;
    view_bank_picture(m, shown, all);
    view_bank_value_line(m, shown, all);
    view_bank_dials(m, shown, all);
    if (!all && strip != was_strip)
        FONT_RECT(0, VIEW_BANK_ROW_Y, 240, VIEW_VOICES_Y - VIEW_BANK_ROW_Y, VIEW_BLACK);
    if (strip)
        view_bank_row(m, shown, VIEW_BANK_ROW_Y, all || strip != was_strip);
    else
        view_bank_steps(m, shown, all || strip != was_strip);
}

/* the bank on the play page. Under each dial, twelve marks: the LFOs with an amount on what its knob holds in the
 * layer in hand (TONE, SPEED, GRIT, LINK; in the second layer FREQ, GAIN and a THROUGH voice's OWN), the
 * current one's cyan while the LFO key is held. On a dial: the range the bank moves its value over, and its value
 * now, with what the bank adds */
static const char *const view_banked_name[PLAY_NBANKED] = {
    [PLAY_BANKED_FREQ] = "FREQ", [PLAY_BANKED_TONE] = "TONE", [PLAY_BANKED_STYLE] = "STYLE", [PLAY_BANKED_GRIT] = "GRIT", [PLAY_BANKED_LINK] = "LINK", [PLAY_BANKED_SPEED] = "SPEED",
    [PLAY_BANKED_GAIN] = "GAIN", [PLAY_BANKED_OWN] = "OWN",
};
static void view_level_text(char *to, uint32_t level)   /* the fader in dB: "0" at the top, "-12", "OFF" at the bottom */
{
    if (!level)
        view_put(to, "OFF");
    else
        view_put_number(view_put(to, mixer_level_db(level) ? "-" : ""), mixer_level_db(level));
}
static const char *view_play_name(int32_t knob) /* what panel_play_knob gave: a knob's name, FREQ, GAIN, or none */
{
    return knob < 0 ? "" : knob == PANEL_PLAY_FREQ ? "FREQ" : knob == PANEL_PLAY_GAIN ? "GAIN" : view_knob_name[knob];
}
static uint32_t view_play_gain(const view_model_t *m)   /* the selected voice's level: its channel's in the mixer */
{
    return m->mixer[m->selected % DRONE_VOICES][MIXER_LEVEL] & 127u;
}
static view_dial_t view_play_dial(const view_model_t *m, uint32_t n, int through)
{
    int32_t knob = panel_play_knob(PANEL_ENC_VALUES + n, m->layer, through), banked = panel_banked(PANEL_ENC_VALUES + n, m->layer, through);
    int32_t value = knob == PANEL_PLAY_FREQ ? (int32_t)m->freq * 256 : knob == PANEL_PLAY_GAIN ? (int32_t)view_play_gain(m) * 256
                  : knob < 0 ? 0 : (m->knob[knob] & 127) * 256;
    uint32_t top = knob == PANEL_PLAY_FREQ ? PLAY_FREQ_TOP : knob >= 0 && knob < PLAY_NKNOB ? play_knob_spec[knob].top : 127u;
    view_dial_t none = { VIEW_NO_DOT, 1, 0, VIEW_NO_DOT, VIEW_AMBER };
    view_moved_t moved;
    if (knob < 0)
        return none;
    if (banked < 0)                             /* it holds nothing */
        return view_dial_of(value, 0, top, VIEW_AMBER);
    /* the range the LFOs move it over; the LFO key held: in cyan, the range the current LFO alone moves it over.
     * A knob: as if it went its whole range each way (127). The FREQ: an octave each
     * way, in 1/256 of a semitone, and its dial is in 1/128 of one */
    moved = view_moved(m, m->bank_amount[banked], knob == PANEL_PLAY_FREQ ? drone_banked_reach[DRONE_BANKED_PITCH] : (int32_t)top,
                       m->lfo_held ? view_bank_lfo(m) : DRONE_BANK_LFOS);
    if (knob == PANEL_PLAY_FREQ) {
        moved.low *= 128;
        moved.high *= 128;
        moved.now *= 128;
    }
    return view_dial_of(value, &moved, top, m->lfo_held ? VIEW_CYAN : VIEW_AMBER);
}
/* the LFO key held: the value line is the knob's name and the current LFO's amount on it, "TONE +38" */
static void view_amount_line_text(char *to, const view_model_t *m)
{
    int32_t banked = panel_banked(m->amount_touched, m->layer, m->link_kind[m->selected % DRONE_VOICES] == DRONE_LINK_THROUGH);
    *to = 0;
    if (banked >= 0)
        view_amount_text(to, view_banked_name[banked], drone_limited(m->bank_amount[banked][view_bank_lfo(m)], DRONE_AMOUNT_TOP));
}
/* the value line: the name and the value of the knob turned last, "TONE 64"; the FREQ in hertz, "FREQ 92.5"; the
 * GAIN as the mixer has the level, in dB, "GAIN -12"; the STYLE that ALGORITHM is on, its name alone, "GROWL": the
 * line has no room for the two. The STYLE has no dial: this line alone shows where ALGORITHM is and, with the LFO
 * key held, an LFO's amount onto it; the top line has the style the voice sounds in now */
static void view_play_value_text(char *to, const view_model_t *m, int32_t touched)
{
    char *value = view_put(view_put(to, view_play_name(touched)), " ");
    if (touched == PLAY_STYLE) {
        view_put(to, style_list[m->knob[PLAY_STYLE] < STYLE_NSTYLE ? m->knob[PLAY_STYLE] : STYLE_NSTYLE - 1u].name);
    } else if (touched == PANEL_PLAY_FREQ) {
        view_freq_text(value, m->freq);         /* "92.5 HZ": the line has no room for the unit */
        while (*value && *value != ' ')
            value++;
        *value = 0;
    } else if (touched == PANEL_PLAY_GAIN) {
        view_level_text(value, view_play_gain(m));
    } else if (touched >= 0) {
        view_put_number(value, m->knob[touched] & 127u);
    } else {
        *to = 0;
    }
}

static void view_play(const view_model_t *m, const view_model_t *shown, int all)
{
    int through = m->layer && m->link_kind[m->selected % DRONE_VOICES] == DRONE_LINK_THROUGH;      /* OWN is there: the dials change with it */
    int was_through = shown->layer && shown->link_kind[shown->selected % DRONE_VOICES] == DRONE_LINK_THROUGH;
    int32_t touched = panel_play_knob(m->touched, m->layer, through);
    int value_new = touched == PANEL_PLAY_FREQ ? m->freq != shown->freq : touched == PANEL_PLAY_GAIN ? view_play_gain(m) != view_play_gain(shown)
                  : touched >= 0 && m->knob[touched] != shown->knob[touched];
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
    if (all || (m->layer != 0) != (shown->layer != 0))
        view_layers(PANEL_PLAY_LAYERS, m->layer != 0);
    for (n = 0; n < 4u; n++) {
        int32_t knob = panel_play_knob(PANEL_ENC_VALUES + n, m->layer, through), banked = panel_banked(PANEL_ENC_VALUES + n, m->layer, through);
        view_dial_t now = view_play_dial(m, n, through), before = view_play_dial(shown, n, was_through);
        const char *name = view_play_name(knob);
        /* the LFO key held: the knob the amount turned last is on */
        int bright = held ? banked >= 0 && m->amount_touched == PANEL_ENC_VALUES + n : knob >= 0 && knob == touched;
        if (amounts_new || all || m->layer != shown->layer)
            view_marks(n, banked >= 0 ? m->bank_amount[banked] : 0, m->lfo_held ? view_bank_lfo(m) : DRONE_BANK_LFOS);
        if (!all && !held_new && m->layer == shown->layer && m->touched == shown->touched && through == was_through
            && m->amount_touched == shown->amount_touched && view_same(&now, &before, sizeof now))
            continue;
        view_dial(VIEW_DIAL_X(n), &now);
        FONT_RECT(VIEW_DIAL_X(n) - 26u, VIEW_DIAL_NAME_Y, 52, 7, VIEW_BLACK);
        font_draw_text(VIEW_DIAL_X(n) - font_text_width(name, 1) / 2u, VIEW_DIAL_NAME_Y, 1, name, bright ? VIEW_WHITE : VIEW_GREY);
    }
    if (all || held_new || m->layer != shown->layer || m->touched != shown->touched || through != was_through
        || value_new || (held && (amounts_new || m->amount_touched != shown->amount_touched))) {
        char text[16] = "";
        if (held)
            view_amount_line_text(text, m);
        else
            view_play_value_text(text, m, touched);
        view_text(8, VIEW_DIAL_VALUE_Y, 120, 2, text, VIEW_WHITE, 0);
    }
    if (all || m->selected != shown->selected || !view_same(m->link_kind, shown->link_kind, sizeof m->link_kind))
        view_text(136, VIEW_DIAL_VALUE_Y, 96, 2, panel_link_name[m->link_kind[m->selected % DRONE_VOICES] < DRONE_NLINK
                                                                 ? m->link_kind[m->selected % DRONE_VOICES] : DRONE_LINK_FOLLOW], VIEW_GREY, 1);
    if (held) {                                 /* the row of the twelve LFOs in the links' place */
        if (held_new)
            FONT_RECT(0, VIEW_LINKS_Y, 240, VIEW_VOICES_Y - VIEW_LINKS_Y, VIEW_BLACK);
        view_bank_row(m, shown, VIEW_LINKS_Y, held_new);
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
        {
            view_dial_t dial = view_dial_of((int32_t)now * 256, 0, spec->top, VIEW_AMBER);
            view_dial(VIEW_DIAL_X(n), &dial);
        }
        FONT_RECT(VIEW_DIAL_X(n) - 26u, VIEW_DIAL_NAME_Y, 52, 7, VIEW_BLACK);
        font_draw_text(VIEW_DIAL_X(n) - font_text_width(knob, 1) / 2u, VIEW_DIAL_NAME_Y, 1, knob, n == touched ? VIEW_WHITE : VIEW_GREY);
    }
    if (values_new || sends_new || m->touched != shown->touched) {
        char text[24] = "";
        if (touched < DRONE_DIS_NPARAM)
            view_dis_text(view_put(view_put(text, panel_dis_knob_name[dis][touched]), " "), dis, touched, value[touched]);
        else if (touched == DRONE_DIS_NPARAM)
            view_put_number(view_put(view_put(text, to_voice), " "), send[selected] & 127u);
        view_text(8, VIEW_DIAL_VALUE_Y, 136, 2, text, VIEW_WHITE, 0);
    }
    if (all || m->selected != shown->selected || !view_same(to, shown->dis_to[dis], sizeof m->dis_to[dis]))
        view_text(148, VIEW_DIAL_VALUE_Y, 84, 2, panel_to_name(dis, to[selected]), VIEW_GREY, 1);
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
                                           || view_bank_own(m, DRONE_BANK_SHAPE) != view_bank_own(shown, DRONE_BANK_SHAPE)))
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
        } else if (bank && m->lfo_held) {       /* the LFO key held: from the source LFO to the shown one */
            view_bank_path(m);
        } else if (bank) {
            view_bank_top(m);
        } else if (play && m->lfo_held) {       /* the LFO key held: the current LFO, and its wave */
            char lfo[8];
            const char *wave = view_bank_shape_name[view_bank_own(m, DRONE_BANK_SHAPE)];
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

    if (bank && !m->info && !m->lfo_held && (top_new || view_bank_beat_lit(m) != view_bank_beat_lit(shown)))
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
