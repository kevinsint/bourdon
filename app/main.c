/* SINT_FM1_DR1: the empty firmware. It has only what keeps the FM-1 replaceable:
 *   - the boot guard: two starts in a row that do not stay up 2 s end in the chip's update mode
 *   - the update entry: the installer (bsp/tools/fm1_install.py) replaces this firmware over USB-MIDI
 *   - update mode on request: the USB-MIDI command, or 'u' on the serial port
 *   - the watchdog and the hardware guards; a crash shows a red screen, then restarts
 * and the sound engine (drone.h), six voices, OP1 to OP6, on the first six black keys:
 *   - a short press of the voice's key turns the voice on, the next one off (when the key is let go)
 *   - a press of a second or more selects the voice and does not turn it on or off
 *   - the seven encoders are the selected voice's parameters; OCT- and OCT+ move its pitch by an octave
 *   - the LFO key opens the voice's LFO 1, 2, 3, the reverb, then the voice again; on an LFO's page the first
 *     four encoders are its speed, wave, depth and target; on the reverb's, the voice's send, then the decay, the
 *     tone and the return of the one reverb all the voices share
 *   - SEL held and the SELECT encoder choose the voice's filter; the voice gets it when SEL is let go
 *   - any other button turns all the voices off; the MASTER knob is the level
 * OP1 to OP3 are patches (drone.h): a chain of blocks that the player makes, on its own page (panel.h):
 *   - the SELECT encoder moves the cursor along the chain; the four right encoders are the values of the block
 *     under it, or on OUT the voice's level, fade and send; the PRESETS encoder is the block's kind
 *   - SEL held and SELECT choose a block for the cursor's place, or none to take the block away; SEL held and the
 *     PRESETS encoder move the block along the chain
 *   - OCT- and OCT+ move the pitch of the oscillator under the cursor; the LFO key opens the reverb, then the chain
 * The screen (view.h) shows the selected voice as its chain: oscillator, filter, level and fade, each a picture,
 * and at the bottom what the open page is for. The two octave buttons held together show the version and how much
 * of its time the processor spends on the sound; that load is also a thin line at the bottom, red above 80 %.
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

/* each voice's key, as felucca-bsp numbers the 27 note keys from the left: 0 is F, so these are the first six
 * black keys, the ones the panel labels OP1 to OP6 (F sharp, G sharp, A sharp, C sharp, D sharp, F sharp) */
static const uint8_t voice_key[DRONE_VOICES] = { 1, 3, 5, 8, 10, 13 };
static key_hold_t voice_key_hold[DRONE_VOICES];
_Static_assert(DRONE_NPARAM == FM1_NENC && VIEW_ENCODERS == FM1_NENC, "one encoder for each parameter");
static uint32_t selected_voice;                 /* the encoders are this voice's parameters */
static uint8_t patch_place[PANEL_PATCH_VOICES]; /* where the cursor is in each patch's chain */
_Static_assert(PANEL_PATCH_VOICES <= DRONE_VOICES && PANEL_ENC_VALUES + 4u <= FM1_NENC, "a patch's page fits the voices and the encoders");
static panel_t panel;                           /* the open page, and the choice of filter while SEL is down */
static uint8_t touched[PANEL_NPAGE] = { [PANEL_VOICE] = DRONE_CUTOFF };  /* the encoder turned last on each page */
static uint32_t load_percent;                   /* the processor's time on the sound; 100: it breaks up */
/* felucca-bsp names only the two octave buttons; these two numbers are read from an FM-1 with tools/fm1_keys.py */
enum { BTN_SEL = 3, BTN_LFO = 5 };
#define ENC_SELECT 0u                           /* the SELECT encoder */
_Static_assert(DRONE_LFO_NPARAM <= FM1_NENC && 1 + DRONE_REVERB_NPARAM <= FM1_NENC && DRONE_VOICES <= 9,
               "an LFO's page and the reverb's fit the encoders; OP and one digit");

static struct { uint32_t buttons, keys_on; } lit;       /* the LEDs as they are */

/* a patch's page: the cursor, the choice of a block, and the encoders of the place under the cursor. The audio
 * interrupt reads the chain while this writes it; a chain read half written sounds wrong for one block of sound
 * at most, as the engine takes any values */
static void patch_controls(volatile drone_voice_controls_t *voice, uint8_t *place_of_voice, int sel_down, int32_t octaves, view_model_t *model)
{
    drone_block_controls_t chain[DRONE_SLOTS];
    uint32_t i, count, at, place = *place_of_voice, type, chosen;
    int32_t clicks, moves;
    for (i = 0; i < DRONE_SLOTS; i++)
        chain[i] = voice->block[i];
    count = panel_patch_count(chain);
    place = place < panel_patch_places(count) ? place : panel_patch_places(count) - 1u;
    at = panel_patch_at(count, place);
    type = at == PANEL_AT_BLOCK ? chain[place].type : DRONE_BLOCK_NONE;

    clicks = fm1_enc_take(ENC_SELECT);
    moves = fm1_enc_take(PANEL_ENC_KIND);
    chosen = panel_choice(&panel, sel_down && at != PANEL_AT_OUT, clicks, &drone_block_type_spec, type);
    if (chosen != type) {                       /* SEL let go with another block chosen */
        panel_patch_put(chain, place, chosen);
    } else if (sel_down) {
        if (moves)
            place = panel_patch_move(chain, place, moves);
    } else {
        place = (uint32_t)param_turned(&(param_spec_t){ (uint8_t)(panel_patch_places(count) - 1u), 1, 0 }, (int32_t)place, clicks);
        at = panel_patch_at(count, place);
        type = at == PANEL_AT_BLOCK ? chain[place].type : DRONE_BLOCK_NONE;
        if (at == PANEL_AT_BLOCK && type == DRONE_BLOCK_VCO) {      /* the octave buttons: this oscillator's pitch */
            int32_t pitch = chain[place].param[DRONE_VCO_PITCH] + 12 * octaves;
            if (pitch >= 0 && pitch <= drone_block_spec[DRONE_BLOCK_VCO][DRONE_VCO_PITCH].top)
                chain[place].param[DRONE_VCO_PITCH] = (uint8_t)pitch;
        }
        for (i = PANEL_ENC_KIND; i < FM1_NENC; i++) {
            const param_spec_t *spec = panel_patch_spec(at, type, i);
            volatile uint8_t *const of_out[3] = { &voice->param[DRONE_LEVEL], &voice->param[DRONE_FADE], &voice->send };
            volatile uint8_t *value = at == PANEL_AT_OUT ? of_out[(i - PANEL_ENC_VALUES) % 3u]
                                    : i == PANEL_ENC_KIND ? &chain[place % DRONE_SLOTS].kind
                                    : &chain[place % DRONE_SLOTS].param[(i - PANEL_ENC_VALUES) % DRONE_BLOCK_NPARAM];
            clicks = i == PANEL_ENC_KIND ? moves : fm1_enc_take(i);
            if (spec && clicks) {
                *value = (uint8_t)param_turned(spec, *value, clicks);
                touched[PANEL_VOICE] = (uint8_t)i;
            }
        }
    }
    for (i = 0; i < DRONE_SLOTS; i++) {
        voice->block[i] = chain[i];
        model->block[i] = chain[i];
    }
    count = panel_patch_count(chain);
    *place_of_voice = (uint8_t)(place < panel_patch_places(count) ? place : panel_patch_places(count) - 1u);
    model->place = *place_of_voice;
    model->choice = panel.choice;
}

static void screen_draw(void)                   /* the whole screen again, after something else wrote on it */
{
    view_reset();
}

/* the main loop: the keys, buttons and encoders into the engine's controls, and all of it shown (view.h) */
static void controls_show(uint32_t now_ms)
{
    uint32_t i, v, buttons = fm1_in.buttons, notes = fm1_in.notes, released, pressed = fm1_input_edges(&released);
    uint32_t keys_on = 0, sel_down = buttons >> BTN_SEL & 1u;
    int32_t octaves = ((pressed >> FM1_BTN_OCT_UP) & 1) - ((pressed >> FM1_BTN_OCT_DOWN) & 1), pitch;
    volatile drone_voice_controls_t *voice;
    view_model_t model = { 0 };
    int patch;
    (void)released;
    for (v = 0; v < DRONE_VOICES; v++) {
        key_gesture_t gesture = key_gesture(&voice_key_hold[v], notes >> voice_key[v] & 1u, now_ms);
        if (gesture == KEY_TOGGLE)
            drone_controls.voice[v].on ^= 1u;
        if (gesture == KEY_SELECT)
            selected_voice = v;
        if (pressed & ~(1u << FM1_BTN_OCT_DOWN | 1u << FM1_BTN_OCT_UP | 1u << BTN_SEL | 1u << BTN_LFO))
            drone_controls.voice[v].on = 0;                     /* any other button: all the voices off */
        keys_on |= (uint32_t)drone_controls.voice[v].on << voice_key[v];
        model.voices_on |= (uint8_t)(drone_controls.voice[v].on << v);
    }
    voice = &drone_controls.voice[selected_voice];
    patch = voice->patch;
    if (pressed >> BTN_LFO & 1u)
        panel_next_page(&panel, patch);
    if (patch && panel.page != PANEL_REVERB)    /* a patch has no LFO pages: from one of them, its chain */
        panel.page = PANEL_VOICE;
    pitch = voice->param[DRONE_PITCH] + 12 * octaves;
    if (!patch && octaves && pitch >= 0 && pitch <= drone_param_spec[DRONE_PITCH].top)
        voice->param[DRONE_PITCH] = (uint8_t)pitch;
    if (patch && panel.page == PANEL_VOICE)
        patch_controls(voice, &patch_place[selected_voice], (int)sel_down, octaves, &model);
    else
        for (i = 0; i < DRONE_SLOTS; i++)       /* a patch's reverb page shows the chain too */
            model.block[i] = voice->block[i];
    for (i = 0; i < FM1_NENC && !(patch && panel.page == PANEL_VOICE); i++) {
        /* the encoder's parameter on the open page, and where its value is; the last encoders have none on some pages */
        const param_spec_t *spec = panel_spec(panel.page, i);
        volatile uint8_t *value = !spec ? 0 : panel.page == PANEL_VOICE ? &voice->param[i]
                                : panel.page != PANEL_REVERB ? &voice->lfo[panel.page - 1u][i]
                                : i ? &drone_controls.reverb[i - 1u] : &voice->send;
        int32_t clicks = fm1_enc_take(i);
        if (i == ENC_SELECT && !patch) {        /* with SEL down it chooses the filter, and its parameter stays */
            voice->filter = (uint8_t)panel_choice(&panel, (int)sel_down, clicks, &drone_filter_spec, voice->filter);
            clicks = sel_down ? 0 : clicks;
        }
        if (spec && clicks) {
            *value = (uint8_t)param_turned(spec, *value, clicks);
            touched[panel.page] = (uint8_t)i;
        }
    }

    model.selected = (uint8_t)selected_voice;
    model.patch = (uint8_t)patch;
    model.page = panel.page;
    model.choosing = panel.choosing;
    model.filter = panel.choosing ? panel.choice : voice->filter;
    model.touched = touched[panel.page];
    for (i = 0; i < DRONE_NPARAM; i++)
        model.param[i] = voice->param[i];
    for (i = 0; i < DRONE_LFO_NPARAM; i++)      /* the open LFO's; on another page, the first one's: not shown */
        model.lfo[i] = voice->lfo[panel.page != PANEL_VOICE && panel.page != PANEL_REVERB ? panel.page - 1u : 0u][i];
    model.send = voice->send;
    for (i = 0; i < DRONE_REVERB_NPARAM; i++)
        model.reverb[i] = drone_controls.reverb[i];
    /* the two octave buttons held together: the version and the load, for the one who builds the firmware */
    model.info = (buttons >> FM1_BTN_OCT_DOWN & buttons >> FM1_BTN_OCT_UP & 1u) != 0;
    model.load_percent = (uint8_t)(load_percent > 255u ? 255u : load_percent);
    model.faults = (uint8_t)((fm1_crash.magic == FM1_CRASH_MAGIC ? VIEW_CRASHED : 0) | (audio_start_error ? VIEW_NO_AUDIO : 0));
    view_show(&model);

    for (i = 0; i < NBUTTON; i++)
        if ((buttons ^ lit.buttons) >> i & 1u)
            fm1_led_key(i, buttons >> i & 1u);                  /* felucca-bsp's ids: buttons 0..13, then the notes */
    for (i = 0; i < NNOTE; i++)                 /* a key's LED: its voice is on */
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
            for (i = 0; i < DRONE_NPARAM; i++)
                serial_hex(drone_controls.voice[v].param[i], 2);
            serial_text(" filter=");
            serial_hex(drone_controls.voice[v].filter, 1);
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
    for (v = 0; v < DRONE_VOICES; v++) {
        drone_controls.voice[v].filter = drone_filter_spec.start;
        drone_controls.voice[v].send = drone_send_spec.start;
        for (i = 0; i < DRONE_NPARAM; i++)
            drone_controls.voice[v].param[i] = drone_param_spec[i].start;
        /* a voice starts at its key's own note, not all at the first one's */
        drone_controls.voice[v].param[DRONE_PITCH] += voice_key[v] - voice_key[0];
        if (v < PANEL_PATCH_VOICES) {           /* a patch starts as the voice it was: oscillator, filter, level */
            drone_controls.voice[v].patch = 1;
            drone_controls.voice[v].block[0] = panel_block_new(DRONE_BLOCK_VCO);
            drone_controls.voice[v].block[0].param[DRONE_VCO_PITCH] = drone_controls.voice[v].param[DRONE_PITCH];
            drone_controls.voice[v].block[1] = panel_block_new(DRONE_BLOCK_VCF);
            drone_controls.voice[v].block[2] = panel_block_new(DRONE_BLOCK_VCA);
        }
        for (i = 0; i < DRONE_LFOS * DRONE_LFO_NPARAM; i++)
            drone_controls.voice[v].lfo[i / DRONE_LFO_NPARAM][i % DRONE_LFO_NPARAM] = drone_lfo_spec[i % DRONE_LFO_NPARAM].start;
    }
    for (i = 0; i < DRONE_REVERB_NPARAM; i++)
        drone_controls.reverb[i] = drone_reverb_spec[i].start;
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
