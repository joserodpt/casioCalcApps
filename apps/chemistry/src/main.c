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
#include "form.h"
#include "keys.h"
#include "menu.h"
#include "units.h"
#include "periodic.h"
#include "usb_screen.h"

#define COLOR   C_RGB(5, 14, 20)
#define NA      6.02214076e23       /* Avogadro */
#define R_GAS   8.314462618         /* J/(mol K) */
#define VM_STP  22.413969           /* L/mol at STP (0 C, 1 atm) */

static double fv(form_t *f, int i) { return form_value(f, i); }

/* Start on Volume (dm3/cm3) */
static void units_run(void) { units_screen(2, COLOR); }

//---
//  Periodic grid (shared by the molar-mass and browser screens)
//---

#if GINT_HW_FX

/* fx-9860G: the table fills the screen without a title bar. Cells are 7x5
   pixels (6x4 drawn), so 18 columns take 126 pixels and the 9 rows (with a
   2-pixel gap before the f-block) end at y=46. The text goes in the empty
   space above the transition metals (x 16..83, two lines) and on two lines
   below the table. Categories are grey shades and patterns */
#define GRID_X   1
#define GRID_Y   1
#define CELL_W   7
#define CELL_H   5
#define FBLOCK_GAP 2
#define GAP_X    16             /* empty area above groups 3-12 */
#define GAP_W    68
#define LINE1_Y  49             /* text lines below the table */
#define LINE2_Y  57

static void grid_cell_xy(int row, int col, int *x, int *y)
{
    *x = GRID_X + col * CELL_W;
    *y = GRID_Y + row * CELL_H + (row >= 7 ? FBLOCK_GAP : 0);
}

/* A 6x4 cell (or a sample of it) in the pattern of category cat */
static void draw_cell(int x, int y, int cat)
{
    int x2 = x + CELL_W - 2, y2 = y + CELL_H - 2;
    switch (cat) {
    case 2: drect(x, y, x2, y2, C_BLACK); break;           /* alkali */
    case 3: drect(x, y, x2, y2, C_DARK); break;            /* alk. earth */
    case 6: drect(x, y, x2, y2, C_LIGHT); break;           /* transition */
    case 7: drect_border(x, y, x2, y2, C_LIGHT, 1, C_BLACK); break;
    case 0: drect_border(x, y, x2, y2, C_WHITE, 1, C_BLACK); break;
    case 5:                                                /* halogen */
        drect_border(x, y, x2, y2, C_WHITE, 1, C_BLACK);
        drect(x + 2, y + 1, x2 - 2, y2 - 1, C_BLACK);
        break;
    case 1: drect_border(x, y, x2, y2, C_WHITE, 1, C_DARK); break;
    default:                    /* metalloid (black), f-block (dark) dots */
        for (int py = y; py <= y2; py++)
        for (int px = x; px <= x2; px++)
            dpixel(px, py, (px + py) & 1 ? C_WHITE
                : cat == 4 ? C_BLACK : C_DARK);
        break;
    }
}

static void draw_grid(int cursor_z)
{
    for (int z = 1; z <= ELEMENT_COUNT; z++) {
        int row, col, x, y;
        element_grid(z, &row, &col);
        grid_cell_xy(row, col, &x, &y);
        draw_cell(x, y, element_category(z));
    }
    /* The cursor: a thick black frame over the cell and its margins */
    int row, col, x, y;
    element_grid(cursor_z, &row, &col);
    grid_cell_xy(row, col, &x, &y);
    drect_border(x - 1, y - 1, x + CELL_W - 1, y + CELL_H - 1, C_WHITE, 2,
        C_BLACK);
}

/* Short category names, to fit next to the table */
static char const *category_short(int z)
{
    static char const * const names[] = { "Nonmetal", "Noble gas",
        "Alkali", "Alk. earth", "Metalloid", "Halogen", "Transition",
        "Post-trans." };
    int cat = element_category(z);
    if (cat < 8) return names[cat];
    return z >= 89 ? "Actinide" : "Lanthanide";
}

#else

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

#endif /* GINT_HW_FX */

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
    bool typing = false;                /* digits go into the last count */

    while (1) {
        dclear(C_WHITE);
#if !GINT_HW_FX
        info_title(L("Molar mass", "Massa molar", "Masse molaire"), COLOR);
#endif
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

#if GINT_HW_FX
        /* The highlighted element above the transition metals, then the
           formula (its end, if it is too long) and its molar mass */
        element_t const *cur = &periodic_table[z - 1];
        char mb[32];
        dprint(GAP_X, 1, C_BLACK, "%d %s", cur->z, cur->symbol);
        dtext(GAP_X, 9, C_DARK, fmt_g(mb, cur->weight, 6));

        char const *fp = formula;
        int w;
        dsize(fp, NULL, &w, NULL);
        if (w > DWIDTH - 2) {
            while (*fp && (dsize(fp, NULL, &w, NULL), w > DWIDTH - 9)) fp++;
            dtext(1, LINE1_Y, C_DARK, "<");
            dtext(8, LINE1_Y, C_BLACK, fp);
        }
        else dtext(1, LINE1_Y, np ? C_BLACK : C_DARK,
            np ? formula : L("EXE add element", "EXE junta elemento",
                "EXE ajoute element"));
        if (np) {
            char b[40];
            sprintf(b, "M = %s g/mol", fmt_g(mb, mass, 7));
            dtext_opt(DWIDTH - 1, LINE2_Y, C_BLACK, C_NONE, DTEXT_RIGHT,
                DTEXT_TOP, b);
        }
        else dtext(1, LINE2_Y, C_DARK, L("0-9 count  DEL undo",
            "0-9 qtd  DEL anula", "0-9 nb  DEL annule"));
#else
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
        dtext(10, y, C_DARK, formula[0] ? formula : L("(empty)", "(vazio)",
            "(vide)"));
        if (np) {
            char b[40];
            sprintf(b, "M = %s g/mol", fmt_g(mb, mass, 7));
            dtext_opt(DWIDTH - 10, y, COLOR, C_NONE, DTEXT_RIGHT, DTEXT_TOP, b);
        }
        dtext(10, 192, C_DARK, L("EXE add  0-9 count  DEL undo  AC clear",
            "EXE junta  0-9 qtd  DEL anula  AC limpa",
            "EXE ajout  0-9 nb  DEL annule  AC vide"));

        fkey_button(1, "+/-");
#endif
        dupdate();

        int key = getkey().key;
        int d = key_digit(key);
        if (key == KEY_EXIT) return;
        else if (key == INFO_KEY)
            info_app(L("Molar mass", "Massa molar", "Masse molaire"), COLOR);
        else if (key == KEY_UP || key == KEY_DOWN || key == KEY_LEFT
            || key == KEY_RIGHT) z = grid_move(z, key);
        else if (key == KEY_EXE) {
            /* Merge with the previous part if it is the same element */
            if (np > 0 && parts[np - 1].z == z) parts[np - 1].count++;
            else if (np < 20) { parts[np].z = z; parts[np].count = 1; np++; }
            typing = false;
        }
        else if (d >= 0 && np > 0) {
            /* The first digit replaces the count, the next ones append */
            int c = typing ? parts[np - 1].count * 10 + d : d;
            typing = true;
            if (c == 0) c = 1;
            parts[np - 1].count = c > 999 ? 999 : c;
        }
        else if (key == KEY_DEL) { if (np) np--; typing = false; }
        else if (key == KEY_ACON) { np = 0; typing = false; }
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
#if GINT_HW_FX
        /* Number, symbol and category (with a sample of its pattern) above
           the transition metals; name and weight below the table */
        draw_grid(z);
        element_t const *e = &periodic_table[z - 1];
        char b[32], w[40];
        dprint(GAP_X, 1, C_BLACK, "%d %s", e->z, e->symbol);
        draw_cell(GAP_X + GAP_W - CELL_W + 1, 2, element_category(z));
        dtext(GAP_X, 9, C_DARK, category_short(z));
        dtext(1, LINE1_Y, C_BLACK, e->name);
        sprintf(w, "%s g/mol", fmt_g(b, e->weight, 6));
        dtext_opt(DWIDTH - 1, LINE2_Y, C_BLACK, C_NONE, DTEXT_RIGHT,
            DTEXT_TOP, w);
#else
        info_title(L("Periodic table", "Tabela periodica",
            "Tableau periodique"), COLOR);
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
#endif
        dupdate();

        int key = getkey().key;
        if (key == KEY_EXIT) return;
        if (key == INFO_KEY)
            info_app(L("Periodic table", "Tabela periodica",
                "Tableau periodique"), COLOR);
        else z = grid_move(z, key);
    }
}

//---
//  Mole calculator
//---

#if GINT_HW_FX

/* fx-9860G: four 8-pixel lines below the two fields */
static void mole_results(form_t *f, int y)
{
    char b[32];
    double n = fv(f, 0), M = fv(f, 1);
    if (isnan(n)) {
        dtext(1, y, C_DARK, L("Enter n (mol)", "Introduza n", "Entrez n"));
        dtext(1, y + FORM_LINE, C_DARK, L("and M for the mass",
            "e M para a massa", "et M pour la masse"));
        return;
    }

    form_result(y, 0, L("N", "N", "N"), C_BLACK, "%s", fmt_g(b, n * NA, 7));
    if (isnan(M)) form_result(y + FORM_LINE, 0, "m", C_DARK, "%s",
        L("need M", "falta M", "manque M"));
    else form_result(y + FORM_LINE, 0, "m", C_BLACK, "%s g",
        fmt_g(b, n * M, 7));
    form_result(y + 2 * FORM_LINE, 0, L("V (STP)", "V (PTN)", "V (CNTP)"),
        C_BLACK, "%s L", fmt_g(b, n * VM_STP, 7));
    dtext(1, y + 3 * FORM_LINE, C_DARK, "N=nNa m=nM V=n22.4");
}

#else

static void mole_results(form_t *f, int y)
{
    char b[32];
    double n = fv(f, 0), M = fv(f, 1);
    if (isnan(n)) {
        dtext(10, y, C_DARK, L("Enter n (mol)", "Introduza n", "Entrez n"));
        return;
    }

    form_result(y, 120, L("Particles N", "Particulas N", "Particules N"),
        COLOR, "%s", fmt_g(b, n * NA, 7));
    if (isnan(M)) form_result(y + 16, 120, L("Mass m", "Massa m", "Masse m"),
        C_BLACK, "%s", L("need M", "falta M", "manque M"));
    else form_result(y + 16, 120, L("Mass m", "Massa m", "Masse m"), C_BLACK,
        "%s g", fmt_g(b, n * M, 7));
    form_result(y + 32, 120, L("Gas V (STP)", "Gas V (PTN)", "Gaz V (CNTP)"),
        C_BLACK, "%s L", fmt_g(b, n * VM_STP, 7));
    dtext(10, y + 54, C_DARK, "N = n Na,  m = n M,  V = n 22.4");
}

#endif /* GINT_HW_FX */

static void mole_screen(void)
{
    static form_t f = {
        .title_en = "Mole calculator", .title_pt = "Calculo de moles",
        .title_fr = "Calcul de moles",
        .color = COLOR, .count = 2,
        .fields = {
            { "n", "n", "n", "mol" },
#if GINT_HW_FX
            { "M", "M", "M", "g/mol" },
#else
            { "M (molar mass)", "M (massa molar)", "M (masse mol.)", "g/mol" },
#endif
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
    int unknown = form_one_empty(f);
    double c = fv(f, 0), n = fv(f, 1), V = fv(f, 2);

    if (unknown == 0 && !isnan(n) && !isnan(V) && V != 0)
        sprintf(a, "c = %s mol/L", fmt_g(b, n / V, 7));
    else if (unknown == 1 && !isnan(c) && !isnan(V))
        sprintf(a, "n = %s mol", fmt_g(b, c * V, 7));
    else if (unknown == 2 && !isnan(c) && !isnan(n) && c != 0)
        sprintf(a, "V = %s L", fmt_g(b, n / c, 7));
    else { strcpy(a, L("Leave one field empty", "Deixe um campo vazio",
        "Laissez un champ vide")); }
#if GINT_HW_FX
    dtext(1, y, C_BLACK, a);
    dtext(1, y + FORM_LINE, C_DARK, "c = n / V");
#else
    dtext(10, y, COLOR, a);
    dtext(10, y + 20, C_DARK, "c = n / V");
#endif
}

static void conc_screen(void)
{
    static form_t f = {
        .title_en = "Concentration", .title_pt = "Concentracao",
        .title_fr = "Concentration",
        .color = COLOR, .count = 3,
        .fields = {
            { "c", "c", "c", "mol/L" },
            { "n", "n", "n", "mol" },
            { "V", "V", "V", "L" },
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
    int unknown = form_one_empty(f);
    double P = fv(f, 0), V = fv(f, 1), n = fv(f, 2), T = fv(f, 3);

    if (unknown == 0 && !isnan(V) && !isnan(n) && !isnan(T) && V != 0)
        sprintf(a, "P = %s kPa", fmt_g(b, n * R_GAS * T / V, 7));
    else if (unknown == 1 && !isnan(P) && !isnan(n) && !isnan(T) && P != 0)
        sprintf(a, "V = %s L", fmt_g(b, n * R_GAS * T / P, 7));
    else if (unknown == 2 && !isnan(P) && !isnan(V) && !isnan(T) && T != 0)
        sprintf(a, "n = %s mol", fmt_g(b, P * V / (R_GAS * T), 7));
    else if (unknown == 3 && !isnan(P) && !isnan(V) && !isnan(n) && n != 0)
        sprintf(a, "T = %s K", fmt_g(b, P * V / (R_GAS * n), 7));
    else strcpy(a, L("Leave one field empty", "Deixe um campo vazio",
        "Laissez un champ vide"));
#if GINT_HW_FX
    dtext(1, y, C_BLACK, a);
    dtext(1, y + FORM_LINE, C_DARK, "PV = nRT  R = 8.314");
#else
    dtext(10, y, COLOR, a);
    dtext(10, y + 20, C_DARK, "P V = n R T   (R = 8.314)");
#endif
}

static void gas_screen(void)
{
    static form_t f = {
        .title_en = "Ideal gas law", .title_pt = "Gas ideal",
        .title_fr = "Gaz parfait",
        .color = COLOR, .count = 4,
        .fields = {
            { "P", "P", "P", "kPa" },
            { "V", "V", "V", "L" },
            { "n", "n", "n", "mol" },
            { "T", "T", "T", "K" },
        },
        .results = gas_results,
    };
    form_run(&f);
}

//---
//  Menu
//---

static menu_item_t const tools[] = {
    { "Unit converter", "Conversor de unidades", "Convertisseur d'unites",
      "Length, volume, mass, temperature...", "Comprimento, volume, massa...",
      "Longueur, volume, masse...", units_run, COLOR },
    { "Molar mass", "Massa molar", "Masse molaire",
      "Build a formula from the elements", "Formula a partir dos elementos",
      "Formule a partir des elements", molar_screen, COLOR },
    { "Mole calculator", "Calculo de moles", "Calcul de moles",
      "mol, particles, mass, gas V", "mol, particulas, massa, V gas",
      "mol, particules, masse, V gaz", mole_screen, COLOR },
    { "Concentration", "Concentracao", "Concentration",
      "c = n / V", "c = n / V", "c = n / V", conc_screen, COLOR },
    { "Ideal gas law", "Gas ideal", "Gaz parfait",
      "P V = n R T", "P V = n R T", "P V = n R T", gas_screen, COLOR },
    { "Periodic table", "Tabela periodica", "Tableau periodique",
      "Browse all 118 elements", "Ver os 118 elementos",
      "Voir les 118 elements", periodic_screen, COLOR },
};

static menu_t const menu = {
#if GINT_HW_FX
    /* Short enough for the 21-character About line */
    "Chemistry", "Quimica", "Chimie", "Units & chem. " APPS_VERSION,
    "Unid. e quimica " APPS_VERSION, "Unites & chimie " APPS_VERSION, COLOR,
#else
    "Chemistry", "Quimica", "Chimie", "Units & chemistry " APPS_VERSION,
    "Unidades e quimica " APPS_VERSION, "Unites et chimie " APPS_VERSION,
    COLOR,
#endif
    tools, (int)(sizeof tools / sizeof *tools),
};

int main(void)
{
    lang_init();
    usb_screen_init();
    screen_init();
    menu_run(&menu);
    return 1;
}
