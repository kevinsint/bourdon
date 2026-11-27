/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kevin Sint */
/* The panel: the LFO key opens the bank's page and goes back; the SELECT encoder chooses a block while the shift key
 * is held, and the choice is the place's only when it is let go; a patch's chain, its LFOs and its feedback on the
 * encoders; the bank's LFOs, and their amounts on the play page. */
#include <stdio.h>
#include <string.h>
#include "panel.h"

static int failed;
static void check(const char *what, int ok)
{
    if (!ok) {
        printf("FAIL: %s\n", what);
        failed = 1;
    }
}

int main(void)
{
    panel_t panel = { 0 };
    uint32_t block = DRONE_BLOCK_VCO, i;
    drone_block_controls_t chain[DRONE_SLOTS] = { { 0 } };

    check("it starts on the voice's page", panel.page == PANEL_VOICE && PANEL_NPATCH == 2 + DRONE_LFOS && PANEL_NPAGE == PANEL_NPATCH + 9);
    for (i = 1; i <= DRONE_LFOS; i++)           /* the patch's pages have no key: they wait for the EDIT key */
        check("a patch's LFO page for each of its LFOs", panel_page_lfo(i) == (int32_t)i - 1);
    panel.page = PANEL_FEEDBACK;
    check("after the last LFO: the feedback's page", panel_page_lfo(PANEL_FEEDBACK) < 0 && panel_page_lfo(PANEL_VOICE) < 0 && !panel_in_eoc(&panel));
    panel_glo_key(&panel);                      /* the GLO key: the mixer, then the end of the chain, its delay first */
    check("the GLO key: the mixer", panel.page == PANEL_MIXER && !panel_in_eoc(&panel));
    panel_eoc_select(&panel, 1);
    check("the end of the chain's SELECT does nothing on the mixer", panel.page == PANEL_MIXER);
    panel_glo_key(&panel);
    check("the GLO key again: the end of the chain, on the delay", panel.page == PANEL_DELAY && panel_in_eoc(&panel) && panel_page_lfo(PANEL_DELAY) < 0);
    panel_eoc_select(&panel, 1);
    check("SELECT: the reverb", panel.page == PANEL_REVERB);
    panel_eoc_select(&panel, 5);
    check("then the compressor, and it stops there", panel.page == PANEL_COMP);
    panel_eoc_select(&panel, -1);
    panel_glo_key(&panel);
    check("the GLO key a third time: the play page", panel.page == PANEL_PLAY);
    panel_eoc_select(&panel, 1);
    check("SELECT is not the end of the chain's there", panel.page == PANEL_PLAY);
    panel_glo_key(&panel);
    panel_glo_key(&panel);
    check("round again: the effect that was open last", panel.page == PANEL_REVERB);
    panel_eoc_select(&panel, -9);
    check("SELECT stops at the delay", panel.page == PANEL_DELAY);
    panel_glo_key(&panel);

    check("the delay's page: its four on the four right encoders",
          panel_eoc_spec(PANEL_DELAY, PANEL_ENC_VALUES) == &drone_delay_spec[DRONE_DELAY_TIME]
          && panel_eoc_spec(PANEL_DELAY, PANEL_ENC_VALUES + 3) == &drone_delay_spec[DRONE_DELAY_LEVEL]);
    check("the reverb's page: the voice's send, then the reverb's three",
          !panel_eoc_spec(PANEL_REVERB, PANEL_ENC_VALUES) && panel_eoc_spec(PANEL_REVERB, PANEL_ENC_VALUES + 1) == &drone_reverb_spec[DRONE_REVERB_DECAY]
          && panel_eoc_spec(PANEL_REVERB, PANEL_ENC_VALUES + 3) == &drone_reverb_spec[DRONE_REVERB_RETURN]);
    check("the compressor's page: its four",
          panel_eoc_spec(PANEL_COMP, PANEL_ENC_VALUES) == &drone_comp_spec[DRONE_COMP_THRESHOLD]
          && panel_eoc_spec(PANEL_COMP, PANEL_ENC_VALUES + 3) == &drone_comp_spec[DRONE_COMP_GAIN]);
    check("no value on the other encoders, or on a patch's page",
          !panel_eoc_spec(PANEL_DELAY, 0) && !panel_eoc_spec(PANEL_COMP, PANEL_ENC_KIND) && !panel_eoc_spec(PANEL_REVERB, PANEL_ENC_MOVE)
          && !panel_eoc_spec(PANEL_VOICE, PANEL_ENC_VALUES) && !panel_eoc_spec(PANEL_FEEDBACK, PANEL_ENC_VALUES));

    block = panel_choice(&panel, 0, 3, &drone_block_type_spec, block);
    check("the encoder without shift does not choose a block", block == DRONE_BLOCK_VCO && !panel.choosing);
    block = panel_choice(&panel, 1, 0, &drone_block_type_spec, block);
    check("shift down: the choice starts at the place's block", panel.choosing && panel.choice == DRONE_BLOCK_VCO);
    block = panel_choice(&panel, 1, 2, &drone_block_type_spec, block);
    check("two clicks: the choice is two further, the place's block is not yet", panel.choice == DRONE_BLOCK_VCA && block == DRONE_BLOCK_VCO);
    block = panel_choice(&panel, 1, -1, &drone_block_type_spec, block);
    check("a click back", panel.choice == DRONE_BLOCK_VCF && block == DRONE_BLOCK_VCO);
    block = panel_choice(&panel, 0, 0, &drone_block_type_spec, block);
    check("shift let go: the place has the choice", block == DRONE_BLOCK_VCF && !panel.choosing);
    block = panel_choice(&panel, 0, 0, &drone_block_type_spec, block);
    check("and keeps it", block == DRONE_BLOCK_VCF);

    block = panel_choice(&panel, 1, 0, &drone_block_type_spec, block);
    block = panel_choice(&panel, 0, 0, &drone_block_type_spec, block);
    check("shift down and up with no turn changes nothing", block == DRONE_BLOCK_VCF);

    block = panel_choice(&panel, 1, 100, &drone_block_type_spec, block);
    check("the choice stops at the last block", panel.choice == DRONE_NBLOCK - 1);
    block = panel_choice(&panel, 1, -100, &drone_block_type_spec, block);
    check("and at none", panel.choice == DRONE_BLOCK_NONE);
    block = panel_choice(&panel, 0, 5, &drone_block_type_spec, block);
    check("clicks in the turn shift is let go do not count", block == DRONE_BLOCK_NONE);

    /* ---- a patch ---- */
    check("an empty chain: the empty place, then OUT",
          panel_patch_count(chain) == 0 && panel_patch_places(0) == 2 && panel_patch_at(0, 0) == PANEL_AT_NEW && panel_patch_at(0, 1) == PANEL_AT_OUT);
    panel_patch_put(chain, 0, DRONE_BLOCK_VCO);
    check("a block put in the empty place: one block, with its values at their start",
          panel_patch_count(chain) == 1 && chain[0].type == DRONE_BLOCK_VCO && chain[0].kind == DRONE_SAW
          && chain[0].param[DRONE_VCO_PITCH] == 42 && chain[0].param[DRONE_VCO_FINE] == 64);
    check("then: the block, the empty place, OUT",
          panel_patch_places(1) == 3 && panel_patch_at(1, 0) == PANEL_AT_BLOCK && panel_patch_at(1, 1) == PANEL_AT_NEW && panel_patch_at(1, 2) == PANEL_AT_OUT);
    panel_patch_put(chain, 1, DRONE_BLOCK_VCF);
    panel_patch_put(chain, 2, DRONE_BLOCK_VCA);
    check("two more: oscillator, filter, level", panel_patch_count(chain) == 3 && chain[1].type == DRONE_BLOCK_VCF && chain[2].type == DRONE_BLOCK_VCA
          && chain[2].param[DRONE_VCA_LEVEL] == 127);
    panel_patch_put(chain, 4, DRONE_BLOCK_VCA);
    check("a place after the empty one gets no block", panel_patch_count(chain) == 3);
    chain[0].param[DRONE_VCO_PITCH] = 50;
    check("a block moved to the right keeps its values, and the place it has is given", panel_patch_move(chain, 0, 1) == 1
          && chain[0].type == DRONE_BLOCK_VCF && chain[1].type == DRONE_BLOCK_VCO && chain[1].param[DRONE_VCO_PITCH] == 50);
    check("and back to the left", panel_patch_move(chain, 1, -1) == 0 && chain[0].type == DRONE_BLOCK_VCO);
    check("the first block does not move further left, the last not further right",
          panel_patch_move(chain, 0, -1) == 0 && chain[0].type == DRONE_BLOCK_VCO && panel_patch_move(chain, 2, 1) == 2 && chain[2].type == DRONE_BLOCK_VCA);
    panel_patch_put(chain, 1, DRONE_BLOCK_VCA);
    check("a block's place can get another block", panel_patch_count(chain) == 3 && chain[1].type == DRONE_BLOCK_VCA);
    panel_patch_put(chain, 0, DRONE_BLOCK_NONE);
    check("a block taken away: those after it move down", panel_patch_count(chain) == 2 && chain[0].type == DRONE_BLOCK_VCA
          && chain[1].type == DRONE_BLOCK_VCA && chain[2].type == DRONE_BLOCK_NONE);
    for (i = 2; i < DRONE_SLOTS; i++)
        panel_patch_put(chain, i, DRONE_BLOCK_VCO);
    check("a full chain: six blocks, then OUT, no empty place", panel_patch_count(chain) == DRONE_SLOTS
          && panel_patch_places(DRONE_SLOTS) == DRONE_SLOTS + 1 && panel_patch_at(DRONE_SLOTS, DRONE_SLOTS) == PANEL_AT_OUT);
    panel_patch_put(chain, DRONE_SLOTS - 1, DRONE_BLOCK_NONE);
    check("the last block of a full chain taken away", panel_patch_count(chain) == DRONE_SLOTS - 1);

    chain[0] = panel_block_new(DRONE_BLOCK_VCO);
    chain[0].amount[DRONE_VCO_PITCH][1] = 40;
    panel_patch_put(chain, 1, DRONE_BLOCK_VCF);
    check("a block's cables go with it when it moves", panel_patch_move(chain, 0, 1) == 1 && chain[1].amount[DRONE_VCO_PITCH][1] == 40
          && chain[0].amount[DRONE_VCO_PITCH][1] == 0);
    panel_patch_put(chain, 1, DRONE_BLOCK_VCA);
    check("a new block has no cable", chain[1].amount[DRONE_VCO_PITCH][1] == 0);

    check("on an oscillator: its wave on ALGORITHM, its pitch on the first right encoder, and no more",
          PANEL_ENC_KIND == 1 && PANEL_ENC_VALUES == 2 && PANEL_ENC_MOVE == 6 && panel_patch_spec(PANEL_AT_BLOCK, DRONE_BLOCK_VCO, PANEL_ENC_KIND, 0) == &drone_block_kind_spec[DRONE_BLOCK_VCO]
          && panel_patch_spec(PANEL_AT_BLOCK, DRONE_BLOCK_VCO, PANEL_ENC_VALUES, 0) == &drone_block_spec[DRONE_BLOCK_VCO][DRONE_VCO_PITCH]
          && !panel_patch_spec(PANEL_AT_BLOCK, DRONE_BLOCK_VCO, PANEL_ENC_VALUES + 1, 0) && !panel_patch_spec(PANEL_AT_BLOCK, DRONE_BLOCK_VCO, 0, 0)
          && !panel_patch_spec(PANEL_AT_BLOCK, DRONE_BLOCK_VCO, PANEL_ENC_MOVE, 0));
    check("with shift: the first right encoder is its fine tuning",
          panel_patch_value(PANEL_AT_BLOCK, DRONE_BLOCK_VCO, PANEL_ENC_VALUES, 1) == DRONE_VCO_FINE
          && panel_patch_spec(PANEL_AT_BLOCK, DRONE_BLOCK_VCO, PANEL_ENC_VALUES, 1) == &drone_block_spec[DRONE_BLOCK_VCO][DRONE_VCO_FINE]);
    check("on a filter: cutoff and resonance, with shift the same two", panel_patch_value(PANEL_AT_BLOCK, DRONE_BLOCK_VCF, PANEL_ENC_VALUES, 1) == DRONE_VCF_CUTOFF
          && panel_patch_value(PANEL_AT_BLOCK, DRONE_BLOCK_VCF, PANEL_ENC_VALUES + 1, 0) == DRONE_VCF_RESONANCE);
    check("a value with shift moves in the smallest steps", panel_fine(&drone_block_spec[DRONE_BLOCK_VCF][DRONE_VCF_RESONANCE]).per_click == 1
          && panel_fine(&drone_block_spec[DRONE_BLOCK_VCF][DRONE_VCF_RESONANCE]).top == 127);
    check("on a level: no kind, one value", !panel_patch_spec(PANEL_AT_BLOCK, DRONE_BLOCK_VCA, PANEL_ENC_KIND, 0)
          && panel_patch_spec(PANEL_AT_BLOCK, DRONE_BLOCK_VCA, PANEL_ENC_VALUES, 0) && !panel_patch_spec(PANEL_AT_BLOCK, DRONE_BLOCK_VCA, PANEL_ENC_VALUES + 1, 0));
    check("on an FX block: its mix, decay and tone on the first three right encoders, no kind, and a cable to the mix only",
          panel_patch_value(PANEL_AT_BLOCK, DRONE_BLOCK_FX, PANEL_ENC_VALUES, 0) == DRONE_FX_MIX
          && panel_patch_value(PANEL_AT_BLOCK, DRONE_BLOCK_FX, PANEL_ENC_VALUES + 1, 0) == DRONE_FX_DECAY
          && panel_patch_value(PANEL_AT_BLOCK, DRONE_BLOCK_FX, PANEL_ENC_VALUES + 2, 1) == DRONE_FX_TONE
          && panel_patch_value(PANEL_AT_BLOCK, DRONE_BLOCK_FX, PANEL_ENC_VALUES + 3, 0) < 0 && !panel_patch_spec(PANEL_AT_BLOCK, DRONE_BLOCK_FX, PANEL_ENC_KIND, 0)
          && panel_patch_cable(PANEL_AT_BLOCK, DRONE_BLOCK_FX, PANEL_ENC_VALUES) == DRONE_FX_MIX
          && panel_patch_cable(PANEL_AT_BLOCK, DRONE_BLOCK_FX, PANEL_ENC_VALUES + 1) < 0 && panel_block_new(DRONE_BLOCK_FX).param[DRONE_FX_MIX] == 64);
    check("on OUT: the voice's level, fade and send", panel_patch_spec(PANEL_AT_OUT, 0, PANEL_ENC_VALUES, 0) == &drone_param_spec[DRONE_LEVEL]
          && panel_patch_spec(PANEL_AT_OUT, 0, PANEL_ENC_VALUES + 1, 0) == &drone_param_spec[DRONE_FADE]
          && panel_patch_spec(PANEL_AT_OUT, 0, PANEL_ENC_VALUES + 2, 0) == &drone_send_spec && !panel_patch_spec(PANEL_AT_OUT, 0, PANEL_ENC_VALUES + 3, 0)
          && !panel_patch_spec(PANEL_AT_OUT, 0, PANEL_ENC_KIND, 0));
    check("on the empty place: nothing to turn", !panel_patch_spec(PANEL_AT_NEW, 0, PANEL_ENC_VALUES, 0) && !panel_patch_spec(PANEL_AT_NEW, 0, PANEL_ENC_KIND, 0));

    {
        uint32_t to[4], what[4], n;
        for (n = 0; n < 4; n++)
            check("the LFO key held on an LFO's page: four cables", panel_lfo_cable(PANEL_ENC_VALUES + n, 1, &to[n], &what[n]));
        check("from the second LFO: to the first one's speed and depth, then the third one's",
              to[0] == 0 && what[0] == DRONE_LFO_TO_SPEED && to[1] == 0 && what[1] == DRONE_LFO_TO_DEPTH && to[2] == 2 && what[2] == DRONE_LFO_TO_SPEED
              && to[3] == 2 && what[3] == DRONE_LFO_TO_DEPTH);
        check("from the first: to the second and the third; from the third: to the first and the second",
              panel_lfo_cable(PANEL_ENC_VALUES, 0, &to[0], &what[0]) && to[0] == 1 && panel_lfo_cable(PANEL_ENC_VALUES + 2, 0, &to[0], &what[0]) && to[0] == 2
              && panel_lfo_cable(PANEL_ENC_VALUES + 2, 2, &to[0], &what[0]) && to[0] == 1);
        check("and no cable on the other encoders", !panel_lfo_cable(PANEL_ENC_KIND, 0, &to[0], &what[0]) && !panel_lfo_cable(PANEL_ENC_MOVE, 0, &to[0], &what[0]));
    }
    check("the feedback's page: from, to, level and time on the four right encoders",
          panel_fb_param(PANEL_ENC_VALUES) == DRONE_FB_FROM && panel_fb_param(PANEL_ENC_VALUES + 1) == DRONE_FB_TO
          && panel_fb_param(PANEL_ENC_VALUES + 2) == DRONE_FB_LEVEL && panel_fb_param(PANEL_ENC_VALUES + 3) == DRONE_FB_TIME
          && panel_fb_param(PANEL_ENC_KIND) < 0 && panel_fb_param(PANEL_ENC_MOVE) < 0);
    check("a patch's LFO page: the wave on ALGORITHM, the speed and the depth on the first two right encoders",
          panel_patch_lfo_param(PANEL_ENC_KIND) == DRONE_LFO_WAVE && panel_patch_lfo_param(PANEL_ENC_VALUES) == DRONE_LFO_SPEED
          && panel_patch_lfo_param(PANEL_ENC_VALUES + 1) == DRONE_LFO_DEPTH && panel_patch_lfo_param(PANEL_ENC_VALUES + 2) < 0 && panel_patch_lfo_param(0) < 0 && panel_patch_lfo_param(PANEL_ENC_MOVE) < 0);
    check("the LFO key held: a cable to each value the LFOs can move",
          panel_patch_cable(PANEL_AT_BLOCK, DRONE_BLOCK_VCO, PANEL_ENC_VALUES) == DRONE_VCO_PITCH
          && panel_patch_cable(PANEL_AT_BLOCK, DRONE_BLOCK_VCF, PANEL_ENC_VALUES + 1) == DRONE_VCF_RESONANCE
          && panel_patch_cable(PANEL_AT_BLOCK, DRONE_BLOCK_VCA, PANEL_ENC_VALUES) == DRONE_VCA_LEVEL
          && panel_patch_cable(PANEL_AT_OUT, 0, PANEL_ENC_VALUES) == DRONE_OUT_LEVEL && panel_patch_cable(PANEL_AT_OUT, 0, PANEL_ENC_VALUES + 2) == DRONE_OUT_SEND);
    check("and none to the fade, an encoder with no value, or the empty place",
          panel_patch_cable(PANEL_AT_OUT, 0, PANEL_ENC_VALUES + 1) < 0 && panel_patch_cable(PANEL_AT_BLOCK, DRONE_BLOCK_VCA, PANEL_ENC_VALUES + 1) < 0
          && panel_patch_cable(PANEL_AT_NEW, 0, PANEL_ENC_VALUES) < 0 && panel_patch_cable(PANEL_AT_BLOCK, DRONE_BLOCK_VCO, PANEL_ENC_KIND) < 0);
    check("a cable's amount after its knob's steps: to the left it is negative, it stops at -100 and 100",
          panel_amount_turned(0, 10, 0) == 10 && panel_amount_turned(0, -15, 0) == -15 && panel_amount_turned(10, 1, 0) == 11
          && panel_amount_turned(90, 45, 0) == 100 && panel_amount_turned(-90, -1000, 0) == -100);
    check("with the snap key: each click to the next of -100, -50, 0, 50, 100 that way",
          panel_amount_turned(0, 1, 1) == 50 && panel_amount_turned(37, 1, 1) == 50 && panel_amount_turned(37, -1, 1) == 0
          && panel_amount_turned(50, 1, 1) == 100 && panel_amount_turned(-20, -1, 1) == -50 && panel_amount_turned(-20, 1, 1) == 0
          && panel_amount_turned(-50, -1, 1) == -100 && panel_amount_turned(100, 1, 1) == 100 && panel_amount_turned(0, -3, 1) == -100
          && panel_amount_turned(13, 2, 1) == 100 && panel_amount_turned(13, 0, 1) == 13);
    /* ---- the play page ---- */
    {
        panel_t play = { .page = PANEL_PLAY };
        panel_glo_key(&play);
        panel_glo_key(&play);
        check("the GLO key two times: from the play page over the mixer into the end of the chain", play.page == PANEL_DELAY);
        panel_eoc_select(&play, 9);
        check("SELECT there stops at the compressor: the play page and the mixer are not its pages", play.page == PANEL_COMP);
        panel_glo_key(&play);
        check("the GLO key again: back on the play page", play.page == PANEL_PLAY);

        {
            uint32_t turned_ms = 0;
            check("no click of SELECT: no step, and the time of the last click stays", panel_freq_steps(0, 5000, &turned_ms) == 0 && turned_ms == 0);
            check("a very slow click of SELECT: the smallest step, 1/128 of a semitone",
                  panel_freq_steps(1, 5000, &turned_ms) == 1 && turned_ms == 5000 && panel_freq_steps(-1, 5250, &turned_ms) == -1);
            check("faster: 2 steps a click from 100 ms a click, 8 from 50 ms, 32 from 25 ms, a semitone from 15 ms",
                  panel_freq_steps(1, 5350, &turned_ms) == 2 && panel_freq_steps(-1, 5400, &turned_ms) == -8
                  && panel_freq_steps(1, 5425, &turned_ms) == 32 && panel_freq_steps(2, 5455, &turned_ms) == 2 * 128);
            check("the fastest: 2 semitones a click, so the 6 clicks of a quarter turn are an octave",
                  panel_freq_steps(6, 5515, &turned_ms) == 12 * 128 && panel_freq_steps(-2, 5515, &turned_ms) == -4 * 128);
            check("then a pause: the smallest step again", panel_freq_steps(1, 6000, &turned_ms) == 1);
        }
        check("SELECT alone: the frequency moves by its steps", panel_freq_turned(5376, 1, 2, PANEL_FREQ_FREE) == 5376 + 2
              && panel_freq_turned(5376, -3, -96, PANEL_FREQ_FREE) == 5376 - 96 && panel_freq_turned(5376, 0, 0, PANEL_FREQ_FREE) == 5376);
        check("by notes: the first click goes to the nearest note that way",
              panel_freq_turned(5376 + 5, 1, 256, PANEL_FREQ_NOTES) == 5376 + 128 && panel_freq_turned(5376 + 5, -1, -1, PANEL_FREQ_NOTES) == 5376
              && panel_freq_turned(5376 + 5, 0, 0, PANEL_FREQ_NOTES) == 5376 + 5);
        check("by notes, from a note: a semitone a click at any speed", panel_freq_turned(5376, 2, 512, PANEL_FREQ_NOTES) == 5376 + 2 * 128
              && panel_freq_turned(5376, -1, -1, PANEL_FREQ_NOTES) == 5376 - 128 && panel_freq_turned(5376 + 5, -2, -2, PANEL_FREQ_NOTES) == 5376 - 128);
        check("by octaves: an octave a click, and the place between two notes stays", panel_freq_turned(5376, 1, 1, PANEL_FREQ_OCTAVES) == 5376 + 12 * 128
              && panel_freq_turned(5376 + 5, -2, -512, PANEL_FREQ_OCTAVES) == 5376 + 5 - 24 * 128);
        check("the frequency stops at its two ends", panel_freq_turned(10, -5, -40, PANEL_FREQ_FREE) == 0
              && panel_freq_turned(PLAY_FREQ_TOP - 10, 5, 40, PANEL_FREQ_FREE) == PLAY_FREQ_TOP
              && panel_freq_turned(100, -100000, -25600000, PANEL_FREQ_FREE) == 0
              && panel_freq_turned(PLAY_FREQ_TOP - 100, 100000, 25600000, PANEL_FREQ_FREE) == PLAY_FREQ_TOP
              && panel_freq_turned(5, -3, -3, PANEL_FREQ_NOTES) == 0 && panel_freq_turned(PLAY_FREQ_TOP - 5, 3, 3, PANEL_FREQ_NOTES) == PLAY_FREQ_TOP);
        check("an octave that does not fit is not taken; the ones before it are", panel_freq_turned(100, -1, -1, PANEL_FREQ_OCTAVES) == 100
              && panel_freq_turned(PLAY_FREQ_TOP - 100, 2, 2, PANEL_FREQ_OCTAVES) == PLAY_FREQ_TOP - 100
              && panel_freq_turned(5376, -4, -4, PANEL_FREQ_OCTAVES) == 5376 - 36 * 128);

        check("KNOB1 to KNOB4: TONE, SPEED, GRIT, LINK", panel_play_knob(PANEL_ENC_VALUES, 0, 0) == PLAY_TONE
              && panel_play_knob(PANEL_ENC_VALUES + 1, 0, 0) == PLAY_SPEED && panel_play_knob(PANEL_ENC_VALUES + 2, 0, 0) == PLAY_GRIT
              && panel_play_knob(PANEL_ENC_VALUES + 3, 0, 0) == PLAY_LINK);
        check("the second layer: KNOB1 is the voice's frequency, KNOB3 its GAIN (the mixer's level); KNOB2 and KNOB4 have none",
              panel_play_knob(PANEL_ENC_VALUES, 1, 0) == PANEL_PLAY_FREQ && PANEL_PLAY_FREQ == PLAY_NKNOB
              && panel_play_knob(PANEL_ENC_VALUES + 1, 1, 0) < 0 && panel_play_knob(PANEL_ENC_VALUES + 2, 1, 0) == PANEL_PLAY_GAIN
              && PANEL_PLAY_GAIN > PLAY_NKNOB && panel_play_knob(PANEL_ENC_VALUES + 2, 0, 0) == PLAY_GRIT
              && panel_play_knob(PANEL_ENC_VALUES + 3, 1, 0) < 0);
        check("the second layer on a THROUGH voice: KNOB4 is OWN, how much of its own oscillators is left", panel_play_knob(PANEL_ENC_VALUES + 3, 1, 1) == PLAY_OWN
              && panel_play_knob(PANEL_ENC_VALUES + 3, 0, 1) == PLAY_LINK && panel_play_knob(PANEL_ENC_VALUES + 1, 1, 1) < 0
              && panel_play_knob(PANEL_ENC_VALUES, 1, 1) == PANEL_PLAY_FREQ);
        check("the two leftmost white keys (keys 0, 2) choose the play page's layer; the others, none", panel_layer_of_keys(1u << 0, PANEL_PLAY_LAYERS) == 0
              && panel_layer_of_keys(1u << 2, PANEL_PLAY_LAYERS) == 1 && panel_layer_of_keys(1u << 4, PANEL_PLAY_LAYERS) < 0
              && panel_layer_of_keys(1u << 6 | 1u << 7 | 1u << 1, PANEL_PLAY_LAYERS) < 0 && panel_layer_of_keys(0, PANEL_PLAY_LAYERS) < 0
              && panel_layer_of_keys(1u << 2 | 1u << 0, PANEL_PLAY_LAYERS) == 0);
        check("their lights: the two keys; the layer in hand's blinks", panel_layer_lights(PANEL_PLAY_LAYERS, 0, 0) == (1u << 0 | 1u << 2)
              && panel_layer_lights(PANEL_PLAY_LAYERS, 0, 256) == 1u << 2 && panel_layer_lights(PANEL_PLAY_LAYERS, 1, 256) == 1u << 0
              && panel_layer_lights(PANEL_PLAY_LAYERS, 1, 512) == (1u << 0 | 1u << 2));
        check("ALGORITHM is the voice's STYLE in the first layer, on any voice; in the second it holds no knob (the link's kind)",
              panel_play_knob(PANEL_ENC_KIND, 0, 0) == PLAY_STYLE && panel_play_knob(PANEL_ENC_KIND, 0, 1) == PLAY_STYLE
              && panel_play_knob(PANEL_ENC_KIND, 1, 0) < 0 && panel_play_knob(PANEL_ENC_KIND, 1, 1) < 0);
        check("SELECT and PRESETS hold no knob", panel_play_knob(0, 0, 0) < 0 && panel_play_knob(0, 1, 0) < 0
              && panel_play_knob(PANEL_ENC_MOVE, 0, 0) < 0 && panel_play_knob(PANEL_ENC_MOVE, 1, 0) < 0);
        check("ALGORITHM with shift goes through the link's kinds: FOLLOW first, then THROUGH, then FM",
              param_turned(&drone_link_kind_spec, DRONE_LINK_FOLLOW, 1) == DRONE_LINK_THROUGH
              && param_turned(&drone_link_kind_spec, DRONE_LINK_THROUGH, 1) == DRONE_LINK_FM
              && param_turned(&drone_link_kind_spec, DRONE_LINK_THROUGH, 5) == DRONE_NLINK - 1 && drone_link_kind_spec.start == DRONE_LINK_FOLLOW
              && panel_link_name[DRONE_LINK_FM][0] == 'F' && panel_link_name[DRONE_LINK_FM][1] == 'M');
        check("the STYLE goes through all the styles, one a click", play_knob_spec[PLAY_STYLE].top == STYLE_NSTYLE - 1
              && play_knob_spec[PLAY_STYLE].per_click == 1);
    }
    /* ---- the mixer ---- */
    {
        mixer_channel_t channel = mixer_channel_new();
        drone_voice_controls_t patch = { .on = 1, .param = { [DRONE_FADE] = 96 } };
        uint32_t voice = 9, value = 9;
        check("a channel starts: its level low, its pan in the centre, its sends a little open, taken before the level",
              channel.value[MIXER_LEVEL] == 96 && channel.value[MIXER_PAN] == MIXER_PAN_CENTRE && channel.value[MIXER_SEND_DELAY] == 32
              && channel.value[MIXER_SEND_REVERB] == 32 && !channel.post);
        mixer_apply(&channel, &patch);
        check("the channel into the voice: its level, its sends, the pan in the centre; the rest of the voice as it was",
              patch.param[DRONE_LEVEL] == 96 && patch.level_log && patch.send == 32 && patch.send_reverb == 32 && patch.pan == 0 && !patch.post
              && patch.on && patch.param[DRONE_FADE] == 96);
        channel.value[MIXER_PAN] = 0;
        channel.value[MIXER_SEND_DELAY] = 90;
        channel.value[MIXER_SEND_REVERB] = 7;
        channel.post = 1;
        mixer_apply(&channel, &patch);
        check("the pan at 0 is all to the left; send 1 is the delay's, send 2 the reverb's; POST", patch.pan == -64 && patch.send == 90
              && patch.send_reverb == 7 && patch.post == 1);
        channel.value[MIXER_PAN] = 128;
        mixer_apply(&channel, &patch);
        check("the pan at the top is all to the right, as far as the left", patch.pan == 64 && mixer_spec[MIXER_PAN].top == 2 * MIXER_PAN_CENTRE);
        check("the pan from each end comes back to the centre", mixer_turned(MIXER_PAN, 128, -64, 0) == MIXER_PAN_CENTRE
              && mixer_turned(MIXER_PAN, 0, 64, 0) == MIXER_PAN_CENTRE);
        check("the pan stops in the centre when a turn goes over it, from any place", mixer_turned(MIXER_PAN, 61, 4, 0) == MIXER_PAN_CENTRE
              && mixer_turned(MIXER_PAN, 66, -3, 0) == MIXER_PAN_CENTRE && mixer_turned(MIXER_PAN, 63, 9, 0) == MIXER_PAN_CENTRE);
        check("and the next turn leaves the centre", mixer_turned(MIXER_PAN, MIXER_PAN_CENTRE, 4, 0) == MIXER_PAN_CENTRE + 4
              && mixer_turned(MIXER_PAN, MIXER_PAN_CENTRE, -1, 0) == MIXER_PAN_CENTRE - 1);
        check("the other values take their knob's steps, and stop at the ends", mixer_turned(MIXER_LEVEL, 96, 8, 0) == 104
              && mixer_turned(MIXER_LEVEL, 96, -1, 0) == 95 && mixer_turned(MIXER_SEND_REVERB, 126, 3, 0) == 127 && mixer_turned(MIXER_LEVEL, 2, -4, 0) == 0);
        check("with the snap key: the level to its middle, its value at power-on (96), its ends; the pan to the left, the centre, the right",
              mixer_turned(MIXER_LEVEL, 70, 1, 1) == 96 && mixer_turned(MIXER_LEVEL, 96, 1, 1) == 127 && mixer_turned(MIXER_LEVEL, 96, -1, 1) == 64
              && mixer_turned(MIXER_LEVEL, 64, -1, 1) == 0 && mixer_turned(MIXER_PAN, 10, 1, 1) == MIXER_PAN_CENTRE
              && mixer_turned(MIXER_PAN, MIXER_PAN_CENTRE, 1, 1) == 2 * MIXER_PAN_CENTRE && mixer_turned(MIXER_PAN, 100, -1, 1) == MIXER_PAN_CENTRE
              && mixer_turned(MIXER_SEND_DELAY, 0, 1, 1) == 32 && mixer_turned(MIXER_SEND_DELAY, 32, 1, 1) == 64);
        check("the fader in dB: 0 at the top, 12 down at its start, 50 down one step from the bottom", mixer_level_db(127) == 0
              && mixer_level_db(96) == 12 && mixer_level_db(1) == 50);

        check("no channel selected: KNOB1 to KNOB3 are the three voices' levels, KNOB4 has none",
              panel_mixer_value(0, PANEL_ENC_VALUES, &voice, &value) && voice == 0 && value == MIXER_LEVEL
              && panel_mixer_value(0, PANEL_ENC_VALUES + 2, &voice, &value) && voice == 2 && value == MIXER_LEVEL
              && !panel_mixer_value(0, PANEL_ENC_VALUES + 3, &voice, &value));
        check("a channel selected: KNOB1 its level, KNOB2 its pan, KNOB3 its send 1, KNOB4 its send 2",
              panel_mixer_value(2, PANEL_ENC_VALUES, &voice, &value) && voice == 1 && value == MIXER_LEVEL
              && panel_mixer_value(2, PANEL_ENC_VALUES + 1, &voice, &value) && voice == 1 && value == MIXER_PAN
              && panel_mixer_value(2, PANEL_ENC_VALUES + 2, &voice, &value) && value == MIXER_SEND_DELAY
              && panel_mixer_value(3, PANEL_ENC_VALUES + 3, &voice, &value) && voice == 2 && value == MIXER_SEND_REVERB);
        check("the left encoders hold no value of the mixer", !panel_mixer_value(1, 0, &voice, &value) && !panel_mixer_value(1, PANEL_ENC_KIND, &voice, &value)
              && !panel_mixer_value(1, PANEL_ENC_MOVE, &voice, &value));
        check("SELECT: none, then each voice's channel, and it stops there", param_turned(&panel_mixer_channel_spec, 0, 1) == 1
              && param_turned(&panel_mixer_channel_spec, 1, 9) == DRONE_VOICES && param_turned(&panel_mixer_channel_spec, 2, -9) == 0);
    }

    /* ---- an encoder turned fast moves its value further ---- */
    {
        uint32_t turned_ms = 0;
        /* a knob's turn: a value of 4 a click (a play knob's) */
        check("no click: nothing, and the time of the last click stays", panel_knob_steps(4, 0, 5000, &turned_ms) == 0 && turned_ms == 0);
        check("a click after a long time: 1, the smallest step", panel_knob_steps(4, 1, 5000, &turned_ms) == 1 && turned_ms == 5000);
        check("notch by notch, a click each 120 ms or more: 1 each, up and down", panel_knob_steps(4, 1, 5120, &turned_ms) == 1
              && panel_knob_steps(4, -1, 5400, &turned_ms) == -1 && panel_knob_steps(4, 1, 5520, &turned_ms) == 1);
        check("a click 100 ms later: half of the value's click, 2", panel_knob_steps(4, 1, 5620, &turned_ms) == 2);
        check("a click 40 ms later: the value's click, 4", panel_knob_steps(4, -1, 5660, &turned_ms) == -4);
        check("a click 20 ms later: twice, 8", panel_knob_steps(4, 1, 5680, &turned_ms) == 8);
        check("a click 5 ms later: four times, 16, the fastest", panel_knob_steps(4, -1, 5685, &turned_ms) == -16);
        check("three clicks in 60 ms: 20 ms each, 8 times three", panel_knob_steps(4, 3, 5745, &turned_ms) == 24);
        check("clicks in the same millisecond: the fastest", panel_knob_steps(4, -2, 5745, &turned_ms) == -32);
        check("then a pause: 1 again", panel_knob_steps(4, 1, 6000, &turned_ms) == 1);
        turned_ms = 0;
        check("a value of 5 a click: 1, 3, 5, 10, 20", panel_knob_steps(5, 1, 1000, &turned_ms) == 1 && panel_knob_steps(5, 1, 1060, &turned_ms) == 3
              && panel_knob_steps(5, 1, 1090, &turned_ms) == 5 && panel_knob_steps(5, 1, 1105, &turned_ms) == 10
              && panel_knob_steps(5, 1, 1110, &turned_ms) == 20);
        turned_ms = 0;
        check("the tempo, 2: 1 BPM a click down to 60 ms a click, then 2, 4, 8", panel_knob_steps(2, 1, 1000, &turned_ms) == 1
              && panel_knob_steps(2, 1, 1060, &turned_ms) == 1 && panel_knob_steps(2, 1, 1090, &turned_ms) == 2
              && panel_knob_steps(2, 1, 1105, &turned_ms) == 4 && panel_knob_steps(2, 1, 1110, &turned_ms) == 8);
        {   /* the steps onto a value: with a click of 1 (panel_fine), and kept in its range */
            param_spec_t by_one = panel_fine(&play_knob_spec[PLAY_TONE]);
            turned_ms = 0;
            check("TONE at 64, one notch: 65; one notch back: 64", param_turned(&by_one, 64, panel_knob_steps(4, 1, 1000, &turned_ms)) == 65
                  && param_turned(&by_one, 65, panel_knob_steps(4, -1, 2000, &turned_ms)) == 64);
            check("a fast turn near the top stops at the top", param_turned(&by_one, 120, panel_knob_steps(4, 2, 2002, &turned_ms)) == 127);
        }
    }

    /* ---- the disruptors: the FX key goes through their pages, the noise, the pulse, the sine, then the play page ---- */
    {
        panel_t fx = { .page = PANEL_PLAY };
        panel_fx_key(&fx);
        check("the FX key on the play page: the noise", fx.page == PANEL_NOISE && panel_page_dis(fx.page) == DRONE_DIS_NOISE && !panel_in_eoc(&fx)
              && panel_page_lfo(PANEL_NOISE) < 0);
        panel_eoc_select(&fx, 1);
        check("the end of the chain's SELECT does nothing there", fx.page == PANEL_NOISE);
        panel_fx_key(&fx);
        check("the FX key again: the pulse", fx.page == PANEL_CHOP && panel_page_dis(fx.page) == DRONE_DIS_CHOP);
        panel_fx_key(&fx);
        check("again: the sine", fx.page == PANEL_RING && panel_page_dis(fx.page) == DRONE_DIS_RING);
        panel_fx_key(&fx);
        check("again: the play page, which is no disruptor's", fx.page == PANEL_PLAY && panel_page_dis(fx.page) < 0 && panel_page_dis(PANEL_MIXER) < 0);
        panel_glo_key(&fx);
        panel_fx_key(&fx);
        check("from the mixer too: the noise first", fx.page == PANEL_NOISE);
        panel_fx_key(&fx);
        panel_glo_key(&fx);
        check("the GLO key on a disruptor's page: the mixer", fx.page == PANEL_MIXER);
        for (i = 0; i < DRONE_DISRUPTORS; i++)
            check("KNOB1 to KNOB3 are the disruptor's own three values; KNOB4 the voice's amount; ALGORITHM where the voice takes it",
                  panel_dis_spec(i, PANEL_ENC_VALUES) == &drone_dis_spec[i][0] && panel_dis_spec(i, PANEL_ENC_VALUES + 1u) == &drone_dis_spec[i][1]
                  && panel_dis_spec(i, PANEL_ENC_VALUES + 2u) == &drone_dis_spec[i][2] && panel_dis_spec(i, PANEL_ENC_VALUES + 3u) == &drone_dis_send_spec
                  && panel_dis_spec(i, PANEL_ENC_KIND) == &drone_to_spec && !panel_dis_spec(i, 0) && !panel_dis_spec(i, PANEL_ENC_MOVE));
        check("no page, no value", !panel_dis_spec(DRONE_DISRUPTORS, PANEL_ENC_VALUES));
        check("the noise goes into a voice's SOUND; the pulse and the sine change its LEVEL; all three its PITCH or its CUTOFF",
              panel_to_name(DRONE_DIS_NOISE, DRONE_TO_SOUND)[0] == 'S' && panel_to_name(DRONE_DIS_CHOP, DRONE_TO_SOUND)[0] == 'L'
              && panel_to_name(DRONE_DIS_RING, DRONE_TO_SOUND)[0] == 'L' && panel_to_name(DRONE_DIS_RING, DRONE_TO_PITCH)[0] == 'P'
              && panel_to_name(DRONE_DIS_CHOP, DRONE_TO_CUTOFF)[0] == 'C');
    }
    {
        play_voice_t play = { .knob = { [PLAY_STYLE] = STYLE_REED }, .freq = 42u * 128u, .dis = { 90, 0, 30 }, .dis_to = { DRONE_TO_PITCH, 0, 9 } };
        drone_voice_controls_t patch;
        style_patch(&play, &patch);
        check("a voice's amount of each disruptor and where it takes it go into its patch", patch.dis[DRONE_DIS_NOISE] == 90
              && patch.dis_to[DRONE_DIS_NOISE] == DRONE_TO_PITCH && patch.dis[DRONE_DIS_CHOP] == 0 && patch.dis[DRONE_DIS_RING] == 30);
        check("a place that is none: its sound", patch.dis_to[DRONE_DIS_RING] == DRONE_TO_SOUND);
    }

    /* ---- the LFO key and the bank's page (V0.32) ---- */
    {
        panel_t bank = { .page = PANEL_PLAY };
        panel_lfo_key(&bank, 1, 0, 0);
        check("the LFO key pressed: no page yet", bank.page == PANEL_PLAY);
        panel_lfo_key(&bank, 0, 1, 0);
        check("let go with nothing turned: the bank's page, which is no other page's", bank.page == PANEL_BANK && !panel_in_eoc(&bank)
              && panel_page_lfo(PANEL_BANK) < 0 && panel_page_dis(PANEL_BANK) < 0);
        panel_lfo_key(&bank, 1, 1, 0);
        check("a short press there: back to the page that was open before", bank.page == PANEL_PLAY);
        bank.page = PANEL_REVERB;
        panel_lfo_key(&bank, 1, 0, 0);
        panel_lfo_key(&bank, 0, 1, 0);
        panel_lfo_key(&bank, 1, 0, 0);
        panel_lfo_key(&bank, 0, 1, 0);
        check("from any page, and back to it", bank.page == PANEL_REVERB);
        panel_lfo_key(&bank, 1, 0, 0);
        panel_lfo_key(&bank, 0, 0, 1);
        panel_lfo_key(&bank, 0, 0, 0);
        panel_lfo_key(&bank, 0, 1, 0);
        check("a knob turned or a white key pressed while it is down: its release opens no page", bank.page == PANEL_REVERB);
        panel_lfo_key(&bank, 1, 0, 1);
        panel_lfo_key(&bank, 0, 1, 0);
        check("a turn in the same turn of the loop as the press counts too", bank.page == PANEL_REVERB);
        panel_lfo_key(&bank, 1, 0, 0);
        panel_lfo_key(&bank, 0, 1, 0);
        check("the next short press opens it again", bank.page == PANEL_BANK);
        panel_lfo_key(&bank, 1, 0, 0);
        panel_lfo_key(&bank, 0, 0, 1);
        panel_lfo_key(&bank, 0, 1, 0);
        check("on the bank's page too: a turn while it is down keeps the page", bank.page == PANEL_BANK);
        panel_glo_key(&bank);
        check("GLO on the bank's page: the mixer, as from the play page", bank.page == PANEL_MIXER);
        panel_lfo_key(&bank, 1, 1, 0);
        panel_fx_key(&bank);
        check("FX on the bank's page: the noise, as from the play page", bank.page == PANEL_NOISE);
        panel_lfo_key(&bank, 1, 1, 0);
        panel_lfo_key(&bank, 1, 1, 0);
        check("the noise, the bank's page, and the noise again", bank.page == PANEL_NOISE);
    }
    {
        static const uint8_t white[DRONE_BANK_LFOS] = { 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26 };
        uint32_t keys = 0, l;
        play_voice_t voices[DRONE_VOICES] = { { 0 } };
        for (l = 0; l < DRONE_BANK_LFOS; l++)   /* the twelve white keys right of the layers' four: they choose nothing */
            keys |= 1u << white[l];
        check("the white keys from the fifth choose no layer: nothing on the bank's page", panel_layer_of_keys(keys, PANEL_BANK_NSET) < 0);
        check("the four leftmost white keys (keys 0, 2, 4, 6): the layers 1 to 4 (WAVE, STEPS, LOOP, MORE), the first layer on the first key whatever the number of layers", panel_layer_of_keys(1u << 0, PANEL_BANK_NSET) == PANEL_BANK_WAVE
              && panel_layer_of_keys(1u << 2, PANEL_BANK_NSET) == PANEL_BANK_STEPS && panel_layer_of_keys(1u << 4, PANEL_BANK_NSET) == PANEL_BANK_LOOP
              && panel_layer_of_keys(1u << 6, PANEL_BANK_NSET) == PANEL_BANK_MORE && PANEL_BANK_NSET == 4);
        check("no layer for the fifth white key, a black key, or none", panel_layer_of_keys(1u << 7, PANEL_BANK_NSET) < 0
              && panel_layer_of_keys(1u << 1 | 1u << 3, PANEL_BANK_NSET) < 0 && panel_layer_of_keys(0, PANEL_BANK_NSET) < 0);
        check("two of them at once: the one on the left", panel_layer_of_keys(1u << 4 | 1u << 2, PANEL_BANK_NSET) == PANEL_BANK_STEPS);
        {
            uint32_t sets = 1u << 0 | 1u << 2 | 1u << 4 | 1u << 6;
            check("their lights: the four keys; the layer in hand's blinks, off in every other quarter of a second",
                  panel_layer_lights(PANEL_BANK_NSET, PANEL_BANK_WAVE, 0) == sets && panel_layer_lights(PANEL_BANK_NSET, PANEL_BANK_WAVE, 255) == sets
                  && panel_layer_lights(PANEL_BANK_NSET, PANEL_BANK_WAVE, 256) == (sets & ~(1u << 0))
                  && panel_layer_lights(PANEL_BANK_NSET, PANEL_BANK_STEPS, 511) == (sets & ~(1u << 2))
                  && panel_layer_lights(PANEL_BANK_NSET, PANEL_BANK_LOOP, 256 + 512) == (sets & ~(1u << 4))
                  && panel_layer_lights(PANEL_BANK_NSET, PANEL_BANK_MORE, 256) == (sets & ~(1u << 6))
                  && panel_layer_lights(PANEL_BANK_NSET, PANEL_BANK_LOOP, 1024) == sets
                  && panel_layer_lights(PANEL_BANK_NSET, 9, 256) == (sets & ~(1u << 0)) && !(panel_layer_lights(PANEL_BANK_NSET, PANEL_BANK_LOOP, 0) & keys));
        }
        {
            uint32_t shown = 4, source = 9;
            panel_bank_choose(&shown, &source, 1, -7);
            check("the LFO key held on the bank's page: SELECT chooses the source, the shown LFO stays", source == 2 && shown == 4);
            panel_bank_choose(&shown, &source, 1, 3);
            check("and on", source == 5 && shown == 4);
            panel_bank_choose(&shown, &source, 0, 3);
            check("the key up: SELECT shows another LFO, the source stays", shown == 7 && source == 5);
            panel_bank_choose(&shown, &source, 0, -2);
            check("and back", shown == 5 && source == 5);
            panel_bank_choose(&shown, &source, 0, 0);
            check("no click: no change", shown == 5 && source == 5);
            panel_bank_choose(&shown, &source, 0, 40);
            check("it stops at the twelfth LFO, and at the first", shown == 11 && (panel_bank_choose(&shown, &source, 0, -40), shown == 0));
        }
        check("the page starts with the WAVE knobs", (panel_t){ 0 }.bank_set == PANEL_BANK_WAVE);

        check("SELECT: the LFO on the screen, 1 to 12, it stops at the ends", panel_bank_select(0, 1) == 1 && panel_bank_select(0, -1) == 0
              && panel_bank_select(10, 5) == DRONE_BANK_LFOS - 1 && panel_bank_select(11, -100) == 0 && panel_bank_select(99, 0) == DRONE_BANK_LFOS - 1);
        {
            /* each layer's four knobs: KNOB1 .. KNOB4 */
            static const uint8_t held_by[PANEL_BANK_NSET][4] = {
                { DRONE_BANK_RATE, DRONE_BANK_WIDTH, DRONE_BANK_LEVEL, DRONE_BANK_OFFSET },
                { DRONE_BANK_PROB, DRONE_BANK_STEPS, DRONE_BANK_TRIGS, DRONE_BANK_SHIFT },
                { DRONE_BANK_LOOP, DRONE_BANK_NAP, DRONE_BANK_WAKE, DRONE_BANK_LSHIFT },
                { DRONE_BANK_PHASE, DRONE_BANK_SLEW, DRONE_BANK_PAD, DRONE_BANK_RAND },
            };
            uint32_t set, n, values = 1, amounts = 1;
            for (set = 0; set < PANEL_BANK_NSET; set++)
                for (n = 0; n < 4; n++) {
                    panel_bank_holds_t a = panel_bank_holds(set, PANEL_ENC_VALUES + n, 0, 0);
                    panel_bank_holds_t h = panel_bank_holds(set, PANEL_ENC_VALUES + n, 1, 0), hs = panel_bank_holds(set, PANEL_ENC_VALUES + n, 1, 1);
                    values &= a.kind == PANEL_HOLDS_VALUE && a.which == held_by[set][n];
                    amounts &= h.kind == PANEL_HOLDS_AMOUNT && drone_bank_cabled_param[h.which] == held_by[set][n] && h.which == panel_bank_cable(set, n)
                                 && hs.kind == PANEL_HOLDS_AMOUNT && hs.which == h.which;
                }
            check("WAVE: RATE WIDTH LEVEL OFFSET; STEPS: PROB STEPS TRIGS SHIFT; LOOP: LOOP NAP WAKE L SHIFT; MORE: PHASE SLEW PAD RAND", values);
            check("the LFO key held: each knob is the amount onto the value it holds, in each layer (the MORE layer too), whatever the sync",
                  amounts && DRONE_BANK_NCABLED == 4 * PANEL_BANK_NSET && panel_bank_cable(PANEL_BANK_NSET, 0) == DRONE_BANK_NCABLED
                  && panel_bank_cable(PANEL_BANK_MORE, 4) == DRONE_BANK_NCABLED);
        }
        check("a synced LFO: KNOB1 of the WAVE knobs is its RATIO; the other layers do not change",
              panel_bank_holds(PANEL_BANK_WAVE, PANEL_ENC_VALUES, 0, 1).which == DRONE_BANK_RATIO
              && panel_bank_holds(PANEL_BANK_STEPS, PANEL_ENC_VALUES, 0, 1).which == DRONE_BANK_PROB
              && panel_bank_holds(PANEL_BANK_LOOP, PANEL_ENC_VALUES, 0, 1).which == DRONE_BANK_LOOP
              && panel_bank_holds(PANEL_BANK_MORE, PANEL_ENC_VALUES, 0, 1).which == DRONE_BANK_PHASE);
        check("ALGORITHM: the SHAPE; in the MORE layer: the RANGE; the LFO key held: nothing",
              panel_bank_holds(PANEL_BANK_STEPS, PANEL_ENC_KIND, 0, 0).kind == PANEL_HOLDS_VALUE && panel_bank_holds(PANEL_BANK_STEPS, PANEL_ENC_KIND, 0, 0).which == DRONE_BANK_SHAPE
              && panel_bank_holds(PANEL_BANK_LOOP, PANEL_ENC_KIND, 0, 1).which == DRONE_BANK_SHAPE
              && panel_bank_holds(PANEL_BANK_MORE, PANEL_ENC_KIND, 0, 1).which == DRONE_BANK_RANGE
              && panel_bank_holds(PANEL_BANK_WAVE, PANEL_ENC_KIND, 1, 0).kind == PANEL_HOLDS_NOTHING
              && panel_bank_holds(PANEL_BANK_MORE, PANEL_ENC_KIND, 1, 0).kind == PANEL_HOLDS_NOTHING);
        check("PRESETS: the tempo; the LFO key held: the amount onto the tempo, in each layer", panel_bank_holds(PANEL_BANK_LOOP, PANEL_ENC_MOVE, 0, 0).kind == PANEL_HOLDS_TEMPO
              && panel_bank_holds(PANEL_BANK_STEPS, PANEL_ENC_MOVE, 1, 0).kind == PANEL_HOLDS_TEMPO_AMOUNT
              && panel_bank_holds(PANEL_BANK_MORE, PANEL_ENC_MOVE, 1, 0).kind == PANEL_HOLDS_TEMPO_AMOUNT);
        check("SELECT holds none of these: it chooses the LFO", panel_bank_holds(PANEL_BANK_WAVE, 0, 0, 0).kind == PANEL_HOLDS_NOTHING
              && panel_bank_holds(PANEL_BANK_WAVE, 0, 1, 0).kind == PANEL_HOLDS_NOTHING && panel_bank_holds(PANEL_BANK_WAVE, 7, 0, 0).kind == PANEL_HOLDS_NOTHING);
        check("a set out of range is the WAVE set", panel_bank_holds(9, PANEL_ENC_VALUES + 1, 0, 0).which == DRONE_BANK_WIDTH);

        check("the RATE after its knob's steps; it stops at 0 and 127", panel_bank_turned(DRONE_BANK_RATE, 40, 2, 0) == 42
              && panel_bank_turned(DRONE_BANK_RATE, 40, -1, 0) == 39 && panel_bank_turned(DRONE_BANK_RATE, 126, 10, 0) == 127
              && panel_bank_turned(DRONE_BANK_RATE, 1, -10, 0) == 0 && drone_bank_spec[DRONE_BANK_RATE].start == 40);
        check("the RATIO: one of its twelve a click, it stops at the ends", panel_bank_turned(DRONE_BANK_RATIO, 6, 1, 0) == 7
              && panel_bank_turned(DRONE_BANK_RATIO, 10, 5, 0) == DRONE_BANK_RATIOS - 1 && panel_bank_turned(DRONE_BANK_RATIO, 0, -1, 0) == 0
              && drone_bank_spec[DRONE_BANK_RATIO].per_click == 1);
        check("the WIDTH", panel_bank_turned(DRONE_BANK_WIDTH, 50, 5, 0) == 55 && panel_bank_turned(DRONE_BANK_WIDTH, 50, -500, 0) == 0);
        check("the PHASE: round the cycle at both ends, 1 a step (2.8 degrees)", panel_bank_turned(DRONE_BANK_PHASE, 0, 4, 0) == 4
              && panel_bank_turned(DRONE_BANK_PHASE, 124, 4, 0) == 0 && panel_bank_turned(DRONE_BANK_PHASE, 0, -4, 0) == 124
              && panel_bank_turned(DRONE_BANK_PHASE, 127, 1, 0) == 0 && panel_bank_turned(DRONE_BANK_PHASE, 0, -1, 0) == 127
              && panel_bank_turned(DRONE_BANK_PHASE, 8, 132, 0) == 12 && panel_bank_turned(DRONE_BANK_PHASE, 8, -100000, 0) < 128
              && panel_bank_turned(DRONE_BANK_PHASE, 200, 1, 0) < 128);
        check("the SLEW, the LEVEL", panel_bank_turned(DRONE_BANK_SLEW, 0, 8, 0) == 8
              && panel_bank_turned(DRONE_BANK_LEVEL, 100, -1, 0) == 99 && panel_bank_turned(DRONE_BANK_LEVEL, 100, 5, 0) == 100);
        check("the OFFSET, from its middle, -100 % .. +100 %", panel_bank_turned(DRONE_BANK_OFFSET, 100, 20, 0) == 120
              && panel_bank_turned(DRONE_BANK_OFFSET, 100, -1, 0) == 99 && panel_bank_turned(DRONE_BANK_OFFSET, 190, 25, 0) == 200
              && panel_bank_turned(DRONE_BANK_OFFSET, 3, -5, 0) == 0);
        check("the PROB, 0 .. 100 %", panel_bank_turned(DRONE_BANK_PROB, 100, -40, 0) == 60
              && panel_bank_turned(DRONE_BANK_PROB, 60, 1, 0) == 61 && panel_bank_turned(DRONE_BANK_PROB, 99, 15, 0) == 100);
        check("with the snap key: the LEVEL to 0, 50, 100; the OFFSET to -100 %, none, +100 %; the RATE to 0, its start (40), its middle (64), its top",
              panel_bank_turned(DRONE_BANK_LEVEL, 30, 1, 1) == 50 && panel_bank_turned(DRONE_BANK_LEVEL, 50, 1, 1) == 100
              && panel_bank_turned(DRONE_BANK_LEVEL, 30, -1, 1) == 0 && panel_bank_turned(DRONE_BANK_OFFSET, 130, -1, 1) == 100
              && panel_bank_turned(DRONE_BANK_OFFSET, 100, -1, 1) == 0 && panel_bank_turned(DRONE_BANK_OFFSET, 100, 1, 1) == 200
              && panel_bank_turned(DRONE_BANK_RATE, 100, -1, 1) == 64 && panel_bank_turned(DRONE_BANK_RATE, 64, -1, 1) == 40
              && panel_bank_turned(DRONE_BANK_RATE, 40, -1, 1) == 0 && panel_bank_turned(DRONE_BANK_RATE, 100, 1, 1) == 127
              && panel_bank_turned(DRONE_BANK_RATE, 10, 2, 1) == 64);
        check("with the snap key: the PHASE to 0, half a cycle, its end; TRIGS never below 1", panel_bank_turned(DRONE_BANK_PHASE, 10, 1, 1) == 64
              && panel_bank_turned(DRONE_BANK_PHASE, 64, 1, 1) == 127 && panel_bank_turned(DRONE_BANK_PHASE, 10, -1, 1) == 0
              && panel_bank_turned(DRONE_BANK_TRIGS, 4, -1, 1) == 1 && panel_bank_turned(DRONE_BANK_TRIGS, 4, 1, 1) == 8);
        check("STEPS 0 (none) .. 16, PAD and RAND 0 .. 8, SHIFT 0 .. 15: one a click", panel_bank_turned(DRONE_BANK_STEPS, 0, 1, 0) == 1
              && panel_bank_turned(DRONE_BANK_STEPS, 15, 9, 0) == 16 && panel_bank_turned(DRONE_BANK_STEPS, 1, -1, 0) == 0
              && panel_bank_turned(DRONE_BANK_PAD, 7, 4, 0) == 8 && panel_bank_turned(DRONE_BANK_RAND, 1, -4, 0) == 0
              && panel_bank_turned(DRONE_BANK_SHIFT, 14, 1, 0) == 15 && panel_bank_turned(DRONE_BANK_SHIFT, 15, 1, 0) == 15);
        check("TRIGS and WAKE never go below 1", panel_bank_turned(DRONE_BANK_TRIGS, 1, -1, 0) == 1 && panel_bank_turned(DRONE_BANK_TRIGS, 4, -100, 0) == 1
              && panel_bank_turned(DRONE_BANK_TRIGS, 0, 0, 0) == 1 && panel_bank_turned(DRONE_BANK_TRIGS, 15, 3, 0) == 16
              && panel_bank_turned(DRONE_BANK_WAKE, 1, -1, 0) == 1 && panel_bank_turned(DRONE_BANK_WAKE, 2, -100000, 0) == 1
              && panel_bank_turned(DRONE_BANK_WAKE, 7, 5, 0) == 8);
        check("LOOP 0 (none) .. 32, NAP 0 .. 8, L SHIFT 0 .. 8", panel_bank_turned(DRONE_BANK_LOOP, 0, 4, 0) == 4 && panel_bank_turned(DRONE_BANK_LOOP, 30, 5, 0) == 32
              && panel_bank_turned(DRONE_BANK_NAP, 1, -1, 0) == 0 && panel_bank_turned(DRONE_BANK_NAP, 7, 3, 0) == 8
              && panel_bank_turned(DRONE_BANK_LSHIFT, 0, 9, 0) == 8);
        check("the SHAPE: GATE .. SOFT_RANDOM, and it stops there", panel_bank_turned(DRONE_BANK_SHAPE, DRONE_BANK_SINE, 1, 0) == DRONE_BANK_HUMP
              && panel_bank_turned(DRONE_BANK_SHAPE, DRONE_BANK_RANDOM, 3, 0) == DRONE_BANK_SOFT_RANDOM && panel_bank_turned(DRONE_BANK_SHAPE, DRONE_BANK_RATCHET2, -4, 0) == DRONE_BANK_GATE);
        check("the RANGE: BOTH, then UP", panel_bank_turned(DRONE_BANK_RANGE, 0, 1, 0) == 1 && panel_bank_turned(DRONE_BANK_RANGE, 1, 3, 0) == 1
              && panel_bank_turned(DRONE_BANK_RANGE, 1, -1, 0) == 0);
        {
            uint8_t knob[DRONE_BANK_NPARAM] = { 0 };
            knob[DRONE_BANK_SHAPE] = DRONE_BANK_SINE;
            panel_bank_turn(knob, DRONE_BANK_SHAPE, 2, 0);
            check("a new SHAPE sets the RANGE it fits: EXP up only", knob[DRONE_BANK_SHAPE] == DRONE_BANK_EXP && knob[DRONE_BANK_RANGE] == 1
                  && !drone_bank_both_ways[DRONE_BANK_EXP]);
            panel_bank_turn(knob, DRONE_BANK_SHAPE, -8, 0);
            check("GATE both ways", knob[DRONE_BANK_SHAPE] == DRONE_BANK_GATE && knob[DRONE_BANK_RANGE] == 0 && drone_bank_both_ways[DRONE_BANK_GATE]);
            panel_bank_turn(knob, DRONE_BANK_SHAPE, 1, 0);
            check("RATCHET 2 up only", knob[DRONE_BANK_SHAPE] == DRONE_BANK_RATCHET2 && knob[DRONE_BANK_RANGE] == 1);
            panel_bank_turn(knob, DRONE_BANK_RANGE, -1, 0);
            check("the RANGE turned: the shape stays", knob[DRONE_BANK_SHAPE] == DRONE_BANK_RATCHET2 && knob[DRONE_BANK_RANGE] == 0);
            knob[DRONE_BANK_SHAPE] = DRONE_BANK_SOFT_RANDOM;
            knob[DRONE_BANK_RANGE] = 1;
            panel_bank_turn(knob, DRONE_BANK_SHAPE, 1, 0);
            check("a turn at the end, where the SHAPE does not change: the RANGE set by hand stays", knob[DRONE_BANK_SHAPE] == DRONE_BANK_SOFT_RANDOM
                  && knob[DRONE_BANK_RANGE] == 1);
            panel_bank_turn(knob, DRONE_BANK_TRIGS, -3, 0);
            panel_bank_turn(knob, DRONE_BANK_LEVEL, 10, 0);
            check("any other value: only that one", knob[DRONE_BANK_TRIGS] == 1 && knob[DRONE_BANK_LEVEL] == 10 && knob[DRONE_BANK_RANGE] == 1);
        }
        {
            /* the bank's page, the LFO key up: an encoder turns LFO 4's own values; held: LFO 9's amounts onto them */
            static drone_controls_t c, before;
            uint32_t set, e, ok = 1;
            c.tempo = 60;
            for (e = 0; e < DRONE_BANK_NPARAM; e++)
                c.bank[4][e] = drone_bank_spec[e].start;
            panel_bank_apply(&c, 4, 9, panel_bank_holds(PANEL_BANK_STEPS, PANEL_ENC_VALUES + 2, 0, 0), 2, 0);
            panel_bank_apply(&c, 4, 9, panel_bank_holds(PANEL_BANK_WAVE, PANEL_ENC_KIND, 0, 0), 2, 0);
            panel_bank_apply(&c, 4, 9, panel_bank_holds(PANEL_BANK_WAVE, PANEL_ENC_MOVE, 0, 0), -5, 0);
            check("the key up: the knobs move the shown LFO's own values, ALGORITHM its SHAPE, PRESETS the tempo", c.bank[4][DRONE_BANK_TRIGS] == 6
                  && c.bank[4][DRONE_BANK_SHAPE] == DRONE_BANK_EXP && c.bank[4][DRONE_BANK_RANGE] == 1 && c.tempo == 55);
            panel_bank_apply(&c, 4, 9, panel_bank_holds(PANEL_BANK_MORE, PANEL_ENC_VALUES + 1, 0, 0), 12, 0);
            panel_bank_apply(&c, 4, 9, panel_bank_holds(PANEL_BANK_MORE, PANEL_ENC_KIND, 0, 0), -1, 0);
            check("the MORE layer: KNOB2 is the SLEW, ALGORITHM the RANGE", c.bank[4][DRONE_BANK_SLEW] == 12 && c.bank[4][DRONE_BANK_RANGE] == 0
                  && c.bank[4][DRONE_BANK_SHAPE] == DRONE_BANK_EXP);
            panel_bank_apply(&c, 4, 9, panel_bank_holds(PANEL_BANK_WAVE, PANEL_ENC_VALUES + 2, 0, 0), -1, 1);
            panel_bank_apply(&c, 4, 9, panel_bank_holds(PANEL_BANK_WAVE, PANEL_ENC_MOVE, 0, 0), 1, 1);
            check("with the snap key: the LEVEL from 100 to 50, the tempo from 55 to 60", c.bank[4][DRONE_BANK_LEVEL] == 50 && c.tempo == 60);
            c.bank[4][DRONE_BANK_LEVEL] = 100;
            c.bank[4][DRONE_BANK_SLEW] = 0;
            c.bank[4][DRONE_BANK_RANGE] = 1;
            c.tempo = 55;
            before = c;
            for (set = 0; set < PANEL_BANK_NSET; set++)
                for (e = PANEL_ENC_KIND; e <= PANEL_ENC_MOVE; e++)
                    panel_bank_apply(&c, 4, 9, panel_bank_holds(set, e, 1, 1), 15, 0);
            for (e = 0; e < DRONE_BANK_NCABLED; e++)
                ok &= c.bank_amount[4][e][9] == 15 && c.bank_amount[4][e][8] == 0 && c.bank_amount[9][e][4] == 0;
            check("the key held: the first three layers' knobs are LFO 9's amounts onto LFO 4's twelve values", ok);
            check("and PRESETS its amount onto the tempo, the same in each layer: four times", c.tempo_amount[9] == 60 && c.tempo_amount[4] == 0);
            check("the key held, no knob moves the shown LFO's own values or the tempo",
                  !memcmp(c.bank, before.bank, sizeof c.bank) && c.tempo == before.tempo);
            panel_bank_apply(&c, 4, 9, panel_bank_holds(PANEL_BANK_LOOP, PANEL_ENC_VALUES + 3, 1, 0), -2, 0);
            panel_bank_apply(&c, 4, 9, panel_bank_holds(PANEL_BANK_LOOP, PANEL_ENC_MOVE, 1, 0), 100, 0);
            check("an amount stops at -100 and 100", c.bank_amount[4][DRONE_BANK_TO_LSHIFT][9] == 13 && c.tempo_amount[9] == 100);
            panel_bank_apply(&c, 4, 9, panel_bank_holds(PANEL_BANK_LOOP, PANEL_ENC_VALUES + 3, 1, 0), 1, 1);
            panel_bank_apply(&c, 4, 9, panel_bank_holds(PANEL_BANK_LOOP, PANEL_ENC_MOVE, 1, 0), -1, 1);
            check("with the snap key: an amount to the next of -100, -50, 0, 50, 100", c.bank_amount[4][DRONE_BANK_TO_LSHIFT][9] == 50 && c.tempo_amount[9] == 50);
        }
        check("PRESETS: the tempo after its knob's steps, 10 .. 300 BPM", panel_tempo_turned(60, 1, 0) == 61 && panel_tempo_turned(60, -3, 0) == 57
              && panel_tempo_turned(12, -5, 0) == DRONE_TEMPO_LOW && panel_tempo_turned(298, 9, 0) == DRONE_TEMPO_HIGH
              && panel_tempo_turned(60, -100000, 0) == DRONE_TEMPO_LOW && panel_tempo_turned(60, 100000, 0) == DRONE_TEMPO_HIGH
              && panel_tempo_turned(5, 0, 0) == DRONE_TEMPO_LOW && panel_tempo_turned(900, 0, 0) == DRONE_TEMPO_HIGH && DRONE_TEMPO_START == 60);
        check("with the snap key: the tempo to the next ten", panel_tempo_turned(60, 1, 1) == 70 && panel_tempo_turned(63, 1, 1) == 70
              && panel_tempo_turned(63, -1, 1) == 60 && panel_tempo_turned(60, -1, 1) == 50 && panel_tempo_turned(295, 2, 1) == DRONE_TEMPO_HIGH
              && panel_tempo_turned(12, -1, 1) == DRONE_TEMPO_LOW && panel_tempo_turned(60, 3, 1) == 90);
        check("the LFO key held on the play page: KNOB1 to KNOB4 are the amounts onto TONE, SPEED, GRIT, LINK; ALGORITHM onto the STYLE, as it holds the STYLE; PRESETS onto the FREQ",
              panel_banked(PANEL_ENC_VALUES, 0, 0) == PLAY_BANKED_TONE && panel_banked(PANEL_ENC_VALUES + 1, 0, 0) == PLAY_BANKED_SPEED
              && panel_banked(PANEL_ENC_VALUES + 2, 0, 0) == PLAY_BANKED_GRIT && panel_banked(PANEL_ENC_VALUES + 3, 0, 0) == PLAY_BANKED_LINK
              && panel_banked(PANEL_ENC_KIND, 0, 0) == PLAY_BANKED_STYLE && panel_banked(PANEL_ENC_MOVE, 0, 0) == PLAY_BANKED_FREQ);
        check("in the second layer: KNOB1 is the amount onto the FREQ and KNOB3 onto the GAIN, as their knobs hold them; KNOB2 holds nothing; PRESETS as before",
              panel_banked(PANEL_ENC_VALUES, 1, 0) == PLAY_BANKED_FREQ && panel_banked(PANEL_ENC_VALUES + 1, 1, 0) < 0
              && panel_banked(PANEL_ENC_VALUES + 2, 1, 0) == PLAY_BANKED_GAIN && panel_banked(PANEL_ENC_MOVE, 1, 0) == PLAY_BANKED_FREQ);
        check("KNOB4 there is the amount onto OWN on a THROUGH voice, as its knob holds OWN then; on another voice it holds nothing",
              panel_banked(PANEL_ENC_VALUES + 3, 1, 1) == PLAY_BANKED_OWN && panel_banked(PANEL_ENC_VALUES + 3, 1, 0) < 0
              && panel_banked(PANEL_ENC_VALUES + 3, 0, 1) == PLAY_BANKED_LINK);
        {   /* the rule: each value that one of the four knobs holds, in each layer, takes an LFO */
            uint32_t layer, n, through, ok = 1;
            for (layer = 0; layer < PANEL_PLAY_LAYERS; layer++)
                for (n = 0; n < 4u; n++)
                    for (through = 0; through < 2u; through++)
                        ok &= (uint32_t)((panel_play_knob(PANEL_ENC_VALUES + n, (int)layer, (int)through) >= 0)
                                         == (panel_banked(PANEL_ENC_VALUES + n, (int)layer, (int)through) >= 0));
            check("each value that KNOB1 to KNOB4 hold, in each layer, takes an LFO's amount on the same knob", ok);
        }
        check("SELECT holds no amount; ALGORITHM none in the second layer", panel_banked(0, 0, 0) < 0 && panel_banked(0, 1, 0) < 0
              && panel_banked(PANEL_ENC_KIND, 1, 0) < 0 && panel_banked(7, 1, 0) < 0);
        check("an amount after its knob's steps, -100 .. 100", panel_amount_turned(0, 15, 0) == 15 && panel_amount_turned(0, -1, 0) == -1
              && panel_amount_turned(98, 5, 0) == 100 && panel_amount_turned(-98, -5, 0) == -100);
        {   /* an amount's knob, as any knob's: its own click is 5 */
            uint32_t at_ms = 0;
            int32_t amount = panel_amount_turned(0, panel_knob_steps(PANEL_AMOUNT_STEP, 1, 1000, &at_ms), 0);
            check("an amount's knob, notch by notch: 1 a click", amount == 1
                  && panel_amount_turned(amount, panel_knob_steps(PANEL_AMOUNT_STEP, -1, 1200, &at_ms), 0) == 0);
            check("turned faster: 3, then 5, 10 and 20 a click; it stops at 100",
                  panel_amount_turned(0, panel_knob_steps(PANEL_AMOUNT_STEP, 1, 1270, &at_ms), 0) == 3
                  && panel_amount_turned(0, panel_knob_steps(PANEL_AMOUNT_STEP, 1, 1310, &at_ms), 0) == 5
                  && panel_amount_turned(0, panel_knob_steps(PANEL_AMOUNT_STEP, -1, 1330, &at_ms), 0) == -10
                  && panel_amount_turned(90, panel_knob_steps(PANEL_AMOUNT_STEP, 1, 1335, &at_ms), 0) == 100);
        }
        check("no voice has an amount: no LFO is in use", panel_bank_used(voices) == 0);
        voices[1].bank_amount[PLAY_BANKED_GRIT][4] = -5;
        voices[2].bank_amount[PLAY_BANKED_FREQ][11] = 30;
        voices[2].bank_amount[PLAY_BANKED_TONE][4] = 1;
        check("an LFO is in use when any voice has an amount of it, on anything", panel_bank_used(voices) == (1u << 4 | 1u << 11));
    }

    puts(failed ? "panel_test: FAILED" : "panel_test: ok");
    return failed;
}
