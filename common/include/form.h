//---
//	form - A screen of number fields with live results below
//
//	static form_t f = {
//	    .title_en = "Dynamics", .title_pt = "Dinamica", .color = COLOR,
//	    .count = 2, .fields = { { "m", "m", "kg" }, { "a", "a", "m/s2" } },
//	    .results = dyn_results,
//	};
//	form_run(&f);
//
//	UP/DOWN/EXE move between fields, LEFT/RIGHT between columns.
//---

#ifndef CASIO_FORM
#define CASIO_FORM

#include <stdbool.h>
#include "numfield.h"

#define FORM_MAX 8

typedef struct form form_t;
struct form {
    char const *title_en, *title_pt;
    int color;
    struct {
        char const *label_en, *label_pt, *unit;
        numfield_t f;
    } fields[FORM_MAX];
    int count;
    int sel;
    int cols;                       /* 1 (default) or 2 columns */
    /* Draws the results from y down; called on every redraw */
    void (*results)(form_t *f, int y);
};

/* form_run(): Edit the form until EXIT; keeps the values in *f */
void form_run(form_t *f);

/* form_value(): Value of field i, NaN if empty or invalid */
double form_value(form_t const *f, int i);

/* form_one_empty(): Index of the only empty field, or -1 if there are
   none or several (for "leave one field empty" solvers) */
int form_one_empty(form_t const *f);

/* form_result(): A result line: label at x=10, value (printf-style) at
   x=val_x */
void form_result(int y, int val_x, char const *label, int color,
    char const *fmt, ...);

#endif /* CASIO_FORM */
