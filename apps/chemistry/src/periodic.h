//---
//  periodic - 118-element table, lookup, layout and formula molar mass
//---

#ifndef CONV_PERIODIC
#define CONV_PERIODIC

#include <stdbool.h>

#define ELEMENT_COUNT 118

typedef struct {
    int z;                  /* atomic number */
    char const *symbol;     /* "Na" */
    char const *name;       /* "Sodium" */
    double weight;          /* standard atomic weight (g/mol) */
} element_t;

extern element_t const periodic_table[ELEMENT_COUNT];

/* element_by_symbol(): find by symbol (case-sensitive, "Na"), or NULL */
element_t const *element_by_symbol(char const *sym, int len);

/* element_grid(): row (0..9) and column (0..17) of element z in the usual
   18-column layout; the lanthanides/actinides sit on rows 8 and 9 */
void element_grid(int z, int *row, int *col);
/* element_at_grid(): atomic number at (row, col), or 0 if empty */
int element_at_grid(int row, int col);

/* element_category(): a coarse category index 0..8 for colouring */
int element_category(int z);
char const *category_name(int cat);
/* category_color(): Colour of a category, for the grid cells */
int category_color(int cat);

//---
//  Molar mass of a formula like "H2SO4", "Ca(OH)2", "CuSO4(H2O)5"
//---

typedef struct {
    double mass;            /* total molar mass, 0 on error */
    bool ok;
    int error_pos;          /* index of the offending character on error */
    int element_count;      /* distinct elements, for the breakdown */
    struct { element_t const *el; int count; } parts[16];
} formula_t;

void formula_parse(char const *s, formula_t *out);

#endif /* CONV_PERIODIC */
