/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kevin Sint */
/* The mixer: a channel for each voice, after its patch. A channel has the voice's level, its pan, and two sends:
 * send 1 into the delay, send 2 into the reverb (the delay's echoes go into the reverb too). A channel's sends are
 * taken before its level (PRE: the voice can be out of the mix and still in the effects) or after it (POST).
 * The level is a fader: a log curve, 0.39 dB a place (the engine's drone_fader_q8).
 * mixer_apply() puts a channel into the engine's controls for its voice. */
#pragma once
#include <stdint.h>
#include "drone.h"

enum { MIXER_LEVEL, MIXER_PAN, MIXER_SEND_DELAY, MIXER_SEND_REVERB, MIXER_NVALUE };
#define MIXER_PAN_CENTRE 64             /* 0: all to the left; 128: all to the right, as far */
#define MIXER_FADER_DB 50               /* from the fader's top down to its place 1 */
static const param_spec_t mixer_spec[MIXER_NVALUE] = {
    [MIXER_LEVEL] = { 127, 4, 96 },             /* 12 dB down, a quarter: all the voices at once stay below the sum's flat top */
    [MIXER_PAN] = { 2 * MIXER_PAN_CENTRE, 4, MIXER_PAN_CENTRE },
    [MIXER_SEND_DELAY] = { 127, 4, 32 }, [MIXER_SEND_REVERB] = { 127, 4, 32 },
};

typedef struct {
    uint8_t value[MIXER_NVALUE];
    uint8_t post;                       /* the two sends are taken after the level */
} mixer_channel_t;

static mixer_channel_t mixer_channel_new(void)
{
    mixer_channel_t channel = { .post = 0 };
    uint32_t n;
    for (n = 0; n < MIXER_NVALUE; n++)
        channel.value[n] = mixer_spec[n].start;
    return channel;
}

/* a value after its knob's steps (its smallest ones: panel_knob_steps), or with `snap` after its clicks to the next
 * of its places (param_snapped). The pan stops in the centre when a turn goes over it, so the centre is there from
 * any place; the next turn leaves it */
static uint32_t mixer_turned(uint32_t value_of, uint32_t value, int32_t steps, int snap)
{
    param_spec_t spec = mixer_spec[value_of % MIXER_NVALUE];
    int32_t to;
    spec.per_click = 1;
    to = snap ? param_snapped(&spec, (int32_t)value, steps) : param_turned(&spec, (int32_t)value, steps);
    if (value_of == MIXER_PAN && ((value < MIXER_PAN_CENTRE && to > MIXER_PAN_CENTRE) || (value > MIXER_PAN_CENTRE && to < MIXER_PAN_CENTRE)))
        to = MIXER_PAN_CENTRE;
    return (uint32_t)to;
}

static uint32_t mixer_level_db(uint32_t level)          /* how far below its top the fader is, in dB (rounded); not for place 0 */
{
    return ((127u - (level > 127u ? 127u : level)) * MIXER_FADER_DB + 63u) / 127u;
}

static void mixer_apply(const mixer_channel_t *channel, drone_voice_controls_t *patch)
{
    uint32_t pan = channel->value[MIXER_PAN] > mixer_spec[MIXER_PAN].top ? mixer_spec[MIXER_PAN].top : channel->value[MIXER_PAN];
    patch->param[DRONE_LEVEL] = channel->value[MIXER_LEVEL] & 127u;
    patch->level_log = 1;
    patch->pan = (int8_t)((int32_t)pan - MIXER_PAN_CENTRE);
    patch->send = channel->value[MIXER_SEND_DELAY] & 127u;
    patch->send_reverb = channel->value[MIXER_SEND_REVERB] & 127u;
    patch->post = channel->post != 0;
}
