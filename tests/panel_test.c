/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kevin Filteau */
/* The panel: the LFO key goes round the pages; the SELECT encoder chooses a block while the shift key is held, and
 * the choice is the place's only when it is let go; a patch's chain, its LFOs and its feedback on the encoders. */
#include <stdio.h>
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

    check("it starts on the voice's page", panel.page == PANEL_VOICE && PANEL_NPATCH == 2 + DRONE_LFOS && PANEL_NPAGE == PANEL_NPATCH + 5);
    for (i = 1; i <= DRONE_LFOS; i++) {
        panel_next_page(&panel);
        check("the LFO key: the next LFO's page", panel.page == i);
    }
    panel_next_page(&panel);
    check("after the last LFO: the feedback's page", panel.page == PANEL_FEEDBACK && panel_page_lfo(PANEL_FEEDBACK) < 0
          && panel_page_lfo(PANEL_VOICE) < 0 && panel_page_lfo(2) == 1 && !panel_in_eoc(&panel));
    panel_glo_key(&panel);                      /* the GLO key: the mixer, then the end of the chain, its delay first */
    check("the GLO key: the mixer", panel.page == PANEL_MIXER && !panel_in_eoc(&panel));
    panel_next_page(&panel);
    panel_eoc_select(&panel, 1);
    check("the LFO key and the end of the chain's SELECT do nothing on the mixer", panel.page == PANEL_MIXER);
    panel_glo_key(&panel);
    check("the GLO key again: the end of the chain, on the delay", panel.page == PANEL_DELAY && panel_in_eoc(&panel) && panel_page_lfo(PANEL_DELAY) < 0);
    panel_next_page(&panel);
    check("the LFO key does nothing there", panel.page == PANEL_DELAY);
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
    check("a cable's amount: 5 a click, 1 with shift, to the left it is negative, it stops at -100 and 100",
          panel_amount_turned(0, 2, 0) == 10 && panel_amount_turned(0, -3, 0) == -15 && panel_amount_turned(10, 1, 1) == 11
          && panel_amount_turned(90, 9, 0) == 100 && panel_amount_turned(-90, -1000, 0) == -100);
    /* ---- the play page ---- */
    {
        panel_t play = { .page = PANEL_PLAY };
        panel_next_page(&play);
        check("on the play page the LFO key opens no page", play.page == PANEL_PLAY && !panel_in_eoc(&play));
        panel_glo_key(&play);
        panel_glo_key(&play);
        check("the GLO key two times: from the play page over the mixer into the end of the chain", play.page == PANEL_DELAY);
        panel_eoc_select(&play, 9);
        check("SELECT there stops at the compressor: the play page and the mixer are not its pages", play.page == PANEL_COMP);
        panel_glo_key(&play);
        check("the GLO key again: back on the play page", play.page == PANEL_PLAY);

        check("a slow click of SELECT: a very small step, 1/64 of a semitone", panel_freq_turned(5376, 1, 1, 0, 0) == 5376 + 2
              && panel_freq_turned(5376, -3, -3, 0, 0) == 5376 - 6 && PANEL_FREQ_STEP == 2);
        check("a faster turn: a click moves it 4 times further each time the speed doubles: 8, 32, then a semitone",
              panel_freq_turned(5376, 1, 2, 0, 0) == 5376 + 8 && panel_freq_turned(5376, -1, -4, 0, 0) == 5376 - 32
              && panel_freq_turned(5376, 1, 8, 0, 0) == 5376 + 128 && panel_freq_turned(5376, 3, 12, 0, 0) == 5376 + 96);
        check("with shift: the smallest steps, at any speed", panel_freq_turned(5376, -3, -24, 1, 0) == 5376 - 3);
        check("the frequency stops at its two ends", panel_freq_turned(10, -5, -40, 0, 0) == 0
              && panel_freq_turned(PLAY_FREQ_TOP - 10, 5, 40, 0, 0) == PLAY_FREQ_TOP && panel_freq_turned(100, -100000, -800000, 0, 0) == 0
              && panel_freq_turned(PLAY_FREQ_TOP - 100, 100000, 800000, 0, 0) == PLAY_FREQ_TOP);
        check("the octave button: twice the frequency, or half", panel_freq_turned(5376, 0, 0, 0, 1) == 5376 + 12 * 128
              && panel_freq_turned(5376, 0, 0, 0, -1) == 5376 - 12 * 128);
        check("an octave that does not fit is not taken", panel_freq_turned(100, 0, 0, 0, -1) == 100
              && panel_freq_turned(PLAY_FREQ_TOP - 100, 0, 0, 0, 1) == PLAY_FREQ_TOP - 100);

        check("KNOB1 to KNOB4: TONE, MOVE, GRIT, LINK", panel_play_knob(PANEL_ENC_VALUES, 0) == PLAY_TONE
              && panel_play_knob(PANEL_ENC_VALUES + 1, 0) == PLAY_MOVE && panel_play_knob(PANEL_ENC_VALUES + 2, 0) == PLAY_GRIT
              && panel_play_knob(PANEL_ENC_VALUES + 3, 0) == PLAY_LINK);
        check("with shift: KNOB2 is SPEED; the others have none (the level and the sends are the mixer's)", panel_play_knob(PANEL_ENC_VALUES, 1) < 0
              && panel_play_knob(PANEL_ENC_VALUES + 1, 1) == PLAY_SPEED && panel_play_knob(PANEL_ENC_VALUES + 2, 1) < 0
              && panel_play_knob(PANEL_ENC_VALUES + 3, 1) < 0);
        check("the left encoders and PRESETS hold no knob", panel_play_knob(0, 0) < 0 && panel_play_knob(PANEL_ENC_KIND, 0) < 0
              && panel_play_knob(PANEL_ENC_MOVE, 1) < 0);
        check("ALGORITHM with shift goes through the link's kinds: FOLLOW first, then THROUGH",
              param_turned(&drone_link_kind_spec, DRONE_LINK_FOLLOW, 1) == DRONE_LINK_THROUGH
              && param_turned(&drone_link_kind_spec, DRONE_LINK_THROUGH, 5) == DRONE_NLINK - 1 && drone_link_kind_spec.start == DRONE_LINK_FOLLOW);
        check("ALGORITHM goes through all the styles", panel_style_spec.top == STYLE_NSTYLE - 1 && panel_style_spec.per_click == 1);
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
        check("the pan from each end comes back to the centre", mixer_turned(MIXER_PAN, 128, -16, 0) == MIXER_PAN_CENTRE
              && mixer_turned(MIXER_PAN, 0, 16, 0) == MIXER_PAN_CENTRE);
        check("the pan stops in the centre when a turn goes over it, from any place", mixer_turned(MIXER_PAN, 61, 1, 0) == MIXER_PAN_CENTRE
              && mixer_turned(MIXER_PAN, 66, -3, 0) == MIXER_PAN_CENTRE && mixer_turned(MIXER_PAN, 63, 9, 1) == MIXER_PAN_CENTRE);
        check("and the next turn leaves the centre", mixer_turned(MIXER_PAN, MIXER_PAN_CENTRE, 1, 0) == MIXER_PAN_CENTRE + 4
              && mixer_turned(MIXER_PAN, MIXER_PAN_CENTRE, -1, 1) == MIXER_PAN_CENTRE - 1);
        check("the other values turn as any value: 4 a click, 1 with shift, and stop at the ends", mixer_turned(MIXER_LEVEL, 96, 2, 0) == 104
              && mixer_turned(MIXER_LEVEL, 96, -1, 1) == 95 && mixer_turned(MIXER_SEND_REVERB, 126, 3, 0) == 127 && mixer_turned(MIXER_LEVEL, 2, -1, 0) == 0);
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
        check("no click: nothing, and the time of the last click stays", panel_velocity(0, 5000, &turned_ms) == 0 && turned_ms == 0);
        check("a click after a long time: one click", panel_velocity(1, 5000, &turned_ms) == 1 && turned_ms == 5000);
        check("the next click 100 ms later: slow, one click", panel_velocity(-1, 5100, &turned_ms) == -1);
        check("a click 40 ms later: two", panel_velocity(1, 5140, &turned_ms) == 2);
        check("a click 20 ms later: four", panel_velocity(-1, 5160, &turned_ms) == -4);
        check("a click 5 ms later: eight", panel_velocity(1, 5165, &turned_ms) == 8);
        check("three clicks in 60 ms: 20 ms each, four times three", panel_velocity(3, 5225, &turned_ms) == 12);
        check("clicks in the same millisecond: the fastest", panel_velocity(-2, 5225, &turned_ms) == -16);
        check("then a pause: one click again", panel_velocity(1, 6000, &turned_ms) == 1);
    }

    puts(failed ? "panel_test: FAILED" : "panel_test: ok");
    return failed;
}
