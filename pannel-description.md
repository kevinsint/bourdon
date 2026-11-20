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
| SELECT | an encoder (no end stops, clicks) | encoder 0: `fm1_enc_take(0)`, `ENC_SELECT` in `app/main.c` | measured (in use) | the selected voice's frequency (FREQ); with OCT- held: in the smallest steps |
| PRESETS | an encoder | encoder 6 | measured | none |
| ALGORITHM | an encoder | encoder 1 | measured | the selected voice's style |

## Below them: two buttons

| Label | Code reference | Sure? | Use in DR1 today |
|---|---|---|---|
| OCT- | button 0: `FM1_BTN_OCT_DOWN` | named | the shift key (see the table of the controls below) |
| OCT+ | button 1: `FM1_BTN_OCT_UP` | named | the selected voice's frequency times two; with OCT- held: divided by two |

## Centre: the screen

A square colour panel, 240 by 240 pixels (ST7789, RGB565). Driver: `bsp/boards/fm1/fm1_lcd.h`
(`fm1_lcd_fill_rect`, `fm1_lcd_blit`). Text: our 5 by 7 font, `app/font.h`. What it shows: `app/view.h`.

## Right of the screen: four knobs

All four are encoders.

| Label | Code reference | Sure? |
|---|---|---|
| KNOB1 | encoder 2 | measured |
| KNOB2 | encoder 3 | measured |
| KNOB3 | encoder 4 | measured |
| KNOB4 | encoder 5 | measured |

The encoders' numbers were read on Kevin's FM-1 on 2026-11-17, by turning each knob: SELECT 0, ALGORITHM 1, KNOB1 2,
KNOB2 3, KNOB3 4, KNOB4 5, PRESETS 6.

### The play page (V0.20)

From V0.20 the instrument is played, not built. There are three voices, OP1 to OP3. Each voice has a style: a patch
made in advance (`app/style.h`: FLOOR, REED, GLASS, GROWL, TIDE, PULSE). There are no notes: a voice's frequency is
free, in steps of 1/128 of a semitone, and the screen shows it in Hz.

| Control | Alone | With OCT- held (shift) |
|---|---|---|
| OP1, OP2, OP3 keys | a short press turns the voice on, or off (V0.26); held 1 s: it is selected; held while a knob or OCT+ moves: it is selected at once, and the move is for it | the same |
| SELECT (encoder 0) | FREQ (V0.27): a slow click is 1/64 semitone; turned faster, a click is 1/16, 1/4, then 1 semitone | FREQ, 1/128 semitone a click at any speed |
| ALGORITHM (encoder 1) | the voice's style | the kind of the voice's link (V0.22): FOLLOW, THROUGH |
| KNOB1 (encoder 2) | TONE: dark to bright | none |
| KNOB2 (encoder 3) | MOVE: how deep the style's LFOs go | SPEED: the LFOs' speed |
| KNOB3 (encoder 4) | GRIT: clean to rough (the resonance, the feedback cable's level) | none |
| KNOB4 (encoder 5) | LINK (V0.21): how much the voice takes from the voice before it; 0 at power-on: the voice is alone | none |
| OCT+ | the frequency times two | divided by two |
| GLO | the next page: play, the mixer, the end of the chain, then play again; its light is on in the mixer and the end of the chain | the same |
| SEL | held: the version and the load on the screen | the same |

A knob at 64 leaves the patch as its style has it. The screen shows the selected voice's sound (9.4 ms of it, ten
times a second), a dial for each right knob, and the name and the value of the knob turned last.

The links (V0.21) go round a ring: OP2 listens to OP1, OP3 to OP2, OP1 to OP3. Each voice's link has a kind (V0.22):

- FOLLOW (at power-on): the louder the other voice is (after its fade, before its LEVEL), the higher this voice's
  cutoff, up to 3 octaves. It is one render call late (5.8 ms).
- THROUGH: the other voice's sound (after its fade, before its LEVEL) goes down this voice's chain with this voice's
  own oscillators: through its filter, its feedback cable and its level. OP2 and OP3 get it with no delay; OP1 gets
  OP3's sound one render call late (5.8 ms), which closes the ring. This voice must be on to let it through.

A voice that is off gives nothing. Over the voices' boxes the screen has an arrow for each open link, thicker as the
LINK is more open, amber for the selected voice's, in dashes for FOLLOW and whole for THROUGH; the selected voice's
kind has its name at the right of the knob's value. Engine: `link` and `link_kind` in `drone_voice_controls_t`,
`loud` and `sent` in `drone_voice_t` (`app/drone.h`).

Code: `style_list` and `style_patch` in `app/style.h`; `panel_freq_turned`, `panel_play_knob` and `panel_voice_key` in
`app/panel.h`; `play_controls` in `app/main.c`; `view_play` in `app/view.h`.

### The mixer (V0.24)

The mixer is after the voices: a channel for each voice, with its level, its pan and two sends. SEND 1 goes into the
delay and SEND 2 into the reverb; the delay's echoes still go into the reverb. A channel's two sends are taken before
its level (PRE, at power-on) or after it (POST). At power-on: the level 12 dB down (place 96), the pan in the centre,
the two sends 32.

The level is a fader with a log curve (V0.25): 0 dB at the top, 0.39 dB a place, 50 dB down at place 1, silent at 0;
the screen shows it in dB. The pan (V0.25) goes as far to the right as to the left (L 64 .. 0 .. R 64) and stops at 0
when a turn goes over the centre; the next turn leaves it.

| Control | Alone | With OCT- held (shift) |
|---|---|---|
| SELECT (encoder 0) | the channel: none (at power-on), OP1, OP2, OP3; a selected channel's voice is the selected voice | the same |
| OP1, OP2, OP3 keys | as on the play page; the voice it selects is the selected channel too | the same |
| KNOB1 to KNOB3, no channel selected | the levels of OP1, OP2, OP3 | the same, in the smallest steps |
| KNOB4, no channel selected | none | none |
| KNOB1 to KNOB4, a channel selected | its LEVEL, its PAN, its SEND 1, its SEND 2 | the same, in the smallest steps |
| OCT+ | the selected channel's sends: PRE or POST | the same |

The pan in the centre has all of the voice at both sides; to one side, the other side goes down to nothing. The
screen has a strip for each voice over its box (the pan's mark, the level's fader, the two sends, PRE or POST), the
selected channel's with the amber frame, and a row with the name and the value of what each right knob holds.

Code: `app/mixer.h`; `panel_mixer_value`, `panel_mixer_key` and `panel_glo_key` in `app/panel.h`; `mixer_controls` in
`app/main.c`; `view_mixer` in `app/view.h`; `pan`, `send`, `send_reverb` and `post` in `drone_voice_controls_t`.

### An encoder's velocity (V0.25)

A value's encoder turned fast moves the value further: its clicks count 1, 2, 4 or 8 times, when a click took 60 ms
or more, 30 ms, 15 ms, or less. This is for FREQ and the four right knobs on the play page, the mixer and the end of
the chain. Not for a choice (the style, the kind of link, a channel, a page), and not with OCT- held for the smallest
steps. Code: `panel_velocity` in `app/panel.h`, `enc_take_value` in `app/main.c`.

### The patch's pages (no key opens them from V0.20; they wait for the EDIT key)

A voice is a patch (from V0.11): a chain of blocks. OCT- is the
shift key (from V0.12), and a patch's pages (PATCH, LFO 1, LFO 2, LFO 3, FEEDBACK from V0.14) use the controls this
way (the REVERB page moved to the end of the chain in V0.19, see below):

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

On the FEEDBACK page (V0.14, the time and the scope from V0.15, the names and the cable over the row from V0.18): KNOB1 is OUTPUT, the place of the block the sound is taken
after, KNOB2 is INPUT, the place of the block it goes back in before (arrows on the cable show the direction), KNOB3 the LEVEL (0: none, 64: as loud as it came, 127: twice),
KNOB4 the TIME, 0.36 to 6.14 ms: 4 frames a click (0.09 ms), 1 frame with OCT- (0.02 ms). With the LFO key held, KNOB3 is
the cable from the LFO to the level. The scope at the left shows 9.4 ms of the sound that comes down the chain (white) and
of the sound the cable brings back (green), ten times a second.

The FX block (V0.16) is the reverb in the chain. There is one reverb: the first FX block of the first patch that has
one gets it, another FX block lets the sound through (its name shows NO REVERB). While a patch has an FX block, the SEND of
every voice goes into the delay only and the REVERB page shows IN OP1 (or 2, 3), with its own three values dim. An LFO's
cable can move the MIX.

The LFO whose cables the knobs turn is the one of the open LFO page, or of the LFO page that was open last (LFO 1 at
power-on).

### The end of the chain: EOC (V0.19)

The GLO button goes from the play page to the mixer, then to the end of the chain, what comes after the mixer; its
light stays on in the two. GLO again goes back to the play page. The screen shows EOC in the place of OP1, and the path as a row: the voices' SEND
goes into the delay (DLY), the delay into the reverb (REV); the voices themselves (DRY) join them before the
compressor (COMP), then OUT. SELECT moves the cursor: DLY, REV, COMP. KNOB1 to KNOB4 are the open effect's values;
with OCT- held they move in the smallest steps. The LFO key does nothing there.

| Effect | KNOB1 | KNOB2 | KNOB3 | KNOB4 |
|---|---|---|---|---|
| DELAY | TIME, 1 ms to 0.99 s (small steps at the short end; a new time bends the echoes, as a tape) | RPT, the repeats: 0 one echo, 127 they go on | TONE, the echoes' highs | LVL, the echoes heard and into the reverb; 0 at power-on: no delay |
| REVERB | none from V0.24 (a voice's sends are the mixer's) | the DECAY's time | TONE | RET, the return |
| COMP | THR, the threshold in dB below a full sample, 0 to -47 | the ratio, 1.0 TO 1 (nothing, at power-on) to LIMIT | the release: 20 ms to 2 s for 20 dB | GAIN, 0 to 24 dB louder |

The compressor is on everything: the voices, the echoes and the reverb together, before the MASTER knob.

Code: `panel_patch_value`, `panel_patch_spec`, `panel_patch_cable`, `panel_patch_put` and `panel_patch_move` in
`app/panel.h`; `patch_controls` in `app/main.c`.

Code: `panel_eoc_key`, `panel_eoc_select` and `panel_eoc_spec(page, encoder)` in `app/panel.h`; the parameter lists are
`drone_param_spec` (the voice's LEVEL and FADE), `drone_block_spec`, `drone_lfo_spec`, `drone_fb_spec`, `drone_send_spec`,
`drone_reverb_spec`, `drone_delay_spec` and `drone_comp_spec` in `app/drone.h`.
`FM1_NENC` is 7.

## Under the right knobs: twelve function buttons, two rows of six

Buttons are numbered 0 to 13 (`NBUTTON` in `app/main.c`; pressed now: `fm1_in.buttons`, bit n).

Top row:

| Label | Code reference | Sure? | Use in DR1 today |
|---|---|---|---|
| FX | button 2 | assumed | all the voices off |
| SEL | button 3: `BTN_SEL` in `app/main.c` | measured | held: the version and the load on the screen |
| ENV | button 4 | assumed | all the voices off |
| LFO | button 5: `BTN_LFO` in `app/main.c` | measured | none from V0.20 (on a patch's pages: the next page; held: the LFO's cables) |
| EDIT | button 6 | assumed | all the voices off |
| GLO | button 7: `BTN_GLO` in `app/main.c` | assumed | the next page: play, the mixer, the end of the chain (EOC), play; its light is on in the mixer and in EOC |

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
| OP1 | F sharp | key 1: `voice_key[0]` in `app/main.c` | measured | voice 1: a short press turns it on, or off; a hold of 1 s, or a hold with a knob moved, selects it |
| OP2 | G sharp | key 3: `voice_key[1]` | measured (in use) | voice 2 |
| OP3 | A sharp | key 5: `voice_key[2]` | measured (in use) | voice 3 |
| OP4 | C sharp | key 8 | assumed | none from V0.17: no voice, its light stays off |
| OP5 | D sharp | key 10 | assumed | none from V0.17 |
| OP6 | F sharp | key 13 | assumed | none from V0.17 |
| PIT (pitch) | G sharp | key 15 | assumed | none |
| GLO (global) | A sharp | key 17 | assumed | none |
| MONO | C sharp | key 20 | assumed | none |
| POLY | D sharp | key 22 | assumed | none |

27 keys from F have an eleventh black key (F sharp, key 25). It has no label in this list.

The white keys (keys 0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26) have no use in DR1 today.

## To check an assumed number

- A button or a key: run `tools/fm1_keys.py` and press it; it prints the number.
