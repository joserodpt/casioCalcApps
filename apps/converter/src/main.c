#include <gint/display.h>
#include <gint/keyboard.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "utils.h"
#include "info.h"
#include "version.h"
#include "i18n.h"
#include "fmt.h"
#include "ui.h"
#include "units.h"
#include "periodic.h"

#define COLOR   C_RGB(5, 14, 20)
#define NA      6.02214076e23       /* Avogadro */
#define R_GAS   8.314462618         /* J/(mol K) */
#define VM_STP  22.413969           /* L/mol at STP (0 C, 1 atm) */

//---
//  Generic numeric form: a list of fields, one may be left empty and solved
//---

typedef struct form form_t;
struct form {
    char const *title_en, *title_pt;
    int color;
    struct {
        char const *label_en, *label_pt, *unit;
        numfield_t f;
    } fields[5];
    int count;
    int sel;
    void (*results)(form_t *, int y);   /* draw results from y */
};

static double fv(form_t *f, int i) { return numfield_value(&f->fields[i].f); }
static bool fempty(form_t *f, int i) { return f->fields[i].f.text[0] == '\0'; }

static void form_draw(form_t *f)
{
    dclear(C_WHITE);
    info_title(lang_current == L_PT ? f->title_pt : f->title_en, f->color);

    for (int i = 0; i < f->count; i++) {
        int y = 24 + i * 20;
        bool sel = i == f->sel;
        char const *lab = lang_current == L_PT ? f->fields[i].label_pt
            : f->fields[i].label_en;
        dtext(10, y + 3, sel ? f->color : C_BLACK, lab);
        numfield_draw(&f->fields[i].f, 120, y, 180, sel, f->color);
        dtext(306, y + 3, C_DARK, f->fields[i].unit);
    }

    int ry = 24 + f->count * 20 + 6;
    dline(6, ry - 4, DWIDTH - 7, ry - 4, C_LIGHT);
    f->results(f, ry);
    dupdate();
}

static void form_run(form_t *f)
{
    while (1) {
        form_draw(f);
        int key = getkey().key;
        if (key == KEY_EXIT) return;
        if (key == INFO_KEY) {
            info_show("Converter", lang_current == L_PT ? f->title_pt
                : f->title_en, f->color);
            continue;
        }
        if (key == KEY_UP) f->sel = (f->sel + f->count - 1) % f->count;
        else if (key == KEY_DOWN || key == KEY_EXE)
            f->sel = (f->sel + 1) % f->count;
        else numfield_key(&f->fields[f->sel].f, key);
    }
}

/* Count empty fields; return the index of the single empty one or -1 */
static int one_empty(form_t *f)
{
    int idx = -1, n = 0;
    for (int i = 0; i < f->count; i++)
        if (fempty(f, i)) { idx = i; n++; }
    return n == 1 ? idx : -1;
}

//---
//  Unit converter
//---

static void units_screen(void)
{
    int cat = 2;        /* start on Volume (dm3/cm3) */
    int from = 1;
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
            L("<> category   ^v input unit",
              "<> categoria   ^v unidade"));

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
        if (key == INFO_KEY) { info_show("Converter",
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
//  Periodic grid (shared by the molar-mass and browser screens)
//---

#define GRID_X   6
#define GRID_Y   18
#define CELL_W   21
#define CELL_H   14
#define FBLOCK_GAP 6

static void grid_cell_xy(int row, int col, int *x, int *y)
{
    *x = GRID_X + col * CELL_W;
    *y = GRID_Y + row * CELL_H + (row >= 7 ? FBLOCK_GAP : 0);
}

static void draw_grid(int cursor_z)
{
    for (int z = 1; z <= ELEMENT_COUNT; z++) {
        int row, col, x, y;
        element_grid(z, &row, &col);
        grid_cell_xy(row, col, &x, &y);
        int cat = element_category(z);
        bool cur = z == cursor_z;
        int bg = cur ? C_WHITE : category_color(cat);
        drect(x, y, x + CELL_W - 2, y + CELL_H - 2, bg);
        if (cur) drect_border(x, y, x + CELL_W - 2, y + CELL_H - 2,
            C_WHITE, 1, C_BLACK);
        dtext_opt(x + CELL_W / 2 - 1, y + 2, cur ? C_BLACK : C_WHITE, C_NONE,
            DTEXT_CENTER, DTEXT_TOP, periodic_table[z - 1].symbol);
    }
}

/* Move the cursor within the grid; returns the new atomic number */
static int grid_move(int z, int key)
{
    int row, col;
    element_grid(z, &row, &col);
    int dr = 0, dc = 0;
    if (key == KEY_UP) dr = -1;
    else if (key == KEY_DOWN) dr = 1;
    else if (key == KEY_LEFT) dc = -1;
    else if (key == KEY_RIGHT) dc = 1;
    else return z;

    for (int step = 0; step < 20; step++) {
        row += dr; col += dc;
        if (row < 0 || row > 8 || col < 0 || col > 17) return z;
        int nz = element_at_grid(row, col);
        if (nz) return nz;
    }
    return z;
}

//---
//  Molar mass: build a formula by picking elements
//---

static void molar_screen(void)
{
    int z = 6;                          /* cursor on Carbon */
    struct { int z, count; } parts[20];
    int np = 0;

    while (1) {
        dclear(C_WHITE);
        info_title(L("Molar mass", "Massa molar"), COLOR);
        draw_grid(z);

        /* Build the formula string and total mass */
        char formula[64] = "";
        double mass = 0;
        for (int i = 0; i < np; i++) {
            element_t const *e = &periodic_table[parts[i].z - 1];
            char seg[12];
            if (parts[i].count == 1) sprintf(seg, "%s", e->symbol);
            else sprintf(seg, "%s%d", e->symbol, parts[i].count);
            strncat(formula, seg, sizeof formula - strlen(formula) - 1);
            mass += e->weight * parts[i].count;
        }

        int y = 160;
        element_t const *cur = &periodic_table[z - 1];
        dprint(10, y, C_DARK, "%d", cur->z);
        dtext(34, y, COLOR, cur->symbol);
        dprint(70, y, C_BLACK, "%s", cur->name);
        char mb[32];
        dtext_opt(DWIDTH - 10, y, C_DARK, C_NONE, DTEXT_RIGHT, DTEXT_TOP,
            fmt_g(mb, cur->weight, 6));

        y = 176;
        dline(6, y - 2, DWIDTH - 7, y - 2, C_LIGHT);
        dtext(10, y, C_DARK, formula[0] ? formula : L("(empty)", "(vazio)"));
        if (np) {
            char b[40];
            sprintf(b, "M = %s g/mol", fmt_g(mb, mass, 7));
            dtext_opt(DWIDTH - 10, y, COLOR, C_NONE, DTEXT_RIGHT, DTEXT_TOP, b);
        }
        dtext(10, 192, C_DARK, L("EXE add  0-9 count  DEL undo  AC clear",
            "EXE junta  0-9 qtd  DEL anula  AC limpa"));

        fkey_button(1, "+/-");
        dupdate();

        int key = getkey().key;
        int d = -1;
        switch (key) {
        case KEY_0: d=0; break; case KEY_1: d=1; break; case KEY_2: d=2; break;
        case KEY_3: d=3; break; case KEY_4: d=4; break; case KEY_5: d=5; break;
        case KEY_6: d=6; break; case KEY_7: d=7; break; case KEY_8: d=8; break;
        case KEY_9: d=9; break;
        }
        if (key == KEY_EXIT) return;
        else if (key == INFO_KEY) info_show("Converter",
            L("Molar mass", "Massa molar"), COLOR);
        else if (key == KEY_UP || key == KEY_DOWN || key == KEY_LEFT
            || key == KEY_RIGHT) z = grid_move(z, key);
        else if (key == KEY_EXE) {
            /* Merge with the previous part if it is the same element */
            if (np > 0 && parts[np - 1].z == z) parts[np - 1].count++;
            else if (np < 20) { parts[np].z = z; parts[np].count = 1; np++; }
        }
        else if (d >= 0 && np > 0) {
            int c = parts[np - 1].count;
            c = (c >= 10 ? 0 : c) * 10 + d;     /* first digit replaces 1 */
            if (c == 0) c = 1;
            parts[np - 1].count = c > 999 ? 999 : c;
        }
        else if (key == KEY_DEL) { if (np) np--; }
        else if (key == KEY_ACON) np = 0;
    }
}

//---
//  Periodic table browser
//---

static void periodic_screen(void)
{
    int z = 1;
    while (1) {
        dclear(C_WHITE);
        info_title(L("Periodic table", "Tabela periodica"), COLOR);
        draw_grid(z);

        element_t const *e = &periodic_table[z - 1];
        int cat = element_category(z);
        int y = 162;
        drect(10, y, 54, y + 30, category_color(cat));
        font_t const *old = dfont(&font_big);
        dtext_opt(32, y + 8, C_WHITE, C_NONE, DTEXT_CENTER, DTEXT_TOP,
            e->symbol);
        dfont(old);

        dprint(66, y, C_BLACK, "%s", e->name);
        dprint(66, y + 15, C_DARK, "Z = %d", e->z);
        char b[32];
        dtext_opt(DWIDTH - 10, y, C_BLACK, C_NONE, DTEXT_RIGHT, DTEXT_TOP,
            fmt_g(b, e->weight, 6));
        dtext_opt(DWIDTH - 10, y + 15, C_DARK, C_NONE, DTEXT_RIGHT, DTEXT_TOP,
            "g/mol");
        dtext(66, y + 30, C_DARK, category_name(cat));
        dupdate();

        int key = getkey().key;
        if (key == KEY_EXIT) return;
        if (key == INFO_KEY) info_show("Converter",
            L("Periodic table", "Tabela periodica"), COLOR);
        else z = grid_move(z, key);
    }
}

//---
//  Mole calculator
//---

static void mole_results(form_t *f, int y)
{
    char a[48], b[32];
    double n = fv(f, 0), M = fv(f, 1);
    if (isnan(n)) { conv_result(y, 120, L("Enter n (mol)", "Introduza n"),
        C_DARK, ""); return; }

    conv_result(y, 120, L("Particles N", "Particulas N"), COLOR,
        fmt_g(b, n * NA, 7));
    if (isnan(M)) conv_result(y + 16, 120, L("Mass m", "Massa m"), C_BLACK,
        L("need M", "falta M"));
    else { sprintf(a, "%s g", fmt_g(b, n * M, 7));
        conv_result(y + 16, 120, L("Mass m", "Massa m"), C_BLACK, a); }
    sprintf(a, "%s L", fmt_g(b, n * VM_STP, 7));
    conv_result(y + 32, 120, L("Gas V (STP)", "Gas V (PTN)"), C_BLACK, a);
    dtext(10, y + 54, C_DARK, "N = n Na,  m = n M,  V = n 22.4");
}

static void mole_screen(void)
{
    static form_t f = {
        .title_en = "Mole calculator", .title_pt = "Calculo de moles",
        .color = COLOR, .count = 2,
        .fields = {
            { "n", "n", "mol" },
            { "M (molar mass)", "M (massa molar)", "g/mol" },
        },
        .results = mole_results,
    };
    form_run(&f);
}

//---
//  Concentration  c = n / V
//---

static void conc_results(form_t *f, int y)
{
    char a[48], b[32];
    int unknown = one_empty(f);
    double c = fv(f, 0), n = fv(f, 1), V = fv(f, 2);

    if (unknown == 0 && !isnan(n) && !isnan(V) && V != 0)
        sprintf(a, "c = %s mol/L", fmt_g(b, n / V, 7));
    else if (unknown == 1 && !isnan(c) && !isnan(V))
        sprintf(a, "n = %s mol", fmt_g(b, c * V, 7));
    else if (unknown == 2 && !isnan(c) && !isnan(n) && c != 0)
        sprintf(a, "V = %s L", fmt_g(b, n / c, 7));
    else { strcpy(a, L("Leave one field empty", "Deixe um campo vazio")); }
    dtext(10, y, COLOR, a);
    dtext(10, y + 20, C_DARK, "c = n / V");
}

static void conc_screen(void)
{
    static form_t f = {
        .title_en = "Concentration", .title_pt = "Concentracao",
        .color = COLOR, .count = 3,
        .fields = {
            { "c", "c", "mol/L" },
            { "n", "n", "mol" },
            { "V", "V", "L" },
        },
        .results = conc_results,
    };
    form_run(&f);
}

//---
//  Ideal gas  P V = n R T   (P in kPa, V in L, T in K)
//---

static void gas_results(form_t *f, int y)
{
    char a[48], b[32];
    int unknown = one_empty(f);
    double P = fv(f, 0), V = fv(f, 1), n = fv(f, 2), T = fv(f, 3);

    if (unknown == 0 && !isnan(V) && !isnan(n) && !isnan(T) && V != 0)
        sprintf(a, "P = %s kPa", fmt_g(b, n * R_GAS * T / V, 7));
    else if (unknown == 1 && !isnan(P) && !isnan(n) && !isnan(T) && P != 0)
        sprintf(a, "V = %s L", fmt_g(b, n * R_GAS * T / P, 7));
    else if (unknown == 2 && !isnan(P) && !isnan(V) && !isnan(T) && T != 0)
        sprintf(a, "n = %s mol", fmt_g(b, P * V / (R_GAS * T), 7));
    else if (unknown == 3 && !isnan(P) && !isnan(V) && !isnan(n) && n != 0)
        sprintf(a, "T = %s K", fmt_g(b, P * V / (R_GAS * n), 7));
    else strcpy(a, L("Leave one field empty", "Deixe um campo vazio"));
    dtext(10, y, COLOR, a);
    dtext(10, y + 20, C_DARK, "P V = n R T   (R = 8.314)");
}

static void gas_screen(void)
{
    static form_t f = {
        .title_en = "Ideal gas law", .title_pt = "Gas ideal",
        .color = COLOR, .count = 4,
        .fields = {
            { "P", "P", "kPa" },
            { "V", "V", "L" },
            { "n", "n", "mol" },
            { "T", "T", "K" },
        },
        .results = gas_results,
    };
    form_run(&f);
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
      "Length, volume, mass, temperature...", "Comprimento, volume, massa...",
      units_screen },
    { "Molar mass", "Massa molar", "Build a formula from the elements",
      "Formula a partir dos elementos", molar_screen },
    { "Mole calculator", "Calculo de moles", "mol, particles, mass, gas V",
      "mol, particulas, massa, V gas", mole_screen },
    { "Concentration", "Concentracao", "c = n / V", "c = n / V", conc_screen },
    { "Ideal gas law", "Gas ideal", "P V = n R T", "P V = n R T", gas_screen },
    { "Periodic table", "Tabela periodica", "Browse all 118 elements",
      "Ver os 118 elementos", periodic_screen },
};
#define TOOL_COUNT (int)(sizeof tools / sizeof *tools)

static void draw_menu(int sel)
{
    dclear(C_WHITE);
    info_title("Converter", COLOR);
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
            info_show("Converter", L("Units & chemistry " APPS_VERSION,
                "Unidades e quimica " APPS_VERSION), COLOR);
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
