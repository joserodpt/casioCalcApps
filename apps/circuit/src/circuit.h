//---
//  circuit - Parts on the links of a grid, and the DC solver
//
//  The grid has NX x NY points. A part sits on the link between two
//  neighbouring points (its ends a and b: a is the left or top one).
//  Links are numbered horizontal ones first, row by row, then vertical.
//---

#ifndef CIRCUIT_H
#define CIRCUIT_H

#include <stdbool.h>
#include <stdint.h>

#define NX       12
#define NY       5
#define NODES    (NX * NY)
#define H_EDGES  (NY * (NX - 1))
#define EDGES    (H_EDGES + (NY - 1) * NX)

typedef enum { P_NONE, P_WIRE, P_RES, P_BATT, P_LAMP, P_SWITCH } kind_t;

typedef struct {
    uint8_t kind;
    uint8_t flip;       /* battery: + terminal at end b instead of a */
    uint8_t closed;     /* switch */
    uint8_t pad;
    double value;       /* resistor: ohms; battery: volts; lamp: rated V */
    double value2;      /* lamp: rated power, W */
} part_t;

typedef struct {
    part_t parts[EDGES];
} circuit_t;

typedef enum {
    S_OK,
    S_EMPTY,            /* no battery, resistor or lamp */
    S_SHORT,            /* a battery's terminals are wired together */
    S_LOOP,             /* batteries in parallel or in a loop: no solution */
    S_TOOBIG,
} status_t;

typedef struct {
    status_t status;
    int bad_edge;           /* S_SHORT: the battery, else -1 */
    double v[NODES];        /* point voltages; NAN where not connected */
    double i[EDGES];        /* current through the part, from end a to b */
    double p[EDGES];        /* power: used, or for batteries supplied */
    double supplied;        /* total power from the batteries */
} result_t;

/* edge_ends(): The two points of a link (a left/top of b) */
void edge_ends(int e, int *a, int *b);
bool edge_horizontal(int e);

/* part_resistance(): Resistance of a resistor or lamp (V^2 / P) */
double part_resistance(part_t const *p);

/* part_default(): A new part of this kind with its default values */
part_t part_default(kind_t kind);

/* circuit_solve(): DC operating point by nodal analysis */
void circuit_solve(circuit_t const *c, result_t *r);

#endif /* CIRCUIT_H */
