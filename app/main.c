/* SINT_FM1_DR1: the empty firmware. It has only what keeps the FM-1 replaceable:
 *   - the boot guard: two starts in a row that do not stay up 2 s end in the chip's update mode
 *   - the update entry: the installer (bsp/tools/fm1_install.py) replaces this firmware over USB-MIDI
 *   - update mode on request: the USB-MIDI command, or 'u' on the serial port
 *   - the watchdog and the hardware guards; a crash shows a red screen, then restarts
 * and the sound engine (drone.h), one voice for now, on the first black key:
 *   - a short press of the voice's key turns the voice on, the next one off (when the key is let go)
 *   - a press of a second or more selects the voice and does not turn it on or off
 *   - the seven encoders are the selected voice's parameters; OCT- and OCT+ move its pitch by an octave
 *   - the LFO key opens the voice's LFO 1, 2, 3, then the voice again; on an LFO's page the first four encoders
 *     are its speed, wave, depth and target
 *   - SEL held and the SELECT encoder choose the voice's filter; the voice gets it when SEL is let go
 *   - any other button turns all the voices off; the MASTER knob is the level
 * The screen shows the selected voice, its filter, the open page, and how much of its time the processor spends
 * on the sound.
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

/* ---- the screen ---- */
#define PROGRESS_Y 216u
#define PROGRESS_H 24u

#define TITLE APP_NAME " " APP_VERSION
#define PARAM_X(i) (8u + (i) * 33u)             /* seven bars, one for each encoder */
#define STATUS_Y 44u                            /* the selected voice, its filter, and the open page at the right */
#define STATUS_FILTER_X (8u + 4u * 12u)         /* after "OP1 " */
#define PARAM_Y 62u
#define PARAM_W 26u
#define PARAM_H 78u
#define LOAD_Y 26u                              /* the processor's time on the sound: a bar and the per cent */
#define LOAD_BAR_W 176u
#define LOAD_TEXT_X 198u
#define BUTTON_X(i) (8u + (i) * 16u)            /* fourteen cells */
#define BUTTON_Y 148u
#define KEY_X(i) (12u + (i) * 8u)               /* twenty-seven cells, the lowest note at the left */
#define KEY_Y 176u
#define NBUTTON 14u
#define NNOTE 27u

/* each voice's key, as felucca-bsp numbers the 27 note keys from the left: 1 is the first black key */
static const uint8_t voice_key[DRONE_VOICES] = { 1 };
static key_hold_t voice_key_hold[DRONE_VOICES];
_Static_assert(DRONE_NPARAM == FM1_NENC, "one encoder for each parameter");
static uint32_t selected_voice;                 /* the encoders are this voice's parameters */
static panel_t panel;                           /* the open page, and the choice of filter while SEL is down */
/* felucca-bsp names only the two octave buttons; these two numbers are read from an FM-1 with tools/fm1_keys.py */
enum { BTN_SEL = 3, BTN_LFO = 5 };
#define ENC_SELECT 0u                           /* the SELECT encoder */
_Static_assert(DRONE_LFO_NPARAM <= FM1_NENC && DRONE_VOICES <= 9, "an LFO's page fits the encoders; OP and one digit");
#define SELECTED_Y (KEY_Y + 30u)                /* a mark under the selected voice's key */

/* what the screen and the LEDs show now; controls_show draws only what is different */
static struct { uint32_t buttons, keys_on, selected_voice, status, bar_height[FM1_NENC], load_percent; } shown;
#define NOT_SHOWN 0xFFFFFFFFu

static uint32_t voice_keys(void)                /* bit n: note key n has a voice */
{
    uint32_t v, keys = 0;
    for (v = 0; v < DRONE_VOICES; v++)
        keys |= 1u << voice_key[v];
    return keys;
}

static void screen_draw(void)                   /* the whole screen again; the controls come back as off and empty */
{
    uint32_t i;
    fm1_lcd_fill_rect(0, 0, 240, 240, BLACK);
    font_draw_text((240u - font_text_width(TITLE, 2)) / 2u, 8, 2, TITLE, WHITE);
    if (fm1_crash.magic == FM1_CRASH_MAGIC)     /* a run before this one crashed */
        fm1_lcd_fill_rect(0, 0, 240, 4, AMBER);
    if (audio_start_error)                      /* no sound: the audio hardware refused to start */
        fm1_lcd_fill_rect(0, 4, 240, 2, RED);
    shown.load_percent = NOT_SHOWN;
    for (i = 0; i < FM1_NENC; i++)
        fm1_lcd_fill_rect(PARAM_X(i), PARAM_Y, PARAM_W, PARAM_H, DIM);
    for (i = 0; i < NBUTTON; i++)
        fm1_lcd_fill_rect(BUTTON_X(i), BUTTON_Y, 14, 16, DIM);
    for (i = 0; i < NNOTE; i++)                 /* a key with no voice is nearly dark */
        fm1_lcd_fill_rect(KEY_X(i), KEY_Y, 7, 28, voice_keys() >> i & 1u ? DIM : FAINT);
    shown.buttons = 0;
    shown.keys_on = 0;
    shown.selected_voice = NOT_SHOWN;
    shown.status = NOT_SHOWN;
    for (i = 0; i < FM1_NENC; i++)
        shown.bar_height[i] = 0;
}

/* the status line: OP and the voice's number, its filter (amber: the choice, not the voice's yet), the page */
static void status_show(uint32_t filter, uint32_t choosing)
{
    char voice_name[4] = "OP1";
    const char *page_name = panel_page_name[panel.page];
    uint32_t status = selected_voice << 16 | (uint32_t)panel.page << 8 | choosing << 4 | filter;
    if (status == shown.status)
        return;
    shown.status = status;
    voice_name[2] = (char)('1' + selected_voice);
    fm1_lcd_fill_rect(0, STATUS_Y, 240, 14, BLACK);
    font_draw_text(8, STATUS_Y, 2, voice_name, WHITE);
    font_draw_text(STATUS_FILTER_X, STATUS_Y, 2, panel_filter_name[filter], choosing ? AMBER : WHITE);
    font_draw_text(232u - font_text_width(page_name, 2), STATUS_Y, 2, page_name, panel.page == PANEL_VOICE ? GREY : WHITE);
}

static void load_show(uint32_t percent)          /* 100: the sound takes all the time there is, and breaks up */
{
    char text[4] = "   ";
    uint32_t bar = (percent > 100u ? 100u : percent) * LOAD_BAR_W / 100u, digit = 3, left = percent > 999u ? 999u : percent;
    if (percent == shown.load_percent)
        return;
    shown.load_percent = percent;
    do {
        text[--digit] = (char)('0' + left % 10u);
        left /= 10u;
    } while (left);
    fm1_lcd_fill_rect(8, LOAD_Y + 2u, bar, 10, percent > 80u ? RED : GREEN);
    fm1_lcd_fill_rect(8u + bar, LOAD_Y + 2u, LOAD_BAR_W - bar, 10, DIM);
    fm1_lcd_fill_rect(LOAD_TEXT_X, LOAD_Y, 36, 14, BLACK);
    font_draw_text(LOAD_TEXT_X, LOAD_Y, 2, text, WHITE);
}

/* the main loop: the keys, buttons and encoders into the engine's controls, and all of it shown */
static void controls_show(uint32_t now_ms)
{
    uint32_t i, v, buttons = fm1_in.buttons, notes = fm1_in.notes, released, pressed = fm1_input_edges(&released);
    uint32_t keys_on = 0, sel_down = buttons >> BTN_SEL & 1u;
    int32_t octaves = ((pressed >> FM1_BTN_OCT_UP) & 1) - ((pressed >> FM1_BTN_OCT_DOWN) & 1), pitch;
    volatile drone_voice_controls_t *voice;
    (void)released;
    if (pressed >> BTN_LFO & 1u)
        panel_next_page(&panel);
    for (v = 0; v < DRONE_VOICES; v++) {
        key_gesture_t gesture = key_gesture(&voice_key_hold[v], notes >> voice_key[v] & 1u, now_ms);
        if (gesture == KEY_TOGGLE)
            drone_controls.voice[v].on ^= 1u;
        if (gesture == KEY_SELECT)
            selected_voice = v;
        if (pressed & ~(1u << FM1_BTN_OCT_DOWN | 1u << FM1_BTN_OCT_UP | 1u << BTN_SEL | 1u << BTN_LFO))
            drone_controls.voice[v].on = 0;                     /* any other button: all the voices off */
        keys_on |= (uint32_t)drone_controls.voice[v].on << voice_key[v];
    }
    voice = &drone_controls.voice[selected_voice];
    pitch = voice->param[DRONE_PITCH] + 12 * octaves;
    if (octaves && pitch >= 0 && pitch <= drone_param_spec[DRONE_PITCH].top)
        voice->param[DRONE_PITCH] = (uint8_t)pitch;
    for (i = 0; i < FM1_NENC; i++) {
        /* the encoder's parameter on the open page: the voice's, or the page's LFO's; the last encoders have none there */
        const param_spec_t *spec = panel.page == PANEL_VOICE ? &drone_param_spec[i] : i < DRONE_LFO_NPARAM ? &drone_lfo_spec[i] : 0;
        volatile uint8_t *value = panel.page == PANEL_VOICE ? &voice->param[i] : &voice->lfo[panel.page - 1u][i < DRONE_LFO_NPARAM ? i : 0];
        int32_t clicks = fm1_enc_take(i);
        uint32_t height = 0;
        if (i == ENC_SELECT) {                  /* with SEL down it chooses the filter, and its parameter stays */
            voice->filter = (uint8_t)panel_filter(&panel, (int)sel_down, clicks, voice->filter);
            clicks = sel_down ? 0 : clicks;
        }
        if (spec) {
            *value = (uint8_t)param_turned(spec, *value, clicks);
            height = param_bar_height(spec, *value, PARAM_H);
        }
        if (height > shown.bar_height[i])
            fm1_lcd_fill_rect(PARAM_X(i), PARAM_Y + PARAM_H - height, PARAM_W, height - shown.bar_height[i], WHITE);
        if (height < shown.bar_height[i])
            fm1_lcd_fill_rect(PARAM_X(i), PARAM_Y + PARAM_H - shown.bar_height[i], PARAM_W, shown.bar_height[i] - height, DIM);
        shown.bar_height[i] = height;
    }
    status_show(panel.choosing ? panel.choice : voice->filter, panel.choosing);
    for (i = 0; i < NBUTTON; i++)
        if ((buttons ^ shown.buttons) >> i & 1u) {
            fm1_lcd_fill_rect(BUTTON_X(i), BUTTON_Y, 14, 16, buttons >> i & 1u ? WHITE : DIM);
            fm1_led_key(i, buttons >> i & 1u);                  /* felucca-bsp's ids: buttons 0..13, then the notes */
        }
    for (i = 0; i < NNOTE; i++)                 /* a key's LED and cell: its voice is on */
        if ((keys_on ^ shown.keys_on) >> i & 1u) {
            fm1_lcd_fill_rect(KEY_X(i), KEY_Y, 7, 28, keys_on >> i & 1u ? WHITE : DIM);
            fm1_led_key(NBUTTON + i, keys_on >> i & 1u);
        }
    if (selected_voice != shown.selected_voice) {
        fm1_lcd_fill_rect(0, SELECTED_Y, 240, 4, BLACK);
        fm1_lcd_fill_rect(KEY_X(voice_key[selected_voice]), SELECTED_Y, 7, 4, AMBER);
        shown.selected_voice = selected_voice;
    }
    shown.buttons = buttons;
    shown.keys_on = keys_on;
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
        serial_hex(shown.load_percent, 2);
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
        for (i = 0; i < DRONE_NPARAM; i++)
            drone_controls.voice[v].param[i] = drone_param_spec[i].start;
        for (i = 0; i < DRONE_LFOS * DRONE_LFO_NPARAM; i++)
            drone_controls.voice[v].lfo[i / DRONE_LFO_NPARAM][i % DRONE_LFO_NPARAM] = drone_lfo_spec[i % DRONE_LFO_NPARAM].start;
    }
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
            load_show((peak * 100u + AUDIO_HALF_TICKS / 2u) / AUDIO_HALF_TICKS);
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
