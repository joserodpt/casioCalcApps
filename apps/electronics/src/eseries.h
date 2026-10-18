//---
//  eseries - Standard resistor values and SMD resistor codes
//---

#ifndef ELEC_ESERIES
#define ELEC_ESERIES

#include <stdbool.h>

/* eseries_nearest(): Nearest value of the E12, E24 or E96 series (log
   scale); with up set, the smallest value >= v instead. NaN if v <= 0 */
double eseries_nearest(int series, double v, bool up);

/* E96 mantissa (100..976) for 1-based index n, or 0 */
int e96_value(int n);

/* EIA-96 multiplier letters and their powers of ten */
#define EIA96_LETTERS 9
extern char const * const eia96_letter_names[EIA96_LETTERS];
extern int const eia96_letter_exp[EIA96_LETTERS];

/* smd_decode(): Value of a 3- or 4-digit code ("472", "4702", "4R7");
   NaN if invalid */
double smd_decode(char const *code);

/* smd_encode(): 3-digit (digits = 2) or 4-digit (digits = 3) code for v
   into buf (8 bytes). Returns false if v cannot be written; sets *exact
   when the code is not a rounded value */
bool smd_encode(char *buf, double v, int digits, bool *exact);

/* eia96_encode(): EIA-96 code ("01C") for the nearest E96 value */
bool eia96_encode(char *buf, double v, bool *exact);

#endif /* ELEC_ESERIES */
