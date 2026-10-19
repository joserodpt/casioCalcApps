#include <gint/display.h>
#include <gint/keyboard.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ui.h"
#include "eseries.h"
#include "tools.h"
#include "info.h"
#include "i18n.h"
#include "keys.h"

#define COLOR   C_RGB(20, 9, 3)

enum {
    BLACK, BROWN, RED, ORANGE, YELLOW, GREEN, BLUE, VIOLET, GREY, WHITE,
    GOLD, SILVER, NONE, COLOR_COUNT
};

static char const * const color_names[COLOR_COUNT] = {
    "Black", "Brown", "Red", "Orange", "Yellow", "Green", "Blue", "Violet",
    "Grey", "White", "Gold", "Silver", "None" };

static char const * const color_names_pt[COLOR_COUNT] = {
    "Preto", "Castanho", "Vermelho", "Laranja", "Amarelo", "Verde", "Azul",
    "Violeta", "Cinza", "Branco", "Ouro", "Prata", "Nenhuma" };

/* Translated colour name */
static char const *cname(int c)
{
    return lang_current == L_PT ? color_names_pt[c] : color_names[c];
}

#if !GINT_HW_FX
static int band_rgb(int c)
{
    switch (c) {
        case BLACK:  return C_RGB(1, 1, 1);
        case BROWN:  return C_RGB(15, 7, 2);
        case RED:    return C_RGB(29, 2, 2);
        case ORANGE: return C_RGB(31, 15, 0);
        case YELLOW: return C_RGB(31, 28, 0);
        case GREEN:  return C_RGB(2, 21, 4);
        case BLUE:   return C_RGB(3, 7, 28);
        case VIOLET: return C_RGB(17, 5, 23);
        case GREY:   return C_RGB(15, 15, 15);
        case WHITE:  return C_RGB(31, 31, 31);
        case GOLD:   return C_RGB(25, 19, 4);
        case SILVER: return C_RGB(23, 23, 25);
        default:     return C_NONE;
    }
}
#endif

/* Tolerance in %, temperature coefficient in ppm/K; 0 = not allowed */
static double const tolerance[COLOR_COUNT] = {
    [BROWN] = 1, [RED] = 2, [ORANGE] = 0.05, [YELLOW] = 0.02, [GREEN] = 0.5,
    [BLUE] = 0.25, [VIOLET] = 0.1, [GREY] = 0.01, [GOLD] = 5, [SILVER] = 10,
    [NONE] = 20 };

static int const tempco[COLOR_COUNT] = {
    [BLACK] = 250, [BROWN] = 100, [RED] = 50, [ORANGE] = 15, [YELLOW] = 25,
    [GREEN] = 20, [BLUE] = 10, [VIOLET] = 5, [GREY] = 1 };

typedef enum { ROLE_DIGIT, ROLE_MULT, ROLE_TOL, ROLE_TEMPCO } role_t;

#if !GINT_HW_FX
static char const * const role_names[] = {
    "Digit", "Multiplier", "Tolerance", "Tempco" };

static char const * const role_names_pt[] = {
    "Digito", "Multiplicador", "Tolerancia", "Tempco" };

static char const *rname(int r)
{
    return lang_current == L_PT ? role_names_pt[r] : role_names[r];
}
#endif

/* Colours each role can take, in up/down order */
static signed char const list_digit[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
static signed char const list_mult[] = {
    SILVER, GOLD, BLACK, BROWN, RED, ORANGE, YELLOW, GREEN, BLUE, VIOLET,
    GREY, WHITE };
static signed char const list_tol[] = {
    GREY, YELLOW, ORANGE, VIOLET, BLUE, GREEN, BROWN, RED, GOLD, SILVER,
    NONE };
static signed char const list_tempco[] = {
    GREY, VIOLET, BLUE, ORANGE, GREEN, YELLOW, RED, BROWN, BLACK };

typedef struct {
    int count;              /* 3 to 6 bands */
    int band[6];
    int sel;
    char note[64];          /* shown after a value was entered */
} cc_t;

static role_t role_of(cc_t const *s, int i)
{
    int digits = s->count >= 5 ? 3 : 2;
    if (i < digits) return ROLE_DIGIT;
    if (i == digits) return ROLE_MULT;
    if (i == digits + 1) return ROLE_TOL;
    return ROLE_TEMPCO;
}

static int digit_count(cc_t const *s)
{
    return s->count >= 5 ? 3 : 2;
}

static void role_list(cc_t const *s, role_t r, signed char const **list,
    int *n)
{
    switch (r) {
    case ROLE_DIGIT:
        *list = list_digit; *n = sizeof list_digit; break;
    case ROLE_MULT:
        *list = list_mult; *n = sizeof list_mult; break;
    case ROLE_TOL:
        /* No band (20%) only exists on 4-band resistors */
        *list = list_tol; *n = sizeof list_tol - (s->count != 4); break;
    default:
        *list = list_tempco; *n = sizeof list_tempco; break;
    }
}

static int mult_exp(int c)
{
    return c == GOLD ? -1 : c == SILVER ? -2 : c;
}

static double cc_value(cc_t const *s)
{
    int digits = digit_count(s);
    double n = 0;
    for (int i = 0; i < digits; i++) n = n * 10 + s->band[i];
    return n * pow(10, mult_exp(s->band[digits]));
}

static double cc_tolerance(cc_t const *s)
{
    return s->count == 3 ? 20 : tolerance[s->band[digit_count(s) + 1]];
}

/* Set the digit and multiplier bands from a value; returns the value the
   bands actually show */
static double cc_set_value(cc_t *s, double v)
{
    int digits = digit_count(s);
    long limit = digits == 2 ? 100 : 1000;

    if (!(v > 0)) v = 0;
    int e = v > 0 ? (int)floor(log10(v)) - (digits - 1) : 0;
    if (e < -2) e = -2;
    if (e > 9) e = 9;
    long n = lround(v / pow(10, e));
    if (n >= limit) {
        if (e < 9) { n = lround(n / 10.0); e++; }
        else n = limit - 1;
    }

    for (int i = digits - 1; i >= 0; i--, n /= 10) s->band[i] = n % 10;
    s->band[digits] = e == -1 ? GOLD : e == -2 ? SILVER : e;
    return cc_value(s);
}

static void cc_set_count(cc_t *s, int count)
{
    double v = cc_value(s);
    int tol = s->count > 3 ? s->band[digit_count(s) + 1]
        : count == 4 ? GOLD : BROWN;

    s->count = count;
    cc_set_value(s, v);
    if (count >= 4) {
        if (tol == NONE && count != 4) tol = BROWN;
        s->band[digit_count(s) + 1] = tol;
    }
    if (count == 6) s->band[5] = BROWN;
    if (s->sel >= count) s->sel = count - 1;
}

/* Up/down: next colour in the band's list */
static void cc_cycle(cc_t *s, int dir)
{
    signed char const *list;
    int n, pos = 0;
    role_list(s, role_of(s, s->sel), &list, &n);
    for (int i = 0; i < n; i++) if (list[i] == s->band[s->sel]) pos = i;
    s->band[s->sel] = list[(pos + dir + n) % n];
}

//---
//  Drawing
//---

#if !GINT_HW_FX

#define BODY_X0   70
#define BODY_X1   326
#define BODY_Y    50        /* centre line */
#define BULB_H    22        /* half height at the ends */
#define NECK_H    16        /* half height in the middle */
#define BULB_R    30        /* half width of the end bulbs */

/* Half height of the body at column x */
static int body_half(int x)
{
    int h = NECK_H;
    int centres[2] = { BODY_X0 + BULB_R, BODY_X1 - BULB_R };
    for (int i = 0; i < 2; i++) {
        double dx = (double)(x - centres[i]) / BULB_R;
        if (fabs(dx) <= 1) {
            int b = (int)(BULB_H * sqrt(1 - dx * dx));
            if (b > h) h = b;
        }
    }
    /* The outer halves of the bulbs are the rounded ends */
    if (x < BODY_X0 + BULB_R || x > BODY_X1 - BULB_R) {
        double dx = x < BODY_X0 + BULB_R
            ? (double)(BODY_X0 + BULB_R - x) / BULB_R
            : (double)(x - BODY_X1 + BULB_R) / BULB_R;
        h = dx <= 1 ? (int)(BULB_H * sqrt(1 - dx * dx)) : 0;
    }
    return h;
}

/* Centre of band i */
static int band_x(cc_t const *s, int i)
{
    int values = digit_count(s) + 1;
    if (i < values) return BODY_X0 + 40 + 26 * i;
    if (s->count == 6) return i == values ? BODY_X1 - 52 : BODY_X1 - 26;
    return BODY_X1 - 44;
}

static void draw_resistor(cc_t const *s)
{
    int body = s->count >= 5 ? C_RGB(13, 21, 28) : C_RGB(29, 25, 17);
    int shine = s->count >= 5 ? C_RGB(20, 27, 31) : C_RGB(31, 29, 23);
    int outline = C_RGB(10, 10, 10);

    /* Leads */
    drect(12, BODY_Y - 1, DWIDTH - 13, BODY_Y + 1, C_RGB(18, 18, 19));

    for (int x = BODY_X0; x <= BODY_X1; x++) {
        int h = body_half(x);
        if (h <= 0) continue;
        int fill = body;
        for (int i = 0; i < s->count; i++) {
            int bx = band_x(s, i);
            if (x >= bx - 5 && x <= bx + 5 && s->band[i] != NONE)
                fill = band_rgb(s->band[i]);
        }
        dline(x, BODY_Y - h, x, BODY_Y + h, fill);
        if (fill == body) dpixel(x, BODY_Y - h + 4, shine);
        dpixel(x, BODY_Y - h, outline);
        dpixel(x, BODY_Y + h, outline);
    }
    /* Close the rounded ends */
    dline(BODY_X0, BODY_Y - 2, BODY_X0, BODY_Y + 2, outline);
    dline(BODY_X1, BODY_Y - 2, BODY_X1, BODY_Y + 2, outline);

    /* Marker under the selected band */
    int mx = band_x(s, s->sel);
    int my = BODY_Y + body_half(mx) + 3;
    int px[3] = { mx, mx - 5, mx + 5 };
    int py[3] = { my, my + 6, my + 6 };
    dpoly(px, py, 3, COLOR, COLOR);
}

static void line_nearest(int y, int series, double v)
{
    char a[32], b[16];
    double e = eseries_nearest(series, v, false);
    char label[8];
    sprintf(label, "E%d", series);
    if (isnan(e)) { result(y, label, C_BLACK, "-"); return; }
    result(y, label, C_BLACK, "%s", fmt_si(a, e, S_OHM, 3));
    result_note(y, "%s", fmt_dev(b, e, v));
}

static void draw_info(cc_t const *s)
{
    char a[32], b[32], text[64];
    double v = cc_value(s);
    double tol = cc_tolerance(s);
    int y = 88;

    /* Value and tolerance */
    fmt_si(a, v, S_OHM, 3);
    snprintf(text, sizeof text, "%s  " S_PM "%s%%", a, fmt_num(b, tol, 2));
    etext(10, y, C_BLACK, DTEXT_LEFT, true, text);

    /* Selected band */
    role_t r = role_of(s, s->sel);
    int c = s->band[s->sel];
    eprint(DWIDTH - 10, y - 1, C_DARK, DTEXT_RIGHT, L("Band %d/%d: %s",
        "Banda %d/%d: %s"), s->sel + 1, s->count, rname(r));
    if (r == ROLE_DIGIT) snprintf(text, sizeof text, "%s = %d",
        cname(c), c);
    else if (r == ROLE_MULT) {
        fmt_si(a, pow(10, mult_exp(c)), "", 3);
        char *sp = strchr(a, ' ');     /* "1 k" -> "1k" */
        if (sp) memmove(sp, sp + 1, strlen(sp));
        snprintf(text, sizeof text, "%s = " S_TIMES "%s", cname(c), a);
    }
    else if (r == ROLE_TOL) snprintf(text, sizeof text, "%s = " S_PM "%s%%",
        cname(c), fmt_num(a, tolerance[c], 2));
    else snprintf(text, sizeof text, "%s = %d ppm/K", cname(c),
        tempco[c]);
    etext(DWIDTH - 10, y + 11, C_BLACK, DTEXT_RIGHT, false, text);

    y = 114;
    dline(6, y - 4, DWIDTH - 7, y - 4, C_LIGHT);
    result(y, L("Range","Gama"), C_BLACK, "%s ... %s",
        fmt_si(a, v * (1 - tol / 100), "", 4),
        fmt_si(b, v * (1 + tol / 100), S_OHM, 4));
    y += RESULT_STEP;
    if (s->count == 6) {
        result(y, "Tempco", C_BLACK, "%d ppm/K", tempco[s->band[5]]);
        y += RESULT_STEP;
    }
    line_nearest(y, 12, v);
    line_nearest(y + RESULT_STEP, 24, v);
    line_nearest(y + 2 * RESULT_STEP, 96, v);
    y += 3 * RESULT_STEP;
    if (s->note[0]) etext(10, y, COLOR, DTEXT_LEFT, false, s->note);
}

static void draw(cc_t const *s)
{
    char label[8];

    dclear(C_WHITE);
    info_title(L("Colour code", "Codigo de cores"), COLOR);
    draw_resistor(s);
    draw_info(s);

    sprintf(label, L("%d BAND","%d BND"), s->count);
    efkey(1, label, COLOR);
    efkey(2, L("VALUE","VALOR"), C_BLACK);
    efkey(4, "E12", C_BLACK);
    efkey(5, "E24", C_BLACK);
    efkey(6, "E96", C_BLACK);
}

#else /* GINT_HW_FX */

/* fx-9860G: four greys cannot show the colours, so the bands are 3-letter
   names on a resistor body; the selected band is white on black */

static char const * const color_codes[COLOR_COUNT] = {
    "blk", "brn", "red", "orn", "yel", "grn", "blu", "vio", "gry", "wht",
    "gld", "slv", "---" };

static char const * const color_codes_pt[COLOR_COUNT] = {
    "pre", "cas", "vrm", "lar", "ama", "vrd", "azu", "vio", "cin", "bra",
    "our", "pra", "---" };

#define CELL_W    19        /* 3 characters and a margin */
#define BAND_Y    10        /* top of the body */

/* Left edge of band i, value bands together and the others apart */
static int band_x(cc_t const *s, int i)
{
    int values = digit_count(s) + 1;
    int x = 0, left = 0;
    for (int k = 0; k < s->count; k++) {
        if (k == i) left = x;
        x += CELL_W + (k == values - 1 ? 3 : 1);
    }
    /* Without the gap after the last band */
    int w = x - (s->count == values ? 3 : 1);
    return (DWIDTH - w) / 2 + left;
}

static void draw_resistor(cc_t const *s)
{
    int x0 = band_x(s, 0) - 2;
    int x1 = band_x(s, s->count - 1) + CELL_W + 1;

    /* Leads, then the body */
    dline(0, BAND_Y + 4, DWIDTH - 1, BAND_Y + 4, C_DARK);
    dline(0, BAND_Y + 5, DWIDTH - 1, BAND_Y + 5, C_DARK);
    drect_border(x0, BAND_Y, x1, BAND_Y + 9, C_LIGHT, 1, C_BLACK);
    dpixel(x0, BAND_Y, C_WHITE);
    dpixel(x0, BAND_Y + 9, C_WHITE);
    dpixel(x1, BAND_Y, C_WHITE);
    dpixel(x1, BAND_Y + 9, C_WHITE);

    for (int i = 0; i < s->count; i++) {
        int x = band_x(s, i);
        bool sel = i == s->sel;
        int c = s->band[i];
        drect(x, BAND_Y + 1, x + CELL_W - 1, BAND_Y + 8,
            sel ? C_BLACK : C_WHITE);
        dtext(x + 1, BAND_Y + 2, sel ? C_WHITE : C_BLACK,
            lang_current == L_PT ? color_codes_pt[c] : color_codes[c]);
    }
}

/* "E12 4.7kΩ (exact)" */
static void line_nearest(int y, int series, double v)
{
    char a[32], b[16];
    double e = eseries_nearest(series, v, false);
    eprint(1, y, C_DARK, DTEXT_LEFT, "E%d", series);
    if (isnan(e)) { etext(22, y, C_BLACK, DTEXT_LEFT, false, "-"); return; }
    fmt_si(a, e, S_OHM, 3);
    etext(22, y, C_BLACK, DTEXT_LEFT, false, a);
    etext(DWIDTH - 1, y, C_DARK, DTEXT_RIGHT, false, fmt_dev(b, e, v));
}

static void draw_info(cc_t const *s)
{
    char a[32], b[32], text[64], what[32];
    double v = cc_value(s);
    double tol = cc_tolerance(s);
    int y = BAND_Y + 11;

    /* Value and tolerance */
    fmt_si(a, v, S_OHM, 3);
    snprintf(text, sizeof text, "%s " S_PM "%s%%", a, fmt_num(b, tol, 2));
    etext(1, y, C_BLACK, DTEXT_LEFT, false, text);
    int left = 1 + etext_width(text, false) + 5;

    /* The selected band, at the right if there is room: "Red " S_TIMES "100" */
    role_t r = role_of(s, s->sel);
    int c = s->band[s->sel];
    if (r == ROLE_DIGIT) snprintf(what, sizeof what, "%d", c);
    else if (r == ROLE_MULT) {
        fmt_si(a, pow(10, mult_exp(c)), "", 3);
        char *sp = strchr(a, ' ');
        if (sp) memmove(sp, sp + 1, strlen(sp));
        snprintf(what, sizeof what, S_TIMES "%s", a);
    }
    else if (r == ROLE_TOL) snprintf(what, sizeof what, S_PM "%s%%",
        fmt_num(a, tolerance[c], 2));
    else snprintf(what, sizeof what, "%dppm", tempco[c]);
    snprintf(text, sizeof text, "%s %s", cname(c), what);
    if (left + etext_width(text, false) > DWIDTH - 1) strcpy(text, what);
    if (left + etext_width(text, false) <= DWIDTH - 1)
        etext(DWIDTH - 1, y, C_DARK, DTEXT_RIGHT, false, text);

    y += 2;
    dline(0, y + 6, DWIDTH - 1, y + 6, C_LIGHT);
    y += 8;
    line_nearest(y, 12, v);
    line_nearest(y + RESULT_STEP, 24, v);
    /* After a value was typed: what it was, instead of E96 */
    if (s->note[0]) etext(1, y + 2 * RESULT_STEP, C_BLACK, DTEXT_LEFT, false,
        s->note);
    else line_nearest(y + 2 * RESULT_STEP, 96, v);
}

static void draw(cc_t const *s)
{
    char label[8];

    dclear(C_WHITE);
    info_title(L("Colour code", "Codigo de cores"), COLOR);
    draw_resistor(s);
    draw_info(s);

    sprintf(label, "%dB", s->count);
    efkey(1, label, C_BLACK);
    efkey(2, L("VAL","VAL"), C_BLACK);
    efkey(4, "E12", C_BLACK);
    efkey(5, "E24", C_BLACK);
    efkey(6, "E96", C_BLACK);
}

#endif /* GINT_HW_FX */

//---
//  Value entry
//---

static void set_from_value(cc_t *s, double v)
{
    char a[32], b[32];
    double shown = cc_set_value(s, v);
#if GINT_HW_FX
    (void)b;
#endif

    if (fabs(shown - v) > v * 1e-9)
#if GINT_HW_FX
        snprintf(s->note, sizeof s->note, L("Typed %s","Pedido %s"),
            fmt_si(a, v, S_OHM, 4));
#else
        snprintf(s->note, sizeof s->note, L("%s shown as %s","%s mostrado %s"), fmt_si(a, v,
            S_OHM, 4), fmt_si(b, shown, S_OHM, 4));
#endif
    else s->note[0] = '\0';
}

/* Prompt for a value at the bottom of the screen. The bands follow the value
   as it is typed; EXE keeps them, EXIT puts the old ones back. */
static void prompt_value(cc_t *s)
{
    field_t f = { .kind = F_NUM, .label = "Value", .unit = S_OHM };
    field_set(&f, cc_value(s));
    cc_t live = *s;

    while (1) {
        draw(&live);
#if GINT_HW_FX
        /* Over the E-series lines; the bands and value stay in view */
        drect(0, 29, DWIDTH - 1, 55, C_WHITE);
        dline(0, 29, DWIDTH - 1, 29, C_BLACK);
        dtext(1, 34, C_BLACK, L("Value","Valor"));
        field_box(&f, 32, 33, 82, true, C_BLACK);
        etext(116, 34, C_DARK, DTEXT_LEFT, false, S_OHM);
        dtext(1, 45, C_DARK, L("EXE ok   EXIT cancel","EXE ok   EXIT anula"));
        field_fkeys(&f, C_BLACK);
#else
        drect(0, 172, DWIDTH - 1, 223, C_WHITE);
        dline(0, 172, DWIDTH - 1, 172, COLOR);
        dtext(10, 180, COLOR, L("Value","Valor"));
        field_box(&f, 60, 177, 160, true, COLOR);
        etext(224, 180, C_DARK, DTEXT_LEFT, false, S_OHM);
        dtext(250, 180, C_DARK, L("EXE: keep","EXE: manter"));
        dtext(250, 192, C_DARK, L("EXIT: cancel","EXIT: cancelar"));
        field_fkeys(&f, COLOR);
#endif
        dupdate();

        int key = getkey().key;
        if (key == KEY_EXIT) return;
        if (key == KEY_EXE) {
            if (!isnan(field_value(&f))) { *s = live; return; }
        }
        else if (field_key(&f, key)) {
            double v = field_value(&f);
            if (!isnan(v) && v >= 0) set_from_value(&live, v);
        }
    }
}

//---
//  Main loop
//---

void colorcode_run(void)
{
    static cc_t s = {
        .count = 4, .band = { YELLOW, VIOLET, RED, GOLD, BROWN, BROWN },
    };
    double v;

    while (1) {
        draw(&s);
        dupdate();

        int key = getkey().key;
        int d = key_digit(key);
        switch (key) {
        case KEY_EXIT:
            return;
        case INFO_KEY:
            info_app(L("Colour code", "Codigo de cores"), COLOR);
            break;
        case KEY_LEFT:
            s.sel = (s.sel + s.count - 1) % s.count;
            break;
        case KEY_RIGHT:
            s.sel = (s.sel + 1) % s.count;
            break;
        case KEY_UP:
        case KEY_DOWN:
            cc_cycle(&s, key == KEY_UP ? 1 : -1);
            s.note[0] = '\0';
            break;
        case KEY_F1:
            cc_set_count(&s, s.count == 6 ? 3 : s.count + 1);
            s.note[0] = '\0';
            break;
        case KEY_F2:
        case KEY_EXE:
            prompt_value(&s);
            break;
        case KEY_F4: case KEY_F5: case KEY_F6: {
            int series = key == KEY_F4 ? 12 : key == KEY_F5 ? 24 : 96;
            /* E96 values have three significant digits */
            if (series == 96 && s.count < 5) cc_set_count(&s, 5);
            v = eseries_nearest(series, cc_value(&s), false);
            if (!isnan(v)) set_from_value(&s, v);
            break; }
        }

        /* Digit keys pick the colour of digit and multiplier bands */
        role_t r = role_of(&s, s.sel);
        if (d >= 0 && (r == ROLE_DIGIT || r == ROLE_MULT)) {
            s.band[s.sel] = d;
            s.note[0] = '\0';
            if (s.sel < s.count - 1) s.sel++;
        }
    }
}
