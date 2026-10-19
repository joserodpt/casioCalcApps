//---
//  ui - Small shared helpers for the Converter screens
//---

#ifndef CONV_UI
#define CONV_UI

#include <gint/display.h>
#include <stdbool.h>

/* A number being typed (digits, '.', EXP, sign) */
typedef struct {
    char text[24];
} numfield_t;

double numfield_value(numfield_t const *f);   /* NaN if empty/invalid */
void numfield_set(numfield_t *f, double v);
bool numfield_key(numfield_t *f, int key);    /* true if the key was used */
void numfield_draw(numfield_t const *f, int x, int y, int w, bool sel,
    int color);

/* result(): label at x=10, value at x=val_x */
void conv_result(int y, int val_x, char const *label, int color,
    char const *value);

/* Colour for an element category index (0..8) */
int category_color(int cat);

#endif /* CONV_UI */
