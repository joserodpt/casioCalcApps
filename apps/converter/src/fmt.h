//---
//  fmt - Format doubles without relying on printf's %f / %g
//        (this SDK's printf supports neither)
//---

#ifndef CONV_FMT
#define CONV_FMT

/* fmt_g(): v with up to `sig` significant digits into buf (>= 32 bytes).
   Fixed notation for moderate magnitudes, scientific (e.g. 6.02e23) for
   very large or very small values. Returns buf. */
char *fmt_g(char *buf, double v, int sig);

/* fmt_fixed(): v with exactly `dec` decimals (integer arithmetic). */
char *fmt_fixed(char *buf, double v, int dec);

#endif /* CONV_FMT */
