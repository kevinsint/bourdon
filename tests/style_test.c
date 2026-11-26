/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kevin Sint */
/* The styles: each one is a patch the engine plays; the frequency is free; the knobs move the patch and keep every
 * value in its range. */
#include <stdio.h>
#include "style.h"

static int failed;
static void check(const char *what, int ok)
{
    if (!ok) {
        printf("FAIL: %s\n", what);
        failed = 1;
    }
}

static play_voice_t plain(uint32_t style)
{
    play_voice_t play = { .freq = 42u * 128u };
    uint32_t k;
    for (k = 0; k < PLAY_NKNOB; k++)
        play.knob[k] = play_knob_spec[k].start;
    play.knob[PLAY_STYLE] = (uint8_t)style;
    return play;
}

static int in_range(const drone_voice_controls_t *patch)     /* every value the engine reads is one it knows */
{
    uint32_t s, p, l, ok = 1;
    for (s = 0; s < DRONE_SLOTS; s++) {
        uint32_t type = patch->block[s].type;
        ok &= type < DRONE_NBLOCK && patch->block[s].kind <= drone_block_kind_spec[type].top;
        for (p = 0; p < DRONE_BLOCK_NPARAM && type < DRONE_NBLOCK; p++)
            ok &= patch->block[s].param[p] <= drone_block_spec[type][p].top;
    }
    for (l = 0; l < DRONE_LFOS; l++)
        for (p = 0; p < DRONE_LFO_NPARAM; p++)
            ok &= patch->lfo[l][p] <= drone_lfo_spec[p].top;
    for (p = 0; p < DRONE_FB_NPARAM; p++)
        ok &= patch->fb[p] <= drone_fb_spec[p].top;
    for (p = 0; p < DRONE_NPARAM; p++)
        ok &= patch->param[p] <= drone_param_spec[p].top;
    return ok && patch->send <= drone_send_spec.top && patch->link <= drone_link_spec.top && patch->link_kind <= drone_link_kind_spec.top;
}

static drone_t engine;
static drone_controls_t controls;
static int32_t out[2u * DRONE_BLOCK_FRAMES];

static uint32_t loudest(const drone_voice_controls_t *patch)    /* the engine plays it alone for half a second */
{
    uint32_t i, block, peak = 0;
    drone_controls_t fresh = { .master = DRONE_MASTER_MAX };
    controls = fresh;
    for (i = 0; i < DRONE_REVERB_NPARAM; i++)
        controls.reverb[i] = drone_reverb_spec[i].start;
    for (i = 0; i < DRONE_DELAY_NPARAM; i++)
        controls.delay[i] = drone_delay_spec[i].start;
    for (i = 0; i < DRONE_COMP_NPARAM; i++)
        controls.comp[i] = drone_comp_spec[i].start;
    controls.voice[0] = *patch;
    controls.voice[0].on = 1;
    controls.voice[0].param[DRONE_LEVEL] = 32;  /* the mixer's, at its start */
    drone_init(&engine);
    for (block = 0; block < 86u; block++) {
        drone_render(&engine, &controls, out, DRONE_BLOCK_FRAMES);
        for (i = 0; i < 2u * DRONE_BLOCK_FRAMES; i++) {
            uint32_t size = (uint32_t)(out[i] < 0 ? -out[i] : out[i]);
            peak = size > peak ? size : peak;
        }
    }
    return peak;
}

int main(void)
{
    uint32_t style, k, s;
    drone_voice_controls_t patch, other;
    play_voice_t play;

    check("six styles, each with a name the screen has room for", STYLE_NSTYLE == 6);
    for (style = 0; style < STYLE_NSTYLE; style++) {
        uint32_t letters = 0, fx = 0;
        while (style_list[style].name[letters])
            letters++;
        play = plain(style);
        style_patch(&play, &patch);
        check("a style's name: one to five letters", letters >= 1 && letters <= 5);
        check("a style starts with an oscillator", patch.block[0].type == DRONE_BLOCK_VCO);
        check("the patch is off: the voice's key turns it on", !patch.on);
        check("with the knobs at their start, every value is in its range", in_range(&patch));
        for (s = 0; s < DRONE_SLOTS; s++)
            fx |= patch.block[s].type == DRONE_BLOCK_FX;
        check("no style takes the one reverb from the end of the chain", !fx);
        check("the engine plays it: there is sound", loudest(&patch) > 1000u);

        for (k = 0; k < PLAY_NKNOB; k++) {      /* each knob at its two ends, and all of them together */
            play = plain(style);
            play.knob[k] = 0;
            style_patch(&play, &patch);
            check("a knob at 0: every value in its range", in_range(&patch));
            play.knob[k] = play_knob_spec[k].top;
            style_patch(&play, &patch);
            check("a knob at its top: every value in its range", in_range(&patch));
        }
        for (k = 0; k < PLAY_NKNOB; k++)
            play.knob[k] = play_knob_spec[k].top;
        play.freq = PLAY_FREQ_TOP;
        style_patch(&play, &patch);
        check("all the knobs and the frequency at the top: in range", in_range(&patch));
        for (k = 0; k < PLAY_NKNOB; k++)
            play.knob[k] = 0;
        play.freq = 0;
        style_patch(&play, &patch);
        check("all at 0: in range", in_range(&patch));

        play = plain(style);                    /* TONE is brighter on every style: its first filter opens */
        style_patch(&play, &patch);
        play.knob[PLAY_TONE] = 127;
        style_patch(&play, &other);
        for (s = 0; s < DRONE_SLOTS && patch.block[s].type != DRONE_BLOCK_VCF; s++)
            ;
        check("a style has a filter, and TONE opens it", s < DRONE_SLOTS
              && other.block[s].param[DRONE_VCF_CUTOFF] > patch.block[s].param[DRONE_VCF_CUTOFF]);
    }

    /* the frequency is free: 1/128 of a semitone, the first oscillator's pitch and fine tuning together */
    play = plain(STYLE_FLOOR);
    play.freq = 42u * 128u;
    style_patch(&play, &patch);
    check("frequency 42 * 128: the first oscillator at pitch 42, in tune",
          patch.block[0].param[DRONE_VCO_PITCH] == 42 && patch.block[0].param[DRONE_VCO_FINE] == 64);
    play.freq = 42u * 128u + 1u;
    style_patch(&play, &other);
    check("one step more: 1/128 of a semitone up, not a note",
          other.block[0].param[DRONE_VCO_PITCH] == 42 && other.block[0].param[DRONE_VCO_FINE] == 65);
    play.freq = 42u * 128u + 100u;
    style_patch(&play, &other);
    check("the fine tuning goes on into the next pitch", other.block[0].param[DRONE_VCO_PITCH] == 43 && other.block[0].param[DRONE_VCO_FINE] == 36);
    check("FLOOR has a second oscillator a little above the first", patch.block[1].type == DRONE_BLOCK_VCO
          && patch.block[1].param[DRONE_VCO_PITCH] * 128 + patch.block[1].param[DRONE_VCO_FINE]
             > patch.block[0].param[DRONE_VCO_PITCH] * 128 + patch.block[0].param[DRONE_VCO_FINE]);
    check("and it stays that far above at another frequency",
          other.block[1].param[DRONE_VCO_PITCH] * 128 + other.block[1].param[DRONE_VCO_FINE]
          - (other.block[0].param[DRONE_VCO_PITCH] * 128 + other.block[0].param[DRONE_VCO_FINE])
          == patch.block[1].param[DRONE_VCO_PITCH] * 128 + patch.block[1].param[DRONE_VCO_FINE]
             - (patch.block[0].param[DRONE_VCO_PITCH] * 128 + patch.block[0].param[DRONE_VCO_FINE]));
    check("the frequency in tenths of a hertz: pitch 42 is 92.5 Hz, pitch 69 is 440 Hz",
          style_freq_hz10(42u * 128u) == 925 && style_freq_hz10(69u * 128u) == 4400);
    check("half a semitone above 440 Hz: about 453 Hz, as the engine goes between two pitches",
          style_freq_hz10(69u * 128u + 64u) >= 4528 && style_freq_hz10(69u * 128u + 64u) <= 4532);

    play = plain(STYLE_REED);
    style_patch(&play, &patch);
    check("a style leaves the voice's level, pan and sends to the mixer", patch.param[DRONE_LEVEL] == 0 && patch.send == 0
          && patch.send_reverb == 0 && patch.pan == 0 && !patch.post && PLAY_NKNOB == 6);
    check("LINK starts closed: a voice is alone until it is turned", play_knob_spec[PLAY_LINK].start == 0 && patch.link == 0);
    play.knob[PLAY_LINK] = 90;
    style_patch(&play, &patch);
    check("LINK is how much the voice takes from the voice before it", patch.link == 90 && patch.link_kind == DRONE_LINK_FOLLOW);
    play.link_kind = DRONE_LINK_THROUGH;
    style_patch(&play, &patch);
    check("the link's kind goes to the engine", patch.link_kind == DRONE_LINK_THROUGH);
    play.link_kind = 99;
    style_patch(&play, &patch);
    check("a kind that does not exist: the first one", patch.link_kind == DRONE_LINK_FOLLOW);
    play.link_kind = DRONE_LINK_FM;
    style_patch(&play, &patch);
    check("the third kind, FM, goes to the engine", patch.link_kind == DRONE_LINK_FM);
    check("OWN starts at the top: a voice has all of its own oscillators", play_knob_spec[PLAY_OWN].start == 127 && patch.own_less == 0);
    play.knob[PLAY_OWN] = 0;
    style_patch(&play, &patch);
    check("OWN at 0: the engine takes all of them away", patch.own_less == 127);
    play.knob[PLAY_OWN] = 100;
    style_patch(&play, &patch);
    check("OWN between: that much is left", patch.own_less == 27);

    /* STYLE is a knob: the patch is that style's. The style's LFOs go as deep as the style has them; SPEED: their speed */
    check("STYLE is a choice: a style a click, all the styles, FLOOR at power-on", play_knob_spec[PLAY_STYLE].top == STYLE_NSTYLE - 1
          && play_knob_spec[PLAY_STYLE].per_click == 1 && play_knob_spec[PLAY_STYLE].start == STYLE_FLOOR);
    play = plain(STYLE_REED);
    style_patch(&play, &other);
    check("the STYLE knob on REED: the patch is REED's, a square into the MS-20", style_now(&play) == STYLE_REED
          && other.block[0].kind == DRONE_SQUARE && other.block[1].type == DRONE_BLOCK_VCF && other.block[1].kind == DRONE_MS20);
    play = plain(STYLE_FLOOR);
    style_patch(&play, &patch);
    check("the style's LFOs go as deep as the style has them", patch.lfo[0][DRONE_LFO_DEPTH] == style_list[STYLE_FLOOR].patch.lfo[0][DRONE_LFO_DEPTH]
          && patch.lfo[1][DRONE_LFO_DEPTH] == style_list[STYLE_FLOOR].patch.lfo[1][DRONE_LFO_DEPTH] && patch.lfo[0][DRONE_LFO_DEPTH] > 0);
    {   /* the bank's LFOs move the STYLE: at 100, an LFO at its top goes the whole list; to the nearest style */
        int16_t lfo_out[DRONE_BANK_LFOS] = { 0 };
        play = plain(STYLE_REED);
        lfo_out[4] = 32767;
        check("no amount: the bank does not push the style", style_pushed(&play, lfo_out) == 0);
        play.bank_amount[PLAY_BANKED_STYLE][4] = 100;
        check("an amount of 100 and the LFO at its top: the whole list up", style_pushed(&play, lfo_out) == STYLE_NSTYLE - 1);
        lfo_out[4] = -32767;
        check("at its bottom: the whole list down", style_pushed(&play, lfo_out) == -(STYLE_NSTYLE - 1));
        lfo_out[4] = 32767 / 5 + 10;
        check("a fifth of the way up: one style", style_pushed(&play, lfo_out) == 1);
        lfo_out[4] = 32767 / 10 - 300;
        check("less than half a style: none", style_pushed(&play, lfo_out) == 0);
        lfo_out[4] = 32767 / 10 + 300;
        check("more than half a style: to the nearest, one", style_pushed(&play, lfo_out) == 1);
        play.bank_amount[PLAY_BANKED_STYLE][4] = -40;
        lfo_out[4] = 32767;
        check("an amount below 0: the LFO upside down, -40 is two styles down", style_pushed(&play, lfo_out) == -2);
        play.bank_amount[PLAY_BANKED_STYLE][4] = 100;
        play.bank_amount[PLAY_BANKED_STYLE][9] = 100;
        lfo_out[4] = 32767 / 5;
        lfo_out[9] = 2 * 32767 / 5;
        check("two LFOs add up: one style and two styles are three", style_pushed(&play, lfo_out) == 3);
        play.style_push = 2;
        style_patch(&play, &other);
        check("REED pushed two styles up is GROWL: the patch is GROWL's, the knob stays on REED", style_now(&play) == STYLE_GROWL
              && other.block[1].kind == DRONE_POLIVOKS && play.knob[PLAY_STYLE] == STYLE_REED);
        play.style_push = STYLE_NSTYLE - 1;
        check("a push past the last style stops at it", style_now(&play) == STYLE_NSTYLE - 1);
        play.style_push = -(int8_t)(STYLE_NSTYLE - 1);
        check("and below the first style, at the first", style_now(&play) == STYLE_FLOOR);
    }
    play = plain(STYLE_FLOOR);
    play.knob[PLAY_SPEED] = 127;
    style_patch(&play, &other);
    check("SPEED at the top: the LFOs are faster, their wave the same", other.lfo[0][DRONE_LFO_SPEED] > patch.lfo[0][DRONE_LFO_SPEED]
          && other.lfo[0][DRONE_LFO_WAVE] == patch.lfo[0][DRONE_LFO_WAVE]);

    /* GRIT on a style with a feedback cable: its level */
    play = plain(STYLE_GROWL);
    style_patch(&play, &patch);
    play.knob[PLAY_GRIT] = 127;
    style_patch(&play, &other);
    check("GROWL has a feedback cable, and GRIT makes it louder", patch.fb[DRONE_FB_LEVEL] > 0
          && other.fb[DRONE_FB_LEVEL] > patch.fb[DRONE_FB_LEVEL]);

    /* the bank's cables onto a played voice: FREQ, TONE, GRIT and LINK in the engine; STYLE is not the engine's */
    play = plain(STYLE_GLASS);
    style_patch(&play, &patch);
    for (s = 0, k = 0; s < DRONE_NBANKED * DRONE_BANK_LFOS; s++)
        k |= (uint32_t)patch.bank_amount[s / DRONE_BANK_LFOS][s % DRONE_BANK_LFOS];
    check("no amount on the play page: none in the patch (it sounds as it did)", k == 0 && PLAY_NBANKED == 5);
    play.bank_amount[PLAY_BANKED_FREQ][3] = 40;
    play.bank_amount[PLAY_BANKED_TONE][5] = -30;
    play.bank_amount[PLAY_BANKED_STYLE][0] = 100;
    play.bank_amount[PLAY_BANKED_GRIT][11] = -100;
    play.bank_amount[PLAY_BANKED_GRIT][2] = 20;
    play.bank_amount[PLAY_BANKED_LINK][7] = 55;
    style_patch(&play, &patch);
    check("FREQ and LINK: the voice's pitch and its link",
          patch.bank_amount[DRONE_BANKED_PITCH][3] == 40 && patch.bank_amount[DRONE_BANKED_LINK][7] == 55 && patch.bank_amount[DRONE_BANKED_PITCH][5] == 0);
    for (s = 0, k = 0; s < DRONE_BANK_LFOS; s++)
        k |= (uint32_t)patch.bank_amount[DRONE_BANKED_LFO_DEPTH][s];
    check("the amount onto STYLE is no cable of the patch: the main loop moves the style", k == 0);
    for (s = 0, k = 1; s < STYLE_ROUTES; s++) {    /* GLASS: TONE to the filter, GRIT to the feedback and the resonance */
        const style_route_t *route = &style_list[STYLE_GLASS].route[s];
        const int8_t *amounts = patch.bank_amount[DRONE_BANKED_ROUTE + s];
        k &= patch.bank_route[s].block == route->block && patch.bank_route[s].param == route->param
             && patch.bank_route[s].reach == route->reach * 127 / 64;
        k &= route->knob == PLAY_TONE ? amounts[5] == -30 && amounts[11] == 0 : amounts[11] == -100 && amounts[2] == 20 && amounts[5] == 0;
    }
    check("TONE and GRIT: each route of the knob, its whole range (127 steps of the knob) each way", k && STYLE_TO_FB == DRONE_SLOTS
          && DRONE_BANK_ROUTES == STYLE_ROUTES);
    play = plain(STYLE_FLOOR);                  /* in the engine: TONE at 0, an LFO always up on it, 100 */
    play.knob[PLAY_TONE] = 0;
    play.bank_amount[PLAY_BANKED_TONE][DRONE_BANK_LFOS - 1u] = 100;
    style_patch(&play, &patch);
    for (s = 0; s < DRONE_LFOS; s++)
        patch.lfo[s][DRONE_LFO_DEPTH] = 0;      /* the style's own LFOs still */
    {
        drone_controls_t fresh = { .master = DRONE_MASTER_MAX, .tempo = DRONE_TEMPO_START };
        uint32_t l, p;
        int32_t want = drone_between(drone_pole_q14, 30 * 256 + ((32767 * (40 * 127 / 64)) >> 7)), got;
        controls = fresh;
        for (l = 0; l < DRONE_BANK_LFOS; l++)
            for (p = 0; p < DRONE_BANK_NPARAM; p++)
                controls.bank[l][p] = drone_bank_spec[p].start;
        controls.bank[DRONE_BANK_LFOS - 1u][DRONE_BANK_LEVEL] = 0;      /* always up: no LEVEL, all the OFFSET up */
        controls.bank[DRONE_BANK_LFOS - 1u][DRONE_BANK_OFFSET] = 200;
        controls.voice[0] = patch;
        controls.voice[0].on = 1;
        drone_init(&engine);
        for (l = 0; l < 4u; l++)
            drone_render(&engine, &controls, out, DRONE_BLOCK_FRAMES);
        got = engine.voice[0].block[2].cutoff.now >> 15;
        check("FLOOR, TONE at 0 and the bank all up on it: its filter 79 steps above the knob's 30", got >= want - 1 && got <= want + 1);
    }

    play = plain(200);
    style_patch(&play, &patch);
    check("a style number that does not exist: the last style, in range", in_range(&patch) && patch.block[0].type == DRONE_BLOCK_VCO
          && style_now(&play) == STYLE_NSTYLE - 1);

    printf(failed ? "style_test: FAILED\n" : "style_test: ok\n");
    return failed;
}
