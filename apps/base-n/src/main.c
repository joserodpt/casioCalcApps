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

//REFERENCIA: https://git.planet-casio.com/Lephenixnoir/gint/src/branch/master/include/gint



enum { MODE_DEC, MODE_HEX, MODE_OCT, MODE_BIN, MODE_COUNT };

typedef struct {
    char const *name;
    int base;
    int group;          // digits per group in the conversion rows
    int color;
} base_t;

static base_t const bases[MODE_COUNT] = {
    { "DEC", 10, 3, C_RGB(3, 9, 24) },   // blue
    { "HEX", 16, 4, C_RGB(26, 9, 0) },   // orange
    { "OCT",  8, 3, C_RGB(2, 17, 6) },   // green
    { "BIN",  2, 4, C_RGB(17, 4, 22) },  // purple
};

static int const widths[] = { 8, 16, 32, 64 };
#define WIDTH_COUNT (int)(sizeof widths / sizeof *widths)

typedef struct {
    uint64_t value;     // raw bits, always masked to the current width
    int mode;
    int width_index;
    bool is_signed;     // decimal shows two's complement values
    bool neg_pending;   // decimal: a '-' typed while the value is still 0
} state_t;

#define BUF_SIZE 80     // 64 binary digits + 15 group spaces + '\0'
#define BIG_BIN_DIGITS 32

static char const digits[] = "0123456789ABCDEF";

//---
//  Value helpers
//---

static int width_of(state_t const *s)
{
    return widths[s->width_index];
}

static uint64_t mask(int bits)
{
    return bits >= 64 ? UINT64_MAX : ((uint64_t)1 << bits) - 1;
}

// Whether the value reads as negative (two's complement, signed mode only)
static bool is_negative(state_t const *s)
{
    return s->is_signed && ((s->value >> (width_of(s) - 1)) & 1);
}

// Absolute value of the number as shown in decimal
static uint64_t magnitude(state_t const *s)
{
    return is_negative(s) ? -s->value & mask(width_of(s)) : s->value;
}

// The number being typed: signed decimal edits the absolute value and keeps
// the sign, every other base edits the raw bits
static uint64_t edit_get(state_t const *s, bool *neg)
{
    // In decimal, the number is negative when its bits say so, or when '-'
    // was pressed while it was still 0 (so the next digits become negative)
    *neg = s->mode == MODE_DEC
        && (is_negative(s) || (s->value == 0 && s->neg_pending));
    return s->mode == MODE_DEC ? magnitude(s) : s->value;
}

static void edit_set(state_t *s, uint64_t mag, bool neg)
{
    s->value = (neg ? -mag : mag) & mask(width_of(s));
}

// Append a digit, unless the result would not fit in the current width
static void type_digit(state_t *s, int d)
{
    bool neg;
    uint64_t mag = edit_get(s, &neg);
    int base = bases[s->mode].base;

    uint64_t limit = mask(width_of(s));
    if (s->mode == MODE_DEC && s->is_signed)
        limit = (limit >> 1) + neg;     // -2^(w-1) .. 2^(w-1)-1

    if (mag > (limit - d) / base) return;
    edit_set(s, mag * base + d, neg);
}

static void delete_digit(state_t *s)
{
    bool neg;
    uint64_t mag = edit_get(s, &neg);
    edit_set(s, mag / bases[s->mode].base, neg);
}

//---
//  Formatting
//---

// Format value into buf with a space every `group` digits (0 = no groups) and
// at least `min_digits` digits; returns a pointer to the first char in buf
static char *format(uint64_t value, int base, int group, int min_digits,
    char buf[BUF_SIZE])
{
    char *p = buf + BUF_SIZE - 1;
    int n = 0;
    *p = '\0';
    do {
        if (group && n && n % group == 0) *--p = ' ';
        *--p = digits[value % base];
        value /= base;
        n++;
    } while (value || n < min_digits);
    return p;
}

// The value as shown in a given base (decimal gets a '-' when negative)
static char *value_text(state_t const *s, int mode, int group,
    char buf[BUF_SIZE])
{
    if (mode == MODE_DEC && is_negative(s)) {
        char *p = format(magnitude(s), 10, group, 1, buf);
        *--p = '-';
        return p;
    }
    return format(s->value, bases[mode].base, group, 1, buf);
}

//---
//  Rendering
//---

#if GINT_HW_FX
// fx-9860G (128x64): no room for the bit grid, so every base gets one block
// of text: the value being typed first (large when it fits), then the three
// conversions. Long values switch to a 3x5 font and binary wraps every six
// nibbles; the layout picks the largest fonts that fit between the title
// bar and the F-keys. The word size and sign mode show on F5 and F6.

#define FX_RIGHT    124     // right edge of every value (cursor at 126..127)
#define FX_LEFT     14      // values start after a 3x5 base label
#define FX_TOP      11
#define FX_BOTTOM   55      // first row left free above the F-keys
#define FX_GAP      2       // between blocks
#define FX_SEP      4       // gap, line and gap below the input block
#define FX_MAX_LINES 4

enum { FONT_BIG, FONT_STD, FONT_MINI };

// 3x5 glyphs: five rows of three bits, one octal digit per row, top row
// first. Hex digits, '-', and the other letters of the base names
static char const mini_chars[] = "0123456789ABCDEF-HXOTIN";
static uint16_t const mini_glyphs[] = {
    075557, 026227, 071747, 071717, 055711, 074717, 074757, 071111,
    075757, 075717, 025755, 065656, 034443, 065556, 074647, 074644,
    000700, 055755, 055255, 025552, 072222, 072227, 065555,
};

static void mini_char(int x, int y, char c, int color)
{
    char const *p = strchr(mini_chars, c);
    if (!c || !p) return;
    int g = mini_glyphs[p - mini_chars];

    for (int r = 0; r < 5; r++)
    for (int col = 0; col < 3; col++) {
        if ((g >> (3 * (4 - r) + 2 - col)) & 1)
            dpixel(x + col, y + r, color);
    }
}

static int advance(int font, char c)
{
    if (font == FONT_MINI) return c == ' ' ? 2 : 4;
    return c == ' ' ? 3 : 6;
}

static int run_width(int font, char const *text, int len)
{
    int w = -1;
    for (int i = 0; i < len; i++) w += advance(font, text[i]);
    return w;
}

// Draw len characters of text in a small font, ending at x = right
static void run_draw(int font, int right, int y, char const *text, int len,
    int color)
{
    int x = right - run_width(font, text, len);
    for (int i = 0; i < len; i++) {
        if (text[i] != ' ') {
            if (font == FONT_MINI) {
                mini_char(x, y, text[i], color);
            } else {
                char ch[2] = { text[i], '\0' };
                dtext(x, y, color, ch);
            }
        }
        x += advance(font, text[i]);
    }
}

typedef struct {
    int mode;
    int font;
    char const *text;
    int lines;
    char const *start[FX_MAX_LINES];    // top line first
    int len[FX_MAX_LINES];
    int height;
    char buf[BUF_SIZE];
} block_t;

static int line_height(int font)
{
    return font == FONT_BIG ? 14 : font == FONT_STD ? 7 : 5;
}

// Lay text out in a small font, breaking at group spaces so that every line
// fits in width; lines are filled from the right (the low digits)
static bool block_wrap(block_t *b, int font, int width)
{
    char const *t = b->text;
    int end = strlen(t), n = 0;
    char const *start[FX_MAX_LINES];
    int len[FX_MAX_LINES];

    while (end > 0) {
        int best = -1;
        for (int p = end - 1; p >= 0; p--) {
            if (run_width(font, t + p, end - p) > width) break;
            if (p == 0 || t[p - 1] == ' ') best = p;
        }
        if (best < 0 || n == FX_MAX_LINES) return false;
        start[n] = t + best;
        len[n++] = end - best;
        end = best - 1;     // skip the group space
    }

    b->font = font;
    b->lines = n;
    for (int i = 0; i < n; i++) {
        b->start[i] = start[n - 1 - i];
        b->len[i] = len[n - 1 - i];
    }
    b->height = n * (line_height(font) + 1) - 1;
    return true;
}

// The value typed: large or in the normal font on one line (both without
// group spaces, as on the fx-CG), else wrapped in the 3x5 font. Returns
// false when it does not fit in the given font
static bool block_input(block_t *b, state_t const *s, int font)
{
    b->mode = s->mode;
    if (font == FONT_MINI) {
        b->text = value_text(s, s->mode, bases[s->mode].group, b->buf);
        return block_wrap(b, FONT_MINI, FX_RIGHT - FX_LEFT + 1);
    }

    int w;
    b->text = value_text(s, s->mode, 0, b->buf);
    dsize(b->text, font == FONT_BIG ? &font_big : NULL, &w, NULL);
    b->font = font;
    b->lines = 1;
    b->start[0] = b->text;
    b->len[0] = strlen(b->text);
    b->height = line_height(font);
    return w <= FX_RIGHT - FX_LEFT + 1;
}

// A conversion: the normal font on one line, else wrapped in the 3x5 font
static void block_row(block_t *b, state_t const *s, int mode)
{
    b->mode = mode;
    b->text = value_text(s, mode, bases[mode].group, b->buf);
    if (!block_wrap(b, FONT_STD, FX_RIGHT - FX_LEFT + 1) || b->lines > 1)
        block_wrap(b, FONT_MINI, FX_RIGHT - FX_LEFT + 1);
}

static void block_draw(block_t const *b, int y, bool active)
{
    // 3x5 base label beside the first line, white on black for the input
    int ly = (b->font == FONT_MINI) ? y : y + 1;
    if (active) drect(0, ly - 1, 12, ly + 5, C_BLACK);
    for (int i = 0; i < 3; i++)
        mini_char(1 + 4 * i, ly, bases[b->mode].name[i],
            active ? C_WHITE : C_DARK);

    for (int i = 0; i < b->lines; i++) {
        if (b->font == FONT_BIG) {
            font_t const *old = dfont(&font_big);
            dtext_opt(FX_RIGHT, y, C_BLACK, C_NONE, DTEXT_RIGHT, DTEXT_TOP,
                b->text);
            dfont(old);
        } else {
            run_draw(b->font, FX_RIGHT, y, b->start[i], b->len[i], C_BLACK);
        }
        if (i < b->lines - 1) y += line_height(b->font) + 1;
    }

    // Cursor after the last digit typed
    if (active)
        drect(FX_RIGHT + 2, y, FX_RIGHT + 3, y + line_height(b->font) - 1,
            bases[b->mode].color);
}

static void draw(state_t const *s)
{
    base_t const *b = &bases[s->mode];
    block_t input, rows[MODE_COUNT - 1];
    char width_label[8];
    int n = 0, rows_h = 0;

    dclear(C_WHITE);
    info_title(L("Base-N Converter", "Conversor Base-N"), b->color);

    for (int m = 0; m < MODE_COUNT; m++) {
        if (m == s->mode) continue;
        block_row(&rows[n], s, m);
        rows_h += rows[n++].height + FX_GAP;
    }
    rows_h -= FX_GAP;

    // Largest input font that leaves room for the conversions
    for (int font = FONT_BIG; font <= FONT_MINI; font++) {
        if (block_input(&input, s, font)
            && input.height + FX_SEP + rows_h <= FX_BOTTOM - FX_TOP) break;
    }

    int y = FX_TOP;
    block_draw(&input, y, true);
    y += input.height + 1;
    dline(0, y, DWIDTH - 1, y, C_LIGHT);
    y += FX_SEP - 1;
    for (int i = 0; i < n; i++) {
        block_draw(&rows[i], y, false);
        y += rows[i].height + FX_GAP;
    }

    for (int i = 0; i < MODE_COUNT; i++) {
        if (i == s->mode) fkey_button_color(i + 1, bases[i].name, b->color);
        else fkey_button(i + 1, bases[i].name);
    }
    snprintf(width_label, sizeof width_label, "%db", width_of(s));
    fkey_button(5, width_label);
    if (s->is_signed) fkey_button_color(6, L("SGN", "C/S"), b->color);
    else fkey_button(6, L("UNS", "S/S"));

    dupdate();
}
#else

#define BIG_RIGHT   376     // right edge of the large value
#define ROWS_Y      76
#define ROW_STEP    14
#define GRID_X      46
#define GRID_Y      150
#define GRID_STEP   9       // cell pitch; cells are 8x10
#define GRID_NIBBLE 3       // extra gap between groups of 4 bits

// Large value being typed, with a cursor; binary wraps over two lines
static void draw_input(state_t const *s)
{
    char buf[BUF_SIZE];
    char *text = value_text(s, s->mode, 0, buf);
    int len = strlen(text);
    int color = bases[s->mode].color;
    int y = 38;

    font_t const *old = dfont(&font_big);
    if (len > BIG_BIN_DIGITS) {
        char *low = text + len - BIG_BIN_DIGITS;
        y = 50;
        dtext_opt(BIG_RIGHT, y, C_BLACK, C_NONE, DTEXT_RIGHT, DTEXT_TOP, low);
        *low = '\0';
        dtext_opt(BIG_RIGHT, y - 18, C_BLACK, C_NONE, DTEXT_RIGHT, DTEXT_TOP,
            text);
    } else {
        dtext_opt(BIG_RIGHT, y, C_BLACK, C_NONE, DTEXT_RIGHT, DTEXT_TOP, text);
    }
    dfont(old);

    drect(BIG_RIGHT + 3, y, BIG_RIGHT + 4, y + 13, color);
}

// One conversion row per base other than the active one
static void draw_rows(state_t const *s)
{
    char buf[BUF_SIZE];
    int y = ROWS_Y;

    for (int m = 0; m < MODE_COUNT; m++) {
        if (m == s->mode) continue;
        dtext(8, y, bases[m].color, bases[m].name);

        // Binary over 32 bits: high bits first, then the low 32 bits
        if (m == MODE_BIN && (s->value >> 32)) {
            dtext(44, y, C_BLACK, format(s->value >> 32, 2, 4, 1, buf));
            y += ROW_STEP;
            dtext(44, y, C_BLACK,
                format(s->value & 0xffffffff, 2, 4, 32, buf));
        } else {
            dtext(44, y, C_BLACK, value_text(s, m, bases[m].group, buf));
        }
        y += ROW_STEP;
    }
}

// 64-bit grid, bits 63..32 on top; bits outside the width are dimmed
static void draw_bit_grid(state_t const *s)
{
    int width = width_of(s);
    int color = bases[s->mode].color;
    int grid_w = 31 * GRID_STEP + 7 * GRID_NIBBLE + 8;

    for (int r = 0; r < 2; r++) {
        int y = GRID_Y + 16 * r;
        int hi = 63 - 32 * r;
        int label = (hi < width) ? C_DARK : C_LIGHT;

        dprint_opt(GRID_X - 5, y + 1, label, C_NONE, DTEXT_RIGHT, DTEXT_TOP,
            "%d", hi);
        dprint(GRID_X + grid_w + 5, y + 1, (hi - 31 < width) ? C_DARK
            : C_LIGHT, "%d", hi - 31);

        for (int c = 0; c < 32; c++) {
            int bit = hi - c;
            int x = GRID_X + c * GRID_STEP + (c / 4) * GRID_NIBBLE;

            if (bit >= width)
                drect_border(x, y, x + 7, y + 9, C_WHITE, 1, C_LIGHT);
            else if ((s->value >> bit) & 1)
                drect(x, y, x + 7, y + 9, color);
            else
                drect_border(x, y, x + 7, y + 9, C_WHITE, 1, C_DARK);
        }
    }
}

static void draw(state_t const *s)
{
    base_t const *b = &bases[s->mode];
    char width_label[8];

    dclear(C_WHITE);

    // Title bar in the colour of the active base
    info_title(L("Base-N Converter", "Conversor Base-N"), b->color);

    // Status line: input base on the left, word size on the right
    dtext(8, 20, b->color, b->name);
    dprint_opt(DWIDTH - 8, 20, C_DARK, C_NONE, DTEXT_RIGHT, DTEXT_TOP,
        "%d-bit  %s", width_of(s),
        s->is_signed ? L("signed", "com sinal") : L("unsigned", "sem sinal"));

    draw_input(s);
    dline(6, 68, DWIDTH - 7, 68, C_LIGHT);
    draw_rows(s);
    draw_bit_grid(s);

    for (int i = 0; i < MODE_COUNT; i++) {
        if (i == s->mode) fkey_button_color(i + 1, bases[i].name, b->color);
        else fkey_button(i + 1, bases[i].name);
    }
    snprintf(width_label, sizeof width_label, "%d-bit", width_of(s));
    fkey_button(5, width_label);
    if (s->is_signed) fkey_button_color(6, L("SIGNED", "SINAL"), b->color);
    else fkey_button(6, L("SIGNED", "SINAL"));

    dupdate();
}

#endif /* GINT_HW_FX */

//---
//  Main loop
//---

int main(void)
{
    state_t s = { .value = 0, .mode = MODE_DEC,
        .width_index = WIDTH_COUNT - 1, .is_signed = false };

    lang_init();
    usb_screen_init();
    screen_init();

    while (1) {
        draw(&s);

        // getkey() only returns KEYEV_DOWN and KEYEV_HOLD (repeat), so
        // holding DEL deletes repeatedly
        int key = getkey().key;
        int d;

        switch (key) {
            case KEY_EXIT:
                return 1;
            case INFO_KEY:
                info_show("Base-N", L("Converter " APPS_VERSION,
                    "Conversor " APPS_VERSION), bases[s.mode].color);
                break;
            case KEY_F1: case KEY_F2: case KEY_F3: case KEY_F4:
                s.mode = key - KEY_F1;
                break;
            case KEY_UP:
                s.mode = (s.mode + MODE_COUNT - 1) % MODE_COUNT;
                break;
            case KEY_DOWN:
                s.mode = (s.mode + 1) % MODE_COUNT;
                break;
            case KEY_F5:
                s.width_index = (s.width_index + 1) % WIDTH_COUNT;
                s.value &= mask(width_of(&s));
                break;
            case KEY_F6:
                s.is_signed = !s.is_signed;
                break;
            case KEY_NEG:
            case KEY_MINUS:     // no subtraction here, so '-' negates too
                if (s.mode == MODE_DEC) {
                    // Negatives only read in signed mode, so turn it on
                    s.is_signed = true;
                    if (s.value == 0) s.neg_pending = !s.neg_pending;
                    else s.value = -s.value & mask(width_of(&s));
                } else {
                    s.value = -s.value & mask(width_of(&s));
                }
                break;
            case KEY_ACON:
                s.value = 0;
                s.neg_pending = false;
                break;
            case KEY_DEL:
                delete_digit(&s);
                break;
            default:
                if ((d = key_hex(key)) >= 0 && d < bases[s.mode].base)
                    type_digit(&s, d);
                break;
        }
    }
}
