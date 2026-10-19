//---
//  ac - Parts on the links of a grid, and the AC (phasor) solver
//
//  The grid is the one of the DC Circuit app: NX x NY points, a part on the
//  link between two neighbouring points (its ends a and b: a is the left or
//  top one), links numbered horizontal ones first, then vertical.
//
//  Every source runs at the circuit's one frequency. Voltages and currents
//  are phasors in rms: |V| is what a meter shows, arg V the phase.
//---

#ifndef AC_H
#define AC_H

#include <stdbool.h>
#include <stdint.h>

#define NX       12
#define NY       5
#define NODES    (NX * NY)
#define H_EDGES  (NY * (NX - 1))
#define EDGES    (H_EDGES + (NY - 1) * NX)

#define FREQ_MIN 1.0
#define FREQ_MAX 1e6

//---
//  Complex numbers
//---

typedef struct { double re, im; } cplx;

static inline cplx c_make(double re, double im)
{
    cplx z = { re, im };
    return z;
}
static inline cplx c_add(cplx a, cplx b) { return c_make(a.re + b.re, a.im + b.im); }
static inline cplx c_sub(cplx a, cplx b) { return c_make(a.re - b.re, a.im - b.im); }
static inline cplx c_scale(cplx a, double k) { return c_make(a.re * k, a.im * k); }
static inline cplx c_conj(cplx a) { return c_make(a.re, -a.im); }
static inline cplx c_mul(cplx a, cplx b)
{
    return c_make(a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re);
}
cplx c_div(cplx a, cplx b);
double c_abs(cplx a);
/* c_deg(): Argument in degrees, -180 to 180 */
double c_deg(cplx a);
/* c_polar(): Magnitude and angle in degrees */
cplx c_polar(double mag, double deg);

//---
//  Parts
//---

typedef enum {
    P_NONE, P_WIRE, P_RES, P_CAP, P_IND, P_SRC, P_LAMP, P_SWITCH,
    P_AMMETER, P_VOLTMETER,
    P_KINDS
} kind_t;

typedef struct {
    uint8_t kind;
    uint8_t flip;       /* source: + (the phase reference) at end b */
    uint8_t closed;     /* switch */
    uint8_t pad;
    double value;       /* R: ohms; C: farads; L: henries; source: volts
                           rms; lamp: rated V */
    double value2;      /* source: phase in degrees; L: winding
                           resistance; lamp: rated W */
} part_t;

typedef struct {
    part_t parts[EDGES];
    double freq;        /* Hz */
} circuit_t;

typedef enum {
    S_OK,
    S_EMPTY,            /* no source, or nothing for it to drive */
    S_SHORT,            /* a source's terminals are wired together */
    S_LOOP,             /* sources in parallel or in a loop: no solution */
    S_TOOBIG,
} status_t;

typedef struct {
    status_t status;
    int bad_edge;           /* S_SHORT: the source, else -1 */
    bool known[NODES];      /* false where not connected */
    cplx v[NODES];          /* point voltages */
    cplx i[EDGES];          /* current through the part, from end a to b */
    cplx s[EDGES];          /* complex power P + jQ: taken by the part, or
                               for sources supplied */
    cplx supplied;          /* total from the sources */
    int ref_source;         /* the first source, for phases; -1 if none */
} result_t;

/* Inductors never have less than this resistance, so that a series LC at
   resonance across a source still has a (large) solution */
#define L_RMIN 1e-3

void edge_ends(int e, int *a, int *b);
bool edge_horizontal(int e);

/* part_impedance(): Impedance of a resistor, capacitor, inductor or lamp
   at f */
cplx part_impedance(part_t const *p, double f);
/* part_default(): A new part of this kind with its default values */
part_t part_default(kind_t kind);
/* part_element(): Parts that go into the equations (R, C, L, lamp,
   source) */
bool part_element(part_t const *p);

/* circuit_solve(): Phasors at frequency f (usually c->freq) */
void circuit_solve(circuit_t const *c, double f, result_t *r);

/* across(): Voltage from end a to end b of a link; zero if unknown */
cplx across(result_t const *r, int e);

#endif /* AC_H */
