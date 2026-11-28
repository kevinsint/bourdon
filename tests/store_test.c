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

    /* ---- a record of V0.38 (layout 1): its play voices had no band. It is taken, with the band open ---- */
    {
        static store_record_v1_t old;
        static store_values_t taken;
        uint32_t v, l, band_open = 1, amounts_none = 1;
        check("V0.38's play voice: 112 bytes; its record: the one it saved", sizeof(store_play_v1_t) == 112u
              && sizeof(store_values_v1_t) - offsetof(store_values_v1_t, mixer) == sizeof(store_values_t) - offsetof(store_values_t, mixer));
        memset(&old, 0, sizeof old);
        old.values.play[1].freq = 5000;
        old.values.play[1].link_kind = DRONE_LINK_FM;
        old.values.play[1].knob[PLAY_TONE] = 99;
        old.values.play[1].knob[PLAY_OWN] = 12;
        old.values.play[2].dis[2] = 70;
        old.values.play[2].bank_amount[PLAY_BANKED_OWN][11] = -33;
        old.values.mixer[2].value[0] = 77;
        old.values.tempo = 133;
        old.values.tempo_amount[4] = 25;
        old.magic = STORE_MAGIC;
        old.layout = 1;
        old.count = 12;
        old.check = store_crc(&old, sizeof old);
        memset(slots, 0, sizeof slots);
        memcpy(&slots[1], &old, sizeof old);    /* as the flash gives it: the record, and after it what the sector has */
        check("a record of V0.38 is a saved one", store_valid(&slots[1]) && store_newest(slots) == 1 && slots[1].count == 12);
        store_values_take(&slots[1], &taken);
        for (v = 0; v < DRONE_VOICES; v++) {
            band_open &= taken.play[v].knob[PLAY_LOW] == 0 && taken.play[v].knob[PLAY_POS] == PLAY_KNOB_MIDDLE
                      && taken.play[v].knob[PLAY_HIGH] == 127;
            for (l = 0; l < DRONE_BANK_LFOS; l++)
                amounts_none &= !taken.play[v].bank_amount[PLAY_BANKED_LOW][l] && !taken.play[v].bank_amount[PLAY_BANKED_POS][l]
                             && !taken.play[v].bank_amount[PLAY_BANKED_HIGH][l];
        }
        check("its values are taken as they were", taken.play[1].freq == 5000 && taken.play[1].link_kind == DRONE_LINK_FM
              && taken.play[1].knob[PLAY_TONE] == 99 && taken.play[1].knob[PLAY_OWN] == 12 && taken.play[2].dis[2] == 70
              && taken.play[2].bank_amount[PLAY_BANKED_OWN][11] == -33 && taken.mixer[2].value[0] == 77 && taken.tempo == 133
              && taken.tempo_amount[4] == 25);
        check("and the band each voice did not have is open, with no LFO on it", band_open && amounts_none);
        old.values.tempo = 134;
        memcpy(&slots[1], &old, sizeof old);
        check("a record of V0.38 changed after its seal: left", !store_valid(&slots[1]));
        store_seal(&slots[0], 3);
        slots[0].values.play[0].knob[PLAY_HIGH] = 40;
        store_seal(&slots[0], 3);
        store_values_take(&slots[0], &taken);
        check("a record of this layout is taken as it is", taken.play[0].knob[PLAY_HIGH] == 40 && slots[0].layout == STORE_LAYOUT && STORE_LAYOUT == 2u);
    }

    printf(failed ? "store_test: FAILED\n" : "store_test: ok\n");
    return failed;
}
