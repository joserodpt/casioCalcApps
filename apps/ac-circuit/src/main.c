#include <gint/display.h>
#include <gint/keyboard.h>
#include <gint/timer.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ac.h"
#include "ui.h"
#include "utils.h"
#include "keys.h"
#include "info.h"
#include "version.h"
#include "i18n.h"
#include "usb_screen.h"

#define PI 3.14159265358979323846

#define COLOR     C_RGB(20, 3, 8)
#define TINT      C_RGB(30, 27, 28)     /* behind the part at the cursor */
#define RES_COLOR C_RGB(18, 8, 2)
#define DOT_COLOR C_RGB(31, 18, 0)      /* moving current dots */
#define GRID_DOT  C_RGB(22, 22, 22)
#define I_COLOR   C_RGB(0, 10, 26)      /* currents (ammeter, graphs) */
#define V_COLOR   C_RGB(22, 0, 16)      /* voltages (voltmeter, graphs) */
#define SRC_COLOR C_RGB(16, 16, 16)     /* the source, in the graphs */

/* Grid on screen: point (x, y) is at GX0 + x * STEP, GY0 + y * STEP */
#define GX0   22
#define GY0   30
#define STEP  32

#define FRAME_US    50000               /* animation frame */
#define ANIM_FRAMES 30                  /* frames per drawn AC cycle */
#define SAVE_PATH   "/ACCircuit.acc"    /* \\fls0\ACCircuit.acc */

static circuit_t circuit;
static result_t res;

/* Cursor in doubled coordinates: even/even is a point, odd/even a
   horizontal link, even/odd a vertical one */
static int cx = 2, cy = 3;
static bool drawing;                    /* F1 on a point: arrows lay wire */

//---
//  Text
//---

/* "1.2 V ∠-30°" */
static char *fmt_phasor(char *buf, cplx z, char const *unit, int sig)
{
    char m[32], d[32];
    fmt_si(m, c_abs(z), unit, sig);
    fmt_num(d, c_abs(z) > 0 ? c_deg(z) : 0, 0);
    sprintf(buf, "%s " S_ANGLE "%s" S_DEG, m, d);
    return buf;
}

/* Angle wrapped to -180..180 */
static double wrap_deg(double d)
{
    while (d > 180) d -= 360;
    while (d <= -180) d += 360;
    return d;
}

static char const *kind_name(int kind)
{
    switch (kind) {
    case P_WIRE: return L("Wire", "Fio");
    case P_RES: return L("Resistor", "Resistencia");
    case P_CAP: return L("Capacitor", "Condensador");
    case P_IND: return L("Inductor", "Bobina");
    case P_SRC: return L("AC source", "Fonte CA");
    case P_LAMP: return L("Lamp", "Lampada");
    case P_SWITCH: return L("Switch", "Interruptor");
    case P_AMMETER: return L("Ammeter", "Amperimetro");
    case P_VOLTMETER: return L("Voltmeter", "Voltimetro");
    }
    return "";
}

/* "0.87 lagging": the power factor of a complex power */
static char *fmt_pf(char *buf, cplx s)
{
    char a[32];
    double mag = c_abs(s);
    if (!(mag > 0)) return strcpy(buf, "-");
    fmt_num(a, fabs(s.re) / mag, 3);
    if (fabs(s.im) < 1e-6 * mag) strcpy(buf, a);
    else sprintf(buf, "%s %s", a, s.im > 0 ? L("lagging", "indutivo")
        : L("leading", "capacitivo"));
    return buf;
}

/* A part's voltage and current, from end a to end b; a source's from its
   - to its + terminal and out of its +, as a source is read */
static cplx part_v(result_t const *r, int e)
{
    part_t const *p = &circuit.parts[e];
    cplx v = across(r, e);
    return p->kind == P_SRC && p->flip ? c_scale(v, -1) : v;
}

static cplx part_i(result_t const *r, int e)
{
    part_t const *p = &circuit.parts[e];
    if (p->kind != P_SRC) return r->i[e];
    return p->flip ? r->i[e] : c_scale(r->i[e], -1);
}

//---
//  Cursor
//---

static int cursor_edge(void)
{
    if ((cx & 1) && !(cy & 1)) return (cy / 2) * (NX - 1) + (cx - 1) / 2;
    if (!(cx & 1) && (cy & 1)) return H_EDGES + ((cy - 1) / 2) * NX + cx / 2;
    return -1;
}

static int cursor_point(void)
{
    if ((cx & 1) || (cy & 1)) return -1;
    return (cy / 2) * NX + cx / 2;
}

static bool cursor_move(int dx, int dy)
{
    int nx = cx + dx, ny = cy + dy;
    /* odd/odd is the middle of a grid square: skip over it */
    if ((nx & 1) && (ny & 1)) { nx += dx; ny += dy; }
    if (nx < 0 || ny < 0 || nx > 2 * (NX - 1) || ny > 2 * (NY - 1))
        return false;
    cx = nx;
    cy = ny;
    return true;
}

//---
//  Drawing in a link's own frame: u along the link (0 at end a, STEP at
//  end b, reversed if flip), w across it
//---

typedef struct { int x, y; bool h, flip; } frame_t;

static frame_t frame_of(int e, bool flip)
{
    int a, b;
    edge_ends(e, &a, &b);
    frame_t f = { GX0 + (a % NX) * STEP, GY0 + (a / NX) * STEP,
        edge_horizontal(e), flip };
    return f;
}

static void fmap(frame_t const *f, int u, int w, int *x, int *y)
{
    if (f->flip) u = STEP - u;
    *x = f->h ? f->x + u : f->x + w;
    *y = f->h ? f->y + w : f->y + u;
}

static void fline(frame_t const *f, int u1, int w1, int u2, int w2, int c)
{
    int x1, y1, x2, y2;
    fmap(f, u1, w1, &x1, &y1);
    fmap(f, u2, w2, &x2, &y2);
    dline(x1, y1, x2, y2, c);
}

static void fbox(frame_t const *f, int u1, int w1, int u2, int w2, int fill,
    int border)
{
    int x1, y1, x2, y2;
    fmap(f, u1, w1, &x1, &y1);
    fmap(f, u2, w2, &x2, &y2);
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }
    if (border == C_NONE) drect(x1, y1, x2, y2, fill);
    else drect_border(x1, y1, x2, y2, fill, 1, border);
}

/* A 2-pixel lead from u1 to u2 */
static void flead(frame_t const *f, int u1, int u2, int c)
{
    fbox(f, u1, 0, u2, 1, c, C_NONE);
}

/* Sources have a + terminal: F5 again turns them around */
static bool polar(int kind)
{
    return kind == P_SRC;
}

static double lamp_brightness(int e)
{
    part_t const *p = &circuit.parts[e];
    if (res.status != S_OK) return 0;
    double b = res.s[e].re / p->value2;
    return b > 1 ? 1 : b < 0 ? 0 : b;
}

static void draw_part(int e)
{
    part_t const *p = &circuit.parts[e];
    frame_t f = frame_of(e, polar(p->kind) && p->flip);
    int ink = e == res.bad_edge ? C_RED : C_BLACK;
    int x, y;

    switch (p->kind) {
    case P_WIRE:
        flead(&f, 0, STEP, ink);
        break;

    case P_RES:
        flead(&f, 0, 7, ink);
        flead(&f, 25, STEP, ink);
        fbox(&f, 7, -5, 25, 6, C_RGB(31, 27, 20), RES_COLOR);
        fbox(&f, 8, -4, 24, 5, C_NONE, RES_COLOR);
        break;

    case P_CAP:
        /* Two plates across the link */
        flead(&f, 0, 13, ink);
        flead(&f, 19, STEP, ink);
        fbox(&f, 13, -9, 14, 10, ink, C_NONE);
        fbox(&f, 18, -9, 19, 10, ink, C_NONE);
        break;

    case P_IND:
        /* Three turns of the coil */
        flead(&f, 0, 7, ink);
        flead(&f, 25, STEP, ink);
        for (int k = 0; k < 3; k++) {
            int uc = 10 + 6 * k, pu = uc - 3, pw = 0;
            for (int t = 1; t <= 8; t++) {
                double a = PI * t / 8;
                int u = uc - (int)floor(3 * cos(a) + 0.5);
                int w = -(int)floor(5 * sin(a) + 0.5);
                fline(&f, pu, pw, u, w, ink);
                fline(&f, pu, pw + 1, u, w + 1, ink);
                pu = u;
                pw = w;
            }
        }
        break;

    case P_SRC:
        /* A circle with a sine wave; + marks the phase reference */
        flead(&f, 0, 7, ink);
        flead(&f, 25, STEP, ink);
        fmap(&f, 16, 0, &x, &y);
        if (f.h) y++; else x++;
        dcircle(x, y, 9, C_WHITE, ink);
        for (int dx = -5; dx < 5; dx++) {
            int y1 = y - (int)floor(3.5 * sin(PI * dx / 5) + 0.5);
            int y2 = y - (int)floor(3.5 * sin(PI * (dx + 1) / 5) + 0.5);
            dline(x + dx, y1, x + dx + 1, y2, COLOR);
        }
        fmap(&f, 5, -11, &x, &y);
        drect(x - 2, y, x + 2, y, C_RED);
        drect(x, y - 2, x, y + 2, C_RED);
        break;

    case P_LAMP: {
        double b = lamp_brightness(e);
        bool over = res.status == S_OK && res.s[e].re > 1.2 * p->value2;
        flead(&f, 0, 8, ink);
        flead(&f, 24, STEP, ink);
        fmap(&f, 16, 0, &x, &y);
        if (f.h) y++; else x++;
        if (b > 0.4) dcircle(x, y, 11, C_NONE, C_RGB(31, 27, 8));
        int fill = C_RGB(28 + (int)(3 * b), 28 + (int)(3 * b),
            28 - (int)(28 * b));
        dcircle(x, y, 8, fill, over ? C_RED : ink);
        dline(x - 5, y - 5, x + 5, y + 5, ink);
        dline(x - 5, y + 5, x + 5, y - 5, ink);
        break;
    }

    case P_SWITCH:
        flead(&f, 0, 7, ink);
        flead(&f, 25, STEP, ink);
        if (p->closed) {
            flead(&f, 7, 25, ink);
            fbox(&f, 7, -1, 25, 2, ink, C_NONE);
        }
        else for (int k = -1; k <= 1; k++)
            fline(&f, 7, k, 23, k - 10, ink);
        fmap(&f, 7, 0, &x, &y);
        dcircle(x + !f.h, y + f.h, 3, C_WHITE, ink);
        fmap(&f, 25, 0, &x, &y);
        dcircle(x + !f.h, y + f.h, 3, C_WHITE, ink);
        break;

    case P_AMMETER:
    case P_VOLTMETER: {
        int c = p->kind == P_AMMETER ? I_COLOR : V_COLOR;
        flead(&f, 0, 8, ink);
        flead(&f, 24, STEP, ink);
        fmap(&f, 16, 0, &x, &y);
        if (f.h) y++; else x++;
        dcircle(x, y, 8, C_WHITE, c);
        dtext_opt(x + 1, y + 1, c, C_NONE, DTEXT_CENTER, DTEXT_MIDDLE,
            p->kind == P_AMMETER ? "A" : "V");
        break;
    }
    }
}

/* Readings of the meters (rms), next to them on the schematic */
static void draw_meter_label(int e)
{
    part_t const *p = &circuit.parts[e];
    if (res.status != S_OK) return;
    if (p->kind != P_AMMETER && p->kind != P_VOLTMETER) return;

    char buf[32];
    double v = c_abs(p->kind == P_AMMETER ? res.i[e] : across(&res, e));
    fmt_si(buf, v, p->kind == P_AMMETER ? "A" : "V", 3);

    int color = p->kind == P_AMMETER ? I_COLOR : V_COLOR;
    frame_t f = frame_of(e, false);
    int x, y, w = etext_width(buf, false);
    fmap(&f, 16, 0, &x, &y);
    if (f.h) {
        drect(x - w / 2 - 1, y - 21, x + w / 2 + 1, y - 11, C_WHITE);
        etext(x, y - 20, color, DTEXT_CENTER, false, buf);
    }
    else if (x + 12 + w < DWIDTH) {
        drect(x + 11, y - 5, x + 13 + w, y + 5, C_WHITE);
        etext(x + 12, y - 4, color, DTEXT_LEFT, false, buf);
    }
    else {
        drect(x - 11 - w, y - 5, x - 9, y + 5, C_WHITE);
        etext(x - 10, y - 4, color, DTEXT_RIGHT, false, buf);
    }
}

/* Dots that swing back and forth along the parts: the current's size sets
   how far, its phase when. Branches out of phase swing at other times */
static void draw_current(int e, int t)
{
    cplx i = res.i[e];
    double mag = c_abs(i);
    if (circuit.parts[e].kind == P_NONE || !(mag > 1e-7)) return;

    double amp = 3 + 1.5 * log10(mag * 1000);
    if (amp < 2) amp = 2;
    if (amp > 7) amp = 7;
    double d = amp * sin(2 * PI * t / ANIM_FRAMES + c_deg(i) * PI / 180);

    frame_t f = frame_of(e, false);
    for (int k = 0; k < 2; k++) {
        int x, y;
        fmap(&f, (int)floor(8 + 16 * k + d + 0.5), 0, &x, &y);
        if (f.h) y++; else x++;
        drect(x - 1, y - 1, x + 1, y + 1, DOT_COLOR);
    }
}

static void draw_grid(int t)
{
    int e = cursor_edge(), pt = cursor_point();
    int frame_color = drawing ? C_RGB(31, 12, 0) : COLOR;

    if (e >= 0) {
        frame_t f = frame_of(e, false);
        fbox(&f, 3, -11, STEP - 3, 12, TINT, frame_color);
    }
    else {
        int x = GX0 + (pt % NX) * STEP, y = GY0 + (pt / NX) * STEP;
        drect_border(x - 6, y - 6, x + 7, y + 7, TINT, 1, frame_color);
    }

    /* Points: faint when unused, a dot where three or more links meet */
    static int degree[NODES];
    memset(degree, 0, sizeof degree);
    for (int k = 0; k < EDGES; k++) {
        if (circuit.parts[k].kind == P_NONE) continue;
        int a, b;
        edge_ends(k, &a, &b);
        degree[a]++;
        degree[b]++;
    }

    for (int k = 0; k < EDGES; k++) draw_part(k);
    if (res.status == S_OK)
        for (int k = 0; k < EDGES; k++) draw_current(k, t);
    for (int k = 0; k < EDGES; k++) draw_meter_label(k);

    for (int n = 0; n < NODES; n++) {
        int x = GX0 + (n % NX) * STEP, y = GY0 + (n / NX) * STEP;
        if (degree[n] == 0) drect(x, y, x + 1, y + 1, GRID_DOT);
        else if (degree[n] >= 3) drect(x - 2, y - 2, x + 3, y + 3, C_BLACK);
    }
}

//---
//  Readings at the bottom
//---

#define INFO_Y  164

static bool editable(int kind)
{
    return kind == P_RES || kind == P_CAP || kind == P_IND || kind == P_SRC
        || kind == P_LAMP;
}

static void draw_info(char const *message, int message_color)
{
    char a[48], b[48], c[48], line[128];
    int e = cursor_edge(), pt = cursor_point();
    bool ok = res.status == S_OK;
    int y1 = INFO_Y, y2 = INFO_Y + 13, y3 = INFO_Y + 26;
    int kind = pt >= 0 ? P_NONE : circuit.parts[e].kind;

    dline(6, INFO_Y - 3, DWIDTH - 7, INFO_Y - 3, C_LIGHT);

    if (pt >= 0) {
        sprintf(line, L("Point %d, %d", "Ponto %d, %d"), pt % NX + 1,
            pt / NX + 1);
        etext(10, y1, COLOR, DTEXT_LEFT, false, line);
        if (ok && res.known[pt]) {
            sprintf(line, L("V = %s  (to the source's -)",
                "V = %s  (em relacao ao - da fonte)"),
                fmt_phasor(a, res.v[pt], "V", 4));
            etext(10, y2, C_BLACK, DTEXT_LEFT, false, line);
        }
        else if (ok) etext(10, y2, C_DARK, DTEXT_LEFT, false,
            L("Not connected", "Sem ligacao"));
    }
    else {
        part_t const *p = &circuit.parts[e];
        cplx v = part_v(&res, e), i = part_i(&res, e);
        char const *status = NULL;
        int status_color = C_DARK;

        /* First line: the part and its values, its impedance or state at
           the right */
        strcpy(line, kind_name(p->kind));
        switch (p->kind) {
        case P_NONE:
            strcpy(line, L("Empty: F1-F6 place a part",
                "Vazio: F1-F6 poem uma peca"));
            break;
        case P_RES:
            sprintf(line + strlen(line), "  %s", fmt_si(a, p->value, S_OHM, 4));
            break;
        case P_CAP:
            sprintf(line + strlen(line), "  %s", fmt_si(a, p->value, "F", 4));
            break;
        case P_IND:
            sprintf(line + strlen(line), "  %s", fmt_si(a, p->value, "H", 4));
            if (p->value2 > 0) sprintf(line + strlen(line), " + %s",
                fmt_si(b, p->value2, S_OHM, 3));
            break;
        case P_SRC:
            sprintf(line + strlen(line), "  %s", fmt_phasor(a,
                c_polar(p->value, p->value2), "V", 4));
            if (ok) {
                status = fmt_pf(c, res.s[e]);
                sprintf(b, "PF %s", c);
                status = b;
            }
            break;
        case P_LAMP:
            sprintf(line + strlen(line), "  %s  %s",
                fmt_si(a, p->value, "V", 3), fmt_si(b, p->value2, "W", 3));
            if (ok) {
                bool over = res.s[e].re > 1.2 * p->value2;
                sprintf(c, over ? L("OVERLOAD", "SOBRECARGA")
                    : L("light %d%%", "luz %d%%"),
                    (int)(lamp_brightness(e) * 100 + 0.5));
                status = c;
                status_color = over ? C_RED : C_RGB(20, 16, 0);
            }
            break;
        case P_SWITCH:
            sprintf(line + strlen(line), "  %s", p->closed
                ? L("closed", "fechado") : L("open", "aberto"));
            break;
        }
        if (p->kind == P_RES || p->kind == P_CAP || p->kind == P_IND) {
            sprintf(c, "Z = %s", fmt_phasor(b, part_impedance(p,
                circuit.freq), S_OHM, 3));
            status = c;
        }
        etext(10, y1, p->kind == P_NONE ? C_DARK : COLOR, DTEXT_LEFT, false,
            line);
        if (status) etext(DWIDTH - 10, y1, status_color, DTEXT_RIGHT, false,
            status);

        /* Second line: the readings (rms phasors) */
        line[0] = '\0';
        if (!ok) ;
        else if (p->kind == P_RES || p->kind == P_CAP || p->kind == P_IND
            || p->kind == P_LAMP)
            sprintf(line, "V = %s    I = %s", fmt_phasor(a, v, "V", 3),
                fmt_phasor(b, i, "A", 3));
        else if (p->kind == P_SRC)
            sprintf(line, L("I = %s    S = %s", "I = %s    S = %s"),
                fmt_phasor(a, i, "A", 3), fmt_si(b, c_abs(res.s[e]), "VA", 3));
        else if (p->kind == P_VOLTMETER || (p->kind == P_SWITCH && !p->closed))
            sprintf(line, p->kind == P_VOLTMETER ? "V = %s"
                : L("V = %s across it", "V = %s aos terminais"),
                fmt_phasor(a, v, "V", 4));
        else if (p->kind != P_NONE)
            sprintf(line, "I = %s", fmt_phasor(a, i, "A", 4));
        etext(10, y2, C_BLACK, DTEXT_LEFT, false, line);
    }

    /* Third line: a message, the circuit's state, or f and the power */
    if (message) {
        etext(10, y3, message_color, DTEXT_LEFT, false, message);
        return;
    }
    if (res.status == S_SHORT) etext(10, y3, C_RED, DTEXT_LEFT, false,
        L("Short circuit across a source!", "Curto-circuito numa fonte!"));
    else if (res.status == S_LOOP) etext(10, y3, C_RED, DTEXT_LEFT, false,
        L("Sources in parallel or in a loop", "Fontes em paralelo ou em ciclo"));
    else if (res.status == S_TOOBIG) etext(10, y3, C_RED, DTEXT_LEFT, false,
        L("Too many parts to solve", "Pecas a mais para resolver"));
    else if (res.status == S_EMPTY) etext(10, y3, C_DARK, DTEXT_LEFT, false,
        L("Add an AC source and a load.  VARS: examples",
          "Junte uma fonte CA e uma carga.  VARS: exemplos"));
    if (res.status != S_OK) return;

    cplx s = res.supplied;
    if (pt < 0 && (kind == P_RES || kind == P_CAP || kind == P_IND
        || kind == P_LAMP)) s = res.s[e];
    sprintf(line, "f = %s    P = %s    Q = %s", fmt_si(a, circuit.freq,
        "Hz", 4), fmt_si(b, s.re, "W", 3), fmt_si(c, s.im, "var", 3));
    etext(10, y3, C_DARK, DTEXT_LEFT, false, line);

    char const *hint = L("1-3: graphs", "1-3: graficos");
    if (kind == P_SWITCH) hint = L("EXE: on/off", "EXE: ligar");
    else if (editable(kind)) hint = L("EXE: edit", "EXE: editar");
    if (etext_width(line, false) + etext_width(hint, false) + 30 < DWIDTH)
        etext(DWIDTH - 10, y3, C_DARK, DTEXT_RIGHT, false, hint);
}

/* Parts in the F6 menu */
static kind_t const menu_kinds[] = { P_LAMP, P_SWITCH, P_AMMETER,
    P_VOLTMETER };
#define MENU_COUNT (int)(sizeof menu_kinds / sizeof *menu_kinds)

static bool in_menu(int kind)
{
    for (int k = 0; k < MENU_COUNT; k++)
        if ((int)menu_kinds[k] == kind) return true;
    return false;
}

static kind_t const fkey_kinds[] = { P_WIRE, P_RES, P_CAP, P_IND, P_SRC };

static void draw_screen(int t, char const *message, int message_color)
{
    dclear(C_WHITE);
    info_title(L("AC Circuit", "Circuitos CA"), COLOR);
    draw_grid(t);
    draw_info(message, message_color);

    int e = cursor_edge();
    int kind = e >= 0 ? circuit.parts[e].kind : drawing ? P_WIRE : P_NONE;
    char const *labels[] = { L("WIRE", "FIO"), "R", "C", "L",
        L("SOURCE", "FONTE"), L("PARTS", "PECAS") };
    for (int k = 0; k < 6; k++) {
        bool on = k < 5 ? kind == (int)fkey_kinds[k] : in_menu(kind);
        if (on) fkey_button_color(k + 1, labels[k], COLOR);
        else fkey_button(k + 1, labels[k]);
    }
}

static void draw(int t, char const *message, int message_color)
{
    draw_screen(t, message, message_color);
    dupdate();
}

/* The F6 menu over the circuit; returns the chosen kind, or -1 */
static int pick_part(int t)
{
    static int sel = 0;
    int x1 = 104, x2 = 292, y1 = 24, row = 16;
    int y2 = y1 + 18 + MENU_COUNT * row + 4;

    while (1) {
        draw_screen(t, NULL, 0);
        drect_border(x1, y1, x2, y2, C_WHITE, 2, COLOR);
        drect(x1, y1, x2, y1 + 15, COLOR);
        dtext(x1 + 8, y1 + 3, C_WHITE, L("Parts", "Pecas"));
        for (int k = 0; k < MENU_COUNT; k++) {
            int y = y1 + 18 + k * row;
            if (k == sel) drect(x1 + 2, y - 1, x2 - 2, y + row - 3, TINT);
            dprint(x1 + 8, y + 2, C_DARK, "%d", k + 1);
            dtext(x1 + 24, y + 2, k == sel ? COLOR : C_BLACK,
                kind_name(menu_kinds[k]));
        }
        dupdate();

        int key = getkey().key;
        int d = key_digit(key);
        if (key == KEY_EXIT || key == KEY_F6) return -1;
        if (key == KEY_UP) sel = (sel + MENU_COUNT - 1) % MENU_COUNT;
        if (key == KEY_DOWN) sel = (sel + 1) % MENU_COUNT;
        if (d >= 1 && d <= MENU_COUNT) sel = d - 1;
        if (key == KEY_EXE || (d >= 1 && d <= MENU_COUNT))
            return menu_kinds[sel];
    }
}

//---
//  Editing a part's values, and the frequency
//---

static part_t *editing;

static void edit_results(eform_t *f, int y)
{
    char a[48];
    int kind = editing->kind;
    double v = field_value(&f->fields[0]);
    part_t p = *editing;

    if (kind == P_CAP || kind == P_IND) {
        p.value = v;
        if (kind == P_IND) {
            double r = field_value(&f->fields[1]);
            p.value2 = r >= 0 ? r : 0;
        }
        if (v > 0) {
            cplx z = part_impedance(&p, circuit.freq);
            result(y, L("Reactance", "Reatancia"), C_BLACK, "%s",
                fmt_si(a, fabs(z.im), S_OHM, 4));
            result(y + RESULT_STEP, L("Impedance", "Impedancia"), C_BLACK,
                "%s", fmt_phasor(a, z, S_OHM, 4));
        }
        result_note(y + 2 * RESULT_STEP, L("at f = %s", "a f = %s"),
            fmt_si(a, circuit.freq, "Hz", 4));
    }
    else if (kind == P_LAMP) {
        double w = field_value(&f->fields[1]);
        if (v > 0 && w > 0) result(y, L("Resistance", "Resistencia"),
            C_BLACK, "%s", fmt_si(a, v * v / w, S_OHM, 4));
        result_note(y + RESULT_STEP, L("Rated values are rms",
            "Valores nominais eficazes"));
    }
    else if (kind == P_SRC) {
        result(y, L("Peak", "Pico"), C_BLACK, "%s",
            fmt_si(a, v > 0 ? v * sqrt(2) : NAN, "V", 4));
        result_note(y + RESULT_STEP, L("The voltage is rms; the phase is "
            "in degrees", "Tensao eficaz; a fase e em graus"));
    }
    result_note(y + 3 * RESULT_STEP, L("F1-F6: p n " S_MU " m k M    "
        "EXIT: back", "F1-F6: p n " S_MU " m k M    EXIT: voltar"));
}

/* Value typed in a field if it is usable, else the old one */
static double field_or(field_t const *f, double old, double lo, double hi)
{
    double v = field_value(f);
    return v >= lo && v <= hi ? v : old;
}

static void edit_part(part_t *p)
{
    static field_t fields[2];
    memset(fields, 0, sizeof fields);
    eform_t f = { .title = kind_name(p->kind), .color = COLOR,
        .fields = fields, .count = 1, .cols = 1, .label_w = 150,
        .results = edit_results };
    editing = p;

    for (int k = 0; k < 2; k++) fields[k].kind = F_NUM;
    switch (p->kind) {
    case P_RES:
        fields[0].label = L("Resistance", "Resistencia");
        fields[0].unit = S_OHM;
        break;
    case P_CAP:
        fields[0].label = L("Capacitance", "Capacidade");
        fields[0].unit = "F";
        break;
    case P_IND:
        f.count = 2;
        fields[0].label = L("Inductance", "Indutancia");
        fields[0].unit = "H";
        fields[1].label = L("Wire resistance", "Resist. do fio");
        fields[1].unit = S_OHM;
        field_set(&fields[1], p->value2);
        break;
    case P_SRC:
        f.count = 2;
        fields[0].label = L("Voltage (rms)", "Tensao (eficaz)");
        fields[0].unit = "V";
        fields[1].label = L("Phase", "Fase");
        fields[1].unit = S_DEG;
        field_set(&fields[1], p->value2);
        break;
    case P_LAMP:
        f.count = 2;
        fields[0].label = L("Rated voltage", "Tensao nominal");
        fields[0].unit = "V";
        fields[1].label = L("Rated power", "Potencia nominal");
        fields[1].unit = "W";
        field_set(&fields[1], p->value2);
        break;
    default:
        return;
    }
    field_set(&fields[0], p->value);
    eform_run(&f);

    switch (p->kind) {
    case P_RES:
        p->value = field_or(&fields[0], p->value, 1e-3, 1e9);
        break;
    case P_CAP:
        p->value = field_or(&fields[0], p->value, 1e-12, 10);
        break;
    case P_IND:
        p->value = field_or(&fields[0], p->value, 1e-9, 1e3);
        p->value2 = field_or(&fields[1], p->value2, 0, 1e6);
        break;
    case P_SRC:
        p->value = field_or(&fields[0], p->value, 1e-3, 1e5);
        p->value2 = wrap_deg(field_or(&fields[1], p->value2, -360, 360));
        break;
    case P_LAMP:
        p->value = field_or(&fields[0], p->value, 1e-3, 1e4);
        p->value2 = field_or(&fields[1], p->value2, 1e-6, 1e5);
        break;
    }
}

static void freq_results(eform_t *f, int y)
{
    char a[48];
    double v = field_value(&f->fields[0]);
    if (v >= FREQ_MIN && v <= FREQ_MAX) {
        result(y, L("Angular", "Angular"), C_BLACK, "%s",
            fmt_si(a, 2 * PI * v, "rad/s", 4));
        result(y + RESULT_STEP, L("Period", "Periodo"), C_BLACK, "%s",
            fmt_si(a, 1 / v, "s", 4));
    }
    else result(y, L("Range", "Intervalo"), C_DARK, "1 Hz - 1 MHz");
    result_note(y + 3 * RESULT_STEP, L("All sources run at this frequency",
        "Todas as fontes usam esta frequencia"));
}

static void edit_freq(void)
{
    static field_t fields[1];
    memset(fields, 0, sizeof fields);
    fields[0].kind = F_NUM;
    fields[0].label = L("Frequency", "Frequencia");
    fields[0].unit = "Hz";
    field_set(&fields[0], circuit.freq);
    eform_t f = { .title = L("Frequency", "Frequencia"), .color = COLOR,
        .fields = fields, .count = 1, .cols = 1, .label_w = 150,
        .results = freq_results };
    eform_run(&f);
    circuit.freq = field_or(&fields[0], circuit.freq, FREQ_MIN, FREQ_MAX);
}

/* +/-: the frequency along the E12 series (1, 1.2, 1.5 ... 8.2) */
static void step_freq(int dir)
{
    static double const e12[12] = { 1.0, 1.2, 1.5, 1.8, 2.2, 2.7, 3.3, 3.9,
        4.7, 5.6, 6.8, 8.2 };
    int dec = (int)floor(log10(circuit.freq) + 1e-9);
    double m = circuit.freq / pow(10, dec);
    int k = 0;
    for (int i = 0; i < 12; i++)
        if (fabs(log(e12[i] / m)) < fabs(log(e12[k] / m))) k = i;
    if (fabs(log(e12[k] / m)) > 0.01) {
        /* Not on the series: go to the neighbour in that direction */
        if (dir > 0 && e12[k] < m) k++;
        if (dir < 0 && e12[k] > m) k--;
    }
    else k += dir;
    if (k >= 12) { k -= 12; dec++; }
    if (k < 0) { k += 12; dec--; }
    double f = e12[k] * pow(10, dec);
    if (f >= FREQ_MIN * 0.999 && f <= FREQ_MAX * 1.001) circuit.freq = f;
}

//---
//  Graphs of one part: phasors, waveforms, frequency response
//---

/* What the graphs show: the part at the cursor, else the first source */
typedef struct {
    int e;
    char const *name;
    bool has_v, has_i;
} target_t;

static target_t get_target(void)
{
    target_t t = { -1, NULL, false, false };
    int e = cursor_edge();
    if (e >= 0 && circuit.parts[e].kind != P_NONE) {
        int k = circuit.parts[e].kind;
        bool closed = k == P_WIRE || k == P_AMMETER
            || (k == P_SWITCH && circuit.parts[e].closed);
        t.e = e;
        t.has_v = !closed;
        t.has_i = k != P_VOLTMETER && !(k == P_SWITCH && !circuit.parts[e].closed);
    }
    else if (res.ref_source >= 0) {
        t.e = res.ref_source;
        t.has_v = t.has_i = true;
    }
    if (t.e >= 0) t.name = kind_name(circuit.parts[t.e].kind);
    return t;
}

static void view_title(char const *title)
{
    dclear(C_WHITE);
    info_title(title, COLOR);
}

static void arrow(int cx, int cy, cplx z, double scale, int color)
{
    int x = cx + (int)floor(z.re * scale + 0.5);
    int y = cy - (int)floor(z.im * scale + 0.5);
    dline(cx, cy, x, y, color);
    dline(cx + 1, cy, x + 1, y, color);
    dline(cx, cy + 1, x, y + 1, color);
    double a = atan2(cy - y, x - cx);
    if (abs(x - cx) + abs(y - cy) < 4) return;
    for (int s = -1; s <= 1; s += 2) {
        double b = a + PI + s * 0.45;
        int hx = x + (int)floor(9 * cos(b) + 0.5);
        int hy = y - (int)floor(9 * sin(b) + 0.5);
        dline(x, y, hx, hy, color);
        dline(x + 1, y, hx + 1, hy, color);
    }
}

/* "I leads V by 51°" */
static void phase_line(int x, int y, cplx v, cplx i)
{
    char a[32], line[64];
    double d = wrap_deg(c_deg(i) - c_deg(v));
    fmt_num(a, fabs(d), 0);
    if (fabs(d) < 0.5) strcpy(line, L("I and V in phase", "I e V em fase"));
    else sprintf(line, d > 0 ? L("I leads V by %s" S_DEG,
        "I adiantada %s" S_DEG " de V") : L("I lags V by %s" S_DEG,
        "I atrasada %s" S_DEG " de V"), a);
    etext(x, y, C_BLACK, DTEXT_LEFT, false, line);
}

static void view_phasors(target_t t)
{
    char a[48], line[96];
    int src = res.ref_source;
    cplx vs = part_v(&res, src), is = part_i(&res, src);
    cplx v = part_v(&res, t.e), i = part_i(&res, t.e);
    bool own = t.e == src;

    view_title(L("Phasors", "Fasores"));

    int cx = 102, cy = 112, r = 82;
    dcircle(cx, cy, r, C_NONE, C_RGB(28, 28, 28));
    dline(cx - r - 6, cy, cx + r + 6, cy, C_LIGHT);
    dline(cx, cy - r - 6, cx, cy + r + 6, C_LIGHT);

    double vmax = c_abs(vs), imax = c_abs(is);
    if (t.has_v && c_abs(v) > vmax) vmax = c_abs(v);
    if (t.has_i && c_abs(i) > imax) imax = c_abs(i);
    double vk = vmax > 0 ? r / vmax : 0, ik = imax > 0 ? r * 0.8 / imax : 0;

    if (!own) {
        arrow(cx, cy, vs, vk, C_RGB(22, 22, 22));
        arrow(cx, cy, is, ik, C_RGB(24, 26, 30));
    }
    if (t.has_v) arrow(cx, cy, v, vk, V_COLOR);
    if (t.has_i) arrow(cx, cy, i, ik, I_COLOR);

    int x = 204, y = 26;
    if (!own) {
        etext(x, y, SRC_COLOR, DTEXT_LEFT, false, L("Source (grey)",
            "Fonte (cinzento)"));
        sprintf(line, "V = %s", fmt_phasor(a, vs, "V", 3));
        etext(x + 8, y + 13, SRC_COLOR, DTEXT_LEFT, false, line);
        sprintf(line, "I = %s", fmt_phasor(a, is, "A", 3));
        etext(x + 8, y + 26, SRC_COLOR, DTEXT_LEFT, false, line);
        y += 46;
    }
    etext(x, y, COLOR, DTEXT_LEFT, false, t.name);
    y += 13;
    if (t.has_v) {
        sprintf(line, "V = %s", fmt_phasor(a, v, "V", 3));
        etext(x + 8, y, V_COLOR, DTEXT_LEFT, false, line);
        y += 13;
    }
    if (t.has_i) {
        sprintf(line, "I = %s", fmt_phasor(a, i, "A", 3));
        etext(x + 8, y, I_COLOR, DTEXT_LEFT, false, line);
        y += 13;
    }
    if (t.has_v && t.has_i && c_abs(i) > 1e-12) {
        sprintf(line, "Z = %s", fmt_phasor(a, c_div(v, i), S_OHM, 3));
        etext(x + 8, y, C_BLACK, DTEXT_LEFT, false, line);
        phase_line(x + 8, y + 13, v, i);
        y += 26;
    }
    if (own) {
        sprintf(line, "PF %s", fmt_pf(a, res.s[src]));
        etext(x + 8, y, C_BLACK, DTEXT_LEFT, false, line);
    }

    etext(x, 172, C_DARK, DTEXT_LEFT, false, L("V and I: separate scales",
        "V e I: escalas separadas"));
    etext(DWIDTH / 2, 192, C_DARK, DTEXT_CENTER, false,
        L("Angles from the source's phase.  Any key: back",
        "Angulos a partir da fase da fonte.  Tecla: voltar"));
    dupdate();
    getkey();
}

static void view_waves(target_t t)
{
    char a[48], b[48], line[96];
    int src = res.ref_source;
    cplx vs = part_v(&res, src);
    cplx v = part_v(&res, t.e), i = part_i(&res, t.e);
    double f = circuit.freq;

    view_title(L("Waveforms", "Formas de onda"));

    int x1 = 44, x2 = DWIDTH - 44, y1 = 34, y2 = 164, mid = (y1 + y2) / 2;
    drect_border(x1, y1, x2, y2, C_WHITE, 1, C_LIGHT);
    for (int x = x1 + 2; x < x2; x += 4) dpixel(x, mid, C_LIGHT);
    for (int k = 1; k < 4; k++) {
        int x = x1 + (x2 - x1) * k / 4;
        for (int y = y1 + 2; y < y2; y += 4) dpixel(x, y, C_LIGHT);
    }

    double vmax = sqrt(2) * c_abs(t.has_v ? v : vs);
    if (sqrt(2) * c_abs(vs) > vmax) vmax = sqrt(2) * c_abs(vs);
    double imax = sqrt(2) * c_abs(i);
    double h = (y2 - y1) / 2 - 6;

    /* Two periods: 4 grid columns are half a period each */
    for (int pass = 0; pass < 3; pass++) {
        cplx z = pass == 0 ? vs : pass == 1 ? v : i;
        double peak = pass == 2 ? imax : vmax;
        int color = pass == 0 ? C_RGB(24, 24, 24) : pass == 1 ? V_COLOR
            : I_COLOR;
        if (pass == 0 && t.e == src) continue;
        if ((pass == 1 && !t.has_v) || (pass == 2 && !t.has_i)) continue;
        if (!(peak > 0)) continue;
        int px = -1, py = 0;
        for (int x = x1 + 1; x < x2; x++) {
            double ph = 4 * PI * (x - x1) / (x2 - x1) + c_deg(z) * PI / 180;
            double val = sqrt(2) * c_abs(z) * cos(ph) / peak;
            int y = mid - (int)floor(val * h + 0.5);
            if (px >= 0) {
                dline(px, py, x, y, color);
                if (pass) dline(px, py + 1, x, y + 1, color);
            }
            px = x;
            py = y;
        }
    }

    /* Peak values on the axes: V at the left, I at the right */
    if (t.has_v || t.e != src) {
        etext(x1 - 4, y1, V_COLOR, DTEXT_RIGHT, false, "+V");
        etext(x1 - 4, y2 - 9, V_COLOR, DTEXT_RIGHT, false, "-V");
    }
    if (t.has_i && imax > 0) {
        etext(x2 + 4, y1, I_COLOR, DTEXT_LEFT, false, "+I");
        etext(x2 + 4, y2 - 9, I_COLOR, DTEXT_LEFT, false, "-I");
    }
    etext(x1, y2 + 3, C_DARK, DTEXT_LEFT, false, "0");
    sprintf(line, "2T = %s", fmt_si(a, 2 / f, "s", 3));
    etext(x2, y2 + 3, C_DARK, DTEXT_RIGHT, false, line);

    line[0] = '\0';
    if (t.has_v) sprintf(line, L("v peak %s", "v pico %s"),
        fmt_si(a, sqrt(2) * c_abs(v), "V", 3));
    etext(x1, 20, V_COLOR, DTEXT_LEFT, false, line);
    line[0] = '\0';
    if (t.has_i) sprintf(line, L("i peak %s", "i pico %s"),
        fmt_si(b, imax, "A", 3));
    etext(x2, 20, I_COLOR, DTEXT_RIGHT, false, line);
    etext(DWIDTH / 2, 20, COLOR, DTEXT_CENTER, false, t.name);

    if (t.has_v && t.has_i && imax > 0) phase_line(x1, 182, v, i);
    if (t.e != src) etext(x2, 182, C_RGB(20, 20, 20), DTEXT_RIGHT, false,
        L("grey: the source", "cinzento: a fonte"));
    etext(DWIDTH / 2, 196, C_DARK, DTEXT_CENTER, false,
        L("Any key: back", "Tecla: voltar"));
    dupdate();
    getkey();
}

//---
//  Frequency response
//---

#define BODE_X1  48
#define BODE_X2  (DWIDTH - 12)
#define BODE_N   ((BODE_X2 - BODE_X1) / 2 + 1)
#define MAG_Y1   34
#define MAG_Y2   112
#define PH_Y1    124
#define PH_Y2    164

/* Decade labels: "1", "100", "10k", "1M" */
static char *fmt_decade(char *buf, double v)
{
    int e = (int)floor(log10(v) + 0.5);
    static char const *const suffix[] = { "", "k", "M" };
    int p = 1;
    for (int k = 0; k < e % 3; k++) p *= 10;
    sprintf(buf, "%d%s", p, suffix[e / 3 < 3 ? e / 3 : 2]);
    return buf;
}

static double bode_x(int k)
{
    return BODE_X1 + 2 * k;
}

static void view_bode(target_t t)
{
    static double mag[BODE_N], ph[BODE_N];
    static result_t tmp;
    char a[48], b[48], c[48], line[128];
    bool gain = t.has_v && t.e != res.ref_source;

    /* Five decades around the frequency, within 1 Hz - 1 MHz */
    double lo = pow(10, floor(log10(circuit.freq)) - 2);
    if (lo < FREQ_MIN) lo = FREQ_MIN;
    if (lo * 1e5 > FREQ_MAX) lo = FREQ_MAX / 1e5;
    double decades = 5;

    view_title(L("Frequency response", "Resposta em frequencia"));
    etext(DWIDTH / 2, 100, C_DARK, DTEXT_CENTER, false,
        L("Solving the circuit at each frequency...",
        "A resolver o circuito em cada frequencia..."));
    dupdate();

    double mmin = INFINITY, mmax = -INFINITY;
    int cur = 0;
    for (int k = 0; k < BODE_N; k++) {
        double f = lo * pow(10, decades * k / (BODE_N - 1));
        if (f <= circuit.freq) cur = k;
        circuit_solve(&circuit, f, &tmp);
        cplx ref = part_v(&tmp, tmp.ref_source);
        cplx z = gain ? part_v(&tmp, t.e) : part_i(&tmp, t.e);
        double m = c_abs(z);
        if (gain) m = c_abs(ref) > 0 ? m / c_abs(ref) : 0;
        mag[k] = tmp.status == S_OK && m > 1e-15 ? (gain ? 20 * log10(m)
            : log10(m)) : NAN;
        ph[k] = wrap_deg(c_deg(z) - c_deg(ref));
        if (!isnan(mag[k])) {
            if (mag[k] < mmin) mmin = mag[k];
            if (mag[k] > mmax) mmax = mag[k];
        }
    }
    if (!(mmin <= mmax)) { mmin = 0; mmax = 1; }
    /* At least 6 dB (gain) or half a decade (current) of range */
    double span = gain ? 6 : 0.5;
    if (mmax - mmin < span) {
        double c = (mmax + mmin) / 2;
        mmin = c - span / 2;
        mmax = c + span / 2;
    }

    while (1) {
        view_title(L("Frequency response", "Resposta em frequencia"));

        /* Decade lines and their labels */
        for (int d = 0; d <= decades; d++) {
            int x = BODE_X1 + (int)((BODE_X2 - BODE_X1) * d / decades);
            for (int y = MAG_Y1; y < MAG_Y2; y += 3) dpixel(x, y, C_LIGHT);
            for (int y = PH_Y1; y < PH_Y2; y += 3) dpixel(x, y, C_LIGHT);
            /* The last one ends at the right edge */
            etext(d < decades ? x : DWIDTH - 2, PH_Y2 + 2, C_DARK,
                d < decades ? DTEXT_CENTER : DTEXT_RIGHT, false,
                fmt_decade(a, lo * pow(10, d)));
        }
        drect_border(BODE_X1, MAG_Y1, BODE_X2, MAG_Y2, C_NONE, 1, C_LIGHT);
        drect_border(BODE_X1, PH_Y1, BODE_X2, PH_Y2, C_NONE, 1, C_LIGHT);
        int ph0 = (PH_Y1 + PH_Y2) / 2, q = (PH_Y2 - PH_Y1) / 4;
        for (int x = BODE_X1; x < BODE_X2; x += 3) {
            dpixel(x, ph0, C_LIGHT);
            if (x % 6 == 0) {
                dpixel(x, ph0 - q, C_LIGHT);
                dpixel(x, ph0 + q, C_LIGHT);
            }
        }

        /* Axis labels */
        if (gain) {
            etext(BODE_X1 - 3, MAG_Y1, V_COLOR, DTEXT_RIGHT, false,
                fmt_num(a, mmax, 1));
            etext(BODE_X1 - 3, MAG_Y2 - 9, V_COLOR, DTEXT_RIGHT, false,
                fmt_num(b, mmin, 1));
            etext(BODE_X1 - 3, (MAG_Y1 + MAG_Y2) / 2 - 4, V_COLOR,
                DTEXT_RIGHT, false, "dB");
        }
        else {
            fmt_si(a, pow(10, mmax), "A", 2);
            fmt_si(b, pow(10, mmin), "A", 2);
            etext(BODE_X1 + 3, MAG_Y1 + 2, I_COLOR, DTEXT_LEFT, false, a);
            etext(BODE_X1 + 3, MAG_Y2 - 11, I_COLOR, DTEXT_LEFT, false, b);
        }
        etext(BODE_X1 - 3, PH_Y1, C_DARK, DTEXT_RIGHT, false, "180" S_DEG);
        etext(BODE_X1 - 3, ph0 - 4, C_DARK, DTEXT_RIGHT, false, "0" S_DEG);
        etext(BODE_X1 - 3, PH_Y2 - 9, C_DARK, DTEXT_RIGHT, false, "-180" S_DEG);

        /* The circuit's frequency, then the curves */
        double fx = BODE_X1 + (BODE_X2 - BODE_X1) * log10(circuit.freq / lo)
            / decades;
        for (int y = MAG_Y1; y < PH_Y2; y += 2)
            if (y < MAG_Y2 || y > PH_Y1) dpixel((int)fx, y, COLOR);

        int color = gain ? V_COLOR : I_COLOR;
        for (int k = 1; k < BODE_N; k++) {
            if (isnan(mag[k]) || isnan(mag[k - 1])) continue;
            int ya = MAG_Y2 - 2 - (int)((mag[k - 1] - mmin) / (mmax - mmin)
                * (MAG_Y2 - MAG_Y1 - 4));
            int yb = MAG_Y2 - 2 - (int)((mag[k] - mmin) / (mmax - mmin)
                * (MAG_Y2 - MAG_Y1 - 4));
            dline(bode_x(k - 1), ya, bode_x(k), yb, color);
            dline(bode_x(k - 1), ya + 1, bode_x(k), yb + 1, color);
        }
        for (int k = 1; k < BODE_N; k++) {
            /* -180 to 180; no line where it wraps around */
            if (fabs(ph[k] - ph[k - 1]) > 180) continue;
            int ya = ph0 - (int)(ph[k - 1] / 180 * (PH_Y2 - PH_Y1 - 2) / 2);
            int yb = ph0 - (int)(ph[k] / 180 * (PH_Y2 - PH_Y1 - 2) / 2);
            dline(bode_x(k - 1), ya, bode_x(k), yb, C_DARK);
            dline(bode_x(k - 1), ya + 1, bode_x(k), yb + 1, C_DARK);
        }

        /* The reading at the cursor */
        int x = bode_x(cur);
        dline(x, MAG_Y1 + 1, x, MAG_Y2 - 1, C_BLACK);
        dline(x, PH_Y1 + 1, x, PH_Y2 - 1, C_BLACK);
        double f = lo * pow(10, decades * cur / (BODE_N - 1));
        if (isnan(mag[cur])) strcpy(b, "-");
        else if (gain) sprintf(b, "%s dB", fmt_num(a, mag[cur], 1));
        else fmt_si(b, pow(10, mag[cur]), "A", 3);
        sprintf(line, "f = %s    %s %s    %s %s" S_DEG,
            fmt_si(a, f, "Hz", 3), gain ? L("gain", "ganho") : "I", b,
            L("phase", "fase"), fmt_num(c, ph[cur], 0));
        etext(10, 20, C_BLACK, DTEXT_LEFT, false, line);
        sprintf(line, gain ? L("%s: V over the source's V", "%s: V sobre V da "
            "fonte") : L("%s: current", "%s: corrente"), t.name);
        etext(DWIDTH / 2, 178, color, DTEXT_CENTER, false, line);

        etext(DWIDTH / 2, 192, C_DARK, DTEXT_CENTER, false,
            L("LEFT/RIGHT: move    EXE: use this f    EXIT: back",
            "ESQ/DIR: mover    EXE: usar esta f    EXIT: voltar"));
        dupdate();

        int key = getkey_opt(GETKEY_DEFAULT | GETKEY_REP_ARROWS, NULL).key;
        if (key == KEY_EXIT) return;
        if (key == KEY_LEFT && cur > 0) cur--;
        if (key == KEY_RIGHT && cur < BODE_N - 1) cur++;
        if (key == KEY_EXE) {
            /* Rounded to 3 significant digits */
            double p = pow(10, floor(log10(f)) - 2);
            circuit.freq = floor(f / p + 0.5) * p;
            return;
        }
    }
}

//---
//  Examples
//---

static void put(circuit_t *c, int x, int y, bool h, part_t p)
{
    c->parts[h ? y * (NX - 1) + x : H_EDGES + y * NX + x] = p;
}

static void wires_h(circuit_t *c, int x1, int x2, int y)
{
    for (int x = x1; x < x2; x++) put(c, x, y, true, part_default(P_WIRE));
}

static void wires_v(circuit_t *c, int x, int y1, int y2)
{
    for (int y = y1; y < y2; y++) put(c, x, y, false, part_default(P_WIRE));
}

static part_t with(kind_t kind, double value, double value2)
{
    part_t p = part_default(kind);
    p.value = value;
    p.value2 = value2;
    return p;
}

#define EXAMPLES 5

/* Returns the example's name */
static char const *load_example(circuit_t *c, int k)
{
    memset(c, 0, sizeof *c);

    /* Every example: the source on the left, rails along rows 1 and 3 */
    put(c, 1, 1, false, with(P_SRC, 10, 0));
    wires_v(c, 1, 2, 3);

    if (k == 1) {
        /* Series RLC with a lamp: 12 Ω, 10 mH, 10 µF, f0 = 503 Hz */
        c->freq = 400;
        c->parts[H_EDGES + NX + 1].value = 6;
        wires_h(c, 1, 3, 1);
        put(c, 3, 1, true, with(P_IND, 10e-3, 0));
        wires_h(c, 4, 6, 1);
        put(c, 6, 1, true, with(P_CAP, 10e-6, 0));
        wires_h(c, 7, 9, 1);
        put(c, 9, 1, false, with(P_LAMP, 6, 3));
        wires_v(c, 9, 2, 3);
        wires_h(c, 1, 5, 3);
        put(c, 5, 3, true, part_default(P_AMMETER));
        wires_h(c, 6, 9, 3);
        return L("Series RLC: tune with +/-, brightest at 503 Hz",
            "RLC serie: sintonize com +/-, maximo a 503 Hz");
    }
    if (k == 2) {
        /* RL load (50 Ω, 0.2 H) on 230 V 50 Hz; a 30 µF capacitor behind
           an open switch corrects its power factor */
        c->freq = 50;
        c->parts[H_EDGES + NX + 1].value = 230;
        wires_h(c, 1, 2, 1);
        put(c, 2, 1, true, part_default(P_AMMETER));
        wires_h(c, 3, 8, 1);
        put(c, 5, 1, false, with(P_CAP, 30e-6, 0));
        part_t sw = part_default(P_SWITCH);
        sw.closed = 0;
        put(c, 5, 2, false, sw);
        put(c, 8, 1, false, with(P_RES, 50, 0));
        put(c, 8, 2, false, with(P_IND, 0.2, 0));
        wires_h(c, 1, 8, 3);
        return L("RL load, PF 0.62: close the switch to fix it",
            "Carga RL, FP 0,62: feche o interruptor");
    }
    if (k == 3) {
        /* Parallel LC tank fed through 330 Ω: 1 mH, 1 µF, f0 = 5.03 kHz */
        c->freq = 4700;
        wires_h(c, 1, 2, 1);
        put(c, 2, 1, true, with(P_RES, 330, 0));
        wires_h(c, 3, 10, 1);
        put(c, 5, 1, false, with(P_IND, 1e-3, 0));
        wires_v(c, 5, 2, 3);
        put(c, 7, 1, false, with(P_CAP, 1e-6, 0));
        wires_v(c, 7, 2, 3);
        put(c, 10, 1, false, part_default(P_VOLTMETER));
        wires_v(c, 10, 2, 3);
        wires_h(c, 1, 10, 3);
        return L("LC tank: the voltage peaks at 5.03 kHz (try +/-)",
            "Circuito LC: tensao maxima a 5,03 kHz (+/-)");
    }

    /* RC filters: 1 kΩ and 100 nF, corner at 1.59 kHz; the voltmeter is
       across the part at the bottom */
    bool high = k == 4;
    c->freq = 1000;
    wires_h(c, 1, 3, 1);
    put(c, 3, 1, true, high ? with(P_CAP, 100e-9, 0) : with(P_RES, 1000, 0));
    wires_h(c, 4, 10, 1);
    put(c, 7, 1, false, high ? with(P_RES, 1000, 0) : with(P_CAP, 100e-9, 0));
    wires_v(c, 7, 2, 3);
    put(c, 10, 1, false, part_default(P_VOLTMETER));
    wires_v(c, 10, 2, 3);
    wires_h(c, 1, 10, 3);
    if (high) return L("RC high-pass: 3 on a part shows its response",
        "Passa-alto RC: 3 numa peca mostra a resposta");
    return L("RC low-pass: corner at 1.59 kHz. 1-3: graphs",
        "Passa-baixo RC: corte 1,59 kHz. 1-3: graficos");
}

//---
//  Saving the circuit between runs
//---

static bool circuit_valid(circuit_t const *c)
{
    if (!(c->freq >= FREQ_MIN && c->freq <= FREQ_MAX)) return false;
    for (int e = 0; e < EDGES; e++) {
        part_t const *p = &c->parts[e];
        int k = p->kind;
        if (k >= P_KINDS) return false;
        if ((k == P_RES || k == P_CAP || k == P_IND || k == P_SRC
            || k == P_LAMP) && !(p->value > 0)) return false;
        if (k == P_LAMP && !(p->value2 > 0)) return false;
        if ((k == P_IND || k == P_SRC) && !isfinite(p->value2)) return false;
    }
    return true;
}

/* The file is "ACC1" then the circuit, read and written in one go */
typedef struct {
    char magic[4];
    circuit_t c;
} save_t;

static bool circuit_load(circuit_t *c)
{
    static save_t s;
    FILE *fp = fopen(SAVE_PATH, "rb");
    if (!fp) return false;
    /* Size 1: this libc's fread returns bytes, not items */
    bool ok = fread(&s, 1, sizeof s, fp) == sizeof s
        && !memcmp(s.magic, "ACC1", 4) && circuit_valid(&s.c);
    fclose(fp);
    if (ok) *c = s.c;
    return ok;
}

static void circuit_save(circuit_t const *c)
{
    static save_t s = { .magic = "ACC1" };
    s.c = *c;
    FILE *fp = fopen(SAVE_PATH, "wb");
    if (!fp) return;
    fwrite(&s, 1, sizeof s, fp);
    fclose(fp);
}

//---
//  Main loop
//---

/* getkey() that gives up after a frame, to animate the current; 0 then */
static int getkey_frame(void)
{
    volatile int flag = 0;
    int t = timer_configure(TIMER_ANY, FRAME_US,
        GINT_CALL(timer_timeout, (void *)&flag));
    if (t < 0) return getkey().key;
    timer_start(t);
    key_event_t ev = getkey_opt(GETKEY_DEFAULT, &flag);
    timer_stop(t);
    return ev.type == KEYEV_NONE ? 0 : ev.key;
}

static bool any_current(void)
{
    if (res.status != S_OK) return false;
    for (int e = 0; e < EDGES; e++)
        if (circuit.parts[e].kind != P_NONE && c_abs(res.i[e]) > 1e-7)
            return true;
    return false;
}

static void solve(void)
{
    circuit_solve(&circuit, circuit.freq, &res);
}

/* Put a part on link e. The same part again turns a source around, and
   opens or closes a switch */
static void place(int e, kind_t kind)
{
    part_t *p = &circuit.parts[e];
    if (p->kind == kind && polar(kind)) p->flip ^= 1;
    else if (p->kind == kind && kind == P_SWITCH) p->closed ^= 1;
    else if (p->kind != kind) *p = part_default(kind);
}

int main(void)
{
    char const *message = NULL;
    int message_color = COLOR;
    int example = 1, t = 0;
    bool dirty = false, confirm_clear = false;

    lang_init();
    usb_screen_init();
    if (!circuit_load(&circuit)) message = load_example(&circuit, 0);
    solve();

    while (1) {
        draw(t, message, message_color);
        int key = any_current() ? getkey_frame() : getkey().key;
        if (!key) { t++; continue; }

        message = NULL;
        message_color = COLOR;
        bool was_confirm = confirm_clear;
        confirm_clear = false;
        int e = cursor_edge();
        part_t *p = e >= 0 ? &circuit.parts[e] : NULL;
        bool changed = false;
        int d = key_digit(key);

        if (key == KEY_EXIT) {
            if (drawing) { drawing = false; continue; }
            if (dirty) circuit_save(&circuit);
            return 1;
        }
        if (key == INFO_KEY) {
            info_show(L("AC Circuit", "Circuitos CA"), L("AC circuit "
                "simulator " APPS_VERSION, "Simulador de circuitos CA "
                APPS_VERSION), COLOR);
            continue;
        }

        int dx = key == KEY_LEFT ? -1 : key == KEY_RIGHT ? 1 : 0;
        int dy = key == KEY_UP ? -1 : key == KEY_DOWN ? 1 : 0;
        if (dx || dy) {
            if (drawing && cursor_point() >= 0) {
                /* Lay a wire on the link and go to the next point */
                if (!cursor_move(dx, dy)) continue;
                part_t *w = &circuit.parts[cursor_edge()];
                if (w->kind == P_NONE) {
                    *w = part_default(P_WIRE);
                    changed = true;
                }
                cursor_move(dx, dy);
            }
            else cursor_move(dx, dy);
        }
        else if (key == KEY_EXE) {
            if (drawing) drawing = false;
            else if (p && p->kind == P_SWITCH) {
                p->closed ^= 1;
                changed = true;
            }
            else if (p && editable(p->kind)) {
                edit_part(p);
                changed = true;
            }
        }
        else if (key >= KEY_F1 && key <= KEY_F5) {
            kind_t kind = fkey_kinds[key - KEY_F1];
            if (!p) {
                if (kind == P_WIRE) drawing = !drawing;
                continue;
            }
            place(e, kind);
            changed = true;
        }
        else if (key == KEY_F6) {
            if (!p) {
                message = L("Move onto a link to place a part",
                    "Va para uma ligacao para por uma peca");
                continue;
            }
            int kind = pick_part(t);
            if (kind < 0) continue;
            place(e, kind);
            changed = true;
        }
        else if (key == KEY_PLUS || key == KEY_MINUS) {
            step_freq(key == KEY_PLUS ? 1 : -1);
            changed = true;
        }
        else if (key == KEY_XOT) {
            edit_freq();
            changed = true;
        }
        else if (d >= 1 && d <= 3) {
            target_t tg = get_target();
            if (res.status != S_OK || tg.e < 0) {
                message = L("Nothing to show: the circuit has no solution",
                    "Nada a mostrar: o circuito nao tem solucao");
                message_color = C_RED;
                continue;
            }
            if (d == 1) view_phasors(tg);
            else if (d == 2) view_waves(tg);
            else {
                double f = circuit.freq;
                view_bode(tg);
                if (circuit.freq != f) changed = true;
            }
        }
        else if (key == KEY_DEL) {
            if (p && p->kind != P_NONE) {
                *p = part_default(P_NONE);
                changed = true;
            }
        }
        else if (key == KEY_ACON) {
            if (was_confirm) {
                double f = circuit.freq;
                memset(&circuit, 0, sizeof circuit);
                circuit.freq = f;
                drawing = false;
                changed = true;
            }
            else {
                confirm_clear = true;
                message = L("Press AC again to clear everything",
                    "Prima AC de novo para limpar tudo");
                message_color = C_RED;
            }
        }
        else if (key == KEY_VARS) {
            message = load_example(&circuit, example);
            example = (example + 1) % EXAMPLES;
            drawing = false;
            changed = true;
        }

        if (changed) {
            dirty = true;
            solve();
        }
    }
}
