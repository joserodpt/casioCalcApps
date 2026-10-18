#include <gint/display.h>
#include <gint/keyboard.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "utils.h"

//REFERENCIA: https://git.planet-casio.com/Lephenixnoir/gint/src/branch/master/include/gint

extern font_t const font_big;

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
    *neg = s->mode == MODE_DEC && is_negative(s);
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

// Key -> digit value (0-15), or -1 if the key is not a digit
static int key_digit(int key)
{
    switch (key) {
        case KEY_0: return 0;
        case KEY_1: return 1;
        case KEY_2: return 2;
        case KEY_3: return 3;
        case KEY_4: return 4;
        case KEY_5: return 5;
        case KEY_6: return 6;
        case KEY_7: return 7;
        case KEY_8: return 8;
        case KEY_9: return 9;
        case KEY_XOT: return 10;
        case KEY_LOG: return 11;
        case KEY_LN:  return 12;
        case KEY_SIN: return 13;
        case KEY_COS: return 14;
        case KEY_TAN: return 15;
        default: return -1;
    }
}

//---
//  Rendering
//---

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
    drect(0, 0, DWIDTH - 1, 15, b->color);
    dtext(6, 3, C_WHITE, "Base-N Converter");
    dtext_opt(DWIDTH - 6, 3, C_WHITE, C_NONE, DTEXT_RIGHT, DTEXT_TOP,
        "v1.1   Coded in Portugal :)");

    // Status line: input base on the left, word size on the right
    dtext(8, 20, b->color, b->name);
    dprint_opt(DWIDTH - 8, 20, C_DARK, C_NONE, DTEXT_RIGHT, DTEXT_TOP,
        "%d-bit  %s", width_of(s), s->is_signed ? "signed" : "unsigned");

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
    if (s->is_signed) fkey_button_color(6, "SIGNED", b->color);
    else fkey_button(6, "SIGNED");

    dupdate();
}

//---
//  Main loop
//---

int main(void)
{
    state_t s = { .value = 0, .mode = MODE_DEC,
        .width_index = WIDTH_COUNT - 1, .is_signed = false };

    while (1) {
        draw(&s);

        // getkey() only returns KEYEV_DOWN and KEYEV_HOLD (repeat), so
        // holding DEL deletes repeatedly
        int key = getkey().key;
        int d;

        switch (key) {
            case KEY_EXIT:
                return 1;
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
                s.value = -s.value & mask(width_of(&s));
                break;
            case KEY_ACON:
                s.value = 0;
                break;
            case KEY_DEL:
                delete_digit(&s);
                break;
            default:
                if ((d = key_digit(key)) >= 0 && d < bases[s.mode].base)
                    type_digit(&s, d);
                break;
        }
    }
}
