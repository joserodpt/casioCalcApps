#include <gint/display.h>
#include <gint/keyboard.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "utils.h"
#include "info.h"
#include "keys.h"
#include "version.h"
#include "i18n.h"
#include "usb_screen.h"

#define COLOR   C_RGB(4, 12, 22)
#define CTRLC   C_RGB(24, 8, 4)     /* control characters */
#define ROWBG   C_RGB(28, 29, 31)

enum { VIEW_LIST, VIEW_GRID, VIEW_UTF8 };

typedef struct {
    int view;
    int sel;            /* selected ASCII code, 0..127 */
    int top;            /* first row in the list view */
    int jump;           /* number being typed to jump to a code */
    uint32_t cp;        /* code point in the UTF-8 view */
    int gtop;           /* fx: first grid row shown */
    int page;           /* fx: UTF-8 view page (0 codes, 1 bits) */
} state_t;

//---
//  Character information
//---

static char const * const ctrl_names[33] = {
    "NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL", "BS", "HT",
    "LF", "VT", "FF", "CR", "SO", "SI", "DLE", "DC1", "DC2", "DC3", "DC4",
    "NAK", "SYN", "ETB", "CAN", "EM", "SUB", "ESC", "FS", "GS", "RS", "US",
    "SP" };

static char const * const ctrl_long[33] = {
    "Null", "Start of heading", "Start of text", "End of text",
    "End of transmission", "Enquiry", "Acknowledge", "Bell", "Backspace",
    "Horizontal tab", "Line feed", "Vertical tab", "Form feed",
    "Carriage return", "Shift out", "Shift in", "Data link escape",
    "Device control 1 (XON)", "Device control 2", "Device control 3 (XOFF)",
    "Device control 4", "Negative acknowledge", "Synchronous idle",
    "End of trans. block", "Cancel", "End of medium", "Substitute",
    "Escape", "File separator", "Group separator", "Record separator",
    "Unit separator", "Space" };

/* C escape sequence, or NULL */
static char const *c_escape(int c)
{
    switch (c) {
        case 0: return "\\0"; case 7: return "\\a"; case 8: return "\\b";
        case 9: return "\\t"; case 10: return "\\n"; case 11: return "\\v";
        case 12: return "\\f"; case 13: return "\\r"; case 27: return "\\e";
        case '\\': return "\\\\"; case '\'': return "\\'";
        default: return NULL;
    }
}

static bool is_control(int c)
{
    return c < 32 || c == 127;
}

/* Short name for the list: mnemonic or character class */
static char const *short_name(int c)
{
    if (c <= 32) return ctrl_names[c];
    if (c == 127) return "DEL";
    if (c >= 'A' && c <= 'Z') return "Upper";
    if (c >= 'a' && c <= 'z') return "Lower";
    if (c >= '0' && c <= '9') return "Digit";
    return "Symbol";
}

static char const *long_name(int c)
{
    if (c <= 32) return ctrl_long[c];
    if (c == 127) return "Delete";
    if (c >= 'A' && c <= 'Z') return "Uppercase letter";
    if (c >= 'a' && c <= 'z') return "Lowercase letter";
    if (c >= '0' && c <= '9') return "Digit";
    return "Punctuation / symbol";
}

static char *bin8(char *buf, int v)
{
    for (int i = 0; i < 8; i++) buf[i + (i >= 4)] = '0' + ((v >> (7 - i)) & 1);
    buf[4] = ' ';
    buf[9] = '\0';
    return buf;
}

//---
//  Unicode
//---

typedef struct { uint32_t start, end; char const *name; } block_t;

/* Block name, with a shorter form for the 21-column fx-9860G screen */
#if GINT_HW_FX
#define BN(name, fx_name) fx_name
#else
#define BN(name, fx_name) name
#endif

static block_t const blocks[] = {
    { 0x0000, 0x007F, "Basic Latin (ASCII)" },
    { 0x0080, 0x00FF, "Latin-1 Supplement" },
    { 0x0100, 0x024F, "Latin Extended" },
    { 0x0250, 0x02FF, BN("IPA / modifier letters", "IPA / modifiers") },
    { 0x0300, 0x036F, "Combining diacritics" },
    { 0x0370, 0x03FF, "Greek and Coptic" },
    { 0x0400, 0x052F, "Cyrillic" },
    { 0x0590, 0x05FF, "Hebrew" },
    { 0x0600, 0x06FF, "Arabic" },
    { 0x0900, 0x097F, "Devanagari" },
    { 0x0E00, 0x0E7F, "Thai" },
    { 0x1E00, 0x1EFF, BN("Latin Extended Additional", "Latin Extended Add.") },
    { 0x2000, 0x206F, "General punctuation" },
    { 0x20A0, 0x20CF, "Currency symbols" },
    { 0x2100, 0x214F, "Letterlike symbols" },
    { 0x2190, 0x21FF, "Arrows" },
    { 0x2200, 0x22FF, BN("Mathematical operators", "Math operators") },
    { 0x2500, 0x257F, "Box drawing" },
    { 0x2580, 0x25FF, "Blocks and shapes" },
    { 0x2600, 0x27BF, "Symbols and dingbats" },
    { 0x3000, 0x303F, "CJK symbols" },
    { 0x3040, 0x30FF, "Hiragana / Katakana" },
    { 0x4E00, 0x9FFF, "CJK ideographs" },
    { 0xAC00, 0xD7AF, "Hangul syllables" },
    { 0xD800, 0xDFFF, BN("Surrogates (not chars)", "Surrogates") },
    { 0xE000, 0xF8FF, "Private use" },
    { 0xFE00, 0xFE0F, "Variation selectors" },
    { 0xFF00, 0xFFEF, "Fullwidth forms" },
    { 0xFFF0, 0xFFFF, "Specials" },
    { 0x1F300, 0x1FAFF, "Emoji and pictographs" },
    { 0x20000, 0x2FFFF, "CJK ideographs ext." },
    { 0xE0000, 0xE007F, "Tags" },
    { 0xF0000, 0x10FFFF,
        BN("Private use (planes 15-16)", "Private use (15-16)") },
};

static char const *block_name(uint32_t cp)
{
    for (unsigned i = 0; i < sizeof blocks / sizeof *blocks; i++)
        if (cp >= blocks[i].start && cp <= blocks[i].end)
            return blocks[i].name;
    return cp <= 0xFFFF
        ? BN("Basic Multilingual Plane", "Basic Multiling Plane")
        : cp <= 0x1FFFF
        ? BN("Supplementary Multilingual", "Supplementary Multil.")
        : "Supplementary plane";
}

static bool is_surrogate(uint32_t cp)
{
    return cp >= 0xD800 && cp <= 0xDFFF;
}

/* UTF-8 bytes of cp into out; returns the number of bytes */
static int utf8_encode(uint32_t cp, uint8_t out[4])
{
    if (cp < 0x80) { out[0] = cp; return 1; }
    if (cp < 0x800) {
        out[0] = 0xC0 | (cp >> 6);
        out[1] = 0x80 | (cp & 0x3F);
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = 0xE0 | (cp >> 12);
        out[1] = 0x80 | ((cp >> 6) & 0x3F);
        out[2] = 0x80 | (cp & 0x3F);
        return 3;
    }
    out[0] = 0xF0 | (cp >> 18);
    out[1] = 0x80 | ((cp >> 12) & 0x3F);
    out[2] = 0x80 | ((cp >> 6) & 0x3F);
    out[3] = 0x80 | (cp & 0x3F);
    return 4;
}

#if GINT_HW_FX

//---
//  fx-9860G (128x64) views: 5x7 font, 6 px per character, title bar at
//  y 0..8 and F-keys at y 56..63
//---

#define LIST_Y     18
#define LIST_STEP  8
#define LIST_ROWS  4

static void draw_list(state_t *s)
{
    char buf[16];

    if (s->sel < s->top) s->top = s->sel;
    if (s->sel >= s->top + LIST_ROWS) s->top = s->sel - LIST_ROWS + 1;

    dtext_opt(17, 10, C_DARK, C_NONE, DTEXT_RIGHT, DTEXT_TOP, "Dec");
    dtext(22, 10, C_DARK, "Hx");
    dtext(38, 10, C_DARK, L("Binary", "Binario", "Binaire"));
    dtext(98, 10, C_DARK, L("Chr", "Car", "Car"));

    for (int r = 0; r < LIST_ROWS; r++) {
        int c = s->top + r;
        int y = LIST_Y + r * LIST_STEP;
        bool sel = (c == s->sel);
        int fg = sel ? C_WHITE : C_BLACK;

        if (sel) drect(0, y - 1, DWIDTH - 7, y + 6, C_BLACK);

        dprint_opt(17, y, fg, C_NONE, DTEXT_RIGHT, DTEXT_TOP, "%d", c);
        dprint(22, y, fg, "%02X", c);
        dtext(38, y, fg, bin8(buf, c));
        if (is_control(c))
            dtext(98, y, sel ? C_WHITE : CTRLC, short_name(c));
        else {
            buf[0] = c; buf[1] = '\0';
            dtext(104, y, fg, buf);
        }
    }
    scrollbar_px(LIST_Y - 1, LIST_Y + LIST_ROWS * LIST_STEP - 1, 0, 128,
        s->top, LIST_ROWS);

    /* What did not fit in the columns: octal, then the name and C escape,
       shortened to what fits on the line */
    int c = s->sel, w;
    char const *e = c_escape(c);
    char line[48];
    sprintf(line, "0o%03o %s%s%s", c, long_name(c), e ? " " : "", e ? e : "");
    dsize(line, NULL, &w, NULL);
    if (w > DWIDTH) {
        sprintf(line, "0o%03o %s", c, long_name(c));
        dsize(line, NULL, &w, NULL);
    }
    if (w > DWIDTH) sprintf(line, "0o%03o %s%s%s", c,
        is_control(c) ? "" : short_name(c), e && !is_control(c) ? " " : "",
        e ? e : "");
    dtext(0, 49, C_DARK, line);
}

#define GRID_X     13
#define GRID_Y     17
#define CELL_W     7
#define CELL_H     8
#define GRID_ROWS  4

static void draw_grid(state_t *s)
{
    char buf[24];

    int row_sel = s->sel / 16;
    if (row_sel < s->gtop) s->gtop = row_sel;
    if (row_sel >= s->gtop + GRID_ROWS) s->gtop = row_sel - GRID_ROWS + 1;

    for (int col = 0; col < 16; col++)
        dprint(GRID_X + col * CELL_W + 1, 10, C_DARK, "%X", col);

    for (int r = 0; r < GRID_ROWS; r++) {
        int row = s->gtop + r;
        int y = GRID_Y + r * CELL_H;
        dprint_opt(11, y + 1, C_DARK, C_NONE, DTEXT_RIGHT, DTEXT_TOP,
            "%Xx", row);

        for (int col = 0; col < 16; col++) {
            int c = row * 16 + col;
            int x = GRID_X + col * CELL_W;
            bool sel = (c == s->sel);

            if (sel) drect(x, y, x + CELL_W - 1, y + CELL_H - 1, C_BLACK);
            else if (is_control(c))
                drect(x, y, x + CELL_W - 1, y + CELL_H - 1, C_LIGHT);
            if (is_control(c)) {
                /* No glyph: a small hollow box */
                drect_border(x + 2, y + 2, x + 4, y + 6, C_NONE, 1,
                    sel ? C_WHITE : CTRLC);
            }
            else {
                buf[0] = c; buf[1] = '\0';
                dtext(x + 1, y + 1, sel ? C_WHITE : C_BLACK, buf);
            }
        }
    }

    /* Details of the selected character */
    int c = s->sel;
    char const *e = c_escape(c);
    if (is_control(c)) dtext(0, 49, CTRLC, short_name(c));
    else {
        buf[0] = c; buf[1] = '\0';
        dtext(6, 49, C_BLACK, buf);
    }
    dprint(24, 49, C_BLACK, "%d 0x%02X 0o%03o", c, c, c);
    if (e) dtext_opt(DWIDTH - 1, 49, C_DARK, C_NONE, DTEXT_RIGHT, DTEXT_TOP,
        e);
}

/* UTF-8 view: two pages (codes, bits) of five lines */
#define U_Y(line)  (10 + 9 * (line))
#define U_X        42

static void draw_utf8(state_t const *s)
{
    uint32_t cp = s->cp;
    char buf[32];

    /* Code point being typed, the character if it is ASCII, the decimal */
    sprintf(buf, "U+%04X", (unsigned)cp);
    dtext(0, U_Y(0), C_BLACK, buf);
    int w;
    dsize(buf, NULL, &w, NULL);
    drect(w + 2, U_Y(0), w + 2, U_Y(0) + 6, C_BLACK);
    if (cp >= 32 && cp < 127) {
        buf[0] = cp; buf[1] = '\0';
        dtext(66, U_Y(0), C_BLACK, buf);
    }
    dprint_opt(DWIDTH - 1, U_Y(0), C_DARK, C_NONE, DTEXT_RIGHT, DTEXT_TOP,
        "%u", (unsigned)cp);

    if (is_surrogate(cp)) {
        dtext(0, U_Y(1), C_BLACK, L("Surrogates D800-DFFF", "Substitutos",
            "Substituts D800-DFFF"));
        dtext(0, U_Y(2), C_BLACK, L("pair up in UTF-16:", "D800-DFFF: pares",
            "en paires (UTF-16):"));
        dtext(0, U_Y(3), C_BLACK, L("they have no UTF-8.",
            "no UTF-16, sem UTF-8", "pas d'UTF-8."));
        return;
    }

    uint8_t bytes[4];
    int n = utf8_encode(cp, bytes);

    if (s->page == 0) {
        dtext(0, U_Y(1), C_DARK, "UTF-8");
        int x = U_X;
        for (int i = 0; i < n; i++, x += 18) {
            sprintf(buf, "%02X", bytes[i]);
            dtext(x, U_Y(1), C_BLACK, buf);
        }

        dtext(0, U_Y(2), C_DARK, "UTF-16");
        if (cp < 0x10000) dprint(U_X, U_Y(2), C_BLACK, "%04X", (unsigned)cp);
        else {
            uint32_t v = cp - 0x10000;
            dprint(U_X, U_Y(2), C_BLACK, "%04X %04X",
                (unsigned)(0xD800 | (v >> 10)),
                (unsigned)(0xDC00 | (v & 0x3FF)));
        }
        dtext(0, U_Y(3), C_DARK, "UTF-32");
        dprint(U_X, U_Y(3), C_BLACK, "%08X", (unsigned)cp);
        dprint(0, U_Y(4), C_BLACK, "&#%u; &#x%X;", (unsigned)cp,
            (unsigned)cp);
        return;
    }

    /* Bits page: the UTF-8 bytes in binary, two per line, with the
       length/continuation markers in grey and the code point bits black */
    dtext(0, U_Y(1), C_DARK, block_name(cp));
    if (n == 1) dtext(0, U_Y(2), C_DARK, L("1 byte, same as ASCII",
        "1 byte, igual a ASCII",
        "1 octet, comme ASCII"));
    else dprint(0, U_Y(2), C_DARK, L("%d bytes, grey: marks",
        "%d bytes; cinza=marca",
        "%d octets; gris=marq."), n);

    int marker[4] = { 1, 0, 0, 0 };
    if (n > 1) {
        marker[0] = n + 1;
        for (int i = 1; i < n; i++) marker[i] = 2;
    }
    for (int i = 0; i < n; i++) {
        int x = (i % 2) * 54, y = U_Y(3 + i / 2);
        for (int k = 7; k >= 0; k--, x += 6) {
            buf[0] = '0' + ((bytes[i] >> k) & 1); buf[1] = '\0';
            dtext(x, y, (7 - k) < marker[i] ? C_LIGHT : C_BLACK, buf);
        }
    }
}

#else

//---
//  List view
//---

#define LIST_Y     34
#define LIST_STEP  13
#define LIST_ROWS  13

static void draw_list(state_t *s)
{
    char buf[16];

    if (s->sel < s->top) s->top = s->sel;
    if (s->sel >= s->top + LIST_ROWS) s->top = s->sel - LIST_ROWS + 1;

    int hy = LIST_Y - 13;
    dtext_opt(36, hy, C_DARK, C_NONE, DTEXT_RIGHT, DTEXT_TOP, "Dec");
    dtext(50, hy, C_DARK, "Hex");
    dtext(92, hy, C_DARK, "Oct");
    dtext(132, hy, C_DARK, "Binary");
    dtext(222, hy, C_DARK, L("Char", "Car", "Car"));
    dtext(270, hy, C_DARK, L("Name", "Nome", "Nom"));
    dline(6, LIST_Y - 2, DWIDTH - 7, LIST_Y - 2, C_LIGHT);

    for (int r = 0; r < LIST_ROWS; r++) {
        int c = s->top + r;
        int y = LIST_Y + r * LIST_STEP;
        bool sel = (c == s->sel);
        int fg = sel ? C_WHITE : C_BLACK;
        int dim = sel ? C_WHITE : C_DARK;

        if (sel) drect(4, y - 2, DWIDTH - 9, y + 10, COLOR);
        else if (r % 2) drect(4, y - 2, DWIDTH - 9, y + 10, ROWBG);

        dprint_opt(36, y, fg, C_NONE, DTEXT_RIGHT, DTEXT_TOP, "%d", c);
        dprint(50, y, fg, "0x%02X", c);
        dprint(92, y, dim, "%03o", c);
        dtext(132, y, fg, bin8(buf, c));
        if (is_control(c))
            dtext(222, y, sel ? C_WHITE : CTRLC, short_name(c));
        else {
            buf[0] = c; buf[1] = '\0';
            dtext(230, y, fg, buf);
        }
        if (!is_control(c) || c == 32)
            dtext(270, y, dim, c == 32 ? L("Space", "Espaco", "Espace")
                : short_name(c));
        else {
            char const *e = c_escape(c);
            if (e) dtext(270, y, dim, e);
        }
    }
    scrollbar_px(LIST_Y - 2, LIST_Y + LIST_ROWS * LIST_STEP - 2, 0, 128,
        s->top, LIST_ROWS);
}

//---
//  Grid view
//---

#define GRID_X  44
#define GRID_Y  35
#define CELL_W  21
#define CELL_H  17

static void draw_grid(state_t const *s)
{
    char buf[16];

    for (int col = 0; col < 16; col++)
        dprint_opt(GRID_X + col * CELL_W + CELL_W / 2, GRID_Y - 13, C_DARK,
            C_NONE, DTEXT_CENTER, DTEXT_TOP, "%X", col);

    for (int row = 0; row < 8; row++) {
        int y = GRID_Y + row * CELL_H;
        dprint_opt(GRID_X - 6, y + 5, C_DARK, C_NONE, DTEXT_RIGHT,
            DTEXT_TOP, "%Xx", row);

        for (int col = 0; col < 16; col++) {
            int c = row * 16 + col;
            int x = GRID_X + col * CELL_W;
            bool sel = (c == s->sel);
            int bg = sel ? COLOR : is_control(c) ? C_RGB(31, 27, 25)
                : (row + col) % 2 ? ROWBG : C_WHITE;

            drect(x, y, x + CELL_W - 2, y + CELL_H - 2, bg);
            if (is_control(c)) {
                /* No glyph: a small hollow box */
                int cx = x + CELL_W / 2 - 3, cy = y + 5;
                drect_border(cx, cy, cx + 5, cy + 6, C_NONE, 1,
                    sel ? C_WHITE : CTRLC);
            }
            else {
                buf[0] = c; buf[1] = '\0';
                dtext_opt(x + CELL_W / 2 - 1, y + 4, sel ? C_WHITE : C_BLACK,
                    C_NONE, DTEXT_CENTER, DTEXT_TOP, buf);
            }
        }
    }

    /* Details of the selected character */
    int c = s->sel;
    int y = GRID_Y + 8 * CELL_H + 6;
    dline(6, y - 3, DWIDTH - 7, y - 3, C_LIGHT);
    if (is_control(c)) dprint(10, y, CTRLC, "%s", short_name(c));
    else {
        buf[0] = c; buf[1] = '\0';
        font_t const *old = dfont(&font_big);
        dtext(12, y, C_BLACK, buf);
        dfont(old);
    }
    dprint(50, y, C_BLACK, "%d   0x%02X   %03o   %s", c, c, c, bin8(buf, c));
    char const *e = c_escape(c);
    dprint(50, y + 12, C_DARK, "%s%s%s", long_name(c), e ? "   " : "",
        e ? e : "");
}

//---
//  UTF-8 view
//---

#define BYTE_COLORS 4
static int const byte_color[BYTE_COLORS] = {
    C_RGB(4, 12, 22), C_RGB(2, 17, 6), C_RGB(26, 9, 0), C_RGB(17, 4, 22) };

#define BIT_W   9

/* One bit cell: filled if one, outlined if zero */
static void bit_cell(int x, int y, int one, int color)
{
    if (one) drect(x, y, x + BIT_W - 2, y + 10, color);
    else drect_border(x, y, x + BIT_W - 2, y + 10, C_WHITE, 1, color);
}

static void draw_utf8(state_t const *s)
{
    uint32_t cp = s->cp;
    char buf[32];
    int y = 22;

    /* Code point being typed */
    font_t const *old = dfont(&font_big);
    sprintf(buf, "U+%04X", (unsigned)cp);
    dtext(10, y, C_BLACK, buf);
    int w;
    dsize(buf, NULL, &w, NULL);
    drect(10 + w + 2, y, 10 + w + 3, y + 13, COLOR);
    if (cp >= 32 && cp < 127) {
        buf[0] = cp; buf[1] = '\0';
        dtext(DWIDTH - 30, y, COLOR, buf);
    }
    dfont(old);

    y = 44;
    dprint(10, y, C_DARK, L("Decimal", "Decimal", "Decimal"));
    dprint(96, y, C_BLACK, "%u", (unsigned)cp);
    dtext_opt(DWIDTH - 10, y, C_DARK, C_NONE, DTEXT_RIGHT, DTEXT_TOP,
        block_name(cp));
    y += 13;

    if (is_surrogate(cp)) {
        dtext(10, y + 6, C_RED, L("Surrogate code points (D800-DFFF) are",
            "Pontos substitutos (D800-DFFF) so",
            "Les substituts (D800-DFFF) vont par"));
        dtext(10, y + 19, C_RED, L("only used in pairs by UTF-16: no UTF-8.",
            "usados em pares no UTF-16: sem UTF-8.",
            "paires en UTF-16 seulement: sans UTF-8."));
        return;
    }

    uint8_t bytes[4];
    int n = utf8_encode(cp, bytes);

    dtext(10, y, C_DARK, "UTF-8");
    int x = 96;
    for (int i = 0; i < n; i++) {
        sprintf(buf, "%02X", bytes[i]);
        dtext(x, y, byte_color[i], buf);
        x += 22;
    }
    dprint(x + 4, y, C_DARK, "(%d byte%s)", n, n > 1 ? "s" : "");
    y += 13;

    dtext(10, y, C_DARK, "UTF-16");
    if (cp < 0x10000) dprint(96, y, C_BLACK, "%04X", (unsigned)cp);
    else {
        uint32_t v = cp - 0x10000;
        dprint(96, y, C_BLACK, "%04X %04X", (unsigned)(0xD800 | (v >> 10)),
            (unsigned)(0xDC00 | (v & 0x3FF)));
        dtext_opt(DWIDTH - 10, y, C_DARK, C_NONE, DTEXT_RIGHT, DTEXT_TOP,
            "surrogate pair");
    }
    y += 13;
    dtext(10, y, C_DARK, "UTF-32");
    dprint(96, y, C_BLACK, "%08X", (unsigned)cp);
    y += 13;
    sprintf(buf, "%u", (unsigned)cp);
    dtext(10, y, C_DARK, "HTML");
    dprint(96, y, C_BLACK, "&#%u;   &#x%X;", (unsigned)cp, (unsigned)cp);
    y += 18;
    dline(6, y - 4, DWIDTH - 7, y - 4, C_LIGHT);

    /* Payload bits per byte: 7, then 5/6, 4/6/6, 3/6/6/6 */
    int payload[4];
    if (n == 1) payload[0] = 7;
    else {
        payload[0] = 7 - n;
        for (int i = 1; i < n; i++) payload[i] = 6;
    }
    int total = 0;
    for (int i = 0; i < n; i++) total += payload[i];

    /* Code point bits, coloured by the byte they go to */
    dtext(10, y + 1, C_DARK, "Bits");
    x = 60;
    int bit = total - 1;
    for (int i = 0; i < n; i++) {
        for (int k = 0; k < payload[i]; k++, bit--, x += BIT_W)
            bit_cell(x, y, (cp >> bit) & 1, byte_color[i]);
        x += 6;
    }
    y += 18;

    /* Bytes: marker bits in grey, payload bits in the byte's colour */
    dtext(10, y + 1, C_DARK, "Bytes");
    x = 60;
    for (int i = 0; i < n; i++) {
        int marker = 8 - payload[i];
        for (int k = 7; k >= 0; k--, x += BIT_W) {
            int one = (bytes[i] >> k) & 1;
            bit_cell(x, y, one, (7 - k) < marker ? C_DARK : byte_color[i]);
        }
        x += 8;
    }
    y += 16;
    dtext(60, y, C_DARK, n == 1 ? L("0xxxxxxx: one byte, same as ASCII",
        "0xxxxxxx: um byte, igual a ASCII",
        "0xxxxxxx: un octet, comme ASCII")
        : L("Grey: length/continuation markers",
        "Cinza: marcas de tamanho/continuacao",
        "Gris: marques de longueur/suite"));
}

#endif /* GINT_HW_FX */

//---
//  Main loop
//---

static void draw(state_t *s)
{
#if GINT_HW_FX
    char const * const titles[] = {
        L("ASCII table", "Tabela ASCII", "Table ASCII"),
        L("ASCII grid", "Grelha ASCII", "Grille ASCII"),
        L("UTF-8 encoder", "UTF-8", "UTF-8") };
#else
    char const * const titles[] = {
        L("ASCII table", "Tabela ASCII", "Table ASCII"),
        L("ASCII grid", "Grelha ASCII", "Grille ASCII"),
        L("UTF-8 encoder", "Codificador UTF-8", "Encodeur UTF-8") };
#endif

    dclear(C_WHITE);
    info_title(titles[s->view], COLOR);

    if (s->view == VIEW_LIST) draw_list(s);
    else if (s->view == VIEW_GRID) draw_grid(s);
    else draw_utf8(s);

#if GINT_HW_FX
    char const * const labels[] = { L("LST", "LST", "LST"),
        L("GRD", "GRL", "GRL"),
        "UTF" };
#else
    char const * const labels[] = { L("LIST", "LISTA", "LISTE"),
        L("GRID", "GRELHA", "GRILLE"), "UTF-8" };
#endif
    for (int i = 0; i < 3; i++) {
        if (i == s->view) fkey_button_color(i + 1, labels[i], COLOR);
        else fkey_button(i + 1, labels[i]);
    }
#if GINT_HW_FX
    /* F6 flips between the two UTF-8 pages */
    if (s->view == VIEW_UTF8)
        fkey_button(6, s->page ? L("ENC", "COD", "COD") : "BIT");
#endif
    dupdate();
}

static void move_sel(state_t *s, int delta)
{
    s->sel = (s->sel + delta + 128) % 128;
    s->jump = 0;
}

int main(void)
{
    state_t s = { .view = VIEW_LIST, .sel = 'A', .top = 'A' - 4,
        .cp = 0xE9 };

    lang_init();
    usb_screen_init();
    screen_init();

    while (1) {
        draw(&s);

        int key = getkey().key;
        int d = key_hex(key);

        if (key == KEY_EXIT) return 1;
        if (key == INFO_KEY) {
#if GINT_HW_FX
            info_show("ASCII", "ASCII/UTF-8 " APPS_VERSION, COLOR);
#else
            info_show("ASCII", L("ASCII / UTF-8 table " APPS_VERSION,
                "Tabela ASCII / UTF-8 " APPS_VERSION,
                "Table ASCII / UTF-8 " APPS_VERSION), COLOR);
#endif
            continue;
        }
        if (key == KEY_F1) { s.view = VIEW_LIST; continue; }
        if (key == KEY_F2) { s.view = VIEW_GRID; continue; }
        if (key == KEY_F3) {
            if (s.view != VIEW_UTF8) s.cp = s.sel;
            s.view = VIEW_UTF8;
            continue;
        }

        if (s.view == VIEW_UTF8) {
            if (key == KEY_UP && s.cp < 0x10FFFF) s.cp++;
            else if (key == KEY_DOWN && s.cp > 0) s.cp--;
            else if (key == KEY_DEL) s.cp >>= 4;
            else if (key == KEY_ACON) s.cp = 0;
#if GINT_HW_FX
            else if (key == KEY_F6 || key == KEY_LEFT || key == KEY_RIGHT)
                s.page = !s.page;
#endif
            else if (d >= 0 && s.cp * 16 + d <= 0x10FFFF) s.cp = s.cp * 16 + d;
            continue;
        }

        int step = s.view == VIEW_GRID ? 16 : LIST_ROWS;
        if (key == KEY_UP) move_sel(&s, s.view == VIEW_GRID ? -16 : -1);
        else if (key == KEY_DOWN) move_sel(&s, s.view == VIEW_GRID ? 16 : 1);
        else if (key == KEY_LEFT) move_sel(&s, s.view == VIEW_GRID ? -1 : -step);
        else if (key == KEY_RIGHT) move_sel(&s, s.view == VIEW_GRID ? 1 : step);
        else if (key == KEY_EXE) {
            s.cp = s.sel;
            s.view = VIEW_UTF8;
        }
        /* Decimal digits jump to a code: 6 then 5 goes to 65 */
        else if (d >= 0 && d <= 9) {
            int v = s.jump * 10 + d;
            s.jump = v < 128 ? v : d;
            s.sel = s.jump;
        }
    }
}
