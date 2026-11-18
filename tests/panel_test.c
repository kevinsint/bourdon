/* The panel: the LFO key goes round the pages; SEL held and the SELECT encoder choose the filter, and the choice
 * is the voice's filter only when SEL is let go. */
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
    panel_t panel = { 0, 0, 0 };
    uint32_t filter = DRONE_LADDER, i;
    drone_block_controls_t chain[DRONE_SLOTS] = { { 0 } };

    check("it starts on the voice's page", panel.page == PANEL_VOICE && PANEL_NPAGE == 2 + DRONE_LFOS);
    for (i = 1; i <= DRONE_LFOS; i++) {
        panel_next_page(&panel);
        check("the LFO key: the next LFO's page", panel.page == i);
    }
    panel_next_page(&panel);
    check("after the last LFO: the reverb", panel.page == PANEL_REVERB);
    panel_next_page(&panel);
    check("after the reverb: the voice again", panel.page == PANEL_VOICE);

    for (i = 0; i < DRONE_NPARAM; i++)
        check("the voice's page: an encoder for each parameter", panel_spec(PANEL_VOICE, i) == &drone_param_spec[i]);
    check("an LFO's page: its four parameters, then nothing",
          panel_spec(2, 0) == &drone_lfo_spec[0] && panel_spec(2, DRONE_LFO_NPARAM - 1) == &drone_lfo_spec[DRONE_LFO_NPARAM - 1]
          && !panel_spec(2, DRONE_LFO_NPARAM));
    check("the reverb's page: the voice's send, the reverb's three, then nothing",
          panel_spec(PANEL_REVERB, 0) == &drone_send_spec && panel_spec(PANEL_REVERB, 1) == &drone_reverb_spec[0]
          && panel_spec(PANEL_REVERB, DRONE_REVERB_NPARAM) == &drone_reverb_spec[DRONE_REVERB_NPARAM - 1]
          && !panel_spec(PANEL_REVERB, DRONE_REVERB_NPARAM + 1));

    filter = panel_choice(&panel, 0, 3, &drone_filter_spec, filter);
    check("the encoder without SEL does not choose a filter", filter == DRONE_LADDER && !panel.choosing);
    filter = panel_choice(&panel, 1, 0, &drone_filter_spec, filter);
    check("SEL down: the choice starts at the voice's filter", panel.choosing && panel.choice == DRONE_LADDER);
    filter = panel_choice(&panel, 1, 2, &drone_filter_spec, filter);
    check("two clicks: the choice is two further, the voice's filter is not yet", panel.choice == DRONE_POLIVOKS && filter == DRONE_LADDER);
    filter = panel_choice(&panel, 1, -1, &drone_filter_spec, filter);
    check("a click back", panel.choice == DRONE_MS20 && filter == DRONE_LADDER);
    filter = panel_choice(&panel, 0, 0, &drone_filter_spec, filter);
    check("SEL let go: the voice has the choice", filter == DRONE_MS20 && !panel.choosing);
    filter = panel_choice(&panel, 0, 0, &drone_filter_spec, filter);
    check("and keeps it", filter == DRONE_MS20);

    filter = panel_choice(&panel, 1, 0, &drone_filter_spec, filter);
    filter = panel_choice(&panel, 0, 0, &drone_filter_spec, filter);
    check("SEL down and up with no turn changes nothing", filter == DRONE_MS20);

    filter = panel_choice(&panel, 1, 100, &drone_filter_spec, filter);
    check("the choice stops at the last filter", panel.choice == DRONE_NFILTER - 1);
    filter = panel_choice(&panel, 1, -100, &drone_filter_spec, filter);
    check("and at the first", panel.choice == 0);
    filter = panel_choice(&panel, 0, 5, &drone_filter_spec, filter);
    check("clicks in the turn SEL is let go do not count", filter == 0);

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
    check("on OUT: the voice's level, fade and send", panel_patch_spec(PANEL_AT_OUT, 0, PANEL_ENC_VALUES, 0) == &drone_param_spec[DRONE_LEVEL]
          && panel_patch_spec(PANEL_AT_OUT, 0, PANEL_ENC_VALUES + 1, 0) == &drone_param_spec[DRONE_FADE]
          && panel_patch_spec(PANEL_AT_OUT, 0, PANEL_ENC_VALUES + 2, 0) == &drone_send_spec && !panel_patch_spec(PANEL_AT_OUT, 0, PANEL_ENC_VALUES + 3, 0)
          && !panel_patch_spec(PANEL_AT_OUT, 0, PANEL_ENC_KIND, 0));
    check("on the empty place: nothing to turn", !panel_patch_spec(PANEL_AT_NEW, 0, PANEL_ENC_VALUES, 0) && !panel_patch_spec(PANEL_AT_NEW, 0, PANEL_ENC_KIND, 0));

    check("a patch's LFO page: the wave on ALGORITHM, the speed on the first right encoder",
          panel_patch_lfo_param(PANEL_ENC_KIND) == DRONE_LFO_WAVE && panel_patch_lfo_param(PANEL_ENC_VALUES) == DRONE_LFO_SPEED
          && panel_patch_lfo_param(PANEL_ENC_VALUES + 1) < 0 && panel_patch_lfo_param(0) < 0 && panel_patch_lfo_param(PANEL_ENC_MOVE) < 0);
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
    puts(failed ? "panel_test: FAILED" : "panel_test: ok");
    return failed;
}
