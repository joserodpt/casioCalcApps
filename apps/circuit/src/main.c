#include <gint/display.h>
#include <gint/keyboard.h>
#include <gint/timer.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "circuit.h"
#include "ui.h"
#include "utils.h"
#include "info.h"
#include "version.h"
#include "i18n.h"

#define COLOR     C_RGB(2, 16, 12)
#define TINT      C_RGB(26, 30, 29)     /* behind the part at the cursor */
#define RES_COLOR C_RGB(18, 8, 2)
#define DOT_COLOR C_RGB(31, 18, 0)      /* moving current dots */
#define GRID_DOT  C_RGB(22, 22, 22)

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
    frame_t f = frame_of(e, p->kind == P_BATT && p->flip);
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
    }
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
    case P_WIRE: return L("Wire", "Fio");
    case P_RES: return L("Resistor", "Resistencia");
    case P_BATT: return L("Battery", "Pilha");
    case P_LAMP: return L("Lamp", "Lampada");
    case P_SWITCH: return L("Switch", "Interruptor");
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
        sprintf(line, L("Point %d, %d", "Ponto %d, %d"), pt % NX + 1,
            pt / NX + 1);
        etext(10, y1, COLOR, DTEXT_LEFT, false, line);
        if (ok && !isnan(res.v[pt])) {
            sprintf(line, L("V = %s  (to the battery's -)",
                "V = %s  (em relacao ao - da pilha)"),
                fmt_si(a, res.v[pt], "V", 4));
            etext(10, y2, C_BLACK, DTEXT_LEFT, false, line);
        }
        else if (ok) etext(10, y2, C_DARK, DTEXT_LEFT, false,
            L("Not connected", "Sem ligacao"));
    }
    else {
        part_t const *p = &circuit.parts[e];
        double i = fabs(res.i[e]);

        if (p->kind == P_NONE)
            etext(10, y1, C_DARK, DTEXT_LEFT, false,
                L("Empty: F1-F5 place a part", "Vazio: F1-F5 poem uma peca"));
        else if (p->kind == P_RES)
            sprintf(line, "%s  %s", kind_name(p->kind),
                fmt_si(a, p->value, S_OHM, 4));
        else if (p->kind == P_BATT)
            sprintf(line, "%s  %s", kind_name(p->kind),
                fmt_si(a, p->value, "V", 4));
        else if (p->kind == P_LAMP)
            sprintf(line, "%s  %s  %s  (%s)", kind_name(p->kind),
                fmt_si(a, p->value, "V", 3), fmt_si(b, p->value2, "W", 3),
                fmt_si(c, part_resistance(p), S_OHM, 3));
        else if (p->kind == P_SWITCH)
            sprintf(line, "%s  %s", kind_name(p->kind), p->closed
                ? L("closed", "fechado") : L("open", "aberto"));
        else strcpy(line, kind_name(p->kind));
        if (p->kind != P_NONE)
            etext(10, y1, COLOR, DTEXT_LEFT, false, line);

        if (ok && p->kind == P_LAMP) {
            double bright = lamp_brightness(e);
            bool over = res.p[e] > 1.2 * p->value2;
            sprintf(line, over ? L("OVERLOAD", "SOBRECARGA")
                : L("light %d%%", "luz %d%%"), (int)(bright * 100 + 0.5));
            etext(DWIDTH - 10, y1, over ? C_RED : C_RGB(20, 16, 0),
                DTEXT_RIGHT, false, line);
        }

        line[0] = '\0';
        if (ok && (p->kind == P_RES || p->kind == P_LAMP))
            sprintf(line, "V = %s    I = %s    P = %s",
                fmt_si(a, i * part_resistance(p), "V", 4),
                fmt_si(b, i, "A", 4), fmt_si(c, res.p[e], "W", 4));
        else if (ok && p->kind == P_BATT)
            sprintf(line, L("I = %s    P = %s supplied",
                "I = %s    P = %s fornecida"), fmt_si(a, i, "A", 4),
                fmt_si(b, res.p[e], "W", 4));
        else if (ok && (p->kind == P_WIRE || p->kind == P_SWITCH)) {
            int pa, pb;
            edge_ends(e, &pa, &pb);
            if (p->kind == P_SWITCH && !p->closed) {
                if (!isnan(res.v[pa]) && !isnan(res.v[pb]))
                    sprintf(line, L("V = %s across it", "V = %s aos terminais"),
                        fmt_si(a, fabs(res.v[pa] - res.v[pb]), "V", 4));
            }
            else sprintf(line, "I = %s", fmt_si(a, i, "A", 4));
        }
        etext(10, y2, C_BLACK, DTEXT_LEFT, false, line);
    }

    /* Third line: a message, the circuit's state, or the total power */
    if (message) {
        etext(10, y3, message_color, DTEXT_LEFT, false, message);
        return;
    }
    if (res.status == S_SHORT) etext(10, y3, C_RED, DTEXT_LEFT, false,
        L("Short circuit across a battery!", "Curto-circuito numa pilha!"));
    else if (res.status == S_LOOP) etext(10, y3, C_RED, DTEXT_LEFT, false,
        L("Batteries in parallel or in a loop", "Pilhas em paralelo ou em ciclo"));
    else if (res.status == S_TOOBIG) etext(10, y3, C_RED, DTEXT_LEFT, false,
        L("Too many parts to solve", "Pecas a mais para resolver"));
    else if (res.status == S_EMPTY) etext(10, y3, C_DARK, DTEXT_LEFT, false,
        L("Add a battery and a load.  VARS: examples",
          "Junte uma pilha e uma carga.  VARS: exemplos"));
    else {
        sprintf(line, L("Total power %s", "Potencia total %s"),
            fmt_si(a, res.supplied, "W", 4));
        etext(10, y3, C_DARK, DTEXT_LEFT, false, line);
    }

    char const *hint = NULL;
    if (pt >= 0) hint = L("F1: draw wire", "F1: desenhar fio");
    else if (circuit.parts[e].kind == P_SWITCH)
        hint = L("EXE: on/off", "EXE: ligar");
    else if (circuit.parts[e].kind >= P_RES)
        hint = L("EXE: edit", "EXE: editar");
    if (hint && res.status != S_LOOP && res.status != S_EMPTY)
        etext(DWIDTH - 10, y3, C_DARK, DTEXT_RIGHT, false, hint);
}

static void draw(int t, char const *message, int message_color)
{
    dclear(C_WHITE);
    info_title(L("Circuit", "Circuitos"), COLOR);
    draw_grid(t);
    draw_info(message, message_color);

    int e = cursor_edge();
    int kind = e >= 0 ? circuit.parts[e].kind : drawing ? P_WIRE : P_NONE;
    char const *labels[] = { L("WIRE", "FIO"), "R", L("BATT", "PILHA"),
        L("LAMP", "LAMP."), L("SWITCH", "INTER."), L("DEL", "APAGAR") };
    for (int k = 0; k < 6; k++) {
        if (k + 1 == kind) fkey_button_color(k + 1, labels[k], COLOR);
        else fkey_button(k + 1, labels[k]);
    }
    dupdate();
}

//---
//  Editing a part's values
//---

static part_t *editing;

static void edit_results(eform_t *f, int y)
{
    char a[32];
    if (editing->kind == P_LAMP) {
        double v = field_value(&f->fields[0]);
        double p = field_value(&f->fields[1]);
        if (v > 0 && p > 0) result(y, L("Resistance", "Resistencia"),
            C_BLACK, "%s", fmt_si(a, v * v / p, S_OHM, 4));
        result_note(y + RESULT_STEP, L("A lamp at its rated voltage gives "
            "its rated power", "A tensao nominal da a potencia nominal"));
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
    eform_t f = { .color = COLOR, .fields = fields, .count = 1, .cols = 1,
        .label_w = 130, .results = edit_results };
    editing = p;

    for (int k = 0; k < 2; k++) fields[k].kind = F_NUM;
    if (p->kind == P_RES) {
        f.title = kind_name(P_RES);
        fields[0].label = L("Resistance", "Resistencia");
        fields[0].unit = S_OHM;
    }
    else if (p->kind == P_BATT) {
        f.title = kind_name(P_BATT);
        fields[0].label = L("Voltage", "Tensao");
        fields[0].unit = "V";
    }
    else {
        f.title = kind_name(P_LAMP);
        f.count = 2;
        fields[0].label = L("Rated voltage", "Tensao nominal");
        fields[0].unit = "V";
        fields[1].label = L("Rated power", "Potencia nominal");
        fields[1].unit = "W";
        field_set(&fields[1], p->value2);
    }
    field_set(&fields[0], p->value);
    eform_run(&f);

    if (p->kind == P_RES) p->value = field_or(&fields[0], p->value, 1e-3, 1e9);
    else if (p->kind == P_BATT)
        p->value = field_or(&fields[0], p->value, 1e-3, 1e4);
    else {
        p->value = field_or(&fields[0], p->value, 1e-3, 1e4);
        p->value2 = field_or(&fields[1], p->value2, 1e-6, 1e5);
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

#define EXAMPLES 3

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
            "Serie: pilha, interruptor, resistencia, lampada");
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
            "Paralelo: duas lampadas a 6 V");
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
        "Divisor de tensao: 12 V em 1k, 2.2k, 4.7k");
}

//---
//  Saving the circuit between runs
//---

static bool circuit_valid(circuit_t const *c)
{
    for (int e = 0; e < EDGES; e++) {
        part_t const *p = &c->parts[e];
        if (p->kind > P_SWITCH) return false;
        if (p->kind >= P_RES && p->kind <= P_LAMP && !(p->value > 0))
            return false;
        if (p->kind == P_LAMP && !(p->value2 > 0)) return false;
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

int main(void)
{
    static kind_t const fkey_kinds[] = { P_WIRE, P_RES, P_BATT, P_LAMP,
        P_SWITCH };
    char const *message = NULL;
    int message_color = COLOR;
    int example = 0, t = 0;
    bool dirty = false, confirm_clear = false;

    lang_init();
    if (!circuit_load(&circuit)) message = load_example(&circuit, 0);
    circuit_solve(&circuit, &res);

    while (1) {
        draw(t, message, message_color);
        int key = any_current() ? getkey_frame() : getkey().key;
        if (!key) { t++; continue; }

        message = NULL;
        message_color = COLOR;
        bool was_confirm = confirm_clear;
        confirm_clear = false;
        int e = cursor_edge();
        bool changed = false;

        if (key == KEY_EXIT) {
            if (drawing) { drawing = false; continue; }
            if (dirty) circuit_save(&circuit);
            return 1;
        }
        if (key == INFO_KEY) {
            info_show(L("Circuit", "Circuitos"), L("DC circuit simulator "
                APPS_VERSION, "Simulador de circuitos " APPS_VERSION), COLOR);
            continue;
        }

        int dx = key == KEY_LEFT ? -1 : key == KEY_RIGHT ? 1 : 0;
        int dy = key == KEY_UP ? -1 : key == KEY_DOWN ? 1 : 0;
        if (dx || dy) {
            if (drawing && cursor_point() >= 0) {
                /* Lay a wire on the link and go to the next point */
                if (!cursor_move(dx, dy)) continue;
                part_t *p = &circuit.parts[cursor_edge()];
                if (p->kind == P_NONE) {
                    *p = part_default(P_WIRE);
                    changed = true;
                }
                cursor_move(dx, dy);
            }
            else cursor_move(dx, dy);
        }
        else if (key == KEY_EXE) {
            if (drawing) drawing = false;
            else if (e >= 0 && circuit.parts[e].kind == P_SWITCH) {
                circuit.parts[e].closed ^= 1;
                changed = true;
            }
            else if (e >= 0 && circuit.parts[e].kind >= P_RES) {
                edit_part(&circuit.parts[e]);
                changed = true;
            }
        }
        else if (key >= KEY_F1 && key <= KEY_F5) {
            kind_t kind = fkey_kinds[key - KEY_F1];
            if (e < 0) {
                if (kind == P_WIRE) drawing = !drawing;
                continue;
            }
            part_t *p = &circuit.parts[e];
            if (p->kind == kind && kind == P_BATT) p->flip ^= 1;
            else if (p->kind == kind && kind == P_SWITCH) p->closed ^= 1;
            else if (p->kind != kind) *p = part_default(kind);
            changed = true;
        }
        else if (key == KEY_F6 || key == KEY_DEL) {
            if (e >= 0 && circuit.parts[e].kind != P_NONE) {
                circuit.parts[e] = part_default(P_NONE);
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
            circuit_solve(&circuit, &res);
        }
    }
}
