#include <gint/display.h>
#include <gint/keyboard.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ui.h"

//---
//  Number field
//---

static int key_digit(int key)
{
    switch (key) {
        case KEY_0: return 0; case KEY_1: return 1; case KEY_2: return 2;
        case KEY_3: return 3; case KEY_4: return 4; case KEY_5: return 5;
        case KEY_6: return 6; case KEY_7: return 7; case KEY_8: return 8;
        case KEY_9: return 9;
        default: return -1;
    }
}

double numfield_value(numfield_t const *f)
{
    char const *s = f->text;
    double mant = 0, frac = 1;
    bool neg = false, point = false, any = false;
    int exp = 0;

    if (*s == '-') { neg = true; s++; }
    for (; *s; s++) {
        if (*s >= '0' && *s <= '9') {
            if (point) mant += (*s - '0') * (frac /= 10);
            else mant = mant * 10 + (*s - '0');
            any = true;
        } else if (*s == '.') {
            if (point) return NAN;
            point = true;
        } else if (*s == 'E') {
            int e = 0, es = 1;
            s++;
            if (*s == '-') { es = -1; s++; }
            else if (*s == '+') s++;
            if (!*s) return NAN;
            for (; *s; s++) {
                if (*s < '0' || *s > '9') return NAN;
                e = e * 10 + (*s - '0');
            }
            exp = es * e;
            break;
        } else return NAN;
    }
    if (!any) return NAN;
    return (neg ? -mant : mant) * pow(10, exp);
}

void numfield_set(numfield_t *f, double v)
{
    /* Only used for simple round-trips; keep it short */
    snprintf(f->text, sizeof f->text, "%d", (int)v);
}

bool numfield_key(numfield_t *f, int key)
{
    int n = strlen(f->text);
    int d = key_digit(key);

    if (key == KEY_DEL) { if (n) f->text[n - 1] = '\0'; return true; }
    if (key == KEY_ACON) { f->text[0] = '\0'; return true; }
    if (n >= (int)sizeof f->text - 1) return false;

    if (d >= 0) { f->text[n] = '0' + d; f->text[n + 1] = '\0'; return true; }
    if (key == KEY_DOT) { f->text[n] = '.'; f->text[n + 1] = '\0'; return true; }
    if (key == KEY_EXP && !strchr(f->text, 'E')) {
        f->text[n] = 'E'; f->text[n + 1] = '\0'; return true;
    }
    if (key == KEY_NEG || key == KEY_MINUS) {
        if (n == 0 || f->text[n - 1] == 'E') {
            f->text[n] = '-'; f->text[n + 1] = '\0';
        }
        return true;
    }
    return false;
}

void numfield_draw(numfield_t const *f, int x, int y, int w, bool sel,
    int color)
{
    drect_border(x, y, x + w - 1, y + 16, C_WHITE, sel ? 2 : 1,
        sel ? color : C_LIGHT);
    dtext(x + 5, y + 3, C_BLACK, f->text);
    if (sel) {
        int tw;
        dsize(f->text, NULL, &tw, NULL);
        int cx = x + 5 + tw + 1;
        drect(cx, y + 3, cx + 1, y + 13, color);
    }
}

//---
//  Results
//---

void conv_result(int y, int val_x, char const *label, int color,
    char const *value)
{
    dtext(10, y, C_DARK, label);
    dtext(val_x, y, color, value);
}

//---
//  Element colours (match periodic.c category order)
//---

int category_color(int cat)
{
    switch (cat) {
        case 0: return C_RGB(7, 20, 10);    /* nonmetal   green  */
        case 1: return C_RGB(20, 12, 24);   /* noble gas  purple */
        case 2: return C_RGB(28, 8, 6);     /* alkali     red    */
        case 3: return C_RGB(28, 16, 4);    /* alk earth  orange */
        case 4: return C_RGB(10, 18, 20);   /* metalloid  teal   */
        case 5: return C_RGB(10, 16, 28);   /* halogen    blue   */
        case 6: return C_RGB(18, 18, 22);   /* transition grey   */
        case 7: return C_RGB(14, 20, 24);   /* post-trans slate  */
        default: return C_RGB(24, 14, 22);  /* lanth/act  pink   */
    }
}
