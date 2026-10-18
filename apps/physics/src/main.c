#include <gint/display.h>
#include <gint/keyboard.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "utils.h"
#include "info.h"
#include "version.h"
#include "i18n.h"
#include "fmt.h"
#include "ui.h"
#include "units.h"

#define COLOR   C_RGB(20, 10, 2)    /* warm orange */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define G0      9.80665             /* standard gravity */
#define DEG     (M_PI / 180.0)

//---
//  Generic numeric form (up to 8 fields; results callback draws below)
//---

typedef struct form form_t;
struct form {
    char const *title_en, *title_pt;
    int color;
    struct {
        char const *label_en, *label_pt, *unit;
        numfield_t f;
    } fields[8];
    int count;
    int sel;
    int cols;                       /* 1 or 2 column layout */
    void (*results)(form_t *, int y);
};

static double fv(form_t *f, int i) { return numfield_value(&f->fields[i].f); }

static void form_draw(form_t *f)
{
    dclear(C_WHITE);
    info_title(lang_current == L_PT ? f->title_pt : f->title_en, f->color);

    int rows = (f->count + f->cols - 1) / f->cols;
    int colw = DWIDTH / f->cols;
    for (int i = 0; i < f->count; i++) {
        int x = (i / rows) * colw;
        int y = 24 + (i % rows) * 20;
        bool sel = i == f->sel;
        char const *lab = lang_current == L_PT ? f->fields[i].label_pt
            : f->fields[i].label_en;
        int bw = f->cols == 2 ? colw - 90 : 170;
        dtext(x + 10, y + 3, sel ? f->color : C_BLACK, lab);
        numfield_draw(&f->fields[i].f, x + 54, y, bw, sel, f->color);
        dtext(x + 54 + bw + 4, y + 3, C_DARK, f->fields[i].unit);
    }

    int ry = 24 + rows * 20 + 6;
    dline(6, ry - 4, DWIDTH - 7, ry - 4, C_LIGHT);
    f->results(f, ry);
    dupdate();
}

static void form_run(form_t *f)
{
    while (1) {
        form_draw(f);
        int key = getkey().key;
        int rows = (f->count + f->cols - 1) / f->cols;
        if (key == KEY_EXIT) return;
        if (key == INFO_KEY) {
            info_show("Physics", lang_current == L_PT ? f->title_pt
                : f->title_en, f->color);
            continue;
        }
        if (key == KEY_UP) f->sel = (f->sel + f->count - 1) % f->count;
        else if (key == KEY_DOWN || key == KEY_EXE)
            f->sel = (f->sel + 1) % f->count;
        else if (key == KEY_LEFT && f->sel - rows >= 0) f->sel -= rows;
        else if (key == KEY_RIGHT && f->sel + rows < f->count) f->sel += rows;
        else numfield_key(&f->fields[f->sel].f, key);
    }
}

/* A result line: label at x=10, value at x=140 */
static void res(int y, char const *label, int color, char const *fmt, ...)
{
    char buf[48];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    dtext(10, y, C_DARK, label);
    dtext(150, y, color, buf);
}

//---
//  Unit converter (same as the Chemistry app)
//---

static void units_screen(void)
{
    int cat = 0, from = 1;
    numfield_t val = { "1" };

    while (1) {
        category_t const *c = &categories[cat];
        dclear(C_WHITE);
        info_title(lang_current == L_PT ? c->name_pt : c->name, COLOR);
        dtext(10, 20, C_DARK, L("Value", "Valor"));
        numfield_draw(&val, 70, 17, 150, true, COLOR);
        dtext(226, 20, COLOR, c->units[from].name);
        dline(6, 36, DWIDTH - 7, 36, C_LIGHT);
        dtext_opt(DWIDTH / 2, 209, C_DARK, C_NONE, DTEXT_CENTER, DTEXT_TOP,
            L("<> category   ^v input unit", "<> categoria   ^v unidade"));

        double x = numfield_value(&val);
        for (int i = 0; i < c->count; i++) {
            int y = 42 + i * 14;
            bool sel = i == from;
            if (sel) drect(4, y - 1, DWIDTH - 9, y + 11, C_RGB(28, 29, 31));
            dtext(14, y, sel ? COLOR : C_BLACK, c->units[i].name);
            char b[32];
            if (isnan(x)) strcpy(b, "-");
            else fmt_g(b, unit_convert(c, from, i, x), 7);
            dtext(120, y, sel ? COLOR : C_BLACK, b);
        }
        dupdate();

        int key = getkey().key;
        if (key == KEY_EXIT) return;
        if (key == INFO_KEY) { info_show("Physics",
            L("Unit converter", "Conversor de unidades"), COLOR); continue; }
        if (key == KEY_LEFT) { cat = (cat + category_count - 1)
            % category_count; from = 0; }
        else if (key == KEY_RIGHT) { cat = (cat + 1) % category_count;
            from = 0; }
        else if (key == KEY_UP) from = (from + c->count - 1) % c->count;
        else if (key == KEY_DOWN) from = (from + 1) % c->count;
        else numfield_key(&val, key);
    }
}

//---
//  Kinematics: SUVAT by constraint propagation
//  vars: 0 s, 1 u, 2 v, 3 a, 4 t
//---

static bool kn(double *val, bool *known, int i, double v)
{
    if (known[i] || isnan(v) || isinf(v)) return false;
    val[i] = v; known[i] = true;
    return true;
}

static void suvat_solve(double *x, bool *k)
{
    double s, u, v, a, t;
    for (int iter = 0; iter < 8; iter++) {
        bool prog = false;
        s = x[0]; u = x[1]; v = x[2]; a = x[3]; t = x[4];

        /* v = u + a t */
        if (k[1] && k[3] && k[4]) prog |= kn(x, k, 2, u + a * t);
        if (k[2] && k[3] && k[4]) prog |= kn(x, k, 1, v - a * t);
        if (k[1] && k[2] && k[4] && t != 0) prog |= kn(x, k, 3, (v - u) / t);
        if (k[1] && k[2] && k[3] && a != 0) prog |= kn(x, k, 4, (v - u) / a);
        /* s = (u + v)/2 * t */
        if (k[1] && k[2] && k[4]) prog |= kn(x, k, 0, (u + v) / 2 * t);
        if (k[0] && k[1] && k[2] && (u + v) != 0)
            prog |= kn(x, k, 4, 2 * s / (u + v));
        if (k[0] && k[2] && k[4] && t != 0)
            prog |= kn(x, k, 1, 2 * s / t - v);
        if (k[0] && k[1] && k[4] && t != 0)
            prog |= kn(x, k, 2, 2 * s / t - u);
        /* s = u t + 1/2 a t^2 */
        if (k[1] && k[3] && k[4]) prog |= kn(x, k, 0, u * t + 0.5 * a * t * t);
        if (k[0] && k[3] && k[4] && t != 0)
            prog |= kn(x, k, 1, (s - 0.5 * a * t * t) / t);
        if (k[0] && k[1] && k[4] && t != 0)
            prog |= kn(x, k, 3, 2 * (s - u * t) / (t * t));
        /* v^2 = u^2 + 2 a s */
        if (k[1] && k[3] && k[0] && (u * u + 2 * a * s) >= 0)
            prog |= kn(x, k, 2, sqrt(u * u + 2 * a * s));
        if (k[2] && k[3] && k[0] && (v * v - 2 * a * s) >= 0)
            prog |= kn(x, k, 1, sqrt(v * v - 2 * a * s));
        if (k[1] && k[2] && k[0] && s != 0)
            prog |= kn(x, k, 3, (v * v - u * u) / (2 * s));
        if (k[1] && k[2] && k[3] && a != 0)
            prog |= kn(x, k, 0, (v * v - u * u) / (2 * a));
        if (!prog) break;
    }
}

static void suvat_results(form_t *f, int y)
{
    char const * const names[5] = { "s", "u", "v", "a", "t" };
    char const * const units_[5] = { "m", "m/s", "m/s", "m/s2", "s" };
    double x[5]; bool k[5];
    int given = 0;

    for (int i = 0; i < 5; i++) {
        x[i] = fv(f, i);
        k[i] = !isnan(x[i]);
        if (k[i]) given++;
    }
    if (given < 3) {
        dtext(10, y, C_DARK, L("Enter any 3 values",
            "Introduza 3 valores"));
        return;
    }
    suvat_solve(x, k);

    char b[48], num[32];
    int line = 0;
    for (int i = 0; i < 5; i++) {
        /* Show only the values that were solved (not user-entered) */
        if (fv(f, i) == fv(f, i)) continue;     /* skip given (non-NaN) */
        if (k[i]) {
            sprintf(b, "%s = %s %s", names[i], fmt_g(num, x[i], 6),
                units_[i]);
            dtext(10, y + line * 14, COLOR, b);
        } else {
            sprintf(b, "%s: %s", names[i], L("no real solution",
                "sem solucao real"));
            dtext(10, y + line * 14, C_RED, b);
        }
        line++;
    }
    if (line == 0) dtext(10, y, C_DARK, L("All values known",
        "Todos conhecidos"));
}

static void kinematics_screen(void)
{
    static form_t f = {
        .title_en = "Kinematics (SUVAT)", .title_pt = "Cinematica (SUVAT)",
        .color = COLOR, .count = 5, .cols = 1,
        .fields = {
            { "s", "s", "m" }, { "u", "u", "m/s" }, { "v", "v", "m/s" },
            { "a", "a", "m/s2" }, { "t", "t", "s" },
        },
        .results = suvat_results,
    };
    form_run(&f);
}

//---
//  Dynamics: F = ma, weight, momentum
//---

static void dyn_results(form_t *f, int y)
{
    double m = fv(f, 0), a = fv(f, 1), v = fv(f, 2), g = fv(f, 3);
    char b[32];
    if (isnan(g)) g = G0;

    if (!isnan(m) && !isnan(a)) res(y, "F = m a", COLOR, "%s N",
        fmt_g(b, m * a, 6));
    else res(y, "F = m a", C_DARK, L("need m, a", "falta m, a"));
    y += 16;
    if (!isnan(m)) res(y, "W = m g", C_BLACK, "%s N", fmt_g(b, m * g, 6));
    y += 16;
    if (!isnan(m) && !isnan(v)) res(y, "p = m v", C_BLACK, "%s kg m/s",
        fmt_g(b, m * v, 6));
    y += 22;
    dtext(10, y, C_DARK, "F = m a,  W = m g,  p = m v");
    dprint(10, y + 14, C_DARK, "g = %s m/s2", fmt_g(b, g, 5));
}

static void dynamics_screen(void)
{
    static form_t f = {
        .title_en = "Dynamics", .title_pt = "Dinamica",
        .color = COLOR, .count = 4, .cols = 1,
        .fields = {
            { "m", "m", "kg" }, { "a", "a", "m/s2" }, { "v", "v", "m/s" },
            { "g", "g", "m/s2" },
        },
        .results = dyn_results,
    };
    form_run(&f);
}

//---
//  Energy, work and power
//---

static void energy_results(form_t *f, int y)
{
    double m = fv(f, 0), v = fv(f, 1), h = fv(f, 2), F = fv(f, 3),
        d = fv(f, 4), t = fv(f, 5), g = fv(f, 6);
    char b[32];
    if (isnan(g)) g = G0;

    if (!isnan(m) && !isnan(v)) res(y, "KE = 1/2 m v2", COLOR, "%s J",
        fmt_g(b, 0.5 * m * v * v, 6));
    y += 16;
    if (!isnan(m) && !isnan(h)) res(y, "PE = m g h", C_BLACK, "%s J",
        fmt_g(b, m * g * h, 6));
    y += 16;
    double W = NAN;
    if (!isnan(F) && !isnan(d)) { W = F * d;
        res(y, "W = F d", C_BLACK, "%s J", fmt_g(b, W, 6)); }
    y += 16;
    if (!isnan(W) && !isnan(t) && t != 0) res(y, "P = W / t", C_BLACK,
        "%s W", fmt_g(b, W / t, 6));
}

static void energy_screen(void)
{
    static form_t f = {
        .title_en = "Energy & power", .title_pt = "Energia e potencia",
        .color = COLOR, .count = 7, .cols = 2,
        .fields = {
            { "m", "m", "kg" }, { "v", "v", "m/s" }, { "h", "h", "m" },
            { "F", "F", "N" }, { "d", "d", "m" }, { "t", "t", "s" },
            { "g", "g", "m/s2" },
        },
        .results = energy_results,
    };
    form_run(&f);
}

//---
//  Waves and optics
//---

static void wave_results(form_t *f, int y)
{
    double fr = fv(f, 0), lam = fv(f, 1), n1 = fv(f, 2), th1 = fv(f, 3),
        n2 = fv(f, 4);
    char b[32];

    if (!isnan(fr) && !isnan(lam)) res(y, "v = f lambda", COLOR, "%s m/s",
        fmt_g(b, fr * lam, 6));
    y += 16;
    if (!isnan(fr) && fr != 0) res(y, "T = 1/f", C_BLACK, "%s s",
        fmt_g(b, 1 / fr, 6));
    y += 22;
    dtext(10, y, C_DARK, "Snell: n1 sin O1 = n2 sin O2");
    y += 16;
    if (!isnan(n1) && !isnan(th1) && !isnan(n2) && n2 != 0) {
        double s2 = n1 * sin(th1 * DEG) / n2;
        if (fabs(s2) <= 1) res(y, "O2", COLOR, "%s deg",
            fmt_g(b, asin(s2) / DEG, 6));
        else res(y, "O2", C_RED, L("total internal refl.",
            "reflexao total"));
    }
}

static void waves_screen(void)
{
    static form_t f = {
        .title_en = "Waves & optics", .title_pt = "Ondas e optica",
        .color = COLOR, .count = 5, .cols = 1,
        .fields = {
            { "f", "f", "Hz" }, { "lambda", "lambda", "m" },
            { "n1", "n1", "" }, { "angle1", "angulo1", "deg" },
            { "n2", "n2", "" },
        },
        .results = wave_results,
    };
    form_run(&f);
}

//---
//  Physical constants
//---

typedef struct { char const *name, *sym, *value, *unit; } constant_t;

static constant_t const constants[] = {
    { "Speed of light", "c", "2.99792458e8", "m/s" },
    { "Gravity (std)", "g", "9.80665", "m/s2" },
    { "Grav. constant", "G", "6.67430e-11", "N m2/kg2" },
    { "Planck", "h", "6.62607015e-34", "J s" },
    { "Reduced Planck", "h-bar", "1.054571817e-34", "J s" },
    { "Elementary charge", "e", "1.602176634e-19", "C" },
    { "Electron mass", "me", "9.1093837e-31", "kg" },
    { "Proton mass", "mp", "1.6726219e-27", "kg" },
    { "Avogadro", "NA", "6.02214076e23", "1/mol" },
    { "Boltzmann", "kB", "1.380649e-23", "J/K" },
    { "Gas constant", "R", "8.314462618", "J/mol/K" },
    { "Vacuum perm.", "e0", "8.8541878e-12", "F/m" },
    { "Vacuum mag.", "u0", "1.25663706e-6", "H/m" },
    { "Coulomb", "k", "8.9875518e9", "N m2/C2" },
    { "Stefan-Boltz.", "sigma", "5.670374e-8", "W/m2/K4" },
    { "Atomic mass u", "u", "1.66053907e-27", "kg" },
};
#define CONST_COUNT (int)(sizeof constants / sizeof *constants)
#define CONST_ROWS  11

static void constants_screen(void)
{
    int sel = 0, top = 0;
    while (1) {
        dclear(C_WHITE);
        info_title(L("Constants", "Constantes"), COLOR);

        if (sel < top) top = sel;
        if (sel >= top + CONST_ROWS) top = sel - CONST_ROWS + 1;

        for (int r = 0; r < CONST_ROWS && top + r < CONST_COUNT; r++) {
            int i = top + r;
            int y = 22 + r * 16;
            bool s = i == sel;
            if (s) drect(4, y - 1, DWIDTH - 9, y + 13, C_RGB(28, 29, 31));
            dtext(10, y, s ? COLOR : C_BLACK, constants[i].name);
            dtext(150, y, s ? COLOR : C_DARK, constants[i].sym);
        }
        scrollbar_px(22, 22 + CONST_ROWS * 16, 0, CONST_COUNT, top,
            CONST_ROWS);

        int y = 200;
        dline(6, y - 4, DWIDTH - 7, y - 4, C_LIGHT);
        dprint(10, y, COLOR, "%s = %s %s", constants[sel].sym,
            constants[sel].value, constants[sel].unit);
        dupdate();

        int key = getkey().key;
        if (key == KEY_EXIT) return;
        if (key == INFO_KEY) info_show("Physics",
            L("Constants", "Constantes"), COLOR);
        else if (key == KEY_UP) sel = (sel + CONST_COUNT - 1) % CONST_COUNT;
        else if (key == KEY_DOWN) sel = (sel + 1) % CONST_COUNT;
    }
}

//---
//  Menu
//---

typedef struct {
    char const *name_en, *name_pt, *desc_en, *desc_pt;
    void (*run)(void);
} tool_t;

static tool_t const tools[] = {
    { "Unit converter", "Conversor de unidades",
      "Length, time, speed, energy...", "Comprimento, tempo, energia...",
      units_screen },
    { "Kinematics", "Cinematica", "SUVAT: enter any 3, solve the rest",
      "SUVAT: 3 valores, resolve o resto", kinematics_screen },
    { "Dynamics", "Dinamica", "F = ma, weight, momentum",
      "F = ma, peso, quantidade de mov.", dynamics_screen },
    { "Energy & power", "Energia e potencia", "KE, PE, work, power",
      "Ec, Ep, trabalho, potencia", energy_screen },
    { "Waves & optics", "Ondas e optica", "v = f lambda, Snell's law",
      "v = f lambda, lei de Snell", waves_screen },
    { "Constants", "Constantes", "c, g, G, h, e, NA, R...",
      "c, g, G, h, e, NA, R...", constants_screen },
};
#define TOOL_COUNT (int)(sizeof tools / sizeof *tools)

static void draw_menu(int sel)
{
    dclear(C_WHITE);
    info_title("Physics", COLOR);
    for (int i = 0; i < TOOL_COUNT; i++) {
        int y = 22 + i * 30;
        if (i == sel) drect(0, y, DWIDTH - 1, y + 27, C_RGB(28, 29, 31));
        drect(8, y + 4, 28, y + 24, COLOR);
        dprint_opt(18, y + 9, C_WHITE, C_NONE, DTEXT_CENTER, DTEXT_TOP,
            "%d", i + 1);
        dtext(38, y + 3, i == sel ? COLOR : C_BLACK,
            lang_current == L_PT ? tools[i].name_pt : tools[i].name_en);
        dtext(38, y + 15, C_DARK,
            lang_current == L_PT ? tools[i].desc_pt : tools[i].desc_en);
        if (i == sel) dtext_opt(DWIDTH - 10, y + 9, COLOR, C_NONE,
            DTEXT_RIGHT, DTEXT_TOP, ">");
    }
    dupdate();
}

int main(void)
{
    int sel = 0;
    lang_init();

    while (1) {
        draw_menu(sel);
        int key = getkey().key;
        int open = -1;
        if (key == KEY_EXIT) return 1;
        else if (key == INFO_KEY)
            info_show("Physics", L("Physics toolkit " APPS_VERSION,
                "Ferramentas de fisica " APPS_VERSION), COLOR);
        else if (key == KEY_UP) sel = (sel + TOOL_COUNT - 1) % TOOL_COUNT;
        else if (key == KEY_DOWN) sel = (sel + 1) % TOOL_COUNT;
        else if (key == KEY_EXE || key == KEY_RIGHT) open = sel;
        else switch (key) {
            case KEY_1: open = 0; break; case KEY_2: open = 1; break;
            case KEY_3: open = 2; break; case KEY_4: open = 3; break;
            case KEY_5: open = 4; break; case KEY_6: open = 5; break;
        }
        if (open >= 0 && open < TOOL_COUNT) { sel = open; tools[open].run(); }
    }
}
