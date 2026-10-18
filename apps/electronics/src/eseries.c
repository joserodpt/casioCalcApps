#include <math.h>
#include <stdio.h>
#include <string.h>

#include "eseries.h"

static short const e12[] = { 10, 12, 15, 18, 22, 27, 33, 39, 47, 56, 68, 82 };

static short const e24[] = {
    10, 11, 12, 13, 15, 16, 18, 20, 22, 24, 27, 30,
    33, 36, 39, 43, 47, 51, 56, 62, 68, 75, 82, 91 };

static short const e96[] = {
    100, 102, 105, 107, 110, 113, 115, 118, 121, 124, 127, 130,
    133, 137, 140, 143, 147, 150, 154, 158, 162, 165, 169, 174,
    178, 182, 187, 191, 196, 200, 205, 210, 215, 221, 226, 232,
    237, 243, 249, 255, 261, 267, 274, 280, 287, 294, 301, 309,
    316, 324, 332, 340, 348, 357, 365, 374, 383, 392, 402, 412,
    422, 432, 442, 453, 464, 475, 487, 499, 511, 523, 536, 549,
    562, 576, 590, 604, 619, 634, 649, 665, 681, 698, 715, 732,
    750, 768, 787, 806, 825, 845, 866, 887, 909, 931, 953, 976 };

_Static_assert(sizeof e96 / sizeof *e96 == 96, "E96 has 96 values");

char const * const eia96_letter_names[EIA96_LETTERS] = {
    "Z", "Y / R", "X / S", "A", "B / H", "C", "D", "E", "F" };
int const eia96_letter_exp[EIA96_LETTERS] = { -3, -2, -1, 0, 1, 2, 3, 4, 5 };

int e96_value(int n)
{
    return (n >= 1 && n <= 96) ? e96[n - 1] : 0;
}

double eseries_nearest(int series, double v, bool up)
{
    short const *table = series == 12 ? e12 : series == 24 ? e24 : e96;
    int count = series == 12 ? 12 : series == 24 ? 24 : 96;
    int scale = series == 96 ? 100 : 10;

    if (!(v > 0) || isinf(v)) return NAN;

    double decade = pow(10, floor(log10(v)));
    double m = v / decade * scale;      /* scale <= m < 10 * scale */
    double best = 10 * scale;           /* first value of the next decade */

    for (int i = count - 1; i >= 0; i--) {
        double t = table[i];
        if (up) {
            if (t >= m * (1 - 1e-9)) best = t;
        }
        else if (fabs(log(t / m)) < fabs(log(best / m))) best = t;
    }
    return best * decade / scale;
}

double smd_decode(char const *code)
{
    int n = strlen(code);
    char const *r = strchr(code, 'R');

    if (n < 1 || n > 4) return NAN;

    /* R marks the decimal point and there is no multiplier */
    if (r) {
        double v = 0, frac = 1;
        bool any = false;
        for (char const *s = code; *s; s++) {
            if (s == r) continue;
            int d = *s - '0';
            if (s < r) v = v * 10 + d;
            else v += d * (frac /= 10);
            any = true;
        }
        return any ? v : NAN;
    }

    /* "0" or "00..." is a 0 Ω jumper */
    if (strspn(code, "0") == (size_t)n) return 0;
    if (n < 3) return NAN;

    int mant = 0;
    for (int i = 0; i < n - 1; i++) mant = mant * 10 + (code[i] - '0');
    return mant * pow(10, code[n - 1] - '0');
}

/* Round v to `digits` significant digits: v ~ mant * 10^exp */
static void round_sig(double v, int digits, long *mant, int *exp)
{
    int e = (int)floor(log10(v)) - (digits - 1);
    long m = lround(v / pow(10, e));
    long limit = digits == 2 ? 100 : 1000;
    if (m >= limit) { m = lround(m / 10.0); e++; }
    if (m < limit / 10) { m *= 10; e--; }
    *mant = m;
    *exp = e;
}

bool smd_encode(char *buf, double v, int digits, bool *exact)
{
    long mant;
    int e;

    if (v == 0) {
        strcpy(buf, digits == 2 ? "000" : "0000");
        *exact = true;
        return true;
    }
    if (!(v > 0)) return false;

    round_sig(v, digits, &mant, &e);
    *exact = fabs(mant * pow(10, e) - v) <= v * 1e-6;

    if (e > 9) return false;
    if (e >= 0) {
        sprintf(buf, "%ld%d", mant, e);     /* mant has `digits` digits */
        return true;
    }

    /* Below 10 (or 100): write R where the decimal point goes */
    int intdigits = digits + e;
    if (intdigits < 0) return false;
    char num[8];
    sprintf(num, "%ld", mant);
    memcpy(buf, num, intdigits);
    buf[intdigits] = 'R';
    strcpy(buf + intdigits + 1, num + intdigits);
    return true;
}

bool eia96_encode(char *buf, double v, bool *exact)
{
    double e96v = eseries_nearest(96, v, false);
    if (isnan(e96v)) return false;

    long mant;
    int e;
    round_sig(e96v, 3, &mant, &e);
    if (e < -3 || e > 5) return false;

    int index = 0;
    for (int i = 0; i < 96; i++) if (e96[i] == mant) index = i + 1;
    if (!index) return false;

    static char const letters[] = "ZYXABCDEF";
    sprintf(buf, "%02d%c", index, letters[e + 3]);
    *exact = fabs(e96v - v) <= v * 1e-6;
    return true;
}
