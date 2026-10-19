#include <math.h>
#include <stdio.h>
#include <string.h>

#include "fmt.h"

/* Strip trailing zeros (and a trailing '.') from a decimal string */
static void strip_zeros(char *s)
{
    if (!strchr(s, '.')) return;
    char *end = s + strlen(s) - 1;
    while (*end == '0') *end-- = '\0';
    if (*end == '.') *end = '\0';
}

char *fmt_fixed(char *buf, double v, int dec)
{
    double scale = 1;
    for (int i = 0; i < dec; i++) scale *= 10;

    double a = fabs(v) * scale;
    if (a > 4.0e18) a = 4.0e18;         /* keep within long long */
    long long n = llround(a);
    long long ip = n / (long long)scale;
    long long fp = n % (long long)scale;
    char const *sign = (v < 0 && n) ? "-" : "";

    if (dec > 0) {
        char frac[24];
        sprintf(frac, "%lld", (long long)scale + fp);   /* leading 1 */
        sprintf(buf, "%s%lld.%s", sign, ip, frac + 1);
    } else {
        sprintf(buf, "%s%lld", sign, ip);
    }
    return buf;
}

char *fmt_g(char *buf, double v, int sig)
{
    if (isnan(v)) return strcpy(buf, "-");
    if (isinf(v)) return strcpy(buf, v < 0 ? "-inf" : "inf");
    if (v == 0) return strcpy(buf, "0");
    if (sig < 1) sig = 1;

    double a = fabs(v);
    int exp = (int)floor(log10(a));

    /* Moderate magnitudes: plain fixed notation */
    if (exp >= -4 && exp < 7 && exp < sig + 3) {
        int dec = sig - 1 - exp;
        if (dec < 0) dec = 0;
        if (dec > 12) dec = 12;
        fmt_fixed(buf, v, dec);
        strip_zeros(buf);
        /* Rounding can push e.g. 9.999 -> 10; that is fine as text */
        return buf;
    }

    /* Scientific: mantissa in [1, 10) */
    double mant = v / pow(10, exp);
    if (fabs(mant) >= 10) { mant /= 10; exp++; }   /* rounding guard */
    if (fabs(mant) < 1) { mant *= 10; exp--; }

    char m[24];
    fmt_fixed(m, mant, sig - 1);
    strip_zeros(m);
    sprintf(buf, "%se%d", m, exp);
    return buf;
}
