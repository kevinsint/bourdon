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
| PRESETS | an encoder | encoder 1 | assumed | VOICE page: WAVE |
| ALGORITHM | an encoder | encoder 2 | assumed | VOICE page: FINE |

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
| KNOB1 | encoder 3 | assumed | CUTOFF | TARGET | RETURN |
| KNOB2 | encoder 4 | assumed | RESONANCE | none | none |
| KNOB3 | encoder 5 | assumed | LEVEL | none | none |
| KNOB4 | encoder 6 | assumed | FADE | none | none |

The pages use the encoders in number order, so the first three parameters of a page are on the left knobs
(SELECT, PRESETS, ALGORITHM) and the next four on KNOB1 to KNOB4:

| Page | Encoder 0 | 1 | 2 | 3 | 4 | 5 | 6 |
|---|---|---|---|---|---|---|---|
| VOICE | PITCH | WAVE | FINE | CUTOFF | RESONANCE | LEVEL | FADE |
| LFO 1, 2, 3 | SPEED | WAVE | DEPTH | TARGET | none | none | none |
| REVERB | SEND (the selected voice) | DECAY | TONE | RETURN | none | none | none |

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
