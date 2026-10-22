/* SysInfo: everything the calculator tells an add-in about itself, on one
   scrolling screen: model, product ID, OS version and language, CPU and
   clocks, memory, battery and power settings, storage, the clock and this
   add-in's own build.

   Sources, by kind:
   - gint's hardware table (gint[HW*]), CPG clocks and the RTC;
   - the OS's ROM: the product ID (8 bytes at 0x8001FFD0), the OS header
     ("CASIOWIN" at 0x80020000) and version string (0x80020020);
   - OS syscalls, which run in the OS world through gint_world_switch().

   Only reads: nothing on the calculator is changed. */

#include <gint/display.h>
#include <gint/keyboard.h>
#include <gint/hardware.h>
#include <gint/clock.h>
#include <gint/rtc.h>
#include <gint/gint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "utils.h"
#include "info.h"
#include "version.h"
#include "usb_screen.h"

#define COLOR    C_RGB(4, 14, 20)
#define HEADC     C_RGB(28, 29, 31)

#define PRODUCT_ID 0x8001FFD0u      /* 8 bytes in the OS's ROM */
#define OS_HEADER  0x80020000u      /* "CASIOWIN" */
#define OS_VERSION 0x80020020u      /* "03.80.0010" */

#define TOP       20                /* list area, between title and F-keys */
#define BOTTOM    202
#define ROW_H     12
#define VISIBLE   ((BOTTOM - TOP) / ROW_H)

//---
//  OS syscalls (fx-CG numbers, as in emuCasioCalc's table)
//---

/* A syscall stub: jump to the OS's syscall entry with the number in r0 */
#define SYSCALL(name, number) \
    __asm__(".text\n.global _" #name "\n.align 1\n_" #name ":\n" \
        "\tmov.l 1f, r2\n\tmov.l 2f, r0\n\tjmp @r2\n\tnop\n" \
        ".align 2\n1:\t.long 0x80020070\n2:\t.long " #number "\n")

SYSCALL(GlibGetOSVersionInfo, 0x002B);
SYSCALL(GetMainBatteryVoltage, 0x1186);
SYSCALL(GetBatteryType, 0x12D5);
SYSCALL(GetBacklightDuration, 0x12D9);
SYSCALL(GetAutoPowerOffTime, 0x1E91);
SYSCALL(GetStackPtr, 0x1A2C);
SYSCALL(Bfile_GetMediaFree_OS, 0x1DA5);

int GlibGetOSVersionInfo(char *major, char *minor, short *c, short *d);
int GetMainBatteryVoltage(int one);
int GetBatteryType(void);
char GetBacklightDuration(void);
int GetAutoPowerOffTime(void);
void *GetStackPtr(void);
int Bfile_GetMediaFree_OS(uint16_t const *media, int *free);
/* In common/src/i18n.c */
int LocalizeMessage1(int msgno, char *result);

/* What the OS reports, filled in OS mode by os_query() */
static struct {
    char major, minor;
    short c, d;
    int battery_cv;             /* centivolts */
    int battery_type;           /* 1 alkaline, 2 Ni-MH */
    int backlight;              /* half-minutes */
    int power_off;              /* minutes */
    uint32_t stack;
    int free_rc;
    int free[2];                /* 32 bits, or 64 bits big-endian */
    char language[88];
} os;

static int os_query(void)
{
    static uint16_t const fls0[] = u"\\\\fls0";

    GlibGetOSVersionInfo(&os.major, &os.minor, &os.c, &os.d);
    os.battery_cv = GetMainBatteryVoltage(1);
    os.battery_type = GetBatteryType();
    os.backlight = (unsigned char)GetBacklightDuration();
    os.power_off = GetAutoPowerOffTime();
    os.stack = (uint32_t)GetStackPtr();
    os.free[0] = os.free[1] = 0;
    os.free_rc = Bfile_GetMediaFree_OS(fls0, os.free);
    os.language[0] = 0;
    LocalizeMessage1(1, os.language);
    return 0;
}

//---
//  Rows
//---

#define MAX_ROWS 48

typedef struct {
    char const *label;          /* NULL: a section heading (value = title) */
    char value[44];
} row_t;

static row_t rows[MAX_ROWS];
static int row_count;

static void section(char const *title)
{
    if (row_count >= MAX_ROWS) return;
    rows[row_count].label = NULL;
    snprintf(rows[row_count].value, sizeof rows[0].value, "%s", title);
    row_count++;
}

__attribute__((format(printf, 2, 3)))
static void row(char const *label, char const *fmt, ...)
{
    if (row_count >= MAX_ROWS) return;
    va_list args;
    va_start(args, fmt);
    rows[row_count].label = label;
    vsnprintf(rows[row_count].value, sizeof rows[0].value, fmt, args);
    va_end(args);
    row_count++;
}

//---
//  Readers
//---

static uint8_t peek(uint32_t addr)
{
    return *(volatile uint8_t *)(uintptr_t)addr;
}

/* emuCasio has no real OS, so the firmware area reads back as all zeros
   (same check as FirmDump) */
static bool in_emulator(void)
{
    uint32_t acc = 0;
    for (uint32_t off = 0; off < 0x10000u; off += 16)
        acc |= peek(0x80000000u + off);
    return acc == 0;
}

/* n bytes of ROM as a string, or "" if any isn't printable ASCII */
static void rom_string(uint32_t addr, int n, char *out)
{
    for (int i = 0; i < n; i++) {
        uint8_t b = peek(addr + i);
        if (b < 32 || b >= 127) { out[0] = 0; return; }
        out[i] = b;
    }
    out[n] = 0;
}

static char const *model_name(void)
{
    switch (gint[HWCALC]) {
    case HWCALC_PRIZM:        return "fx-CG10 / fx-CG20 (Prizm)";
    case HWCALC_FXCG50:       return "fx-CG50 / Graph 90+E";
    case HWCALC_FXCG_MANAGER: return "fx-CG Manager (PC)";
#ifdef HWCALC_FXCP400   /* newer gint */
    case HWCALC_FXCP400:      return "fx-CP400";
#endif
    default:                  return "?";
    }
}

static char const *mpu_name(void)
{
    switch (gint[HWMPU]) {
    case HWMPU_SH7305: return "SH7305 (SH-4A)";
    case HWMPU_SH7724: return "SH7724 (SH-4A)";
    case HWMPU_SH7337: return "SH7337 (SH-3)";
    case HWMPU_SH7355: return "SH7355 (SH-3)";
    default:           return "unknown (SH-4A)";
    }
}

/* A size in bytes as "N MB" / "N KB" */
static void size_text(char *out, int n, uint32_t bytes)
{
    if (bytes >= (1u << 20) && !(bytes & 0xfffff))
        snprintf(out, n, "%u MB", (unsigned)(bytes >> 20));
    else if (bytes >= (1u << 20))
        snprintf(out, n, "%u.%u MB", (unsigned)(bytes >> 20),
            (unsigned)((bytes & 0xfffff) * 10 >> 20));
    else snprintf(out, n, "%u KB", (unsigned)(bytes >> 10));
}

/* A gint[HW*] size, or "?" when gint didn't load it */
static void hw_size(char const *label, int key)
{
    char buf[24] = "?";
    if (gint[key] & HW_LOADED) size_text(buf, sizeof buf, gint[key] & 0x7fffffff);
    row(label, "%s", buf);
}

static void mhz(char const *label, int hz)
{
    row(label, "%d.%02d MHz", hz / 1000000, hz % 1000000 / 10000);
}

/* Fill rows[] with everything; called at start and on F1 (battery, clock
   and storage change while the app runs) */
static void collect(void)
{
    char buf[40];
    row_count = 0;
    gint_world_switch(GINT_CALL(os_query));
    usb_screen_init();          /* the world switch closes the USB link */
    bool emu = in_emulator();

    section("Calculator");
    row("Model", "%s", model_name());
    if (emu) row("Running on", "%s", "emulator (OS area blank)");
    uint8_t id[8];
    for (int i = 0; i < 8; i++) id[i] = peek(PRODUCT_ID + i);
    row("Product ID", "%02X%02X%02X%02X %02X%02X%02X%02X",
        id[0], id[1], id[2], id[3], id[4], id[5], id[6], id[7]);
    rom_string(PRODUCT_ID, 8, buf);
    if (buf[0]) row("Product ID (text)", "%s", buf);

    section("Operating system");
    rom_string(OS_VERSION, 10, buf);
    if (buf[0]) row("OS version", "%s", buf);
    row("OS version (syscall)", "%d.%02d  (%d, %d)",
        os.major, os.minor, os.c, os.d);
    rom_string(OS_HEADER, 8, buf);
    row("OS header", "%s", buf[0] ? buf : "-");
    /* Casio's accented letters aren't ASCII: show them as '?' */
    for (char *p = os.language; *p; p++)
        if ((unsigned char)*p >= 127 || (unsigned char)*p < 32) *p = '?';
    row("Language", "%s", os.language[0] ? os.language : "?");

    section("Processor");
    row("MPU", "%s", mpu_name());
    row("CPU PVR", "0x%08X", (unsigned)gint[HWCPUVR]);
    row("CPU PRR", "0x%08X", (unsigned)gint[HWCPUPR]);
    clock_frequency_t const *f = clock_freq();
    mhz("CPU clock (I)", f->Iphi_f);
    mhz("Bus clock (B)", f->Bphi_f);
    mhz("Peripheral clock (P)", f->Pphi_f);
    int speed = clock_get_speed();
    if (speed >= CLOCK_SPEED_F1 && speed <= CLOCK_SPEED_F5)
        row("Speed level", "F%d%s", speed,
            speed == CLOCK_SPEED_DEFAULT ? " (default)" : "");
    else row("Speed level", "%s", "custom");

    section("Memory");
    hw_size("RAM", HWRAM);
    hw_size("Add-in RAM", HWURAM);
    hw_size("Flash ROM", HWROM);
    if (os.free_rc >= 0) {
        uint32_t free = os.free[0] ? (uint32_t)os.free[0] : (uint32_t)os.free[1];
        size_text(buf, sizeof buf, free);
        row("Storage free", "%s", buf);
    }
    else row("Storage free", "%s (%d)", "error", os.free_rc);
    row("Filesystem", "%s",
        gint[HWFS] == HWFS_FUGUE ? "Fugue (FAT)" :
        gint[HWFS] == HWFS_CASIOWIN ? "CASIOWIN" : "?");
    row("Stack pointer", "0x%08X", (unsigned)os.stack);

    section("Power");
    row("Battery", "%d.%02d V", os.battery_cv / 100,
        os.battery_cv % 100);
    row("Battery type", "%s",
        os.battery_type == 1 ? "Alkaline" :
        os.battery_type == 2 ? "Ni-MH" : "?");
    if (os.backlight % 2)
        row("Backlight off after", "%d min 30 s",
            os.backlight / 2);
    else row("Backlight off after", "%d min",
        os.backlight / 2);
    row("Auto power off", "%d min", os.power_off);

    section("Display & keyboard");
    row("Screen", "%dx%d, 16-bit, R61524", DWIDTH, DHEIGHT);
    row("Keyboard scan", "%s",
        gint[HWKBD] & HWKBD_KSI ? "KEYSC" :
        gint[HWKBD] & HWKBD_IO ? "I/O ports" : "?");

    section("Clock");
    rtc_time_t t;
    rtc_get_time(&t);
    row("Date", "%04d-%02d-%02d", t.year, t.month + 1, t.month_day);
    row("Time", "%02d:%02d:%02d", t.hours, t.minutes, t.seconds);

    section("This add-in");
    row("Version", "SysInfo %s", APPS_VERSION);
    row("Built", "%s %s", __DATE__, __TIME__);
}

//---
//  Screen
//---

static void draw(int top)
{
    dclear(C_WHITE);
    info_title("System info", COLOR);

    for (int k = 0; k < VISIBLE && top + k < row_count; k++) {
        row_t const *r = &rows[top + k];
        int y = TOP + k * ROW_H;
        if (!r->label) {
            drect(0, y, DWIDTH - 8, y + ROW_H - 2, HEADC);
            dtext(6, y + 1, COLOR, r->value);
            continue;
        }
        dtext(12, y + 1, C_DARK, r->label);
        dtext_opt(DWIDTH - 12, y + 1, C_BLACK, C_NONE, DTEXT_RIGHT,
            DTEXT_TOP, r->value);
    }
    if (row_count > VISIBLE)
        scrollbar_px(TOP, BOTTOM, 0, row_count, top, VISIBLE);

    fkey_button_color(1, "UPDATE", COLOR);
    fkey_button(6, "ABOUT");
    dupdate();
}

int main(void)
{
    int top = 0;

    collect();

    while (1) {
        int max_top = row_count > VISIBLE ? row_count - VISIBLE : 0;
        if (top > max_top) top = max_top;
        draw(top);

        int key = getkey().key;
        if (key == KEY_EXIT) return 1;
        if (key == INFO_KEY || key == KEY_F6)
            info_show("SysInfo", "System info " APPS_VERSION, COLOR);
        else if (key == KEY_F1) collect();
        else if (key == KEY_UP && top > 0) top--;
        else if (key == KEY_DOWN && top < max_top) top++;
        else if (key == KEY_LEFT) top = top > VISIBLE ? top - VISIBLE : 0;
        else if (key == KEY_RIGHT) top = top + VISIBLE < max_top
            ? top + VISIBLE : max_top;
    }
}
