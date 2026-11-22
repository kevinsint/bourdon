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
| SELECT | an encoder (no end stops, 23 clicks a turn) | encoder 0: `fm1_enc_take(0)`, `ENC_SELECT` in `app/main.c` | measured (in use) | the selected voice's frequency (FREQ); with OCT- held: by notes; with OCT+ held: by octaves. On the bank's page, and with the LFO key held on the play page: the bank's LFO (V0.32) |
| PRESETS | an encoder | encoder 6 | measured | on the bank's page: the clock's tempo (V0.32); with the LFO key held on the play page: the amount onto FREQ |
| ALGORITHM | an encoder | encoder 1 | measured | the selected voice's style; on the bank's page: the LFO's wave |

## Below them: two buttons

| Label | Code reference | Sure? | Use in DR1 today |
|---|---|---|---|
| OCT- | button 0: `FM1_BTN_OCT_DOWN` | named | the shift key (see the table of the controls below) |
| OCT+ | button 1: `FM1_BTN_OCT_UP` | named | held: SELECT moves the selected voice's frequency by octaves (V0.31); in the mixer: PRE or POST; on the bank's page: the LFO's SYNC (V0.32) |

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
| OP1, OP2, OP3 keys | a short press turns the voice on, or off (V0.26); held 1 s: it is selected; held while a knob moves or OCT+ is pressed: it is selected at once, and the move is for it | the same |
| SELECT (encoder 0) | FREQ (V0.31): a very slow click (250 ms or more) is 1/128 semitone; turned faster, a click is 1/64, 1/16, 1/4, 1, then 2 semitones (a click in less than 15 ms): a quarter of a turn, 6 clicks, is then an octave. With OCT+ held: an octave a click | FREQ by notes (V0.31): the first click goes to the nearest note that way, then a semitone a click at any speed |
| ALGORITHM (encoder 1) | the voice's style | the kind of the voice's link (V0.22): FOLLOW, THROUGH, FM (V0.28) |
| KNOB1 (encoder 2) | TONE: dark to bright | none |
| KNOB2 (encoder 3) | MOVE: how deep the style's LFOs go | SPEED: the LFOs' speed |
| KNOB3 (encoder 4) | GRIT: clean to rough (the resonance, the feedback cable's level) | none |
| KNOB4 (encoder 5) | LINK (V0.21): how much the voice takes from the voice before it; 0 at power-on: the voice is alone | OWN (V0.28), only when the voice's link is THROUGH: how much of its own oscillators is left; 127 at power-on: all; 0: only the other voice's sound. With another kind: none |
| OCT+ | held: SELECT moves FREQ by octaves; an octave that does not fit is not taken | the same |
| GLO | the next page: play, the mixer, the end of the chain, then play again; its light is on in the mixer and the end of the chain | the same |
| FX | the next disruptor's page (V0.30): the noise, the pulse (CHOP), the sine (RING), then the play page; its light is on in the three | the same |
| LFO | pressed and let go: the bank's page (V0.32). Held: the amounts of the bank's LFOs on the selected voice (see the bank's page) | the same |
| SEL | held: the version and the load on the screen | the same |

When the frequency is on a note, the top line shows the note's name in white (A4 is 440 Hz) in the place of the hertz.

A knob at 64 leaves the patch as its style has it. The screen shows the selected voice's sound (9.4 ms of it, ten
times a second), a dial for each right knob, and the name and the value of the knob turned last. Under each dial
(V0.32), twelve small marks: one for each of the bank's LFOs, taller and grey when it has an amount on that knob for
the selected voice. When the bank moves a knob's value, a white dot on its dial shows the value now.

The links (V0.21) go round a ring: OP2 listens to OP1, OP3 to OP2, OP1 to OP3. Each voice's link has a kind (V0.22):

- FOLLOW (at power-on): the louder the other voice is (after its fade, before its LEVEL), the higher this voice's
  cutoff, up to 3 octaves. It is one render call late (5.8 ms).
- THROUGH: the other voice's sound (after its fade, before its LEVEL) goes down this voice's chain with this voice's
  own oscillators: through its filter, its feedback cable and its level. OP2 and OP3 get it with no delay; OP1 gets
  OP3's sound one render call late (5.8 ms), which closes the ring. This voice must be on to let it through. OWN
  makes this voice's own oscillators quieter, down to nothing.
- FM (V0.28): the other voice's sound (after its fade, before its LEVEL) bends the pitch of this voice's oscillators,
  frame by frame. With LINK all open and the other voice at a full sample, the frequency goes from nearly nothing to
  nearly twice (never below zero). A slow other voice: the pitch goes up and down. A fast one: new parts in the sound.
  Same delay as THROUGH.

A voice that is off gives nothing. Over the voices' boxes the screen has an arrow for each open link, thicker as the
LINK is more open, amber for the selected voice's, in dashes for FOLLOW, whole for THROUGH and in dots for FM; the selected voice's
kind has its name at the right of the knob's value. Engine: `link`, `link_kind` and `own_less` in `drone_voice_controls_t`,
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

### The disruptors (the noise V0.29; the pulse and the sine V0.30)

A disruptor is a source that the three voices share; each voice takes an amount of it. There are three, each with a
page. The FX button goes from any other page to the first one, then to the next: NOISE (DS1 on the screen), CHOP (DS2),
RING (DS3), then the play page. Its light is on in the three. GLO there opens the mixer.

The controls are the same on the three pages:

| Control | Alone | With OCT- held (shift) |
|---|---|---|
| SELECT, OCT+, the OP keys | as on the play page: the selected voice's frequency, and the voices on, off and selected | the same |
| ALGORITHM | where the selected voice takes this disruptor: its sound (SOUND or LEVEL on the screen), PITCH, CUTOFF | the same |
| KNOB1 to KNOB3 | the disruptor's own three values, the same for the three voices | the same, in the smallest steps |
| KNOB4 | how much of this disruptor the selected voice takes: 0 (none, at power-on) to 127 | the same, in the smallest steps |

| Disruptor | KNOB1 | KNOB2 | KNOB3 |
|---|---|---|---|
| NOISE | RATE: a new value 0.7 times a second to every frame (44 kHz), 8 steps for twice as often; at power-on: every frame | BITS: 1 to 12; at power-on: 12 | DUST: values left out, 0 (none, at power-on) to 127 (nearly all) |
| CHOP | RATE: 0.26 to 392 pulses a second, 12 steps for twice as fast; at power-on: 6 a second | WIDTH: the part of each cycle the pulse is open, 1 % to 100 %; at power-on: 50 % | SKIP: pulses left out at random, 0 (none, at power-on) to 127 (nearly all) |
| RING | FREQ: 8.2 Hz to 12.5 kHz, a semitone a click; at power-on: 220 Hz | FOLD: 0 (a sine, at power-on) to 127 (four times its height, folded back at a full sample: harsh) | DRIFT: its frequency wanders, 0 (not, at power-on) to 127 (an octave each way; a new place to go to five times a second) |

The noise takes a new random value at its RATE and holds it until the next one. BITS is how many heights a value can
have: 12 bits is smooth, 1 bit is two heights only (a full sample up or down). DUST leaves values out, with silence in
their place: the higher it is, the more the noise is separate cracks. The pulse is 0 or a full sample; its edges take
half a millisecond, so that a cut has no click.

Where a voice takes a disruptor:

- Its sound. NOISE (SOUND on the screen): the noise is added at the start of the voice's chain, so the voice's filter
  and its feedback cable shape it. CHOP (LEVEL): after the voice's chain, the voice is cut while the pulse is closed;
  with the amount all open it is nearly silent there, with less it only gets quieter. RING (LEVEL): after the voice's
  chain, the voice's sound times the sine, a ring modulation: with the amount all open only the sum and the
  difference of the two frequencies are left, with less the voice's own sound stays with them. The voice must be on.
- PITCH: the disruptor bends the pitch of the voice's oscillators, frame by frame, as an FM link does: with the amount
  all open, the noise and the sine take it from nearly nothing to nearly twice the frequency, and the pulse lifts it to
  nearly twice while it is open. Several disruptors and an FM link bend it together.
- CUTOFF: the disruptor moves the cutoff of the voice's filters, up to 3 octaves (the noise and the sine each way, the
  pulse up). The cutoff takes its value one time a render call (172 times a second) and glides there: the noise's and
  the sine's first value of the call, the pulse's mean over the call. So a fast noise or sine gives a shake and a slow
  one gives steps or a sweep.

The screen shows OFF while no voice that is on takes any of the page's disruptor: it then does not run and takes no
processor time. The picture is the disruptor as its values make it (not to a scale of time; a pulse that SKIP leaves
out is a dim outline); over each voice's box, a bar for its amount and an arrow when it has some.

Code: `drone_noise`, `drone_chop`, `drone_ring` and `drone_dis_spec` in `app/drone.h`, `dis` and `dis_to` in
`drone_voice_controls_t` and `play_voice_t`; `panel_fx_key`, `panel_page_dis` and `panel_dis_spec` in `app/panel.h`;
`dis_controls` in `app/main.c`; `view_dis` in `app/view.h`.

### The LFO bank (V0.32)

Twelve LFOs for the whole instrument (`drone_bank_t` in `app/drone.h`), apart from the LFOs inside each style, and a
clock: a tempo, 10 to 300 BPM, 60 at power-on. A bank LFO is free, at its own RATE, or synced: locked to the clock by a
ratio. The bank runs all the time, also with every voice off.

A short press of the LFO key (pressed and let go with no knob turned and no white key pressed while it is down) opens
the bank's page from any page; on the bank's page it goes back to the page that was open before. Its light is on in
the bank's page. GLO and FX there open their pages as from the play page.

| Control | Alone | With OCT- held (shift) |
|---|---|---|
| White keys 1 to 12 (keys 0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19) | show that LFO; the twelve keys' lights are on, and the shown LFO's goes off while its wave is at its middle or below: it beats with the wave | the same |
| SELECT | the LFO shown, 1 to 12; it stops at the ends | the same |
| ALGORITHM | WAVE: SINE, TRI, PULSE, STEPS, FLIP | RANGE: BOTH (up and down from the value) or UP (up only) |
| KNOB1 | free: RATE, one cycle in 62.6 s to 24.5 cycles a second, 12 steps for twice as fast (2 a click). Synced: the RATIO, one cycle in 32, 16, 8, 4, 3, 2 beats, 1 beat, then 2, 3, 4, 8, 16 cycles a beat | the same, in the smallest steps |
| KNOB2 | the wave's SHAPE: SINE's SKEW, TRI's SLOPE (0 a ramp down, 64 a triangle, 127 a ramp up), PULSE's WIDTH (1 % to 99 % of the cycle up), STEPS' JUMP (how far a new random height can go), FLIP's BIAS (how often it is up) | PHASE: where the LFO is in its cycle, 0 to 357 degrees, 11.25 degrees a click; it goes round. A synced LFO: its place against the clock |
| KNOB3 | SLEW: 0 none; 127: a full swing takes a whole cycle | the same, in the smallest steps |
| KNOB4 | DEPTH: how far it goes; all its amounts share it | the same, in the smallest steps |
| OCT+ | SYNC on or off | the same |
| PRESETS | the tempo, 1 BPM a click | the same |
| OP1 to OP3 keys | as on the play page | the same |
| LFO key | pressed and let go: back to the page before; held: nothing more yet | the same |

The screen: LFO and its number at the top, FREE or SYNC at the right and the tempo under it, with a small square that
is lit in the first quarter of each beat. The wave: this cycle and the next one, as the engine plays them (the shape,
the random heights, the slew, the range and the depth), with an amber mark where the LFO is now; a synced LFO slower
than a beat has a small tick for each beat. Below it the name and the value of the control turned last, with the
wave's name in grey; a dial for each right knob; the row of the twelve LFOs, each with a bar that follows its output:
the shown one has the amber frame, the number is brighter when a voice has an amount of it, and a short line is under
a synced one. The picture, the bars and the square follow the engine ten times a second; the keys' lights at once.

**The amounts.** Hold the LFO key on the play page. The first twelve white keys come on, as on the bank's page. A white
key or SELECT chooses the LFO (the same one the bank's page shows). Then:

| Control, the LFO key held on the play page | Alone | With OCT- held (shift) |
|---|---|---|
| KNOB1 to KNOB4 | the LFO's amount on the selected voice's TONE, MOVE, GRIT and LINK: -100 to 100, 5 a click; 0 is none, below 0 the LFO upside down | 1 a click |
| PRESETS | the amount on its FREQ: at 100, an octave each way | 1 a click |
| ALGORITHM, OCT+ | nothing | nothing |

At 100 and the LFO at its top, TONE, MOVE and GRIT move as if their knob went its whole range, LINK its whole range.
A turn or a white key while the LFO key is down: its release opens no page. The top line shows LFO and its number in
cyan in the place of the style, and its wave in grey in the place of the frequency; the dials show its amounts from
their centre in cyan (to the right: more; to the left: upside down); its mark under each dial is cyan; the value line
is the knob's name and the amount (TONE +38); the row of the twelve LFOs is in the place of the links, with a cyan
frame on the chosen one. Let go of the key: the page is as before.

Code: `drone_bank_spec`, `drone_bank_wave` and `drone_bank_view_t` in `app/drone.h`; `bank_amount` in `play_voice_t`
and `style_patch` in `app/style.h`; `panel_lfo_key`, `panel_bank_param`, `panel_bank_turned`, `panel_tempo_turned`,
`panel_banked` and `panel_bank_lights` in `app/panel.h`; `bank_controls`, `amount_controls` and `bank_take` in
`app/main.c`; `view_bank` and `view_play` in `app/view.h`.

### An encoder's velocity (V0.25)

A value's encoder turned fast moves the value further: its clicks count 1, 2, 4 or 8 times, when a click took 60 ms
or more, 30 ms, 15 ms, or less. This is for the four right knobs on the play page, the mixer and the end of
the chain, and on the bank's page for the RATE, SHAPE, PHASE, SLEW, DEPTH and the tempo (V0.32). Not for an amount. FREQ has its own steps (V0.31, see the play page): `panel_freq_steps` in `app/panel.h`. Not for a choice (the style, the kind of link, a channel, a page), and not with OCT- held for the smallest
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
| LFO key | none from V0.32 (it opens the bank's page) | the same | - |
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
| FX | button 2: `BTN_FX` in `app/main.c` | assumed | the next disruptor's page: NOISE, CHOP, RING, then the play page; its light is on in the three |
| SEL | button 3: `BTN_SEL` in `app/main.c` | measured | held: the version and the load on the screen |
| ENV | button 4 | assumed | all the voices off |
| LFO | button 5: `BTN_LFO` in `app/main.c` | measured | pressed and let go: the bank's page, and back (V0.32); held on the play page: the bank's amounts on the selected voice; its light is on in the bank's page |
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

The white keys:

| Keys | Sure? | Use in DR1 today |
|---|---|---|
| the first twelve: 0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19 (`panel_bank_key` in `app/panel.h`) | assumed (the numbers come from the pattern of the black keys) | the bank's LFOs 1 to 12 (V0.32): on the bank's page, and with the LFO key held on the play page, a press chooses that LFO; their lights are on there |
| 21, 23, 24, 26 | assumed | none |

## To check an assumed number

- A button or a key: run `tools/fm1_keys.py` and press it; it prints the number.
