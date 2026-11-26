/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kevin Sint */
/* SINT_BOURDON (was SINT_FM1_DR1): the empty firmware. It has only what keeps the FM-1 replaceable:
 *   - the boot guard: two starts in a row that do not stay up 2 s end in the chip's update mode
 *   - the update entry: the installer (bsp/tools/fm1_install.py) replaces this firmware over USB-MIDI
 *   - update mode on request: the USB-MIDI command, or 'u' on the serial port
 *   - the watchdog and the hardware guards; a crash shows a red screen, then restarts
 * and the sound engine (drone.h), three voices, OP1 to OP3, on the first three black keys:
 *   - a short press of the voice's key turns the voice on, the next one off (when the key is let go)
 *   - a press of a second or more selects the voice and does not turn it on or off
 *   - any button but OCT-, OCT+, SEL, the LFO key, FX and GLO turns all the voices off; the MASTER knob is the level
 * Each voice is a patch (drone.h): a chain of blocks that the player makes, on its own page (panel.h):
 *   - the SELECT encoder moves the cursor along the chain; the four right encoders are the values of the block
 *     under it, or on OUT the voice's level, fade and send; the ALGORITHM encoder is the block's kind
 *   - OCT- was the shift key until V0.35 (SELECT chose a block, PRESETS moved it, the encoders had their second
 *     values). It is the snap key now: held, a knob's click takes its value to the next of its places (its bottom,
 *     middle, value at power-on, top). The second sets are layers, on the rightmost white keys: the play page's two
 *     (keys 13 and 14), the bank's page's four (keys 13 to 16). No key opens a patch's page, so its second set has none
 *   - OCT+ moves the pitch of the oscillator under the cursor an octave up; with OCT- held, an octave down
 *   - the LFO key, pressed and let go, opens the bank's page (V0.32): the twelve LFOs for the whole instrument and
 *     the clock's tempo, white keys 13 to 16 the layer, the set of an LFO's values the right encoders hold; again, it
 *     goes back. Held there: a source LFO's amounts onto the shown one's values and the tempo. Held on the play
 *     page: the bank's amounts onto the selected voice.
 *     (The patch's LFO and feedback pages have no key from V0.20.) On an LFO's page ALGORITHM is its wave, the first
 *     right encoder its speed and the second its depth. On the feedback's page the four right encoders are its two
 *     blocks, its level and its time
 *   - the GLO key opens the end of the chain (EOC), what all the voices go through: the delay, then the reverb,
 *     then the compressor. SELECT goes from one to the next and the four right encoders are its values (the
 *     reverb's first one is the selected voice's send); OCT- held: they snap. GLO again: back to the patch
 *   - the LFO key held: an encoder is the amount of a cable from the LFO (the open page's, or the last one open).
 *     On the chain's page: to the encoder's value. On an LFO's page: to the speed and the depth of the two other
 *     LFOs. On the feedback's page: to its level
 *   - an FX block is the reverb in the chain: its mix, decay and tone on the first three right encoders. There is
 *     one reverb: while a patch has an FX block, the voices' sends go into the delay only, and the reverb's page
 *     says which voice
 *   - SEL held shows the version and how much of its time the processor spends on the sound
 * The screen (view.h) shows the selected voice as its chain, each block a picture, and below it what the open page
 * is for. The load is also a thin line at the bottom, red above 80 %.
 * The chip and board code is felucca-bsp's (bsp/, MPL-2.0), included unchanged. */
#include <stdint.h>
#include "app_id.h"
/* felucca-bsp's build settings */
#define FELUCCA_CDC 1                           /* the serial port */
#define FELUCCA_OTA 1                           /* the update entry */
#define FELUCCA_ID APP_PACKAGE_ID
#define FELUCCA_USB_MANUFACTURER "KevinSint"
#define FELUCCA_USB_PRODUCT APP_NAME
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
#include "fm1_time.h"
#include "fm1_sys.h"
#include "fm1_irq.h"
#include "fm1_guard.h"
#include "fm1_timer.h"
#include "fm1_input.h"
#include "fm1_adc.h"
#include "fm1_audio.h"
#include "fm1_lcd.h"
#include "fm1_flash.h"
#include "libc.c"
#include "usb.c"
#include "ota.c"
#define FONT_RECT(x, y, w, h, colour) fm1_lcd_fill_rect(x, y, w, h, colour)
#include "font.h"
#include "params.h"
#include "keys.h"
#include "drone.h"
#include "panel.h"
#define VIEW_BLIT(x, y, w, h, pixels) fm1_lcd_blit(x, y, w, h, pixels, (w) * (h))
#include "view.h"

enum { BLACK = 0x0000, WHITE = 0xFFFF, GREY = 0x8410, DIM = 0x2104, FAINT = 0x0841, RED = 0xF800, GREEN = 0x07E0, AMBER = 0xFD20 };
#define TICKS_PER_MS (FM1_INPUT_HZ / 1000u)
#define TICKS_PER_USB_POLL (FM1_INPUT_HZ / 2000u)   /* usb_poll wants 2 kHz */
#define HEALTHY_AFTER_MS 2000u

static volatile uint32_t uptime_ms_count;
static uint32_t uptime_ms(void) { return uptime_ms_count; }

void fm1_timer5_irq(void)                       /* the tick, FM1_INPUT_HZ: one column of the key scan each time */
{
    static uint32_t ticks_in_ms, ticks_in_usb_poll;
    fm1_timer5_ack();
    fm1_input_tick();
    if (++ticks_in_ms == TICKS_PER_MS) {
        ticks_in_ms = 0;
        uptime_ms_count++;
    }
    if (++ticks_in_usb_poll == TICKS_PER_USB_POLL) {
        ticks_in_usb_poll = 0;
        usb_poll();
    }
}
extern void isr_timer5(void);

/* ---- the sound ----
 * The audio hardware plays two halves of audio_buffer in turn, and asks for the half it has just left. */
#define AUDIO_HALF_WORDS (DRONE_BLOCK_FRAMES * FM1_AUDIO_CHANNELS)
#define AUDIO_HALF_TICKS (DRONE_BLOCK_FRAMES * FM1_AUDIO_RATE_DEN)     /* the time of one half, in fm1_ticks */
static int32_t audio_buffer[2u * AUDIO_HALF_WORDS] __attribute__((aligned(4)));
static drone_t drone_engine;
static volatile drone_controls_t drone_controls;        /* the main loop writes, the audio interrupt reads */
static volatile uint32_t render_ticks_peak;             /* the longest render since the main loop took it */
static volatile uint32_t voices_sounding;
static int audio_start_error;                           /* not 0: the audio did not start (FM1_AUDIO_ERR_*) */
extern void isr_alnk0(void);

void fm1_alnk0_irq(void)
{
    uint8_t pending = fm1_audio_pending();
    fm1_audio_ack_aux(pending);
    if (pending & FM1_AUDIO_HALF) {
        uint32_t started = fm1_ticks(), took;
        drone_controls_t controls_now = drone_controls;
        drone_render(&drone_engine, &controls_now, audio_buffer + fm1_audio_free_half() * AUDIO_HALF_WORDS,
                     DRONE_BLOCK_FRAMES);
        fm1_audio_ack_half();
        voices_sounding = drone_voices_sounding(&drone_engine);
        took = fm1_ticks() - started;           /* with the key scan's interrupts in it: the time really gone */
        if (took > render_ticks_peak)
            render_ticks_peak = took;
    }
}

/* ---- the boot guard ----
 * In RAM that a reset keeps. volatile: without it the compiler takes the record as zero at start and drops the
 * compare with the magic (felucca-bsp#17), and what the firmware before left in RAM passes for a record. */
#define BOOT_RECORD_MAGIC 0x44523147u
static volatile struct { uint32_t magic, start_pending, failed_starts; } boot_record __attribute__((section(".noinit")));

static void boot_guard_check(void)              /* at the very start, before anything can fail */
{
    if (boot_record.magic != BOOT_RECORD_MAGIC || boot_record.start_pending > 1u || boot_record.failed_starts > 1u) {
        boot_record.magic = BOOT_RECORD_MAGIC;
        boot_record.start_pending = 0;
        boot_record.failed_starts = 0;
    }
    if (boot_record.start_pending)              /* the start before this one did not reach the healthy point */
        boot_record.failed_starts++;
    boot_record.start_pending = 1;
    if (boot_record.failed_starts >= 2u) {
        boot_record.start_pending = 0;
        boot_record.failed_starts = 0;
        fm1_enter_uboot();
    }
}

static void boot_guard_pass(void)               /* this start is good, or the reset that follows is wanted */
{
    boot_record.start_pending = 0;
    boot_record.failed_starts = 0;
}

/* ---- the screen and the controls ---- */
#define PROGRESS_Y 216u
#define PROGRESS_H 24u
#define NBUTTON 14u
#define NNOTE 27u

/* each voice's key, as felucca-bsp numbers the 27 note keys from the left: 0 is F, so these are the first three
 * black keys, the ones the panel labels OP1 to OP3 (F sharp, G sharp, A sharp) */
static const uint8_t voice_key[DRONE_VOICES] = { 1, 3, 5 };
static play_voice_t play_voice[DRONE_VOICES];   /* what each voice is played with: its style, its frequency, its knobs */
static mixer_channel_t mixer_channels[DRONE_VOICES];    /* and its channel in the mixer */
static uint32_t mixer_selected;                 /* the channel selected on the mixer's page: 0 none, 1 .. the voices' */
static uint32_t enc_turned_ms[FM1_NENC];        /* when each encoder gave its last click: a fast turn moves a value further */
static uint32_t loop_ms;                        /* the time of this turn of the main loop */

/* a knob's clicks as the steps its value moves, the value's smallest ones (panel_knob_steps): 1 a click notch by
 * notch, further when it is turned fast; `per_click`: the value's own click */
static int32_t enc_steps(uint32_t encoder, int32_t clicks, uint32_t per_click)
{
    return panel_knob_steps(per_click, clicks, loop_ms, &enc_turned_ms[encoder]);
}
/* a value after its knob's clicks: the one way every knob turns. By its steps (enc_steps); with OCT- held (`snap`),
 * each click to the next of the value's places: its bottom, its middle, its value at power-on, its top */
static uint8_t knob_turned(uint32_t encoder, const param_spec_t *spec, uint32_t value, int32_t clicks, int snap)
{
    param_spec_t by_one = panel_fine(spec);
    return (uint8_t)(snap ? param_snapped(spec, (int32_t)value, clicks)
                          : param_turned(&by_one, (int32_t)value, enc_steps(encoder, clicks, spec->per_click)));
}
/* the same on a patch's page, where a value of one step a click (a choice, a count, a pitch in semitones) takes its
 * clicks as they are */
static uint8_t value_turned(uint32_t encoder, const param_spec_t *spec, uint32_t value, int32_t clicks, int snap)
{
    return spec->per_click > 1u || snap ? knob_turned(encoder, spec, value, clicks, snap) : (uint8_t)param_turned(spec, (int32_t)value, clicks);
}
/* a cable's amount after its knob's clicks, as any knob's: its own click is 5; with `snap`, to the next of -100, -50,
 * 0, 50, 100 */
static int8_t amount_turned(uint32_t encoder, int32_t amount, int32_t clicks, int snap)
{
    return (int8_t)panel_amount_turned(amount, snap ? clicks : enc_steps(encoder, clicks, PANEL_AMOUNT_STEP), snap);
}
static key_hold_t voice_key_hold[DRONE_VOICES];
_Static_assert(VIEW_ENCODERS == FM1_NENC, "the screen knows each encoder");
static uint32_t selected_voice;                 /* the encoders are this voice's parameters */
static uint8_t patch_place[DRONE_VOICES];       /* where the cursor is in each patch's chain */
static uint8_t patch_lfo;                       /* the LFO the cables turned are of: the one whose page was open last */
static int8_t scope_picture[2][DRONE_SCOPE_POINTS];     /* the feedback page's scope: the last picture the engine took, as the screen wants it */
static uint32_t scope_asked_at;                 /* ms */
#define SCOPE_EVERY_MS 100u
static uint32_t bank_lfo;                       /* the bank's LFO on its page, and the one the LFO key's amounts are of on the play page */
static uint32_t bank_source;                    /* the LFO key held on the bank's page: the LFO whose amounts the knobs are */
static uint8_t amount_touched = PANEL_ENC_VALUES;       /* the encoder turned last for an amount, with the LFO key held */
static drone_bank_view_t bank_now, bank_shown;  /* the bank as the engine gave it last: the key lights follow it, the screen
                                                 * takes it ten times a second */
static uint32_t bank_shown_at;                  /* ms */
_Static_assert(PANEL_ENC_VALUES + 4u <= FM1_NENC && 0u < PANEL_ENC_KIND && PANEL_ENC_KIND < PANEL_ENC_VALUES,
               "a patch's page fits the encoders");
static panel_t panel = { .page = PANEL_PLAY, .before_bank = PANEL_PLAY };     /* the open page, and the choice of a block while OCT- is down */
static uint8_t touched[PANEL_NPAGE] = { [PANEL_VOICE] = PANEL_ENC_VALUES + 1u, [PANEL_DELAY] = PANEL_ENC_VALUES + 3u, [PANEL_PLAY] = PANEL_ENC_VALUES,
                                        [PANEL_NOISE] = PANEL_ENC_VALUES + 3u, [PANEL_CHOP] = PANEL_ENC_VALUES + 3u, [PANEL_RING] = PANEL_ENC_VALUES + 3u,
                                        [PANEL_BANK] = DRONE_BANK_RATE };      /* the encoder turned last on each page; the bank's: its value */
static uint32_t load_percent;                   /* the processor's time on the sound; 100: it breaks up */
/* felucca-bsp names only the two octave buttons; these two numbers are read from an FM-1 with tools/fm1_keys.py */
enum { BTN_SEL = 3, BTN_LFO = 5,
       BTN_FX = 2, BTN_GLO = 7 };               /* these two not read from an FM-1 yet: the top row counts 2 to 7, FX its first and GLO its last */
#define ENC_SELECT 0u                           /* the SELECT encoder */
_Static_assert(DRONE_VOICES <= 9, "OP and one digit");

static struct { uint32_t buttons, keys_on; } lit;       /* the LEDs as they are */

/* a patch's chain and LFO pages: the cursor, the choice of a block, the encoders of the place under the cursor or
 * of the open LFO, and the cables. The audio interrupt reads the chain while this writes it; a chain read half
 * written sounds wrong for one block of sound at most, as the engine takes any values */
static void patch_controls(volatile drone_voice_controls_t *voice, uint8_t *place_of_voice, int snap, int lfo_down, int32_t octaves,
                           view_model_t *model)
{
    const int shift = 0;                        /* the second set these pages had with OCT- held: no key gives it now */
    drone_block_controls_t chain[DRONE_SLOTS];
    uint32_t i, count, at, place = *place_of_voice, type, chosen;
    int32_t select = fm1_enc_take(ENC_SELECT), moves = fm1_enc_take(PANEL_ENC_MOVE);
    for (i = 0; i < DRONE_SLOTS; i++)
        chain[i] = voice->block[i];
    count = panel_patch_count(chain);
    place = place < panel_patch_places(count) ? place : panel_patch_places(count) - 1u;
    at = panel_patch_at(count, place);
    type = at == PANEL_AT_BLOCK ? chain[place].type : DRONE_BLOCK_NONE;

    /* the choice of a block starts with the first click of SELECT while shift is down, and ends when shift is let go */
    chosen = panel_choice(&panel, shift && at != PANEL_AT_OUT && (panel.choosing || select), shift ? select : 0,
                          &drone_block_type_spec, type);
    if (chosen != type)
        panel_patch_put(chain, place, chosen);
    else if (shift)
        place = panel_patch_move(chain, place, moves);
    else
        place = (uint32_t)param_turned(&(param_spec_t){ (uint8_t)(panel_patch_places(count) - 1u), 1, 0 }, (int32_t)place, select);
    count = panel_patch_count(chain);
    place = place < panel_patch_places(count) ? place : panel_patch_places(count) - 1u;
    at = panel_patch_at(count, place);
    type = at == PANEL_AT_BLOCK ? chain[place].type : DRONE_BLOCK_NONE;

    if (octaves && type == DRONE_BLOCK_VCO) {   /* the octave button: this oscillator's pitch */
        int32_t pitch = chain[place].param[DRONE_VCO_PITCH] + 12 * octaves;
        if (pitch >= 0 && pitch <= drone_block_spec[DRONE_BLOCK_VCO][DRONE_VCO_PITCH].top)
            chain[place].param[DRONE_VCO_PITCH] = (uint8_t)pitch;
    }
    for (i = PANEL_ENC_KIND; i < FM1_NENC; i++) {      /* all but SELECT; PRESETS is taken above, so it has no clicks here */
        int32_t clicks = fm1_enc_take(i), value = panel_patch_value(at, type, i, shift), cable = panel_patch_cable(at, type, i);
        int32_t lfo_param = panel_patch_lfo_param(i);
        const param_spec_t *spec = panel_patch_spec(at, type, i, shift);
        volatile uint8_t *const of_out[PANEL_OUT_NVALUE] = { &voice->param[DRONE_LEVEL], &voice->param[DRONE_FADE], &voice->send };
        volatile uint8_t *turned;
        if (!clicks)
            continue;
        if (panel.page == PANEL_FEEDBACK) {     /* the feedback cable's values; with the LFO key, the cable to its level */
            int32_t fb_param = panel_fb_param(i);
            if (lfo_down && fb_param == DRONE_FB_LEVEL) {
                voice->fb_amount[patch_lfo] = amount_turned(i, voice->fb_amount[patch_lfo], clicks, snap);
            } else if (!lfo_down && fb_param >= 0) {
                voice->fb[fb_param] = value_turned(i, &drone_fb_spec[fb_param], voice->fb[fb_param], clicks, snap);
                touched[panel.page] = (uint8_t)i;
            }
            continue;
        }
        if (panel.page != PANEL_VOICE) {        /* an LFO's page: its wave, speed and depth; with the LFO key, its cables to the other LFOs */
            uint32_t to, what;
            if (lfo_down && panel_lfo_cable(i, patch_lfo, &to, &what)) {
                voice->lfo_amount[to][what][patch_lfo] = amount_turned(i, voice->lfo_amount[to][what][patch_lfo], clicks, snap);
            } else if (!lfo_down && lfo_param >= 0) {
                voice->lfo[patch_lfo][lfo_param] = value_turned(i, &drone_lfo_spec[lfo_param], voice->lfo[patch_lfo][lfo_param], clicks, snap);
                touched[panel.page] = (uint8_t)i;
            }
            continue;
        }
        if (lfo_down) {                         /* the chain's page: the cable from the LFO to this encoder's value */
            volatile int8_t *amount = at == PANEL_AT_OUT ? &voice->out_amount[cable < 0 ? 0 : cable][patch_lfo]
                                                         : (volatile int8_t *)&chain[place % DRONE_SLOTS].amount[cable < 0 ? 0 : cable][patch_lfo];
            if (cable >= 0)
                *amount = amount_turned(i, *amount, clicks, snap);
            continue;
        }
        if (!spec)
            continue;
        turned = i == PANEL_ENC_KIND ? &chain[place % DRONE_SLOTS].kind : at == PANEL_AT_OUT ? of_out[value]
               : &chain[place % DRONE_SLOTS].param[value];
        *turned = value_turned(i, spec, *turned, clicks, snap);
        /* the screen has the fine tuning in the row below the pitch */
        touched[PANEL_VOICE] = (uint8_t)(type == DRONE_BLOCK_VCO && value == DRONE_VCO_FINE ? PANEL_ENC_VALUES + 1u : i);
    }
    for (i = 0; i < DRONE_SLOTS; i++)
        voice->block[i] = chain[i];
    *place_of_voice = (uint8_t)place;
    model->place = (uint8_t)place;
    model->choice = panel.choice;
}

/* a voice's style, frequency and knobs, and its channel of the mixer, into the engine's controls; the voice stays on
 * or off as it is. The audio
 * interrupt reads the patch while this writes it: as for patch_controls, one block of sound at most is wrong */
static void play_apply(uint32_t v)
{
    drone_voice_controls_t patch;
    style_patch(&play_voice[v], &patch);
    mixer_apply(&mixer_channels[v], &patch);
    patch.on = drone_controls.voice[v].on;
    drone_controls.voice[v] = patch;
}

/* a voice's frequency after the clicks of the encoder that holds it (KNOB1 in the play page's second layer; SELECT
 * held it until V0.35): slow is fine, fast goes far; with OCT- held (`snap`) it goes by notes, with OCT+ held by octaves */
static uint32_t freq_turned(uint32_t freq, uint32_t encoder, int snap, int oct_up)
{
    int32_t freq_clicks = fm1_enc_take(encoder);
    return panel_freq_turned(freq, freq_clicks, panel_freq_steps(freq_clicks, loop_ms, &enc_turned_ms[encoder]),
                             oct_up ? PANEL_FREQ_OCTAVES : snap ? PANEL_FREQ_NOTES : PANEL_FREQ_FREE);
}

/* the play page: ALGORITHM is the voice's style, the right encoders its knobs: TONE, MOVE, GRIT, LINK. In the second
 * layer (`layer`), KNOB1 is its frequency (OCT- held: by notes; OCT+ held: by octaves), KNOB2 its SPEED, KNOB3 its
 * GAIN (its level, the mixer's), KNOB4 a THROUGH voice's OWN, and ALGORITHM the kind of its link. OCT- held (`snap`): a knob snaps. SELECT holds nothing
 * here: its clicks are dropped */
static void play_controls(uint32_t v, int layer, int snap, int oct_up)
{
    play_voice_t *play = &play_voice[v];
    uint32_t i, freq = play->freq;
    int32_t kind_clicks = fm1_enc_take(PANEL_ENC_KIND);         /* ALGORITHM: the style; in the second layer, the kind of the link */
    uint32_t style = (uint32_t)param_turned(&panel_style_spec, play->style, layer ? 0 : kind_clicks);
    uint32_t link_kind = (uint32_t)param_turned(&drone_link_kind_spec, play->link_kind < DRONE_NLINK ? play->link_kind : 0, layer ? kind_clicks : 0);
    int changed = 0;
    fm1_enc_take(ENC_SELECT);
    for (i = PANEL_ENC_VALUES; i < FM1_NENC; i++) {     /* PRESETS too: it has no knob, its clicks are dropped */
        int32_t knob = panel_play_knob(i, layer, link_kind == DRONE_LINK_THROUGH), clicks;
        if (knob == PANEL_PLAY_FREQ) {
            uint32_t to = freq_turned(freq, i, snap, oct_up);
            if (to != freq)
                touched[PANEL_PLAY] = (uint8_t)i;
            freq = to;
            continue;
        }
        clicks = fm1_enc_take(i);
        if (!clicks || knob < 0)
            continue;
        if (knob == PANEL_PLAY_GAIN) {          /* the voice's level: its channel's in the mixer, the same value */
            uint8_t *level = &mixer_channels[v].value[MIXER_LEVEL];
            *level = (uint8_t)mixer_turned(MIXER_LEVEL, *level, snap ? clicks : enc_steps(i, clicks, mixer_spec[MIXER_LEVEL].per_click), snap);
            touched[PANEL_PLAY] = (uint8_t)i;
            changed = 1;
            continue;
        }
        play->knob[knob] = knob_turned(i, &play_knob_spec[knob], play->knob[knob], clicks, snap);
        touched[PANEL_PLAY] = (uint8_t)i;
        changed = 1;
    }
    changed |= freq != play->freq || style != play->style || link_kind != play->link_kind;
    play->freq = (uint16_t)freq;
    play->style = (uint8_t)style;
    play->link_kind = (uint8_t)link_kind;
    if (changed)
        play_apply(v);
}

/* the LFO key held on the play page: SELECT chooses the bank's LFO, the right encoders are its amounts
 * onto what they hold in the layer in hand (TONE, MOVE, GRIT, LINK; in the second layer, KNOB1 onto the FREQ), PRESETS
 * onto its FREQ: as any knob, 1 a click notch by notch; OCT- held: to the next of -100, -50, 0, 50, 100. ALGORITHM does nothing
 * here; its clicks are dropped, or they would come after the key is let go */
static void amount_controls(uint32_t v, int layer, int snap)
{
    play_voice_t *play = &play_voice[v];
    uint32_t i;
    int changed = 0;
    bank_lfo = panel_bank_select(bank_lfo, fm1_enc_take(ENC_SELECT));
    fm1_enc_take(PANEL_ENC_KIND);
    for (i = PANEL_ENC_VALUES; i < FM1_NENC; i++) {
        int32_t clicks = fm1_enc_take(i), banked = panel_banked(i, layer);
        int8_t *amount;
        if (!clicks || banked < 0)
            continue;
        amount = &play->bank_amount[banked][bank_lfo];
        *amount = amount_turned(i, *amount, clicks, snap);
        amount_touched = (uint8_t)i;
        changed = 1;
    }
    if (changed)
        play_apply(v);
}

/* the bank's page: SELECT shows an LFO, white keys 13 to 16 choose the layer, the set of its values the right
 * encoders hold (panel_bank_holds), ALGORITHM is its shape (in MORE: its range), OCT+ its sync, PRESETS the tempo;
 * OCT- held (`snap`): a knob snaps. The
 * LFO key held (`held`): SELECT chooses the source LFO, and the encoders are its amounts; ALGORITHM's
 * clicks and OCT+ are dropped, or they would come after the key is let go. A value of one step a click takes its
 * clicks as they are, as a choice does; the others as any knob (panel_knob_steps): 1 notch by notch, further fast */
static void bank_controls(int snap, int held, int oct_up_pressed, int32_t key_set)
{
    volatile uint8_t *knob;
    uint32_t i;
    panel_bank_choose(&bank_lfo, &bank_source, held, fm1_enc_take(ENC_SELECT));
    knob = drone_controls.bank[bank_lfo];
    if (!held && key_set >= 0) {                /* a new set: its first value is the one on the value line */
        panel.bank_set = (uint8_t)key_set;
        touched[PANEL_BANK] = panel_bank_knob[key_set][0];
    }
    if (!held && oct_up_pressed) {
        knob[DRONE_BANK_SYNC] ^= 1u;
        touched[PANEL_BANK] = DRONE_BANK_SYNC;
    }
    for (i = PANEL_ENC_KIND; i < FM1_NENC; i++) {
        panel_bank_holds_t holds = panel_bank_holds(panel.bank_set, i, held, knob[DRONE_BANK_SYNC] != 0);
        /* the knob's own click: the tempo's 2 (1 BPM a click, up to 8), an amount's 5, a value's own; a value of one
         * step a click (a choice, a count) takes its clicks as they are, and so does a snap */
        uint32_t per_click = holds.kind == PANEL_HOLDS_TEMPO ? 2u : holds.kind == PANEL_HOLDS_VALUE ? drone_bank_spec[holds.which].per_click
                           : PANEL_AMOUNT_STEP;
        int32_t clicks = fm1_enc_take(i);
        if (!clicks || holds.kind == PANEL_HOLDS_NOTHING)
            continue;
        panel_bank_apply(&drone_controls, bank_lfo, bank_source, holds, snap || per_click <= 1u ? clicks : enc_steps(i, clicks, per_click),
                         snap && i != PANEL_ENC_KIND);
        if (held)
            amount_touched = (uint8_t)i;
        else
            touched[PANEL_BANK] = (uint8_t)(holds.kind == PANEL_HOLDS_TEMPO ? PANEL_BANK_TEMPO : holds.which);
    }
}

/* the bank's picture: take the one the engine has made, and ask for the next, as for the scope. The key lights follow
 * it at once; the screen takes it ten times a second, so that it draws no more often */
static void bank_take(uint32_t now_ms)
{
    drone_bank_view_t *view = &drone_engine.bank.view;
    if (!view->wanted) {
        __asm__ volatile("" ::: "memory");      /* read the picture after `wanted`: the render wrote it before it cleared that */
        bank_now = *view;
        drone_bank_ask(view);
    }
    if (now_ms - bank_shown_at >= SCOPE_EVERY_MS) {
        bank_shown = bank_now;
        bank_shown_at = now_ms;
    }
}

/* a disruptor's page: KNOB1 to KNOB3 are the disruptor's values, KNOB4 the voice's amount of it, ALGORITHM where the
 * voice takes it; OCT- held (`snap`): a knob snaps. SELECT holds nothing here: its clicks are dropped */
static void dis_controls(uint32_t v, uint32_t dis, int snap)
{
    play_voice_t *play = &play_voice[v];
    uint32_t i;
    int changed = 0;
    fm1_enc_take(ENC_SELECT);
    for (i = PANEL_ENC_KIND; i < FM1_NENC; i++) {       /* PRESETS too: it holds nothing, its clicks are dropped */
        const param_spec_t *spec = panel_dis_spec(dis, i);
        int32_t clicks = fm1_enc_take(i);
        if (!clicks || !spec)
            continue;
        if (i == PANEL_ENC_KIND) {
            play->dis_to[dis] = (uint8_t)param_turned(spec, play->dis_to[dis] < DRONE_NTO ? play->dis_to[dis] : 0, clicks);
        } else if (spec == &drone_dis_send_spec) {
            play->dis[dis] = knob_turned(i, spec, play->dis[dis], clicks, snap);
            touched[panel.page] = (uint8_t)i;
        } else {
            volatile uint8_t *value = &drone_controls.dis[dis][i - PANEL_ENC_VALUES];
            *value = knob_turned(i, spec, *value, clicks, snap);
            touched[panel.page] = (uint8_t)i;
            continue;                           /* the disruptor's own: no voice's patch has it */
        }
        changed = 1;
    }
    if (changed)
        play_apply(v);
}

/* the mixer's page: SELECT chooses a channel (it is the selected voice then), the right encoders are the channel's
 * values or, with none selected, the voices' levels; OCT+ changes the selected channel's sends from PRE to POST */
static void mixer_controls(int snap, int oct_up)
{
    uint32_t i;
    mixer_selected = (uint32_t)param_turned(&panel_mixer_channel_spec, (int32_t)mixer_selected, fm1_enc_take(ENC_SELECT));
    if (mixer_selected)
        selected_voice = mixer_selected - 1u;
    if (oct_up && mixer_selected) {
        mixer_channels[selected_voice].post ^= 1u;
        play_apply(selected_voice);
    }
    for (i = PANEL_ENC_KIND; i < FM1_NENC; i++) {       /* ALGORITHM and PRESETS too: they hold nothing, their clicks are dropped */
        uint32_t voice, value;
        int held = panel_mixer_value(mixer_selected, i, &voice, &value);
        int32_t clicks = fm1_enc_take(i);
        if (!clicks || !held)
            continue;
        mixer_channels[voice].value[value] = (uint8_t)mixer_turned(value, mixer_channels[voice].value[value],
                                                                   snap ? clicks : enc_steps(i, clicks, mixer_spec[value % MIXER_NVALUE].per_click), snap);
        play_apply(voice);
    }
}

static void screen_draw(void)                   /* the whole screen again, after something else wrote on it */
{
    view_reset();
}

/* the main loop: the keys, buttons and encoders into the engine's controls, and all of it shown (view.h) */
static void controls_show(uint32_t now_ms)
{
    uint32_t i, v, buttons = fm1_in.buttons, notes = fm1_in.notes, released, pressed = fm1_input_edges(&released);
    uint32_t notes_pressed = fm1_input_note_edges();
    int32_t key_set = panel_layer_of_keys(notes_pressed, PANEL_BANK_NSET);      /* one of white keys 13 to 16 pressed: a layer of the bank's page */
    int32_t key_layer = panel_layer_of_keys(notes_pressed, PANEL_PLAY_LAYERS);  /* or of the play page: keys 13 and 14 */
    uint32_t keys_on = 0, sel_down = buttons >> BTN_SEL & 1u;
    uint32_t snap = buttons >> FM1_BTN_OCT_DOWN & 1u, lfo_down = buttons >> BTN_LFO & 1u;      /* OCT- held: a knob snaps to its places */
    uint32_t oct_up = buttons >> FM1_BTN_OCT_UP & 1u;           /* held: the FREQ knob moves the frequency by octaves */
    /* a press of OCT+ on a patch's page: its oscillator an octave up, with OCT- down; on the mixer's: PRE or POST */
    int32_t octaves = (pressed >> FM1_BTN_OCT_UP & 1u) ? (snap ? -1 : 1) : 0;
    volatile drone_voice_controls_t *voice;
    view_model_t model = { 0 };
    int moved = (pressed >> FM1_BTN_OCT_UP & 1u) != 0;         /* a control has moved in this turn: an encoder has clicks, or OCT+ is pressed */
    int turned = 0;                                             /* an encoder has clicks */
    loop_ms = now_ms;
    for (i = 0; i < FM1_NENC; i++)
        turned |= fm1_in.enc_steps[i] != 0;
    moved |= turned;
    for (v = 0; v < DRONE_VOICES; v++) {
        /* its key: a short press turns it on, or off; held a second it is selected, or at once when a control moves
         * while it is down, so that the move is for this voice. On the mixer's page its channel is selected too */
        key_gesture_t gesture = key_gesture(&voice_key_hold[v], notes >> voice_key[v] & 1u, now_ms, moved);
        if (gesture == KEY_TOGGLE)
            drone_controls.voice[v].on ^= 1u;
        if (gesture == KEY_SELECT) {
            selected_voice = v;
            if (panel.page == PANEL_MIXER)
                mixer_selected = v + 1u;
        }
        if (pressed & ~(1u << FM1_BTN_OCT_DOWN | 1u << FM1_BTN_OCT_UP | 1u << BTN_SEL | 1u << BTN_LFO | 1u << BTN_GLO | 1u << BTN_FX))
            drone_controls.voice[v].on = 0;                     /* any other button: all the voices off */
        keys_on |= (uint32_t)drone_controls.voice[v].on << voice_key[v];
        model.voices_on |= (uint8_t)(drone_controls.voice[v].on << v);
    }
    voice = &drone_controls.voice[selected_voice];
    /* the LFO key let go with nothing turned while it was down: the bank's page, or back */
    panel_lfo_key(&panel, (int)(pressed >> BTN_LFO & 1u), (int)(released >> BTN_LFO & 1u), lfo_down && turned);
    if (pressed >> BTN_GLO & 1u)               /* the play page, the mixer, the end of the chain, and round */
        panel_glo_key(&panel);
    if (pressed >> BTN_FX & 1u)                /* the disruptors: the noise, the pulse, the sine, and the play page again */
        panel_fx_key(&panel);
    if (panel_page_lfo(panel.page) >= 0)
        patch_lfo = (uint8_t)panel_page_lfo(panel.page);
    if (panel.page == PANEL_PLAY && key_layer >= 0)
        panel.play_layer = (uint8_t)key_layer;
    if (panel.page == PANEL_PLAY && lfo_down)
        amount_controls(selected_voice, panel.play_layer, (int)snap);
    else if (panel.page == PANEL_PLAY)
        play_controls(selected_voice, panel.play_layer, (int)snap, (int)oct_up);
    else if (panel.page == PANEL_BANK)
        bank_controls((int)snap, (int)lfo_down, octaves != 0, key_set);
    else if (panel.page == PANEL_MIXER)
        mixer_controls((int)snap, octaves != 0);
    else if (panel_page_dis(panel.page) >= 0)
        dis_controls(selected_voice, (uint32_t)panel_page_dis(panel.page), (int)snap);
    else if (!panel_in_eoc(&panel))
        patch_controls(voice, &patch_place[selected_voice], (int)snap, (int)lfo_down, octaves, &model);
    for (i = 0; i < DRONE_SLOTS; i++)
        model.block[i] = voice->block[i];
    for (i = 0; i < DRONE_OUT_NCABLED * DRONE_LFOS; i++)
        model.out_amount[i / DRONE_LFOS][i % DRONE_LFOS] = voice->out_amount[i / DRONE_LFOS][i % DRONE_LFOS];
    for (i = 0; i < DRONE_LFOS * DRONE_LFO_NCABLED * DRONE_LFOS; i++)
        model.lfo_amount[i / (DRONE_LFO_NCABLED * DRONE_LFOS)][i / DRONE_LFOS % DRONE_LFO_NCABLED][i % DRONE_LFOS] =
            voice->lfo_amount[i / (DRONE_LFO_NCABLED * DRONE_LFOS)][i / DRONE_LFOS % DRONE_LFO_NCABLED][i % DRONE_LFOS];
    for (i = 0; i < DRONE_FB_NPARAM; i++)
        model.fb[i] = voice->fb[i];
    for (i = 0; i < DRONE_LFOS; i++)
        model.fb_amount[i] = voice->fb_amount[i];
    if (panel.page == PANEL_FEEDBACK || panel.page == PANEL_PLAY) {     /* the scope: take the picture that is ready, and ask for the next */
        drone_scope_t *scope = &drone_engine.scope;
        int out = panel.page == PANEL_PLAY;     /* there: the voice's sound, not its feedback */
        int ours = scope->voice == selected_voice && scope->out == out;         /* the picture, or the one asked, is of this voice and page */
        for (i = 0; i < DRONE_SCOPE_POINTS && ours && !scope->wanted; i++) {
            scope_picture[0][i] = (int8_t)((scope->in[i] * 28) >> 15);
            scope_picture[1][i] = (int8_t)((scope->back[i] * 28) >> 15);
        }
        /* another voice is selected now: ask at once, or a picture asked of a silent voice stops this one's for good */
        if (!ours || (!scope->wanted && now_ms - scope_asked_at >= SCOPE_EVERY_MS)) {
            drone_scope_ask(scope, selected_voice, out);
            scope_asked_at = now_ms;
        } else if (scope->wanted && now_ms - scope_asked_at >= 5u * SCOPE_EVERY_MS) {  /* none comes: the voice is silent */
            for (i = 0; i < 2u * DRONE_SCOPE_POINTS; i++)
                scope_picture[i / DRONE_SCOPE_POINTS][i % DRONE_SCOPE_POINTS] = 0;
        }
        for (i = 0; i < 2u * DRONE_SCOPE_POINTS; i++)
            model.scope[i / DRONE_SCOPE_POINTS][i % DRONE_SCOPE_POINTS] = scope_picture[i / DRONE_SCOPE_POINTS][i % DRONE_SCOPE_POINTS];
    }
    model.reverb_holder = DRONE_VOICES;         /* the reverb: the first FX block of the first patch that has one, as drone_reverb_holder */
    model.fx_place = DRONE_SLOTS;
    for (v = DRONE_VOICES; v-- > 0;)
        for (i = DRONE_SLOTS; i-- > 0;)
            if (drone_controls.voice[v].block[i].type == DRONE_BLOCK_FX) {
                model.reverb_holder = (uint8_t)v;
                model.fx_place = (uint8_t)i;
            }
    if (model.reverb_holder != selected_voice)
        model.fx_place = DRONE_SLOTS;
    model.place = patch_place[selected_voice];
    model.lfo_held = (uint8_t)lfo_down;
    model.lfo_current = patch_lfo;
    if (panel_in_eoc(&panel))                   /* SELECT: the delay, the reverb, the compressor */
        panel_eoc_select(&panel, fm1_enc_take(ENC_SELECT));
    for (i = 0; i < FM1_NENC && panel_in_eoc(&panel); i++) {
        /* the encoder's parameter on the open effect's page, and where its value is; the left encoders have none */
        const param_spec_t *spec = panel_eoc_spec(panel.page, i);
        uint32_t n = i - PANEL_ENC_VALUES;
        int32_t clicks = fm1_enc_take(i);
        if (spec && clicks) {
            volatile uint8_t *value = panel.page == PANEL_DELAY ? &drone_controls.delay[n] : panel.page == PANEL_COMP ? &drone_controls.comp[n]
                                    : &drone_controls.reverb[n - 1u];       /* the reverb's first encoder has no spec */
            *value = knob_turned(i, spec, *value, clicks, (int)snap);
            touched[panel.page] = (uint8_t)i;
        }
    }

    /* on the mixer's page the amber mark is the selected channel's: with none selected, no voice has it */
    model.selected = (uint8_t)(panel.page == PANEL_MIXER && !mixer_selected ? DRONE_VOICES : selected_voice);
    model.channel = (uint8_t)mixer_selected;
    for (v = 0; v < DRONE_VOICES; v++) {
        model.post[v] = mixer_channels[v].post;
        for (i = 0; i < MIXER_NVALUE; i++)
            model.mixer[v][i] = mixer_channels[v].value[i];
    }
    model.page = panel.page;
    model.choosing = panel.choosing;
    model.touched = touched[panel.page];
    for (i = 0; i < DRONE_NPARAM; i++)
        model.param[i] = voice->param[i];
    for (i = 0; i < DRONE_LFO_NPARAM; i++)      /* the open LFO's; on another page, the first one's: not shown */
        model.lfo[i] = voice->lfo[panel_page_lfo(panel.page) >= 0 ? panel_page_lfo(panel.page) : 0][i];
    model.send = voice->send;
    model.style = play_voice[selected_voice].style;
    model.freq = play_voice[selected_voice].freq;
    for (i = 0; i < PLAY_NKNOB; i++)
        model.knob[i] = play_voice[selected_voice].knob[i];
    model.layer = panel.play_layer;
    for (v = 0; v < DRONE_VOICES; v++)
        model.link[v] = play_voice[v].knob[PLAY_LINK];
    for (v = 0; v < DRONE_VOICES; v++)
        model.link_kind[v] = play_voice[v].link_kind;
    for (i = 0; i < DRONE_DISRUPTORS; i++) {
        for (v = 0; v < DRONE_VOICES; v++) {
            model.dis_send[i][v] = play_voice[v].dis[i];
            model.dis_to[i][v] = play_voice[v].dis_to[i];
        }
        for (v = 0; v < DRONE_DIS_NPARAM; v++)
            model.dis[i][v] = drone_controls.dis[i][v];
    }
    for (i = 0; i < DRONE_REVERB_NPARAM; i++)
        model.reverb[i] = drone_controls.reverb[i];
    for (i = 0; i < DRONE_DELAY_NPARAM; i++)
        model.delay[i] = drone_controls.delay[i];
    for (i = 0; i < DRONE_COMP_NPARAM; i++)
        model.comp[i] = drone_controls.comp[i];
    bank_take(now_ms);                          /* the bank: the shown LFO as the engine gave it, its cables, the selected voice's amounts */
    view_bank_take(&model, &drone_controls, &bank_shown, bank_lfo, bank_source, panel.bank_set);
    model.amount_touched = amount_touched;
    model.bank_used = (uint16_t)panel_bank_used(play_voice);
    for (i = 0; i < PLAY_NBANKED * DRONE_BANK_LFOS; i++)
        model.bank_amount[i / DRONE_BANK_LFOS][i % DRONE_BANK_LFOS] = play_voice[selected_voice].bank_amount[i / DRONE_BANK_LFOS][i % DRONE_BANK_LFOS];
    model.clock_phase = bank_now.clock_phase;   /* the beat's square: taken ten times a second it would miss beats at a fast tempo */
    model.info = (uint8_t)sel_down;             /* SEL held: the version and the load, for the one who builds the firmware */
    model.load_percent = (uint8_t)(load_percent > 255u ? 255u : load_percent);
    model.faults = (uint8_t)((fm1_crash.magic == FM1_CRASH_MAGIC ? VIEW_CRASHED : 0) | (audio_start_error ? VIEW_NO_AUDIO : 0));
    view_show(&model);

    buttons |= (uint32_t)(panel.page == PANEL_MIXER || panel_in_eoc(&panel)) << BTN_GLO;    /* the GLO key's light stays on in the mixer and the end of the chain */
    buttons |= (uint32_t)(panel_page_dis(panel.page) >= 0) << BTN_FX;   /* and the FX key's in the disruptors */
    buttons |= (uint32_t)(panel.page == PANEL_BANK) << BTN_LFO;         /* and the LFO key's in the bank's page */
    if (panel.page == PANEL_BANK && !lfo_down)  /* the layers' keys; the one in hand's blinks */
        keys_on |= panel_layer_lights(PANEL_BANK_NSET, panel.bank_set, now_ms);
    if (panel.page == PANEL_PLAY)
        keys_on |= panel_layer_lights(PANEL_PLAY_LAYERS, panel.play_layer, now_ms);
    for (i = 0; i < NBUTTON; i++)
        if ((buttons ^ lit.buttons) >> i & 1u)
            fm1_led_key(i, buttons >> i & 1u);                  /* felucca-bsp's ids: buttons 0..13, then the notes */
    for (i = 0; i < NNOTE; i++)                 /* a key's LED: its voice is on, or it is a set's key */
        if ((keys_on ^ lit.keys_on) >> i & 1u)
            fm1_led_key(NBUTTON + i, keys_on >> i & 1u);
    lit.buttons = buttons;
    lit.keys_on = keys_on;
}

static void fm1_fault(const fm1_crash_t *c)     /* felucca-bsp calls this on a crash; the record stays in fm1_crash */
{
    (void)c;
    fm1_lcd_fill_rect(0, 0, 240, 240, RED);
    fm1_delay_ms(1500);
    fm1_reboot();
}

/* ---- the update entry: what felucca-bsp's ota.c asks of the firmware ---- */
static uint8_t update_allowed;                  /* the flash part is one the flash driver is known to work with */

static int update_in_staging(uint32_t off, uint32_t n) { return FL_IN(off, n, OTA_AREA, OTA_AREA + OTA_AREA_LEN); }
static uint32_t ota_now_ms(void) { return uptime_ms(); }
static void ota_idle(void) { fm1_wdt_feed(); }
static int ota_erase(uint32_t off)
{
    uint32_t took_us;
    if ((off & 0xFFFu) || !update_in_staging(off, 0x1000u))
        return -8;
    return fl_erase4k(off, &took_us);
}
static int ota_prog(uint32_t off, const void *p, uint32_t n)
{
    return update_in_staging(off, n) ? fl_write(off, p, n) : -8;
}
static int ota_fread(uint32_t off, void *p, uint32_t n) { return fl_read(off, p, n); }
static void ota_show(uint32_t step, int32_t code)   /* steps 0..8, then the result: code 1 is success */
{
    fm1_lcd_fill_rect(0, PROGRESS_Y, 240, PROGRESS_H, BLACK);
    if (step < 9u)
        fm1_lcd_fill_rect(0, PROGRESS_Y, (step > 6u ? 6u : step) * 40u, PROGRESS_H, WHITE);
    else
        fm1_lcd_fill_rect(0, PROGRESS_Y, 240, PROGRESS_H, code == 1 ? GREEN : RED);
}
static void ota_commit(const uint8_t *parm)     /* the package is checked: restart into the update loader */
{
    boot_guard_pass();
    usb_detach();
    fm1_delay_ms(30);
    fm1_enter_update(parm);
}

/* ---- the serial port ---- */
static void serial_put(char c)
{
    if (!cdc.dtr || co_w - co_r >= CO_N)        /* no one listens, or the queue is full: drop it */
        return;
    cdc_out[co_w % CO_N] = (uint8_t)c;
    RING_PUBLISH();
    co_w++;
}
static void serial_text(const char *s)
{
    while (*s)
        serial_put(*s++);
}
static void serial_hex(uint32_t v, uint32_t digits)
{
    while (digits--)
        serial_put("0123456789ABCDEF"[(v >> (4u * digits)) & 15u]);
}

static void leave_usb(void)                     /* let the text out, then go off the bus before a restart */
{
    fm1_delay_ms(30);
    usb_detach();
}

static void enter_update_mode(void)
{
    boot_guard_pass();
    serial_text("update mode\r\n");
    leave_usb();
    fm1_enter_uboot();
}

static void serial_command(char c, uint32_t flash_id)
{
    uint32_t i, v;
    switch (c) {
    case 'u':
        enter_update_mode();
        break;
    case 'r':
        boot_guard_pass();
        serial_text("restart\r\n");
        leave_usb();
        fm1_reboot();
        break;
    default:
        serial_text(APP_NAME " " APP_VERSION " " APP_PACKAGE_ID " flash=");
        serial_hex(flash_id, 6);
        serial_text(update_allowed ? " update=yes" : " update=NO");
        serial_text(fm1_crash.magic == FM1_CRASH_MAGIC ? " crashed-before" : "");
        serial_text(audio_start_error ? " audio=FAILED" : " audio=ok");
        serial_text(" load%=0x");
        serial_hex(load_percent, 2);
        serial_text(" voices=0x");
        serial_hex(voices_sounding, 2);
        serial_text(" master=0x");
        serial_hex(drone_controls.master, 3);
        serial_text(" buttons=");
        serial_hex(fm1_in.buttons, 4);
        serial_text(" keys=");
        serial_hex(fm1_in.notes, 7);
        serial_text(" page=");
        serial_hex(panel.page, 1);
        for (v = 0; v < DRONE_VOICES; v++) {
            serial_text(v == selected_voice ? " *voice" : " voice");
            serial_text(drone_controls.voice[v].on ? "=on:" : "=off:");
            for (i = 0; i < DRONE_NPARAM; i++)          /* its level and its fade */
                serial_hex(drone_controls.voice[v].param[i], 2);
        }
        serial_text("  (u: update mode, r: restart)\r\n");
        break;
    }
}

static void app_main(void)
{
    uint32_t flash_id = fl_jedec(), healthy = 0, i, v, measured_at = 0, master_at = 0;
    /* the Puya P25Q80 (the first units) and the UC25HQ80 (newer ones): the parts fm1-base accepts with this driver */
    update_allowed = flash_id == 0x856014u || flash_id == 0xB36014u;
    if (update_allowed)
        ota_boot_cleanup();                     /* what a stopped update left in the staging area */
    for (v = 0; v < DRONE_VOICES; v++) {        /* each voice starts with its own style, at its own frequency: 92, 159 and 228 Hz, no chord */
        static const uint16_t freq_at_start[DRONE_VOICES] = { 5376, 6577, 7377 };
        play_voice[v].style = (uint8_t)(v % STYLE_NSTYLE);
        play_voice[v].freq = freq_at_start[v];
        mixer_channels[v] = mixer_channel_new();
        for (i = 0; i < PLAY_NKNOB; i++)
            play_voice[v].knob[i] = play_knob_spec[i].start;
        play_apply(v);
    }
    for (i = 0; i < DRONE_REVERB_NPARAM; i++)
        drone_controls.reverb[i] = drone_reverb_spec[i].start;
    for (i = 0; i < DRONE_DELAY_NPARAM; i++)
        drone_controls.delay[i] = drone_delay_spec[i].start;
    for (i = 0; i < DRONE_COMP_NPARAM; i++)
        drone_controls.comp[i] = drone_comp_spec[i].start;
    for (i = 0; i < DRONE_DISRUPTORS * DRONE_DIS_NPARAM; i++)
        drone_controls.dis[i / DRONE_DIS_NPARAM][i % DRONE_DIS_NPARAM] = drone_dis_spec[i / DRONE_DIS_NPARAM][i % DRONE_DIS_NPARAM].start;
    for (i = 0; i < DRONE_BANK_LFOS * DRONE_BANK_NPARAM; i++)
        drone_controls.bank[i / DRONE_BANK_NPARAM][i % DRONE_BANK_NPARAM] = drone_bank_spec[i % DRONE_BANK_NPARAM].start;
    drone_controls.tempo = DRONE_TEMPO_START;
    drone_init(&drone_engine);
    fm1_lcd_init();
    fm1_input_init();
    fm1_adc_init();
    usb_start();
    fm1_timer5_start(isr_timer5, 4);            /* above the audio: a long render does not delay the key scan */
    audio_start_error = fm1_audio_init_checked(audio_buffer, sizeof audio_buffer / sizeof audio_buffer[0],
                                               AUDIO_HALF_WORDS, FM1_AUDIO_RATE, isr_alnk0, 3);
    screen_draw();
    fm1_guard_lock_top();
    fm1_irq_enable_all();
    fm1_lcd_backlight(1);
    for (;;) {
        uint32_t now = uptime_ms();
        fm1_wdt_feed();
        usb_retry(now);
        if (!healthy && now > HEALTHY_AFTER_MS) {
            healthy = 1;
            boot_guard_pass();
        }
        if (usb.uboot_req)                      /* the USB-MIDI command for update mode */
            enter_update_mode();
        ota_service();                          /* answers the installer's identity query */
        if (usb.ota_req) {                      /* the installer starts an update: it restarts, or fails and returns */
            usb.ota_req = 0;
            if (update_allowed)
                ota_session();
            screen_draw();
        }
        controls_show(now);
        if (now - master_at >= 20u) {           /* the MASTER knob: 0..1023 from the converter */
            int32_t knob = fm1_adc_read(FM1_ADC_MASTER);
            master_at = now;
            if (knob >= 0)
                drone_controls.master = (uint16_t)(((uint32_t)knob * DRONE_MASTER_MAX + 511u) / 1023u);
        }
        if (now - measured_at >= 250u) {        /* the load: the longest render of the last quarter second */
            uint32_t peak = render_ticks_peak;
            render_ticks_peak = 0;
            measured_at = now;
            load_percent = (peak * 100u + AUDIO_HALF_TICKS / 2u) / AUDIO_HALF_TICKS;
        }
        while (ci_r != ci_w)
            serial_command((char)cdc_in[ci_r++ % CI_N], flash_id);
    }
}

extern uint32_t _data_start[], _data_end[], _data_load[], _bss_start[], _bss_end[], _rt_start[], _rt_end[], _rt_load[];

void fm1_cstart(void)                           /* felucca-bsp's crt0.S comes here */
{
    uint32_t *from, *to;
    fm1_time_init();
    fm1_wdt_arm(0x0D);
    boot_guard_check();
    fm1_irq_init();
    for (to = _bss_start; to < _bss_end; to++)
        *to = 0;
    for (from = _data_load, to = _data_start; to < _data_end;)
        *to++ = *from++;
    for (from = _rt_load, to = _rt_start; to < _rt_end;)    /* the flash driver runs from RAM */
        *to++ = *from++;
    fm1_guard_enable(FM1_GUARD_STACK | FM1_GUARD_WRITE | FM1_GUARD_BUS | FM1_GUARD_PC);
    app_main();
    for (;;)
        ;
}
