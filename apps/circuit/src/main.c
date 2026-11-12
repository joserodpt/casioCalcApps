#include <gint/display.h>
#include <gint/keyboard.h>
#include <gint/timer.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "circuit.h"
#include "ui.h"
#include "utils.h"
#include "keys.h"
#include "info.h"
#include "version.h"
#include "i18n.h"
#include "usb_screen.h"

#define COLOR     C_RGB(2, 16, 12)
#define TINT      C_RGB(26, 30, 29)     /* behind the part at the cursor */
#define RES_COLOR C_RGB(18, 8, 2)
#define DOT_COLOR C_RGB(31, 18, 0)      /* moving current dots */
#define GRID_DOT  C_RGB(22, 22, 22)
#define AMP_COLOR C_RGB(0, 10, 26)      /* ammeter readings */
#define VOLT_COLOR C_RGB(22, 0, 16)     /* voltmeter readings */

/* Grid on screen: point (x, y) is at GX0 + x * STEP, GY0 + y * STEP */
#define GX0   22
#define GY0   30
#define STEP  32

#define FRAME_US  50000                 /* animation frame */
#define SAVE_PATH "/Circuit.cir"        /* \\fls0\Circuit.cir */

static circuit_t circuit;
static result_t res;

/* Cursor in doubled coordinates: even/even is a point, odd/even a
   horizontal link, even/odd a vertical one */
static int cx = 4, cy = 3;
static bool drawing;                    /* F1 on a point: arrows lay wire */

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

/* Parts with a direction: F3 or the parts menu again turns them around */
static bool polar(int kind)
{
    return kind == P_BATT || kind == P_DIODE || kind == P_LED;
}

/* Voltage between the ends of a link, NAN if not known */
static double across(int e)
{
    int a, b;
    edge_ends(e, &a, &b);
    if (res.status != S_OK || isnan(res.v[a]) || isnan(res.v[b])) return NAN;
    return fabs(res.v[a] - res.v[b]);
}

static int const led_colors[LED_COLORS] = { C_RGB(31, 4, 4),
    C_RGB(30, 26, 0), C_RGB(2, 25, 6), C_RGB(6, 12, 31), C_RGB(30, 30, 31) };

/* Mix two RGB565 colours: t = 0 gives a, t = 1 gives b */
static int mix(int a, int b, double t)
{
    int r = ((a >> 11) & 31) + (int)((((b >> 11) & 31) - ((a >> 11) & 31)) * t);
    int g = ((a >> 5) & 63) + (int)((((b >> 5) & 63) - ((a >> 5) & 63)) * t);
    int bl = (a & 31) + (int)(((b & 31) - (a & 31)) * t);
    return (r << 11) | (g << 5) | bl;
}

static double led_brightness(int e)
{
    if (res.status != S_OK) return 0;
    double b = fabs(res.i[e]) / LED_RATED;
    return b > 1 ? 1 : b;
}

static double lamp_brightness(int e)
{
    part_t const *p = &circuit.parts[e];
    if (res.status != S_OK) return 0;
    double b = res.p[e] / p->value2;
    return b > 1 ? 1 : b;
}

static void draw_part(int e)
{
    part_t const *p = &circuit.parts[e];
    frame_t f = frame_of(e, polar(p->kind) && p->flip);
    bool bad = e == res.bad_edge;
    int ink = bad ? C_RED : C_BLACK;

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

    case P_BATT:
        /* Long thin plate is +, short thick plate is - */
        flead(&f, 0, 12, ink);
        flead(&f, 20, STEP, ink);
        fbox(&f, 12, -10, 13, 11, ink, C_NONE);
        fbox(&f, 18, -5, 20, 6, ink, C_NONE);
        /* + and - signs, upright whatever the direction */
        int x, y;
        fmap(&f, 7, -10, &x, &y);
        drect(x - 2, y, x + 2, y, C_RED);
        drect(x, y - 2, x, y + 2, C_RED);
        fmap(&f, 25, -10, &x, &y);
        drect(x - 2, y, x + 2, y, ink);
        break;

    case P_LAMP: {
        double b = lamp_brightness(e);
        bool over = res.status == S_OK && res.p[e] > 1.2 * p->value2;
        int x, y;
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

    case P_SWITCH: {
        int x, y;
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
    }

    case P_DIODE:
    case P_LED: {
        /* Triangle from the anode, bar at the cathode */
        int px[3], py[3];
        bool led = p->kind == P_LED;
        double b = led ? led_brightness(e) : 0;
        int color = led ? led_colors[p->level % LED_COLORS] : ink;
        int fill = led ? mix(C_WHITE, color, b) : ink;
        int edge = led && p->level == LED_WHITE ? C_DARK : color;
        flead(&f, 0, 10, ink);
        flead(&f, 21, STEP, ink);
        if (led && b > 0.4) {
            int x, y;
            fmap(&f, 16, 0, &x, &y);
            dcircle(x + !f.h, y + f.h, 12, C_NONE, mix(C_WHITE, color, b));
        }
        fmap(&f, 10, -6, &px[0], &py[0]);
        fmap(&f, 10, 7, &px[1], &py[1]);
        fmap(&f, 21, 0, &px[2], &py[2]);
        if (f.h) py[2]++; else px[2]++;
        dpoly(px, py, 3, fill, led ? edge : ink);
        fbox(&f, 21, -6, 22, 7, led ? edge : ink, C_NONE);
        if (led) {
            /* Two small arrows: light going out */
            fline(&f, 13, -8, 17, -12, edge);
            fline(&f, 17, -8, 21, -12, edge);
            fline(&f, 17, -12, 15, -12, edge);
            fline(&f, 21, -12, 19, -12, edge);
        }
        break;
    }

    case P_AMMETER:
    case P_VOLTMETER: {
        int x, y;
        flead(&f, 0, 8, ink);
        flead(&f, 24, STEP, ink);
        fmap(&f, 16, 0, &x, &y);
        if (f.h) y++; else x++;
        dcircle(x, y, 8, C_WHITE, p->kind == P_AMMETER ? AMP_COLOR
            : VOLT_COLOR);
        dtext_opt(x + 1, y + 1, p->kind == P_AMMETER ? AMP_COLOR : VOLT_COLOR,
            C_NONE, DTEXT_CENTER, DTEXT_MIDDLE, p->kind == P_AMMETER ? "A" : "V");
        break;
    }

    case P_POT:
        flead(&f, 0, 7, ink);
        flead(&f, 25, STEP, ink);
        fbox(&f, 7, -5, 25, 6, C_RGB(31, 27, 20), RES_COLOR);
        fbox(&f, 8, -4, 24, 5, C_NONE, RES_COLOR);
        /* The arrow across means it can be adjusted */
        fline(&f, 6, 9, 26, -9, ink);
        fline(&f, 7, 9, 27, -9, ink);
        fline(&f, 27, -9, 22, -9, ink);
        fline(&f, 27, -9, 26, -4, ink);
        break;

    case P_FUSE: {
        int c = p->closed ? ink : C_RED;
        flead(&f, 0, 8, ink);
        flead(&f, 24, STEP, ink);
        fbox(&f, 8, -3, 24, 4, C_WHITE, c);
        if (p->closed) flead(&f, 8, 24, ink);
        else {
            fline(&f, 9, 0, 13, -2, C_RED);
            fline(&f, 23, 1, 19, 3, C_RED);
        }
        break;
    }
    }
}

/* Readings of the meters, next to them on the schematic */
static void draw_meter_label(int e)
{
    part_t const *p = &circuit.parts[e];
    if (res.status != S_OK) return;
    if (p->kind != P_AMMETER && p->kind != P_VOLTMETER) return;

    char buf[32];
    double v = p->kind == P_AMMETER ? fabs(res.i[e]) : across(e);
    if (isnan(v)) return;
    fmt_si(buf, v, p->kind == P_AMMETER ? "A" : "V", 3);

    int color = p->kind == P_AMMETER ? AMP_COLOR : VOLT_COLOR;
    frame_t f = frame_of(e, false);
    int x, y;
    fmap(&f, 16, 0, &x, &y);
    int w;
    dsize(buf, NULL, &w, NULL);
    if (f.h)
        dtext_opt(x, y - 10, color, C_WHITE, DTEXT_CENTER, DTEXT_BOTTOM, buf);
    else if (x + 12 + w < DWIDTH)
        dtext_opt(x + 12, y, color, C_WHITE, DTEXT_LEFT, DTEXT_MIDDLE, buf);
    else
        dtext_opt(x - 10, y, color, C_WHITE, DTEXT_RIGHT, DTEXT_MIDDLE, buf);
}

/* Dots that move along the parts in the direction of the current, faster
   when it is larger */
static void draw_current(int e, int t)
{
    double i = res.i[e];
    if (circuit.parts[e].kind == P_NONE || !(fabs(i) > 1e-7)) return;

    double speed = 0.5 + 0.5 * log10(fabs(i) * 1000);
    if (speed < 0.25) speed = 0.25;
    if (speed > 2.5) speed = 2.5;
    double off = fmod(t * speed, 16);

    frame_t f = frame_of(e, i < 0);
    for (int k = 0; k < 2; k++) {
        int u = (int)(off + 16 * k);
        if (u > STEP) break;
        int x, y;
        fmap(&f, u, 0, &x, &y);
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

static char const *kind_name(int kind)
{
    switch (kind) {
    case P_WIRE: return L("Wire", "Fio", "Fil");
    case P_RES: return L("Resistor", "Resistencia", "Resistance");
    case P_BATT: return L("Battery", "Pilha", "Pile");
    case P_LAMP: return L("Lamp", "Lampada", "Lampe");
    case P_SWITCH: return L("Switch", "Interruptor", "Interrupteur");
    case P_DIODE: return L("Diode", "Diodo", "Diode");
    case P_LED: return L("LED", "LED", "DEL");
    case P_AMMETER: return L("Ammeter", "Amperimetro", "Amperemetre");
    case P_VOLTMETER: return L("Voltmeter", "Voltimetro", "Voltmetre");
    case P_POT: return L("Potentiometer", "Potenciometro",
        "Potentiometre");
    case P_FUSE: return L("Fuse", "Fusivel", "Fusible");
    }
    return "";
}

static void draw_info(char const *message, int message_color)
{
    char a[32], b[32], c[32], line[96];
    int e = cursor_edge(), pt = cursor_point();
    bool ok = res.status == S_OK;
    int y1 = INFO_Y, y2 = INFO_Y + 13, y3 = INFO_Y + 26;

    dline(6, INFO_Y - 3, DWIDTH - 7, INFO_Y - 3, C_LIGHT);

    if (pt >= 0) {
        sprintf(line, L("Point %d, %d", "Ponto %d, %d", "Point %d, %d"),
            pt % NX + 1, pt / NX + 1);
        etext(10, y1, COLOR, DTEXT_LEFT, false, line);
        if (ok && !isnan(res.v[pt])) {
            sprintf(line, L("V = %s  (to the battery's -)",
                "V = %s  (em relacao ao - da pilha)",
                "V = %s  (relatif au - de la pile)"),
                fmt_si(a, res.v[pt], "V", 4));
            etext(10, y2, C_BLACK, DTEXT_LEFT, false, line);
        }
        else if (ok) etext(10, y2, C_DARK, DTEXT_LEFT, false,
            L("Not connected", "Sem ligacao", "Non connecte"));
    }
    else {
        part_t const *p = &circuit.parts[e];
        double i = fabs(res.i[e]), v = across(e);
        char const *status = NULL;
        int status_color = C_DARK;

        /* First line: the part and its values, and its state at the right */
        strcpy(line, kind_name(p->kind));
        switch (p->kind) {
        case P_NONE:
            strcpy(line, L("Empty: F1-F5 place a part",
                "Vazio: F1-F5 poem uma peca", "Vide: F1-F5 pour une piece"));
            break;
        case P_RES:
            sprintf(line + strlen(line), "  %s", fmt_si(a, p->value, S_OHM, 4));
            break;
        case P_BATT:
            sprintf(line + strlen(line), "  %s", fmt_si(a, p->value, "V", 4));
            break;
        case P_LAMP:
            sprintf(line + strlen(line), "  %s  %s  (%s)",
                fmt_si(a, p->value, "V", 3), fmt_si(b, p->value2, "W", 3),
                fmt_si(c, part_resistance(p), S_OHM, 3));
            if (ok) {
                bool over = res.p[e] > 1.2 * p->value2;
                sprintf(a, over ? L("OVERLOAD", "SOBRECARGA", "SURCHARGE")
                    : L("light %d%%", "luz %d%%", "eclat %d%%"),
                    (int)(lamp_brightness(e) * 100 + 0.5));
                status = a;
                status_color = over ? C_RED : C_RGB(20, 16, 0);
            }
            break;
        case P_SWITCH:
            sprintf(line + strlen(line), "  %s", p->closed
                ? L("closed", "fechado", "ferme")
                : L("open", "aberto", "ouvert"));
            break;
        case P_DIODE:
            sprintf(line + strlen(line), "  %s", fmt_si(a, p->value, "V", 3));
            if (ok) {
                status = i > 1e-9 ? L("conducting", "a conduzir", "passant")
                    : L("blocking", "bloqueado", "bloque");
                status_color = i > 1e-9 ? C_RGB(0, 18, 4) : C_DARK;
            }
            break;
        case P_LED:
            sprintf(line + strlen(line), "  %s", led_name(p->level));
            if (ok) {
                bool over = i > 1.5 * LED_RATED;
                if (over) strcpy(c, L("OVERLOAD", "SOBRECARGA", "SURCHARGE"));
                else if (i > 1e-9) sprintf(c,
                    L("light %d%%", "luz %d%%", "eclat %d%%"),
                    (int)(led_brightness(e) * 100 + 0.5));
                else strcpy(c, L("off", "apagado", "eteinte"));
                status = c;
                status_color = over ? C_RED : C_RGB(20, 16, 0);
            }
            break;
        case P_POT:
            sprintf(line + strlen(line), L("  %s at %d%%  (%s)",
                "  %s a %d%%  (%s)", "  %s a %d%%  (%s)"),
                fmt_si(a, p->value, S_OHM, 3),
                p->level, fmt_si(b, part_resistance(p), S_OHM, 3));
            break;
        case P_FUSE:
            sprintf(line + strlen(line), "  %s", fmt_si(a, p->value, "A", 3));
            status = p->closed ? "OK" : L("BLOWN", "FUNDIDO", "GRILLE");
            status_color = p->closed ? C_DARK : C_RED;
            break;
        }
        etext(10, y1, p->kind == P_NONE ? C_DARK : COLOR, DTEXT_LEFT, false,
            line);
        if (status) etext(DWIDTH - 10, y1, status_color, DTEXT_RIGHT, false,
            status);

        /* Second line: the readings */
        line[0] = '\0';
        if (!ok) ;
        else if (p->kind == P_RES || p->kind == P_LAMP || p->kind == P_POT
            || p->kind == P_DIODE || p->kind == P_LED)
            sprintf(line, "V = %s    I = %s    P = %s", fmt_si(a, v, "V", 4),
                fmt_si(b, i, "A", 4), fmt_si(c, res.p[e], "W", 4));
        else if (p->kind == P_BATT)
            sprintf(line, L("I = %s    P = %s supplied",
                "I = %s    P = %s fornecida", "I = %s    P = %s fournie"),
                fmt_si(a, i, "A", 4),
                fmt_si(b, res.p[e], "W", 4));
        else if (p->kind == P_VOLTMETER || (p->kind == P_SWITCH && !p->closed)
            || (p->kind == P_FUSE && !p->closed)) {
            if (!isnan(v)) sprintf(line, p->kind == P_VOLTMETER ? "V = %s"
                : L("V = %s across it", "V = %s aos terminais",
                    "V = %s aux bornes"),
                fmt_si(a, v, "V", 4));
        }
        else if (p->kind != P_NONE)
            sprintf(line, "I = %s", fmt_si(a, i, "A", 4));
        etext(10, y2, C_BLACK, DTEXT_LEFT, false, line);
    }

    /* Third line: a message, the circuit's state, or the total power */
    if (message) {
        etext(10, y3, message_color, DTEXT_LEFT, false, message);
        return;
    }
    if (res.status == S_SHORT) etext(10, y3, C_RED, DTEXT_LEFT, false,
        L("Short circuit across a battery!", "Curto-circuito numa pilha!",
          "Court-circuit sur une pile!"));
    else if (res.status == S_LOOP) etext(10, y3, C_RED, DTEXT_LEFT, false,
        L("Batteries in parallel or in a loop",
          "Pilhas em paralelo ou em ciclo",
          "Piles en parallele ou en boucle"));
    else if (res.status == S_TOOBIG) etext(10, y3, C_RED, DTEXT_LEFT, false,
        L("Too many parts to solve", "Pecas a mais para resolver",
          "Trop de pieces a calculer"));
    else if (res.status == S_EMPTY) etext(10, y3, C_DARK, DTEXT_LEFT, false,
        L("Add a battery and a load.  VARS: examples",
          "Junte uma pilha e uma carga.  VARS: exemplos",
          "Ajoutez pile et charge.  VARS: exemples"));
    else {
        sprintf(line, L("Total power %s", "Potencia total %s",
            "Puissance totale %s"),
            fmt_si(a, res.supplied, "W", 4));
        etext(10, y3, C_DARK, DTEXT_LEFT, false, line);
    }

    char const *hint = NULL;
    int kind = pt >= 0 ? P_NONE : circuit.parts[e].kind;
    if (pt >= 0) hint = L("F1: draw wire", "F1: desenhar fio",
        "F1: tracer fil");
    else if (kind == P_SWITCH) hint = L("EXE: on/off", "EXE: ligar",
        "EXE: basculer");
    else if (kind == P_POT) hint = L("+/-: turn  EXE: edit",
        "+/-: rodar  EXE: editar", "+/-: tourner  EXE: editer");
    else if (kind == P_FUSE && !circuit.parts[e].closed)
        hint = L("EXE: replace", "EXE: trocar", "EXE: changer");
    else if (kind >= P_RES && kind != P_AMMETER && kind != P_VOLTMETER)
        hint = L("EXE: edit", "EXE: editar", "EXE: editer");
    if (hint && res.status != S_LOOP && res.status != S_EMPTY)
        etext(DWIDTH - 10, y3, C_DARK, DTEXT_RIGHT, false, hint);
}

/* Parts in the F5 menu */
static kind_t const menu_kinds[] = { P_SWITCH, P_DIODE, P_LED, P_AMMETER,
    P_VOLTMETER, P_POT, P_FUSE };
#define MENU_COUNT (int)(sizeof menu_kinds / sizeof *menu_kinds)

static bool in_menu(int kind)
{
    for (int k = 0; k < MENU_COUNT; k++)
        if ((int)menu_kinds[k] == kind) return true;
    return false;
}

static void draw_screen(int t, char const *message, int message_color)
{
    dclear(C_WHITE);
    info_title(L("Circuit", "Circuitos", "Circuits"), COLOR);
    draw_grid(t);
    draw_info(message, message_color);

    int e = cursor_edge();
    int kind = e >= 0 ? circuit.parts[e].kind : drawing ? P_WIRE : P_NONE;
    static kind_t const fkey_kinds[] = { P_WIRE, P_RES, P_BATT, P_LAMP };
    char const *labels[] = { L("WIRE", "FIO", "FIL"), "R",
        L("BATT", "PILHA", "PILE"), L("LAMP", "LAMP.", "LAMPE"),
        L("PARTS", "PECAS", "PIECES"), L("DEL", "APAGAR", "SUPPR") };
    for (int k = 0; k < 6; k++) {
        bool on = k < 4 ? kind == (int)fkey_kinds[k] : k == 4 && in_menu(kind);
        if (on) fkey_button_color(k + 1, labels[k], COLOR);
        else fkey_button(k + 1, labels[k]);
    }
}

static void draw(int t, char const *message, int message_color)
{
    draw_screen(t, message, message_color);
    dupdate();
}

/* The F5 menu over the circuit; returns the chosen kind, or -1 */
static int pick_part(int t)
{
    static int sel = 0;
    int x1 = 104, x2 = 292, y1 = 24, row = 16;
    int y2 = y1 + 18 + MENU_COUNT * row + 4;

    while (1) {
        draw_screen(t, NULL, 0);
        drect_border(x1, y1, x2, y2, C_WHITE, 2, COLOR);
        drect(x1, y1, x2, y1 + 15, COLOR);
        dtext(x1 + 8, y1 + 3, C_WHITE, L("Parts", "Pecas", "Pieces"));
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
        if (key == KEY_EXIT || key == KEY_F5) return -1;
        if (key == KEY_UP) sel = (sel + MENU_COUNT - 1) % MENU_COUNT;
        if (key == KEY_DOWN) sel = (sel + 1) % MENU_COUNT;
        if (d >= 1 && d <= MENU_COUNT) sel = d - 1;
        if (key == KEY_EXE || (d >= 1 && d <= MENU_COUNT))
            return menu_kinds[sel];
    }
}

//---
//  Editing a part's values
//---

static part_t *editing;

static void edit_results(eform_t *f, int y)
{
    char a[32];
    int kind = editing->kind;
    if (kind == P_LAMP) {
        double v = field_value(&f->fields[0]);
        double p = field_value(&f->fields[1]);
        if (v > 0 && p > 0) result(y,
            L("Resistance", "Resistencia", "Resistance"), C_BLACK, "%s",
            fmt_si(a, v * v / p, S_OHM, 4));
        result_note(y + RESULT_STEP, L("A lamp at its rated voltage gives "
            "its rated power", "A tensao nominal da a potencia nominal",
            "La tension nominale donne la puissance nominale"));
    }
    else if (kind == P_LED) {
        int color = f->fields[0].choice;
        result(y, L("Forward voltage", "Tensao direta", "Tension directe"),
            C_BLACK, L("%s at 20 mA", "%s a 20 mA", "%s a 20 mA"),
            fmt_si(a, led_forward(color) + LED_RATED * 10, "V", 2));
        result_note(y + RESULT_STEP, L("Lights fully at 20 mA; more than "
            "30 mA is too much", "Acende bem a 20 mA; mais de 30 mA e demais",
            "Pleine lumiere a 20 mA; plus de 30 mA, c'est trop"));
    }
    else if (kind == P_POT) {
        double max = field_value(&f->fields[0]);
        double pos = field_value(&f->fields[1]);
        if (max > 0 && pos >= 0 && pos <= 100)
            result(y, L("Resistance now", "Resistencia atual",
                "Resistance actuelle"), C_BLACK,
                "%s", fmt_si(a, max * pos / 100, S_OHM, 4));
        result_note(y + RESULT_STEP, L("On the circuit, + and - turn it by "
            "5%", "No circuito, + e - rodam 5%",
            "Sur le circuit, + et - tournent de 5%"));
    }
    else if (kind == P_FUSE)
        result_note(y, L("It blows above this current; editing it "
            "puts in a new one", "Funde acima desta corrente; edita-lo "
            "poe um novo", "Grille au-dela de ce courant; l'editer en "
            "remet un neuf"));
    else if (kind == P_DIODE)
        result_note(y, L("Conducts from the triangle to the bar",
            "Conduz do triangulo para a barra",
            "Conduit du triangle vers la barre"));

    if (kind != P_LED) result_note(y + 3 * RESULT_STEP, L("F1-F6: p n " S_MU
        " m k M    EXIT: back", "F1-F6: p n " S_MU " m k M    EXIT: voltar",
        "F1-F6: p n " S_MU " m k M    EXIT: retour"));
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
    static char const *colors[LED_COLORS];
    memset(fields, 0, sizeof fields);
    eform_t f = { .title = kind_name(p->kind), .color = COLOR,
        .fields = fields, .count = 1, .cols = 1, .label_w = 150,
        .results = edit_results };
    editing = p;

    for (int k = 0; k < 2; k++) fields[k].kind = F_NUM;
    switch (p->kind) {
    case P_RES:
        fields[0].label = L("Resistance", "Resistencia", "Resistance");
        fields[0].unit = S_OHM;
        break;
    case P_BATT:
        fields[0].label = L("Voltage", "Tensao", "Tension");
        fields[0].unit = "V";
        break;
    case P_LAMP:
        f.count = 2;
        fields[0].label = L("Rated voltage", "Tensao nominal",
            "Tension nominale");
        fields[0].unit = "V";
        fields[1].label = L("Rated power", "Potencia nominal",
            "Puiss. nominale");
        fields[1].unit = "W";
        field_set(&fields[1], p->value2);
        break;
    case P_DIODE:
        fields[0].label = L("Forward voltage", "Tensao direta",
            "Tension directe");
        fields[0].unit = "V";
        break;
    case P_LED:
        for (int k = 0; k < LED_COLORS; k++) colors[k] = led_name(k);
        fields[0].kind = F_CHOICE;
        fields[0].label = L("Colour", "Cor", "Couleur");
        fields[0].options = colors;
        fields[0].option_count = LED_COLORS;
        fields[0].choice = p->level % LED_COLORS;
        break;
    case P_POT:
        f.count = 2;
        fields[0].label = L("Maximum", "Maximo", "Maximum");
        fields[0].unit = S_OHM;
        fields[1].label = L("Position", "Posicao", "Position");
        fields[1].unit = "%";
        field_set(&fields[1], p->level);
        break;
    case P_FUSE:
        fields[0].label = L("Rating", "Corrente nominal",
            "Courant nominal");
        fields[0].unit = "A";
        break;
    default:
        return;
    }
    if (p->kind != P_LED) field_set(&fields[0], p->value);
    eform_run(&f);

    switch (p->kind) {
    case P_RES:
        p->value = field_or(&fields[0], p->value, 1e-3, 1e9);
        break;
    case P_BATT:
    case P_LAMP:
        p->value = field_or(&fields[0], p->value, 1e-3, 1e4);
        if (p->kind == P_LAMP)
            p->value2 = field_or(&fields[1], p->value2, 1e-6, 1e5);
        break;
    case P_DIODE:
        p->value = field_or(&fields[0], p->value, 0.05, 10);
        break;
    case P_LED:
        p->level = fields[0].choice;
        break;
    case P_POT:
        p->value = field_or(&fields[0], p->value, 1e-3, 1e9);
        p->level = (int)(field_or(&fields[1], p->level, 0, 100) + 0.5);
        break;
    case P_FUSE:
        p->value = field_or(&fields[0], p->value, 1e-6, 1e3);
        p->closed = 1;
        break;
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
    if (kind == P_LAMP) p.value2 = value2;
    return p;
}

#define EXAMPLES 5

/* Returns the example's name */
static char const *load_example(circuit_t *c, int k)
{
    memset(c, 0, sizeof *c);

    if (k == 0) {
        /* Battery, switch, resistor and lamp in series: 9 V / 18 Ω */
        put(c, 2, 1, false, with(P_BATT, 9, 0));
        wires_v(c, 2, 2, 3);
        wires_h(c, 2, 4, 1);
        put(c, 4, 1, true, part_default(P_SWITCH));
        wires_h(c, 5, 6, 1);
        put(c, 6, 1, true, with(P_RES, 6, 0));
        wires_h(c, 7, 9, 1);
        put(c, 9, 1, false, with(P_LAMP, 6, 3));
        wires_v(c, 9, 2, 3);
        wires_h(c, 2, 9, 3);
        return L("Series: battery, switch, resistor, lamp",
            "Serie: pilha, interruptor, resistencia, lampada",
            "Serie: pile, interrupteur, resistance, lampe");
    }
    if (k == 1) {
        /* Two different lamps in parallel */
        put(c, 2, 1, false, with(P_BATT, 6, 0));
        wires_v(c, 2, 2, 3);
        wires_h(c, 2, 9, 1);
        put(c, 5, 1, false, with(P_LAMP, 6, 3));
        wires_v(c, 5, 2, 3);
        put(c, 9, 1, false, with(P_LAMP, 6, 1));
        wires_v(c, 9, 2, 3);
        wires_h(c, 2, 9, 3);
        return L("Parallel: two lamps on 6 V",
            "Paralelo: duas lampadas a 6 V",
            "Parallele: deux lampes sur 6 V");
    }
    if (k == 3) {
        /* Red LED with its resistor and an ammeter: (9 - 2) V / 340 Ω */
        put(c, 2, 1, false, with(P_BATT, 9, 0));
        wires_v(c, 2, 2, 3);
        wires_h(c, 2, 4, 1);
        put(c, 4, 1, true, with(P_RES, 330, 0));
        wires_h(c, 5, 6, 1);
        put(c, 6, 1, true, part_default(P_AMMETER));
        wires_h(c, 7, 9, 1);
        put(c, 9, 1, false, part_default(P_LED));
        wires_v(c, 9, 2, 3);
        wires_h(c, 2, 9, 3);
        return L("LED with a 330 " S_OHM " resistor and an ammeter",
            "LED com resistencia de 330 " S_OHM " e amperimetro",
            "DEL avec resistance de 330 " S_OHM " et amperemetre");
    }
    if (k == 4) {
        /* Dimmer: fuse, potentiometer and lamp, voltmeter on the lamp */
        put(c, 1, 1, false, with(P_BATT, 12, 0));
        wires_v(c, 1, 2, 3);
        wires_h(c, 1, 2, 1);
        put(c, 2, 1, true, part_default(P_FUSE));
        wires_h(c, 3, 4, 1);
        part_t pot = with(P_POT, 40, 0);
        put(c, 4, 1, true, pot);
        wires_h(c, 5, 10, 1);
        put(c, 8, 1, false, with(P_LAMP, 12, 6));
        wires_v(c, 8, 2, 3);
        put(c, 10, 1, false, part_default(P_VOLTMETER));
        wires_v(c, 10, 2, 3);
        wires_h(c, 1, 10, 3);
        return L("Dimmer: turn the potentiometer with + and -",
            "Regulador: rode o potenciometro com + e -",
            "Variateur: + et - tournent le potentiometre");
    }
    /* Voltage divider: 12 V across 1k + 2.2k + 4.7k */
    put(c, 1, 1, false, with(P_BATT, 12, 0));
    wires_v(c, 1, 2, 3);
    wires_h(c, 1, 3, 1);
    put(c, 3, 1, true, with(P_RES, 1000, 0));
    wires_h(c, 4, 6, 1);
    put(c, 6, 1, true, with(P_RES, 2200, 0));
    wires_h(c, 7, 9, 1);
    put(c, 9, 1, false, with(P_RES, 4700, 0));
    wires_v(c, 9, 2, 3);
    wires_h(c, 1, 9, 3);
    return L("Voltage divider: 12 V on 1k, 2.2k, 4.7k",
        "Divisor de tensao: 12 V em 1k, 2.2k, 4.7k",
        "Diviseur de tension: 12 V, 1k, 2.2k, 4.7k");
}

//---
//  Saving the circuit between runs
//---

static bool circuit_valid(circuit_t const *c)
{
    for (int e = 0; e < EDGES; e++) {
        part_t const *p = &c->parts[e];
        int k = p->kind;
        if (k >= P_KINDS) return false;
        if ((k == P_RES || k == P_BATT || k == P_LAMP || k == P_DIODE
            || k == P_POT || k == P_FUSE) && !(p->value > 0)) return false;
        if (k == P_LAMP && !(p->value2 > 0)) return false;
        if (k == P_LED && p->level >= LED_COLORS) return false;
        if (k == P_POT && p->level > 100) return false;
    }
    return true;
}

/* The file is "CIR1" then the parts, read and written in one go */
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
        && !memcmp(s.magic, "CIR1", 4) && circuit_valid(&s.c);
    fclose(fp);
    if (ok) *c = s.c;
    return ok;
}

static void circuit_save(circuit_t const *c)
{
    static save_t s = { .magic = "CIR1" };
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
        if (circuit.parts[e].kind != P_NONE && fabs(res.i[e]) > 1e-7)
            return true;
    return false;
}

/* Solve, blowing fuses one by one; returns how many blew */
static int solve(void)
{
    int blown = 0;
    circuit_solve(&circuit, &res);
    while (blown < EDGES && circuit_blow_fuses(&circuit, &res)) {
        blown++;
        circuit_solve(&circuit, &res);
    }
    return blown;
}

/* Put a part on link e. The same part again turns a battery, diode or LED
   around, and opens or closes a switch */
static void place(int e, kind_t kind)
{
    part_t *p = &circuit.parts[e];
    if (p->kind == kind && polar(kind)) p->flip ^= 1;
    else if (p->kind == kind && kind == P_SWITCH) p->closed ^= 1;
    else if (p->kind != kind) *p = part_default(kind);
}

int main(void)
{
    static kind_t const fkey_kinds[] = { P_WIRE, P_RES, P_BATT, P_LAMP };
    char const *message = NULL;
    int message_color = COLOR;
    int example = 0, t = 0;
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

        if (key == KEY_EXIT) {
            if (drawing) { drawing = false; continue; }
            if (dirty) circuit_save(&circuit);
            return 1;
        }
        if (key == INFO_KEY) {
            info_show(L("Circuit", "Circuitos", "Circuits"),
                L("DC circuit simulator " APPS_VERSION,
                "Simulador de circuitos " APPS_VERSION,
                "Simulateur de circuits CC " APPS_VERSION), COLOR);
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
            else if (p && p->kind >= P_RES && p->kind != P_AMMETER
                && p->kind != P_VOLTMETER) {
                edit_part(p);
                changed = true;
            }
        }
        else if (key >= KEY_F1 && key <= KEY_F4) {
            kind_t kind = fkey_kinds[key - KEY_F1];
            if (!p) {
                if (kind == P_WIRE) drawing = !drawing;
                continue;
            }
            place(e, kind);
            changed = true;
        }
        else if (key == KEY_F5) {
            if (!p) {
                message = L("Move onto a link to place a part",
                    "Va para uma ligacao para por uma peca",
                    "Allez sur un lien pour poser une piece");
                continue;
            }
            int kind = pick_part(t);
            if (kind < 0) continue;
            place(e, kind);
            changed = true;
        }
        else if ((key == KEY_PLUS || key == KEY_MINUS) && p
            && p->kind == P_POT) {
            int level = p->level + (key == KEY_PLUS ? 5 : -5);
            p->level = level < 0 ? 0 : level > 100 ? 100 : level;
            changed = true;
        }
        else if (key == KEY_F6 || key == KEY_DEL) {
            if (p && p->kind != P_NONE) {
                *p = part_default(P_NONE);
                changed = true;
            }
        }
        else if (key == KEY_ACON) {
            if (was_confirm) {
                memset(&circuit, 0, sizeof circuit);
                drawing = false;
                changed = true;
            }
            else {
                confirm_clear = true;
                message = L("Press AC again to clear everything",
                    "Prima AC de novo para limpar tudo",
                    "Rappuyez sur AC pour tout effacer");
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
            if (solve()) {
                message = L("A fuse blew! EXE on it puts in a new one",
                    "Um fusivel fundiu! EXE nele poe um novo",
                    "Fusible grille! EXE dessus le remplace");
                message_color = C_RED;
            }
        }
    }
}
