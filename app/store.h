/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kevin Sint */
/* The saved values: what the SAVE button writes into the flash and a start reads back, so that the instrument
 * starts as it was saved. All the values the player sets are in it: each voice's style, frequency, knobs, disruptor
 * amounts and bank amounts, the mixer, the end of the chain, the disruptors, the bank's LFOs with their cables, and
 * the tempo. Not in it: the mixer's mutes (all start muted), the open page, the MASTER knob (it has its own place).
 * The flash has two slots, the two sectors of the board's autosave area (bsp/docs/FLASH.md). A save goes into the
 * slot that does not hold the newest record, so a save that the power cuts leaves the one before it whole. A record
 * is taken only when its check is right: erased flash, a write cut short or a record of another layout are left, and
 * the instrument starts with its own values. A record of V0.38 (layout 1, its voices with no band) is taken too:
 * store_values_take gives its values, with each voice's band open.
 * A record stays below byte 0xF00 of its sector: the update loader looks for its own records above that. */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "style.h"
#include "mixer.h"

#define STORE_SLOTS 2u
#define STORE_SLOT_AT(slot) (0xE5000u + (slot) * 0x1000u)      /* the flash offset of a slot */
#define STORE_SLOT_ROOM 0xF00u
#define STORE_MAGIC 0x4E445242u         /* "BRDN" */
#define STORE_LAYOUT 2u                 /* a change to store_values_t, or to what one of its values means: the next
                                         * number, and a record of the old layout is left, or taken by store_values_take */
typedef struct {
    play_voice_t play[DRONE_VOICES];
    mixer_channel_t mixer[DRONE_VOICES];
    uint8_t reverb[DRONE_REVERB_NPARAM];        /* these: as drone_controls_t has them */
    uint8_t delay[DRONE_DELAY_NPARAM];
    uint8_t comp[DRONE_COMP_NPARAM];
    uint8_t dis[DRONE_DISRUPTORS][DRONE_DIS_NPARAM];
    uint8_t bank[DRONE_BANK_LFOS][DRONE_BANK_NPARAM];
    int8_t bank_amount[DRONE_BANK_LFOS][DRONE_BANK_NCABLED][DRONE_BANK_LFOS];
    int8_t tempo_amount[DRONE_BANK_LFOS];
    uint16_t tempo;
} store_values_t;

typedef struct {
    uint32_t magic, check;              /* the check is of all that follows it */
    uint32_t layout, count;             /* the count: one more at each save, so the highest is the newest */
    store_values_t values;
} store_record_t;
_Static_assert(sizeof(store_record_t) <= STORE_SLOT_ROOM, "a record fits in a slot");

/* layout 1, V0.38's: a play voice had 6 knobs and 8 amounts (no band); all after the voices is as now */
typedef struct {
    uint8_t link_kind;
    int8_t style_push;
    uint16_t freq;
    uint8_t knob[PLAY_OWN + 1];
    uint8_t dis[DRONE_DISRUPTORS];
    uint8_t dis_to[DRONE_DISRUPTORS];
    int8_t bank_amount[PLAY_BANKED_OWN + 1][DRONE_BANK_LFOS];
} store_play_v1_t;
typedef struct {
    store_play_v1_t play[DRONE_VOICES];
    mixer_channel_t mixer[DRONE_VOICES];
    uint8_t reverb[DRONE_REVERB_NPARAM];
    uint8_t delay[DRONE_DELAY_NPARAM];
    uint8_t comp[DRONE_COMP_NPARAM];
    uint8_t dis[DRONE_DISRUPTORS][DRONE_DIS_NPARAM];
    uint8_t bank[DRONE_BANK_LFOS][DRONE_BANK_NPARAM];
    int8_t bank_amount[DRONE_BANK_LFOS][DRONE_BANK_NCABLED][DRONE_BANK_LFOS];
    int8_t tempo_amount[DRONE_BANK_LFOS];
    uint16_t tempo;
} store_values_v1_t;
typedef struct {
    uint32_t magic, check, layout, count;
    store_values_v1_t values;
} store_record_v1_t;
_Static_assert(sizeof(store_values_v1_t) - offsetof(store_values_v1_t, mixer) == sizeof(store_values_t) - offsetof(store_values_t, mixer),
               "all after the voices is as layout 1 has it");

/* CRC-32 (0xEDB88320) of a record of `size` bytes: of its layout, its count and its values */
static uint32_t store_crc(const void *record, uint32_t size)
{
    const uint8_t *byte = (const uint8_t *)record + 2u * sizeof(uint32_t), *end = (const uint8_t *)record + size;
    uint32_t crc = 0xFFFFFFFFu, bit;
    for (; byte < end; byte++)
        for (crc ^= *byte, bit = 0; bit < 8u; bit++)
            crc = crc >> 1 ^ (crc & 1u ? 0xEDB88320u : 0u);
    return ~crc;
}
static uint32_t store_size(uint32_t layout)     /* a record's bytes in a layout; 0: a layout not known */
{
    return layout == STORE_LAYOUT ? sizeof(store_record_t) : layout == 1u ? sizeof(store_record_v1_t) : 0u;
}
static uint32_t store_check(const store_record_t *record)
{
    return store_crc(record, sizeof *record);
}

/* make the record, with its values in it, the save number `count` */
static void store_seal(store_record_t *record, uint32_t count)
{
    record->magic = STORE_MAGIC;
    record->layout = STORE_LAYOUT;
    record->count = count;
    record->check = store_check(record);
}

static int store_valid(const store_record_t *record)    /* of this layout, or of layout 1 */
{
    uint32_t size = store_size(record->layout);
    return record->magic == STORE_MAGIC && size && record->check == store_crc(record, size);
}

static void store_bytes(void *to, const void *from, uint32_t count)
{
    uint8_t *t = to;
    const uint8_t *f = from;
    while (count--)
        *t++ = *f++;
}
/* the values of a valid record into `values` (not in the record): of layout 1, each voice as it was, its band open
 * (its knobs at their start) with no LFO on it */
static void store_values_take(const store_record_t *record, store_values_t *values)
{
    const uint8_t *from = (const uint8_t *)&record->values;
    uint32_t v, k, l;
    if (record->layout == STORE_LAYOUT) {
        store_bytes(values, from, sizeof *values);
        return;
    }
    for (v = 0; v < DRONE_VOICES; v++) {
        store_play_v1_t old;
        play_voice_t *play = &values->play[v];
        store_bytes(&old, from + v * sizeof old, sizeof old);
        play->link_kind = old.link_kind;
        play->style_push = old.style_push;
        play->freq = old.freq;
        for (k = 0; k < PLAY_NKNOB; k++)
            play->knob[k] = k < sizeof old.knob ? old.knob[k] : (uint8_t)play_knob_spec[k].start;
        store_bytes(play->dis, old.dis, sizeof play->dis);
        store_bytes(play->dis_to, old.dis_to, sizeof play->dis_to);
        for (k = 0; k < PLAY_NBANKED; k++)
            for (l = 0; l < DRONE_BANK_LFOS; l++)
                play->bank_amount[k][l] = k <= PLAY_BANKED_OWN ? old.bank_amount[k][l] : 0;
    }
    store_bytes(values->mixer, from + offsetof(store_values_v1_t, mixer), sizeof *values - offsetof(store_values_t, mixer));
}

/* the slot of the record saved last, or -1: no slot has a saved record */
static int32_t store_newest(const store_record_t record[STORE_SLOTS])
{
    int32_t newest = -1;
    uint32_t slot;
    for (slot = 0; slot < STORE_SLOTS; slot++)
        if (store_valid(&record[slot]) && (newest < 0 || record[slot].count > record[newest].count))
            newest = (int32_t)slot;
    return newest;
}
