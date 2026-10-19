#include <gint/display.h>
#include <gint/keyboard.h>
#include <math.h>
#include <stdio.h>

#include "utils.h"
#include "info.h"
#include "version.h"
#include "i18n.h"
#include "fmt.h"
#include "form.h"
#include "menu.h"
#include "units.h"
#include "usb_screen.h"

#define COLOR   C_RGB(20, 10, 2)    /* warm orange */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define G0      9.80665             /* standard gravity */
#define DEG     (M_PI / 180.0)

/* A result line: label at x=10, value at x=150 */
#define res(y, label, color, ...) \
    form_result(y, 150, label, color, __VA_ARGS__)

static double fv(form_t *f, int i) { return form_value(f, i); }

static void units_run(void) { units_screen(0, COLOR); }

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

typedef struct { char const *name, *name_pt, *sym, *value, *unit; } constant_t;

static constant_t const constants[] = {
    { "Speed of light in vacuum", "Velocidade da luz no vazio", "c", "2.99792458e8", "m/s" },
    { "Standard gravity", "Aceleracao da gravidade", "g", "9.80665", "m/s2" },
    { "Gravitational constant", "Constante gravitacional", "G", "6.67430e-11", "N m2/kg2" },
    { "Planck constant", "Constante de Planck", "h", "6.62607015e-34", "J s" },
    { "Reduced Planck constant", "Constante de Planck reduzida", "h-bar", "1.054571817e-34", "J s" },
    { "Elementary charge", "Carga elementar", "e", "1.602176634e-19", "C" },
    { "Electron mass", "Massa do eletrao", "me", "9.1093837e-31", "kg" },
    { "Proton mass", "Massa do protao", "mp", "1.6726219e-27", "kg" },
    { "Avogadro constant", "Constante de Avogadro", "NA", "6.02214076e23", "1/mol" },
    { "Boltzmann constant", "Constante de Boltzmann", "kB", "1.380649e-23", "J/K" },
    { "Molar gas constant", "Constante dos gases ideais", "R", "8.314462618", "J/mol/K" },
    { "Vacuum permittivity", "Permitividade do vazio", "e0", "8.8541878e-12", "F/m" },
    { "Vacuum permeability", "Permeabilidade do vazio", "u0", "1.25663706e-6", "H/m" },
    { "Coulomb constant", "Constante de Coulomb", "k", "8.9875518e9", "N m2/C2" },
    { "Stefan-Boltzmann constant", "Constante de Stefan-Boltzmann", "sigma", "5.670374e-8", "W/m2/K4" },
    { "Atomic mass unit", "Unidade de massa atomica", "u", "1.66053907e-27", "kg" },
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
            dtext(10, y, s ? COLOR : C_BLACK, lang_current == L_PT
                ? constants[i].name_pt : constants[i].name);
            dtext(310, y, s ? COLOR : C_DARK, constants[i].sym);
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
        if (key == INFO_KEY) info_app(L("Constants", "Constantes"), COLOR);
        else if (key == KEY_UP) sel = (sel + CONST_COUNT - 1) % CONST_COUNT;
        else if (key == KEY_DOWN) sel = (sel + 1) % CONST_COUNT;
    }
}

//---
//  Menu
//---

static menu_item_t const tools[] = {
    { "Unit converter", "Conversor de unidades",
      "Length, time, speed, energy...", "Comprimento, tempo, energia...",
      units_run, COLOR },
    { "Kinematics", "Cinematica", "SUVAT: enter any 3, solve the rest",
      "SUVAT: 3 valores, resolve o resto", kinematics_screen, COLOR },
    { "Dynamics", "Dinamica", "F = ma, weight, momentum",
      "F = ma, peso, quantidade de mov.", dynamics_screen, COLOR },
    { "Energy & power", "Energia e potencia", "KE, PE, work, power",
      "Ec, Ep, trabalho, potencia", energy_screen, COLOR },
    { "Waves & optics", "Ondas e optica", "v = f lambda, Snell's law",
      "v = f lambda, lei de Snell", waves_screen, COLOR },
    { "Constants", "Constantes", "c, g, G, h, e, NA, R...",
      "c, g, G, h, e, NA, R...", constants_screen, COLOR },
};

static menu_t const menu = {
    "Physics", "Fisica", "Physics toolkit " APPS_VERSION,
    "Ferramentas de fisica " APPS_VERSION, COLOR,
    tools, (int)(sizeof tools / sizeof *tools),
};

int main(void)
{
    lang_init();
    usb_screen_init();
    menu_run(&menu);
    return 1;
}
