/* SINT_FM1_DR1: the empty firmware. It has only what keeps the FM-1 replaceable:
 *   - the boot guard: two starts in a row that do not stay up 2 s end in the chip's update mode
 *   - the update entry: the installer (bsp/tools/fm1_install.py) replaces this firmware over USB-MIDI
 *   - update mode on request: the USB-MIDI command, or 'u' on the serial port
 *   - the watchdog and the hardware guards; a crash shows a red screen, then restarts
 * and one screen with its name. Everything else is to be built on top.
 * The chip and board code is felucca-bsp's (bsp/, MPL-2.0), included unchanged. */
#include <stdint.h>
#include "app_id.h"
/* felucca-bsp's build settings */
#define FELUCCA_CDC 1                           /* the serial port */
#define FELUCCA_OTA 1                           /* the update entry */
#define FELUCCA_ID APP_PACKAGE_ID
#define FELUCCA_USB_MANUFACTURER "KevinSint"
#define FELUCCA_USB_PRODUCT APP_NAME
#define FM1_T5_PERIOD 3000u                     /* the tick: 6 MHz / 3000 = 2 kHz, the rate usb_poll wants */
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
#include "fm1_time.h"
#include "fm1_sys.h"
#include "fm1_irq.h"
#include "fm1_guard.h"
#include "fm1_timer.h"
#include "fm1_lcd.h"
#include "fm1_flash.h"
#include "libc.c"
#include "usb.c"
#include "ota.c"
#define FONT_RECT(x, y, w, h, colour) fm1_lcd_fill_rect(x, y, w, h, colour)
#include "font.h"

enum { BLACK = 0x0000, WHITE = 0xFFFF, GREY = 0x8410, RED = 0xF800, GREEN = 0x07E0, AMBER = 0xFD20 };
#define TICKS_PER_MS 2u
#define HEALTHY_AFTER_MS 2000u

static volatile uint32_t tick_count;
static uint32_t uptime_ms(void) { return tick_count / TICKS_PER_MS; }

void fm1_timer5_irq(void)
{
    fm1_timer5_ack();
    tick_count++;
    usb_poll();
}
void fm1_alnk0_irq(void) {}                     /* felucca-bsp links the audio interrupt's entry; no audio runs yet */
extern void isr_timer5(void);

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

static void screen_draw(void)
{
    fm1_lcd_fill_rect(0, 0, 240, 240, BLACK);
    font_draw_text((240u - font_text_width(APP_NAME, 3)) / 2u, 96, 3, APP_NAME, WHITE);
    font_draw_text((240u - font_text_width(APP_VERSION, 2)) / 2u, 132, 2, APP_VERSION, GREY);
    if (fm1_crash.magic == FM1_CRASH_MAGIC)     /* a run before this one crashed */
        fm1_lcd_fill_rect(0, 0, 240, 4, AMBER);
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
        serial_text("  (u: update mode, r: restart)\r\n");
        break;
    }
}

static void app_main(void)
{
    uint32_t flash_id = fl_jedec(), healthy = 0;
    /* the Puya P25Q80 (the first units) and the UC25HQ80 (newer ones): the parts fm1-base accepts with this driver */
    update_allowed = flash_id == 0x856014u || flash_id == 0xB36014u;
    if (update_allowed)
        ota_boot_cleanup();                     /* what a stopped update left in the staging area */
    fm1_lcd_init();
    screen_draw();
    usb_start();
    fm1_timer5_start(isr_timer5, 4);
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
