/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kevin Sint */
/* The saved values: a record that is whole is taken, a record that is not (erased flash, a changed byte, another
 * layout) is left, and of two the one saved last is the one loaded. */
#include <stdio.h>
#include <string.h>
#include "store.h"

static int failed;
static void check(const char *what, int ok)
{
    if (!ok) {
        printf("FAIL: %s\n", what);
        failed = 1;
    }
}

static store_record_t slots[STORE_SLOTS];

int main(void)
{
    store_record_t *record = &slots[0];
    check("a record fits in a slot, below the bytes the update loader reads", sizeof(store_record_t) <= STORE_SLOT_ROOM);
    check("the two slots are the two sectors of the board's autosave area", STORE_SLOT_AT(0) == 0xE5000u && STORE_SLOT_AT(1) == 0xE6000u);

    check("a record that is all zero is not a saved one", !store_valid(record));
    memset(slots, 0xFF, sizeof slots);
    check("erased flash is not a saved record", !store_valid(record) && store_newest(slots) < 0);

    memset(slots, 0, sizeof slots);
    record->values.tempo = 120;
    record->values.play[1].freq = 6000;
    record->values.play[2].bank_amount[PLAY_BANKED_TONE][11] = -100;
    store_seal(record, 7);
    check("a sealed record is a saved one, with its count", store_valid(record) && record->count == 7);
    check("its values are as they were", record->values.tempo == 120 && record->values.play[1].freq == 6000
          && record->values.play[2].bank_amount[PLAY_BANKED_TONE][11] == -100);
    check("with one saved record, it is the newest", store_newest(slots) == 0);

    record->values.tempo = 121;
    check("a value changed after the seal: not a saved record", !store_valid(record));
    record->values.tempo = 120;
    record->count = 8;
    check("a count changed after the seal: not a saved record", !store_valid(record));
    store_seal(record, 7);
    record->layout++;
    check("a record of another layout is left", !store_valid(record));
    store_seal(record, 7);
    ((uint8_t *)record)[sizeof *record - 1] ^= 1u;
    check("a write cut short (the last byte wrong): not a saved record", !store_valid(record) && store_newest(slots) < 0);

    store_seal(&slots[0], 7);
    slots[1].values.tempo = 90;
    store_seal(&slots[1], 8);
    check("of two saved records, the one with the higher count", store_newest(slots) == 1);
    store_seal(&slots[0], 9);
    check("and the other way round", store_newest(slots) == 0);
    slots[0].check ^= 1u;
    check("the newest one broken: the one before it", store_newest(slots) == 1);

    printf(failed ? "store_test: FAILED\n" : "store_test: ok\n");
    return failed;
}
