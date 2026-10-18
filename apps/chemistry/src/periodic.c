#include <string.h>

#include "periodic.h"

element_t const *element_by_symbol(char const *sym, int len)
{
    for (int i = 0; i < ELEMENT_COUNT; i++) {
        char const *s = periodic_table[i].symbol;
        if ((int)strlen(s) == len && !strncmp(s, sym, len))
            return &periodic_table[i];
    }
    return NULL;
}

//---
//  Layout
//---

/* Column (1..18) for groups; period from atomic number ranges. The f-block
   is placed on two extra rows below the main table. */
void element_grid(int z, int *row, int *col)
{
    /* f-block below the main table: lanthanides 57-71 -> row 7,
       actinides 89-103 -> row 8, indented under group 3 */
    if (z >= 57 && z <= 71) { *row = 7; *col = 2 + (z - 57); return; }
    if (z >= 89 && z <= 103) { *row = 8; *col = 2 + (z - 89); return; }

    static struct { int z, period, group; } const main_[] = {
        {1,0,0},{2,0,17},
        {3,1,0},{4,1,1},{5,1,12},{6,1,13},{7,1,14},{8,1,15},{9,1,16},{10,1,17},
        {11,2,0},{12,2,1},{13,2,12},{14,2,13},{15,2,14},{16,2,15},{17,2,16},
        {18,2,17},
    };
    for (unsigned i = 0; i < sizeof main_ / sizeof *main_; i++)
        if (main_[i].z == z) { *row = main_[i].period; *col = main_[i].group;
            return; }

    /* Periods 4-7: 18 columns each, La/Ac blocks skipped into their rows */
    if (z >= 19 && z <= 36) { *row = 3; *col = z - 19; return; }
    if (z >= 37 && z <= 54) { *row = 4; *col = z - 37; return; }
    if (z >= 55 && z <= 56) { *row = 5; *col = z - 55; return; }
    if (z >= 72 && z <= 86) { *row = 5; *col = z - 72 + 3; return; }
    if (z >= 87 && z <= 88) { *row = 6; *col = z - 87; return; }
    if (z >= 104 && z <= 118) { *row = 6; *col = z - 104 + 3; return; }
    *row = 0; *col = 0;
}

int element_at_grid(int row, int col)
{
    for (int z = 1; z <= ELEMENT_COUNT; z++) {
        int r, c;
        element_grid(z, &r, &c);
        if (r == row && c == col) return z;
    }
    return 0;
}

//---
//  Categories (coarse, for colouring)
//---

enum { CAT_NONMETAL, CAT_NOBLE, CAT_ALKALI, CAT_ALKEARTH, CAT_METALLOID,
    CAT_HALOGEN, CAT_TRANSITION, CAT_POSTTRANS, CAT_LANTHACT };

static char const * const cat_names[] = {
    "Nonmetal", "Noble gas", "Alkali metal", "Alkaline earth", "Metalloid",
    "Halogen", "Transition", "Post-transition", "Lanth/Actinide" };

char const *category_name(int cat) { return cat_names[cat]; }

static bool in_set(int z, int const *set)
{
    for (; *set; set++) if (*set == z) return true;
    return false;
}

int element_category(int z)
{
    static int const nonmetal[] = { 1, 6, 7, 8, 15, 16, 34, 0 };
    static int const noble[] = { 2, 10, 18, 36, 54, 86, 118, 0 };
    static int const alkali[] = { 3, 11, 19, 37, 55, 87, 0 };
    static int const alkearth[] = { 4, 12, 20, 38, 56, 88, 0 };
    static int const metalloid[] = { 5, 14, 32, 33, 51, 52, 85, 0 };
    static int const halogen[] = { 9, 17, 35, 53, 85, 117, 0 };
    static int const posttrans[] = {
        13, 31, 49, 50, 81, 82, 83, 84, 113, 114, 115, 116, 0 };

    if (in_set(z, noble)) return CAT_NOBLE;
    if (in_set(z, halogen)) return CAT_HALOGEN;
    if (in_set(z, alkali)) return CAT_ALKALI;
    if (in_set(z, alkearth)) return CAT_ALKEARTH;
    if (in_set(z, metalloid)) return CAT_METALLOID;
    if (in_set(z, nonmetal)) return CAT_NONMETAL;
    if ((z >= 57 && z <= 71) || (z >= 89 && z <= 103)) return CAT_LANTHACT;
    if (in_set(z, posttrans)) return CAT_POSTTRANS;
    return CAT_TRANSITION;
}

//---
//  Formula parser
//
//  element := Upper (lower)?
//  group   := '(' seq ')' number?
//  seq     := (element | group) number?  repeated
//---

static void add_part(formula_t *f, element_t const *el, int count)
{
    for (int i = 0; i < f->element_count; i++)
        if (f->parts[i].el == el) { f->parts[i].count += count; return; }
    if (f->element_count < 16) {
        f->parts[f->element_count].el = el;
        f->parts[f->element_count].count = count;
        f->element_count++;
    }
}

/* Parse a sequence starting at *p, stopping at ')' or end. Adds the
   elements it finds to f (counts already scaled by any trailing numbers)
   and returns the mass. Sets f->ok = false on error. */
static double parse_seq(formula_t *f, char const **p, int depth)
{
    double mass = 0;

    if (depth > 8) { f->ok = false; return 0; }

    while (**p && **p != ')') {
        char c = **p;

        if (c == '(') {
            (*p)++;
            formula_t grp = { .ok = true, .element_count = 0 };
            double inner = parse_seq(&grp, p, depth + 1);
            if (!grp.ok) { f->ok = false; return 0; }
            if (**p != ')') { f->ok = false; return 0; }
            (*p)++;
            int n = 0;
            while (**p >= '0' && **p <= '9') n = n * 10 + (*(*p)++ - '0');
            if (n == 0) n = 1;
            mass += inner * n;
            for (int i = 0; i < grp.element_count; i++)
                add_part(f, grp.parts[i].el, grp.parts[i].count * n);
            continue;
        }

        if (c >= 'A' && c <= 'Z') {
            char const *start = *p;
            (*p)++;
            if (**p >= 'a' && **p <= 'z') (*p)++;
            int len = *p - start;
            element_t const *el = element_by_symbol(start, len);
            if (!el) { f->ok = false; return 0; }
            int n = 0;
            while (**p >= '0' && **p <= '9') n = n * 10 + (*(*p)++ - '0');
            if (n == 0) n = 1;
            mass += el->weight * n;
            add_part(f, el, n);
            continue;
        }

        if (c == ' ') { (*p)++; continue; }
        f->ok = false;
        return 0;
    }
    return mass;
}

void formula_parse(char const *s, formula_t *out)
{
    out->ok = true;
    out->mass = 0;
    out->element_count = 0;
    out->error_pos = -1;

    if (!*s) { out->ok = false; return; }

    char const *p = s;
    out->mass = parse_seq(out, &p, 0);
    if (*p == ')') out->ok = false;        /* stray close paren */
    if (!out->ok) {
        out->error_pos = p - s;
        out->mass = 0;
        out->element_count = 0;
    }
}
