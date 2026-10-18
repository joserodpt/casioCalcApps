#include <gint/display.h>
#include <gint/keyboard.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "ui.h"
#include "info.h"
#include "i18n.h"

//---
//  Symbol glyphs
//---

typedef struct {
    char ch;
    bool big;
    int yoff;                   /* rows below the top of the text */
    char const *rows[12];       /* '#' = pixel, NULL ends the glyph */
} glyph_t;

static glyph_t const glyphs[] = {
    { CH_OHM, false, 0, {
        "..####..", ".##..##.", "##....##", "##....##", "##....##",
        ".##..##.", "..#..#..", "###..###", "###..###", NULL } },
    { CH_MU, false, 2, {
        "##..##", "##..##", "##..##", "##..##", "##..##", "##.###",
        "###.##", "##....", "##....", NULL } },
    { CH_PM, false, 1, {
        "..##..", "..##..", "######", "..##..", "..##..", "......",
        "######", NULL } },
    { CH_TIMES, false, 3, {
        "#...#", ".#.#.", "..#..", ".#.#.", "#...#", NULL } },
    { CH_PI, false, 2, {
        "#######", ".##.##.", ".##.##.", ".##.##.", ".##.##.", ".##.##.",
        ".##..##", NULL } },
    { CH_OHM, true, 1, {
        "...####...", "..######..", ".###..###.", "##......##",
        "##......##", "##......##", "##......##", ".##....##.",
        "..##..##..", "..##..##..", "####..####", "####..####" } },
    { CH_PM, true, 2, {
        "....##....", "....##....", "....##....", "##########",
        "##########", "....##....", "....##....", "....##....",
        "..........", "##########", "##########", NULL } },
};

static glyph_t const *find_glyph(char ch, bool big)
{
    glyph_t const *fallback = NULL;
    for (unsigned i = 0; i < sizeof glyphs / sizeof *glyphs; i++) {
        if (glyphs[i].ch != ch) continue;
        if (glyphs[i].big == big) return &glyphs[i];
        fallback = &glyphs[i];
    }
    return fallback;
}

static bool is_symbol(char c)
{
    return c >= CH_OHM && c <= CH_PI;
}

static int glyph_width(glyph_t const *g)
{
    return g ? (int)strlen(g->rows[0]) : 0;
}

static void glyph_draw(glyph_t const *g, int x, int y, int fg)
{
    if (!g) return;
    for (int r = 0; r < 12 && g->rows[r]; r++)
        for (int c = 0; g->rows[r][c]; c++)
            if (g->rows[r][c] == '#') dpixel(x + c, y + g->yoff + r, fg);
}

//---
//  Text
//---

/* Walk the string as runs of plain text and symbols; draw if draw is set */
static int etext_walk(int x, int y, int fg, bool big, char const *s,
    bool draw)
{
    font_t const *old = dfont(big ? &font_big : dfont_default());
    char run[64];
    int start = x;

    while (*s) {
        if (is_symbol(*s)) {
            glyph_t const *g = find_glyph(*s, big);
            if (draw) glyph_draw(g, x, y, fg);
            x += glyph_width(g) + 1;
            s++;
            continue;
        }
        int n = 0;
        while (s[n] && !is_symbol(s[n]) && n < (int)sizeof run - 1) n++;
        memcpy(run, s, n);
        run[n] = '\0';
        int w;
        dsize(run, NULL, &w, NULL);
        if (draw) dtext(x, y, fg, run);
        x += w + 1;
        s += n;
    }

    dfont(old);
    return x > start ? x - start - 1 : 0;
}

int etext_width(char const *s, bool big)
{
    return etext_walk(0, 0, 0, big, s, false);
}

void etext(int x, int y, int fg, int halign, bool big, char const *s)
{
    if (halign == DTEXT_CENTER) x -= etext_width(s, big) / 2;
    else if (halign == DTEXT_RIGHT) x -= etext_width(s, big);
    etext_walk(x, y, fg, big, s, true);
}

void eprint(int x, int y, int fg, int halign, char const *fmt, ...)
{
    char buf[96];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof buf, fmt, args);
    va_end(args);
    etext(x, y, fg, halign, false, buf);
}

//---
//  Number formatting
//---

/* Prefixes for 10^-12 .. 10^9; index 4 is no prefix */
static char const prefix_chars[] = { 'p', 'n', CH_MU, 'm', 0, 'k', 'M', 'G' };
#define PREFIX_NONE 4
#define PREFIX_COUNT 8

/* v with exactly dec decimals, using integer arithmetic (printf here has
   no %f, %lld or '*' width); |v| * 10^dec must fit in a long */
static char *fmt_fixed(char *buf, double v, int dec)
{
    long scale = 1;
    for (int i = 0; i < dec; i++) scale *= 10;
    double a = fabs(v) * scale;
    if (a > 2e9) a = 2e9;
    long n = lround(a);
    char const *sign = (v < 0 && n) ? "-" : "";

    if (dec > 0) {
        /* scale + fraction has a leading 1 followed by dec digits */
        char frac[16];
        sprintf(frac, "%ld", scale + n % scale);
        sprintf(buf, "%s%ld.%s", sign, n / scale, frac + 1);
    }
    else sprintf(buf, "%s%ld", sign, n);
    return buf;
}

/* Remove trailing zeros after the decimal point */
static void strip_zeros(char *num)
{
    if (!strchr(num, '.')) return;
    char *end = num + strlen(num) - 1;
    while (*end == '0') *end-- = '\0';
    if (*end == '.') *end = '\0';
}

/* Format v as a mantissa in num, return the prefix index */
static int fmt_eng(char *num, double v, int sig)
{
    double a = fabs(v);
    int p = PREFIX_NONE;

    if (a > 0) {
        p = (int)floor(log10(a) / 3) + PREFIX_NONE;
        if (p < 0) p = 0;
        if (p >= PREFIX_COUNT) p = PREFIX_COUNT - 1;
    }
    double m = v / pow(10, 3 * (p - PREFIX_NONE));

    for (int pass = 0; pass < 2; pass++) {
        double am = fabs(m);
        int idigits = am >= 100 ? 3 : am >= 10 ? 2 : 1;
        int dec = sig > idigits ? sig - idigits : 0;
        double scale = pow(10, dec);
        /* Rounding up to 1000 moves to the next prefix */
        if (pass == 0 && round(am * scale) / scale >= 1000
            && p < PREFIX_COUNT - 1) {
            p++;
            m /= 1000;
            continue;
        }
        fmt_fixed(num, m, dec);
        break;
    }
    strip_zeros(num);
    return p;
}

char *fmt_si(char *buf, double v, char const *unit, int sig)
{
    if (isnan(v)) return strcpy(buf, "-");
    if (isinf(v)) return strcpy(buf, "inf");

    char num[24];
    int p = fmt_eng(num, v, sig);
    if (p == PREFIX_NONE) sprintf(buf, "%s %s", num, unit);
    else sprintf(buf, "%s %c%s", num, prefix_chars[p], unit);
    return buf;
}

char *fmt_num(char *buf, double v, int dec)
{
    fmt_fixed(buf, v, dec);
    strip_zeros(buf);
    return buf;
}

char *fmt_dev(char *buf, double actual, double target)
{
    char num[32];
    double dev = (actual - target) / target * 100;
    if (fabs(dev) < 0.005) return strcpy(buf, L("(exact)", "(exato)"));
    fmt_fixed(num, dev, fabs(dev) < 10 ? 2 : 1);
    sprintf(buf, "(%s%s%%)", dev > 0 ? "+" : "", num);
    return buf;
}

//---
//  F-keys
//---

void efkey(int position, char const *text, int color)
{
    int x = 4 + 65 * (position - 1);
    int y = 207;
    int w = 63;

    dline(x + 1, y, x + w - 2, y, color);
    dline(x + 1, y + 14, x + w - 2, y + 14, color);
    drect(x, y + 1, x + w - 1, y + 13, color);
    etext(x + w / 2, y + 3, C_WHITE, DTEXT_CENTER, false, text);
}

//---
//  Fields
//---

static int prefix_exponent(char c)
{
    for (int i = 0; i < PREFIX_COUNT; i++)
        if (i != PREFIX_NONE && prefix_chars[i] == c)
            return 3 * (i - PREFIX_NONE);
    return 99;
}

static bool is_prefix(char c)
{
    return c && prefix_exponent(c) != 99;
}

double field_value(field_t const *f)
{
    char const *s = f->text;
    double mant = 0, frac = 1;
    bool neg = false, point = false, any = false;
    int exp10 = 0;

    if (f->kind != F_NUM) return NAN;
    if (*s == '-') { neg = true; s++; }

    for (; *s; s++) {
        if (*s >= '0' && *s <= '9') {
            if (point) mant += (*s - '0') * (frac /= 10);
            else mant = mant * 10 + (*s - '0');
            any = true;
        }
        else if (*s == '.') {
            if (point) return NAN;
            point = true;
        }
        else if (is_prefix(*s)) {
            /* The prefix may stand for the decimal point: "4k7" */
            if (exp10 || (point && s[1])) return NAN;
            exp10 = prefix_exponent(*s);
            point = true;
        }
        else if (*s == 'E') {
            int e = 0, sign = 1;
            s++;
            if (*s == '-') { sign = -1; s++; }
            if (!*s) return NAN;
            for (; *s; s++) {
                if (*s < '0' || *s > '9') return NAN;
                e = e * 10 + (*s - '0');
            }
            exp10 += sign * e;
            break;
        }
        else return NAN;
    }
    if (!any) return NAN;
    double v = mant * pow(10, exp10);
    return neg ? -v : v;
}

void field_set(field_t *f, double v)
{
    if (isnan(v)) { f->text[0] = '\0'; return; }
    char num[24];
    int p = fmt_eng(num, v, 4);
    if (p == PREFIX_NONE) snprintf(f->text, sizeof f->text, "%s", num);
    else snprintf(f->text, sizeof f->text, "%s%c", num, prefix_chars[p]);
}

static void append(field_t *f, char c, int maxlen)
{
    int n = strlen(f->text);
    if (n >= maxlen) return;
    f->text[n] = c;
    f->text[n + 1] = '\0';
}

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

/* SI prefixes on F1..F6 */
static char const fkey_prefixes[6] = { 'p', 'n', CH_MU, 'm', 'k', 'M' };

bool field_key(field_t *f, int key)
{
    int n = strlen(f->text);
    int d = key_digit(key);
    int maxlen = f->kind == F_CODE ? f->maxlen : (int)sizeof f->text - 1;

    if (f->kind == F_CHOICE) {
        if (key == KEY_LEFT)
            f->choice = (f->choice + f->option_count - 1) % f->option_count;
        else if (key == KEY_RIGHT)
            f->choice = (f->choice + 1) % f->option_count;
        else return false;
        return true;
    }

    if (key == KEY_DEL) {
        if (n) f->text[n - 1] = '\0';
        return true;
    }
    if (key == KEY_ACON) {
        f->text[0] = '\0';
        return true;
    }
    if (d >= 0) {
        append(f, '0' + d, maxlen);
        return true;
    }

    if (f->kind == F_CODE) {
        if (key == KEY_DOT && !f->digits_only && !strchr(f->text, 'R')) {
            append(f, 'R', maxlen);
            return true;
        }
        return false;
    }

    /* F_NUM */
    if (key == KEY_DOT) {
        append(f, '.', maxlen);
        return true;
    }
    if (key == KEY_EXP) {
        if (!strchr(f->text, 'E')) append(f, 'E', maxlen);
        return true;
    }
    if (key == KEY_NEG) {
        if (n == 0 || f->text[n - 1] == 'E') append(f, '-', maxlen);
        return true;
    }
    if (key >= KEY_F1 && key <= KEY_F6) {
        char c = fkey_prefixes[key - KEY_F1];
        if (n && is_prefix(f->text[n - 1])) f->text[n - 1] = c;
        else {
            for (int i = 0; i < n; i++)
                if (is_prefix(f->text[i]) || f->text[i] == 'E') return true;
            append(f, c, maxlen);
        }
        return true;
    }
    return false;
}

void field_box(field_t const *f, int x, int y, int w, bool selected,
    int color)
{
    int border = selected ? color : C_LIGHT;
    int fg = f->disabled ? C_LIGHT : C_BLACK;

    drect_border(x, y, x + w - 1, y + 14, C_WHITE, selected ? 2 : 1, border);

    if (f->kind == F_CHOICE) {
        char const *opt = f->options[f->choice];
        etext(x + w / 2, y + 3, fg, DTEXT_CENTER, false, opt);
        if (selected) {
            dtext(x + 5, y + 3, color, "<");
            dtext_opt(x + w - 5, y + 3, color, C_NONE, DTEXT_RIGHT,
                DTEXT_TOP, ">");
        }
        return;
    }

    if (f->kind == F_NUM && f->text[0] && isnan(field_value(f)))
        fg = C_RED;
    etext(x + 5, y + 3, fg, DTEXT_LEFT, false, f->text);
    if (selected) {
        int cx = x + 5 + etext_width(f->text, false) + (f->text[0] ? 2 : 0);
        drect(cx, y + 3, cx + 1, y + 11, color);
    }
}

void field_fkeys(field_t const *f, int color)
{
    if (f->kind != F_NUM) return;
    char label[2] = { 0, 0 };
    for (int i = 0; i < 6; i++) {
        label[0] = fkey_prefixes[i];
        efkey(i + 1, label, color);
    }
}

//---
//  Forms
//---

#define FORM_Y      21
#define FORM_STEP   17
#define UNIT_W      22

static int form_rows(form_t const *f)
{
    return (f->count + f->cols - 1) / f->cols;
}

static void form_draw(form_t *f)
{
    int rows = form_rows(f);
    int colw = DWIDTH / f->cols;

    dclear(C_WHITE);
    info_title(f->title, f->color);

    for (int i = 0; i < f->count; i++) {
        field_t const *fd = &f->fields[i];
        int x = (i / rows) * colw;
        int y = FORM_Y + (i % rows) * FORM_STEP;
        int bx = x + f->label_w;
        int bw = colw - f->label_w - 8 - (fd->kind == F_NUM ? UNIT_W : 0);
        bool sel = (i == f->sel);

        dtext(x + 8, y + 3, fd->disabled ? C_LIGHT : sel ? f->color
            : C_BLACK, fd->label);
        field_box(fd, bx, y, bw, sel, f->color);
        if (fd->kind == F_NUM)
            etext(bx + bw + 4, y + 3, fd->disabled ? C_LIGHT : C_DARK,
                DTEXT_LEFT, false, fd->unit);
    }

    int ry = FORM_Y + rows * FORM_STEP + 2;
    dline(6, ry, DWIDTH - 7, ry, C_LIGHT);
    f->results(f, ry + 5);

    field_fkeys(&f->fields[f->sel], f->color);
    dupdate();
}

/* Next enabled field in direction dir (wraps around) */
static void form_move(form_t *f, int dir)
{
    for (int i = 0; i < f->count; i++) {
        f->sel = (f->sel + dir + f->count) % f->count;
        if (!f->fields[f->sel].disabled) return;
    }
}

void form_run(form_t *f)
{
    if (f->changed) f->changed(f, -1);

    while (1) {
        form_draw(f);

        int key = getkey().key;
        field_t *fd = &f->fields[f->sel];
        int rows = form_rows(f);

        if (key == KEY_EXIT) return;
        if (key == INFO_KEY) {
            info_show("Electronics", f->title, f->color);
            continue;
        }
        if (key == KEY_UP) { form_move(f, -1); continue; }
        if (key == KEY_DOWN || key == KEY_EXE) { form_move(f, +1); continue; }

        /* Left/right switch columns, except on choice fields */
        if ((key == KEY_LEFT || key == KEY_RIGHT) && fd->kind != F_CHOICE) {
            int to = f->sel + (key == KEY_LEFT ? -rows : rows);
            if (to >= 0 && to < f->count && !f->fields[to].disabled)
                f->sel = to;
            continue;
        }

        if (field_key(fd, key) && f->changed) f->changed(f, f->sel);
    }
}

//---
//  Results
//---

void result(int y, char const *label, int color, char const *fmt, ...)
{
    char buf[96];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof buf, fmt, args);
    va_end(args);

    /* Values line up at x = 100 unless the label is longer */
    int x = 10 + etext_width(label, false) + 8;
    etext(10, y, C_DARK, DTEXT_LEFT, false, label);
    etext(x > 100 ? x : 100, y, color, DTEXT_LEFT, false, buf);
}

void result_note(int y, char const *fmt, ...)
{
    char buf[96];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof buf, fmt, args);
    va_end(args);

    etext(DWIDTH - 10, y, C_DARK, DTEXT_RIGHT, false, buf);
}
