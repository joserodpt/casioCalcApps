//---
//	numfield - A number being typed (digits, '.', EXP, sign)
//---

#ifndef CASIO_NUMFIELD
#define CASIO_NUMFIELD

#include <stdbool.h>

typedef struct {
    char text[24];
} numfield_t;

/* numfield_value(): The typed number, NaN if empty or invalid */
double numfield_value(numfield_t const *f);
/* numfield_key(): Apply a key (digits . EXP (-) DEL AC); true if used */
bool numfield_key(numfield_t *f, int key);
/* numfield_draw(): Input box with a cursor when selected */
void numfield_draw(numfield_t const *f, int x, int y, int w, bool sel,
    int color);

#endif /* CASIO_NUMFIELD */
