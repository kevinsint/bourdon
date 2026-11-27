/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kevin Sint */
/* The saved values: what the SAVE button writes into the flash and a start reads back, so that the instrument
 * starts as it was saved. All the values the player sets are in it: each voice's style, frequency, knobs, disruptor
 * amounts and bank amounts, the mixer, the end of the chain, the disruptors, the bank's LFOs with their cables, and
 * the tempo. Not in it: which voices are on (all start off), the open page, the MASTER knob (it has its own place).
 * The flash has two slots, the two sectors of the board's autosave area (bsp/docs/FLASH.md). A save goes into the
 * slot that does not hold the newest record, so a save that the power cuts leaves the one before it whole. A record
 * is taken only when its check is right: erased flash, a write cut short or a record of another layout are left, and
 * the instrument starts with its own values.
 * A record stays below byte 0xF00 of its sector: the update loader looks for its own records above that. */
#pragma once
#include <stdint.h>
#include "style.h"
#include "mixer.h"

#define STORE_SLOTS 2u
#define STORE_SLOT_AT(slot) (0xE5000u + (slot) * 0x1000u)      /* the flash offset of a slot */
#define STORE_SLOT_ROOM 0xF00u
#define STORE_MAGIC 0x4E445242u         /* "BRDN" */
#define STORE_LAYOUT 1u                 /* a change to store_values_t, or to what one of its values means: the next
                                         * number, and a record of the old layout is left */
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

static uint32_t store_check(const store_record_t *record)      /* CRC-32 (0xEDB88320) of the layout, the count and the values */
{
    const uint8_t *byte = (const uint8_t *)&record->layout, *end = (const uint8_t *)(record + 1);
    uint32_t crc = 0xFFFFFFFFu, bit;
    for (; byte < end; byte++)
        for (crc ^= *byte, bit = 0; bit < 8u; bit++)
            crc = crc >> 1 ^ (crc & 1u ? 0xEDB88320u : 0u);
    return ~crc;
}

/* make the record, with its values in it, the save number `count` */
static void store_seal(store_record_t *record, uint32_t count)
{
    record->magic = STORE_MAGIC;
    record->layout = STORE_LAYOUT;
    record->count = count;
    record->check = store_check(record);
}

static int store_valid(const store_record_t *record)
{
    return record->magic == STORE_MAGIC && record->layout == STORE_LAYOUT && record->check == store_check(record);
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
