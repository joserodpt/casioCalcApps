#include <gint/display.h>
#include <gint/keyboard.h>
#include <math.h>
#include <string.h>

#include "numfield.h"
#include "keys.h"

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
