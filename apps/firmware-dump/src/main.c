/* FirmDump: read a region of the calculator's memory (the OS firmware in
   flash, the full ROM, or a custom address range) and save it to storage as
   \\fls0\DUMP\<name>.bin, in numbered parts, for analysis on a PC.

   The flash ROM is memory-mapped at P1 = 0x80000000 (cached) on the SH7305;
   the Casio OS lives in the first few MB. Reads from the mapped ROM are
   always safe. A custom range may hit unmapped memory, so use it with care.

   Nothing is modified on the calculator: it only reads memory and writes
   new files under \\fls0\DUMP. */

#include <gint/display.h>
#include <gint/keyboard.h>
#include <gint/bfile.h>
#include <gint/hardware.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "utils.h"
#include "info.h"

#define VERSION   "v1.0"
#define COLOR     C_RGB(7, 7, 9)
#define OKC       C_RGB(2, 17, 6)
#define ERRC      C_RGB(26, 4, 4)

#define ROM_BASE  0x80000000u       /* P1: cached, memory-mapped flash */
#define OS_SIZE   0x00400000u       /* the OS occupies the first ~4 MB */
#define PART_SIZE (1u << 20)        /* one output file per 1 MB */
#define CHUNK     4096              /* bytes per read/write step */

//---
//  Regions
//---

enum { REG_OS, REG_ROM, REG_CUSTOM, REGION_COUNT };

typedef struct {
    char const *name;
    char const *file;           /* base name under \\fls0\DUMP */
    uint32_t base;
    uint32_t size;
    bool editable;
} region_t;

static region_t regions[REGION_COUNT] = {
    { "OS firmware", "OS",   ROM_BASE, OS_SIZE,          false },
    { "Full ROM",    "ROM",  ROM_BASE, 0,                false },
    { "Custom",      "MEM",  ROM_BASE, 0x00100000,       true  },
};

//---
//  Memory access
//---

/* Read one byte from a mapped address */
static uint8_t peek(uint32_t addr)
{
    return *(volatile uint8_t *)(uintptr_t)addr;
}

/* Windows that are always safe to read: P1/P2 ROM (64 MB) and RAM. Reading
   outside these (an unmapped address) faults, so the live preview skips it;
   the dump itself reads whatever the user chose. */
static bool mapped(uint32_t addr)
{
    uint32_t off = addr & 0x1fffffffu;      /* strip the P1/P2 segment */
    uint32_t seg = addr & 0xe0000000u;
    if (seg != 0x80000000u && seg != 0xa0000000u) return false;
    if (off < 0x04000000u) return true;                 /* ROM, 64 MB */
    if (off >= 0x08000000u && off < 0x08200000u) return true;   /* RAM */
    return false;
}

/* Guess whether we are running in emuCasio rather than on a calculator.
   The emulator has no real OS, so the firmware area reads back as all
   zeros; a real OS fills it with code and data. Sample the first 64 KB. */
static bool in_emulator(void)
{
    uint32_t acc = 0;
    for (uint32_t off = 0; off < 0x10000u; off += 16) acc |= peek(ROM_BASE + off);
    return acc == 0;
}

//---
//  Path building: FONTCHARACTER (UCS-2) paths
//---

static uint16_t const DUMP_DIR[] = u"\\\\fls0\\DUMP";

/* "\\fls0\DUMP\<file>NN.bin" into out (UCS-2) */
static void make_path(uint16_t *out, char const *file, int part)
{
    char ascii[40];
    snprintf(ascii, sizeof ascii, "\\\\fls0\\DUMP\\%s%02d.bin", file, part);
    int i = 0;
    for (; ascii[i]; i++) out[i] = (uint8_t)ascii[i];
    out[i] = 0;
}

//---
//  Dump state
//---

typedef enum { ST_IDLE, ST_RUNNING, ST_DONE, ST_ERROR, ST_ABORT } status_t;

typedef struct {
    status_t status;
    uint32_t done;              /* bytes written so far */
    int parts;                  /* output files written */
    char message[48];
} progress_t;

/* Draw the region list and, if a dump happened, its result */
static void draw(int sel, int digit, progress_t const *pg)
{
    char buf[64];

    dclear(C_WHITE);
    info_title("Firmware dump", COLOR);

    dtext(10, 22, C_DARK, "Region");
    bool emu = in_emulator();
    dtext_opt(DWIDTH - 12, 22, emu ? ERRC : OKC, C_NONE, DTEXT_RIGHT,
        DTEXT_TOP, emu ? "emulator (OS area blank)" : "real hardware");
    for (int i = 0; i < REGION_COUNT; i++) {
        region_t const *r = &regions[i];
        int y = 40 + i * 30;
        bool s = (i == sel);
        if (s) drect(6, y - 4, DWIDTH - 7, y + 22, C_RGB(28, 29, 31));

        dtext(14, y, s ? COLOR : C_BLACK, r->name);
        uint32_t size = r->size;
        if (size >= (1u << 20))
            snprintf(buf, sizeof buf, "%u MB", (unsigned)(size >> 20));
        else snprintf(buf, sizeof buf, "%u KB", (unsigned)(size >> 10));
        dprint(150, y, C_BLACK, "0x%08X", (unsigned)r->base);
        dprint(250, y, C_BLACK, "+ 0x%06X", (unsigned)size);
        dtext_opt(DWIDTH - 12, y, C_DARK, C_NONE, DTEXT_RIGHT, DTEXT_TOP,
            buf);
        if (r->editable)
            dtext(14, y + 12, s ? COLOR : C_DARK,
                "F4 base  F5 size  (hex digits to edit)");
        else dprint(14, y + 12, C_DARK, "-> DUMP\\%s00.bin ...", r->file);
    }
    (void)digit;

    /* Live hex preview of the selected base (two rows = 32 bytes) */
    region_t const *r = &regions[sel];
    int y = 134;
    dline(6, y - 6, DWIDTH - 7, y - 6, C_LIGHT);
    dtext(10, y, C_DARK, "Preview");
    for (int row = 0; row < 2; row++) {
        uint32_t a = r->base + row * 16;
        char hex[64], asc[20];
        int hp = 0;
        for (int c = 0; c < 16; c++) {
            bool ok = mapped(a + c);
            uint8_t b = ok ? peek(a + c) : 0;
            hp += snprintf(hex + hp, sizeof hex - hp, "%s%s",
                ok ? (char[]){ "0123456789ABCDEF"[b >> 4],
                    "0123456789ABCDEF"[b & 15], 0 } : "--",
                c == 7 ? " " : " ");
            asc[c] = ok && b >= 32 && b < 127 ? b : '.';
        }
        asc[16] = '\0';
        int ry = y + 13 + row * 12;
        dprint(10, ry, C_DARK, "%08X", (unsigned)a);
        dtext(78, ry, C_BLACK, hex);
        (void)asc;
    }

    /* Status line, then progress bar */
    if (pg->message[0]) {
        int c = pg->status == ST_DONE ? OKC : pg->status == ST_ERROR
            || pg->status == ST_ABORT ? ERRC : C_BLACK;
        dtext_opt(DWIDTH / 2, 176, c, C_NONE, DTEXT_CENTER, DTEXT_TOP,
            pg->message);
    }
    if (pg->status == ST_RUNNING || pg->status == ST_DONE
        || pg->status == ST_ABORT) {
        uint32_t total = r->size ? r->size : 1;
        int by = 190, x1 = 10, x2 = DWIDTH - 11;
        drect_border(x1, by, x2, by + 9, C_WHITE, 1, COLOR);
        int w = (int)((uint64_t)(x2 - x1 - 3) * pg->done / total);
        if (w > 0) drect(x1 + 2, by + 2, x1 + 2 + w - 1, by + 7, COLOR);
    }

    fkey_button_color(1, "DUMP", COLOR);
    fkey_button(2, "USB?");
    fkey_button(6, "ABOUT");
    dupdate();
}

//---
//  Writing
//---

static int create_part(uint16_t const *path, uint32_t size)
{
    int isize = (int)size;
    BFile_Remove(path);                 /* ignore "not found" */
    int rc = BFile_Create(path, BFile_File, &isize);
    if (rc < 0) return rc;
    return BFile_Open(path, BFile_WriteOnly);
}

/* Dump the region to numbered files, refreshing the screen as it goes.
   Returns via pg->status */
static void run_dump(int sel, progress_t *pg)
{
    static uint8_t buf[CHUNK];
    region_t const *r = &regions[sel];
    uint16_t path[48];

    pg->status = ST_RUNNING;
    pg->done = 0;
    pg->parts = 0;

    BFile_Create(DUMP_DIR, BFile_Folder, NULL);     /* ok if it exists */

    uint32_t remaining = r->size;
    int part = 0;

    while (remaining > 0) {
        uint32_t part_len = remaining < PART_SIZE ? remaining : PART_SIZE;
        make_path(path, r->file, part);
        int fd = create_part(path, part_len);
        if (fd < 0) {
            snprintf(pg->message, sizeof pg->message,
                "Create failed (%d) at part %d", fd, part);
            pg->status = ST_ERROR;
            return;
        }

        for (uint32_t off = 0; off < part_len; off += CHUNK) {
            uint32_t n = part_len - off < CHUNK ? part_len - off : CHUNK;
            uint32_t src = r->base + (uint32_t)part * PART_SIZE + off;
            for (uint32_t i = 0; i < n; i++) buf[i] = peek(src + i);

            int wr = BFile_Write(fd, buf, (n + 1) & ~1u);    /* keep even */
            if (wr < 0) {
                BFile_Close(fd);
                snprintf(pg->message, sizeof pg->message,
                    "Write failed (%d)", wr);
                pg->status = ST_ERROR;
                return;
            }
            pg->done += n;

            /* Refresh and allow aborting roughly every 64 KB */
            if ((off & 0xFFFF) == 0) {
                snprintf(pg->message, sizeof pg->message,
                    "Writing %s%02d.bin ...", r->file, part);
                draw(sel, 0, pg);
                clearevents();
                if (keydown(KEY_EXIT)) {
                    BFile_Close(fd);
                    pg->status = ST_ABORT;
                    snprintf(pg->message, sizeof pg->message,
                        "Aborted after %u KB", (unsigned)(pg->done >> 10));
                    return;
                }
            }
        }
        BFile_Close(fd);
        remaining -= part_len;
        part++;
        pg->parts = part;
    }

    pg->status = ST_DONE;
    snprintf(pg->message, sizeof pg->message,
        "Done: %u KB in %d file%s under \\fls0\\DUMP",
        (unsigned)(pg->done >> 10), pg->parts, pg->parts == 1 ? "" : "s");
}

//---
//  Custom region editing
//---

static int key_hex(int key)
{
    switch (key) {
        case KEY_0: return 0; case KEY_1: return 1; case KEY_2: return 2;
        case KEY_3: return 3; case KEY_4: return 4; case KEY_5: return 5;
        case KEY_6: return 6; case KEY_7: return 7; case KEY_8: return 8;
        case KEY_9: return 9;
        case KEY_XOT: return 10; case KEY_LOG: return 11;
        case KEY_LN: return 12; case KEY_SIN: return 13;
        case KEY_COS: return 14; case KEY_TAN: return 15;
        default: return -1;
    }
}

int main(void)
{
    int sel = 0;
    int edit = 0;               /* 0 none, 1 base, 2 size */
    bool fresh = false;         /* next digit starts a new value */
    progress_t pg = { .status = ST_IDLE };

    /* Full ROM size from the hardware info, capped to the mapped window */
    uint32_t rom = (gint[HWROM] & HW_LOADED) ? (gint[HWROM] & 0x7fffffff) : 0;
    if (rom == 0 || rom > 0x04000000u) rom = 0x04000000u;    /* 64 MB cap */
    regions[REG_ROM].size = rom;

    strcpy(pg.message, "F1 dumps the selected region to \\fls0\\DUMP");

    while (1) {
        draw(sel, edit, &pg);

        int key = getkey().key;
        int d = key_hex(key);
        region_t *r = &regions[sel];

        if (key == KEY_EXIT) return 1;
        if (key == INFO_KEY || key == KEY_F6) {
            info_show("FirmDump", "Firmware dump " VERSION, COLOR);
            continue;
        }
        if (key == KEY_UP) { sel = (sel + REGION_COUNT - 1) % REGION_COUNT;
            edit = 0; continue; }
        if (key == KEY_DOWN) { sel = (sel + 1) % REGION_COUNT;
            edit = 0; continue; }

        if (key == KEY_F1) {
            run_dump(sel, &pg);
            continue;
        }
        if (key == KEY_F2) {
            snprintf(pg.message, sizeof pg.message,
                "USB streaming not available yet");
            pg.status = ST_IDLE;
            continue;
        }

        /* Editing the custom region */
        if (r->editable) {
            if (key == KEY_F4) { edit = 1; fresh = true; continue; }
            if (key == KEY_F5) { edit = 2; fresh = true; continue; }
            if (edit && d >= 0) {
                uint32_t *field = edit == 1 ? &r->base : &r->size;
                *field = (fresh ? 0u : *field << 4) | d;
                fresh = false;
                continue;
            }
            if (edit && key == KEY_DEL) {
                uint32_t *field = edit == 1 ? &r->base : &r->size;
                *field >>= 4;
                continue;
            }
            if (key == KEY_ACON) { r->base = ROM_BASE; r->size = 0x100000;
                edit = 0; continue; }
        }
    }
}
