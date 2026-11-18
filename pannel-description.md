# The FM-1 front panel

What is on the panel, where it is, and the name or number the code uses for each item.

How sure each code reference is:

- **measured**: read from this FM-1 with `tools/fm1_keys.py`, or proved in use.
- **named**: felucca-bsp gives it a name (`bsp/boards/fm1/fm1_input.h`, `bsp/boards/fm1/fm1_adc.h`).
- **assumed**: taken from the panel order and not yet checked on the FM-1. Check it before the code depends on it.

## The layout

```
 MASTER   SELECT                          KNOB1  KNOB2  KNOB3  KNOB4
 PRESETS  ALGORITHM      [ screen ]
                                          FX    SEL   ENV   LFO   EDIT  GLO
 OCT-     OCT+                            HOME  SAVE  ARP   SEQ   PLAY  REC

 [ keyboard: 27 keys, F to G, the black keys labelled OP1 ... POLY ]
```

The exact place of the four left knobs in their cluster is not confirmed (drawn here as two rows of two).

## Top left: four knobs

| Label | What it is | Code reference | Sure? | Use in DR1 today |
|---|---|---|---|---|
| MASTER | a potentiometer (it has end stops) | `fm1_adc_read(FM1_ADC_MASTER)`, 0..1023 | named | the output level: `drone_controls.master` |
| SELECT | an encoder (no end stops, clicks) | encoder 0: `fm1_enc_take(0)`, `ENC_SELECT` in `app/main.c` | measured (in use) | VOICE page: PITCH. With SEL held: the filter |
| PRESETS | an encoder | encoder 6 | measured | VOICE page: FADE |
| ALGORITHM | an encoder | encoder 1 | measured | VOICE page: WAVE |

## Below them: two buttons

| Label | Code reference | Sure? | Use in DR1 today |
|---|---|---|---|
| OCT- | button 0: `FM1_BTN_OCT_DOWN` | named | the selected voice an octave down; held with OCT+: the version and the load on the screen |
| OCT+ | button 1: `FM1_BTN_OCT_UP` | named | the selected voice an octave up |

## Centre: the screen

A square colour panel, 240 by 240 pixels (ST7789, RGB565). Driver: `bsp/boards/fm1/fm1_lcd.h`
(`fm1_lcd_fill_rect`, `fm1_lcd_blit`). Text: our 5 by 7 font, `app/font.h`. What it shows: `app/view.h`.

## Right of the screen: four knobs

All four are encoders.

| Label | Code reference | Sure? | VOICE page | LFO page | REVERB page |
|---|---|---|---|---|---|
| KNOB1 | encoder 2 | measured | FINE | DEPTH | TONE |
| KNOB2 | encoder 3 | measured | CUTOFF | TARGET | RETURN |
| KNOB3 | encoder 4 | measured | RESONANCE | none | none |
| KNOB4 | encoder 5 | measured | LEVEL | none | none |

The encoders' numbers were read on Kevin's FM-1 on 2026-11-17, by turning each knob: SELECT 0, ALGORITHM 1, KNOB1 2,
KNOB2 3, KNOB3 4, KNOB4 5, PRESETS 6. The pages of OP4 to OP6 use the encoders in number order:

| Page | Encoder 0 | 1 | 2 | 3 | 4 | 5 | 6 |
|---|---|---|---|---|---|---|---|
| VOICE | PITCH | WAVE | FINE | CUTOFF | RESONANCE | LEVEL | FADE |
| LFO 1, 2, 3 | SPEED | WAVE | DEPTH | TARGET | none | none | none |
| REVERB | SEND (the selected voice) | DECAY | TONE | RETURN | none | none | none |

OP1 to OP3 are patches from V0.11: a chain of blocks in place of the fixed voice. From V0.12 OCT- is their shift
key, and their pages (PATCH, LFO 1, LFO 2, LFO 3, FEEDBACK from V0.14, REVERB) use the controls this way:

| Control | Alone | With OCT- held (shift) | With the LFO key held |
|---|---|---|---|
| SELECT (encoder 0) | the cursor along the chain: each block, the empty place, OUT | the block for the cursor's place: none (take it away), VCO, VCF, VCA, FX; applied when OCT- is let go | none |
| PRESETS (encoder 6) | none | the block one place left or right | none |
| ALGORITHM (encoder 1) | the block's kind: the VCO's wave, the VCF's filter. On an LFO page: the LFO's wave | the same | none |
| KNOB1 (encoder 2) | VCO: PITCH. VCF: CUTOFF. VCA: LEVEL. FX: MIX. OUT: LEVEL. On an LFO page: SPEED | VCO: FINE. The others: the same value in the smallest steps | the amount of the LFO's cable to that value, 5 a click (with OCT-: 1) |
| KNOB2 (encoder 3) | VCF: RESONANCE. FX: DECAY. OUT: FADE | the same value in the smallest steps | the cable to that value (not to FADE) |
| KNOB3 (encoder 4) | FX: TONE. OUT: SEND | the same value in the smallest steps | the cable to SEND |
| KNOB4 (encoder 5) | none | none | none |
| OCT+ | the VCO under the cursor an octave up | an octave down | the same |
| LFO key | pressed and let go: the next page | the same | - |
| SEL | held: the version and the load on the screen | the same | the same |

On an LFO page (V0.14): KNOB1 is the SPEED and KNOB2 the DEPTH, which all the LFO's cables share. With the LFO key held
there, KNOB1 and KNOB2 are the cables from this LFO to the speed and the depth of the first other LFO, KNOB3 and KNOB4
to the second other LFO (from LFO 2: LFO 1, then LFO 3).

On the FEEDBACK page (V0.14, the time and the scope from V0.15): KNOB1 is the place of the block the sound is taken
after, KNOB2 the place of the block it goes back in before, KNOB3 the LEVEL (0: none, 64: as loud as it came, 127: twice),
KNOB4 the TIME, 0.36 to 6.14 ms: 4 frames a click (0.09 ms), 1 frame with OCT- (0.02 ms). With the LFO key held, KNOB3 is
the cable from the LFO to the level. The scope at the left shows 9.4 ms of the sound that comes down the chain (white) and
of the sound the cable brings back (green), ten times a second.

The FX block (V0.16) is the reverb in the chain. There is one reverb: the first FX block of the first patch that has
one gets it, another FX block lets the sound through (its name shows NO REVERB). While a patch has an FX block, the SEND of
every voice goes nowhere and the REVERB page shows IN OP1 (or 2, 3) in place of the send. An LFO's cable can move the MIX.

The LFO whose cables the knobs turn is the one of the open LFO page, or of the LFO page that was open last (LFO 1 at
power-on). The REVERB page of a patch is as the other voices' (SEND, DECAY, TONE, RETURN on encoders 0 to 3: SELECT, ALGORITHM, KNOB1, KNOB2).

Code: `panel_patch_value`, `panel_patch_spec`, `panel_patch_cable`, `panel_patch_put` and `panel_patch_move` in
`app/panel.h`; `patch_controls` in `app/main.c`.

Code: `panel_spec(page, encoder)` in `app/panel.h`; the parameter lists are `drone_param_spec`, `drone_lfo_spec`,
`drone_send_spec` and `drone_reverb_spec` in `app/drone.h`. `FM1_NENC` is 7.

## Under the right knobs: twelve function buttons, two rows of six

Buttons are numbered 0 to 13 (`NBUTTON` in `app/main.c`; pressed now: `fm1_in.buttons`, bit n).

Top row:

| Label | Code reference | Sure? | Use in DR1 today |
|---|---|---|---|
| FX | button 2 | assumed | all the voices off |
| SEL | button 3: `BTN_SEL` in `app/main.c` | measured | held, with SELECT: choose the filter |
| ENV | button 4 | assumed | all the voices off |
| LFO | button 5: `BTN_LFO` in `app/main.c` | measured | the next page: VOICE, LFO 1, LFO 2, LFO 3, REVERB |
| EDIT | button 6 | assumed | all the voices off |
| GLO | button 7 | assumed | all the voices off |

Bottom row:

| Label | Code reference | Sure? | Use in DR1 today |
|---|---|---|---|
| HOME | button 8 | assumed | all the voices off |
| SAVE | button 9 | assumed | all the voices off |
| ARP | button 10 | assumed | all the voices off |
| SEQ | button 11 | assumed | all the voices off |
| PLAY/STOP | button 12 | assumed | all the voices off |
| REC | button 13 | assumed | all the voices off |

SEL at 3 and LFO at 5 agree with a top row that counts 2 to 7 from the left; the other ten numbers come from that
pattern only.

Each button has a light: `fm1_led_key(n, on)`, n = the button's number.

## Bottom: the keyboard

27 keys, a little more than two octaves, from F to G. The keys are numbered 0 to 26 from the left (`NNOTE` in
`app/main.c`; pressed now: `fm1_in.notes`, bit n). Key 0 is F, so key 1 is the first black key.

Each key has a light: `fm1_led_key(14 + n, on)`, n = the key's number.

The black keys and their labels:

| Label | Note | Code reference | Sure? | Use in DR1 today |
|---|---|---|---|---|
| OP1 | F sharp | key 1: `voice_key[0]` in `app/main.c` | measured | voice 1: a short press toggles it, a long press selects it |
| OP2 | G sharp | key 3: `voice_key[1]` | measured (in use) | voice 2 |
| OP3 | A sharp | key 5: `voice_key[2]` | measured (in use) | voice 3 |
| OP4 | C sharp | key 8: `voice_key[3]` | assumed | voice 4 |
| OP5 | D sharp | key 10: `voice_key[4]` | assumed | voice 5 |
| OP6 | F sharp | key 13: `voice_key[5]` | assumed | voice 6 |
| PIT (pitch) | G sharp | key 15 | assumed | none |
| GLO (global) | A sharp | key 17 | assumed | none |
| MONO | C sharp | key 20 | assumed | none |
| POLY | D sharp | key 22 | assumed | none |

27 keys from F have an eleventh black key (F sharp, key 25). It has no label in this list.

The white keys (keys 0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26) have no use in DR1 today.

## To check an assumed number

- A button or a key: run `tools/fm1_keys.py` and press it; it prints the number.
- An encoder: on the VOICE page, turn the knob and read the large name at the bottom of the screen. It must be the
  parameter the table above gives for that knob.
