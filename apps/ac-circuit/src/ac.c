#include <math.h>
#include <string.h>

#include "ac.h"

#define PI 3.14159265358979323846

//---
//  Complex numbers
//---

cplx c_div(cplx a, cplx b)
{
    /* Smith's method: no overflow for large or small parts */
    if (fabs(b.re) >= fabs(b.im)) {
        double r = b.im / b.re, d = b.re + b.im * r;
        return c_make((a.re + a.im * r) / d, (a.im - a.re * r) / d);
    }
    double r = b.re / b.im, d = b.re * r + b.im;
    return c_make((a.re * r + a.im) / d, (a.im * r - a.re) / d);
}

double c_abs(cplx a)
{
    return hypot(a.re, a.im);
}

double c_deg(cplx a)
{
    if (a.re == 0 && a.im == 0) return 0;
    return atan2(a.im, a.re) * 180 / PI;
}

cplx c_polar(double mag, double deg)
{
    double t = deg * PI / 180;
    return c_make(mag * cos(t), mag * sin(t));
}

//---
//  Parts
//---

void edge_ends(int e, int *a, int *b)
{
    if (e < H_EDGES) {
        int y = e / (NX - 1), x = e % (NX - 1);
        *a = y * NX + x;
        *b = *a + 1;
    }
    else {
        e -= H_EDGES;
        *a = e;
        *b = e + NX;
    }
}

bool edge_horizontal(int e)
{
    return e < H_EDGES;
}

cplx part_impedance(part_t const *p, double f)
{
    double w = 2 * PI * f;
    switch (p->kind) {
    case P_CAP: return c_make(0, -1 / (w * p->value));
    case P_IND: return c_make(p->value2 > L_RMIN ? p->value2 : L_RMIN,
        w * p->value);
    case P_LAMP: return c_make(p->value * p->value / p->value2, 0);
    default: return c_make(p->value, 0);
    }
}

part_t part_default(kind_t kind)
{
    part_t p = { .kind = kind };
    if (kind == P_RES) p.value = 100;
    if (kind == P_CAP) p.value = 10e-6;
    if (kind == P_IND) p.value = 10e-3;
    if (kind == P_SRC) p.value = 10;
    if (kind == P_LAMP) { p.value = 6; p.value2 = 3; }
    if (kind == P_SWITCH) p.closed = 1;
    return p;
}

/* Parts that join their two points into one node */
static bool joins(part_t const *p)
{
    return p->kind == P_WIRE || p->kind == P_AMMETER
        || (p->kind == P_SWITCH && p->closed);
}

/* Impedances */
static bool passive(part_t const *p)
{
    return p->kind == P_RES || p->kind == P_CAP || p->kind == P_IND
        || p->kind == P_LAMP;
}

bool part_element(part_t const *p)
{
    return passive(p) || p->kind == P_SRC;
}

cplx across(result_t const *r, int e)
{
    int a, b;
    edge_ends(e, &a, &b);
    if (r->status != S_OK || !r->known[a] || !r->known[b]) return c_make(0, 0);
    return c_sub(r->v[a], r->v[b]);
}

//---
//  Union-find, to merge points joined by wires into nodes
//---

static int uf_find(int *uf, int a)
{
    while (uf[a] != a) a = uf[a] = uf[uf[a]];
    return a;
}

static void uf_join(int *uf, int a, int b)
{
    uf[uf_find(uf, a)] = uf_find(uf, b);
}

//---
//  Linear solver: complex Gaussian elimination with partial pivoting
//---

#define MAXN 72
static cplx M[MAXN][MAXN + 1];

static void m_clear(int n)
{
    for (int r = 0; r < n; r++)
        memset(M[r], 0, (n + 1) * sizeof(cplx));
}

/* |re| + |im|: good enough to choose pivots, and cheap */
static double c_norm1(cplx a)
{
    return fabs(a.re) + fabs(a.im);
}

/* Solve M[0..n-1][0..n-1] x = M[.][n]; false if (nearly) singular */
static bool m_solve(int n, cplx *x)
{
    double scale = 0;
    for (int r = 0; r < n; r++)
        for (int c = 0; c < n; c++)
            if (c_norm1(M[r][c]) > scale) scale = c_norm1(M[r][c]);

    for (int c = 0; c < n; c++) {
        int p = c;
        for (int r = c + 1; r < n; r++)
            if (c_norm1(M[r][c]) > c_norm1(M[p][c])) p = r;
        if (c_norm1(M[p][c]) <= 1e-14 * scale) return false;
        if (p != c) {
            for (int k = c; k <= n; k++) {
                cplx t = M[c][k];
                M[c][k] = M[p][k];
                M[p][k] = t;
            }
        }
        for (int r = c + 1; r < n; r++) {
            if (M[r][c].re == 0 && M[r][c].im == 0) continue;
            cplx f = c_div(M[r][c], M[c][c]);
            for (int k = c; k <= n; k++)
                M[r][k] = c_sub(M[r][k], c_mul(f, M[c][k]));
        }
    }
    for (int r = n - 1; r >= 0; r--) {
        cplx s = M[r][n];
        for (int k = r + 1; k < n; k++) s = c_sub(s, c_mul(M[r][k], x[k]));
        x[r] = c_div(s, M[r][r]);
    }
    return true;
}

/* Admittance y between unknowns i and j (-1: the 0 V reference) */
static void stamp_y(int i, int j, cplx y)
{
    if (i >= 0) M[i][i] = c_add(M[i][i], y);
    if (j >= 0) M[j][j] = c_add(M[j][j], y);
    if (i >= 0 && j >= 0) {
        M[i][j] = c_sub(M[i][j], y);
        M[j][i] = c_sub(M[j][i], y);
    }
}

//---
//  Wire currents
//
//  Wires have no impedance, so the nodal solution gives no current for
//  them. As in the DC app, each node is solved again as a network of equal
//  small resistors fed by the currents the parts take in and out at each
//  point: that is how the current splits between parallel wires.
//---

static void wire_currents(circuit_t const *c, result_t *r, int *uf, int node)
{
    static int pos[NODES];
    static int pts[NODES];
    static cplx out[NODES];
    static cplx x[MAXN];
    int m = 0;

    for (int n = 0; n < NODES; n++) {
        pos[n] = -1;
        out[n] = c_make(0, 0);
    }
    for (int e = 0; e < EDGES; e++) {
        int a, b;
        edge_ends(e, &a, &b);
        part_t const *p = &c->parts[e];
        if (part_element(p)) {
            out[a] = c_add(out[a], r->i[e]);
            out[b] = c_sub(out[b], r->i[e]);
        }
        if (!joins(p) || uf_find(uf, a) != node) continue;
        if (pos[a] < 0) { pos[a] = m; pts[m++] = a; }
        if (pos[b] < 0) { pos[b] = m; pts[m++] = b; }
    }
    if (m < 2 || m - 1 > MAXN) return;

    /* Point 0 is the reference; unknowns are the others (index - 1) */
    m_clear(m - 1);
    for (int e = 0; e < EDGES; e++) {
        int a, b;
        edge_ends(e, &a, &b);
        if (!joins(&c->parts[e]) || pos[a] < 0) continue;
        stamp_y(pos[a] - 1, pos[b] - 1, c_make(1, 0));
    }
    for (int k = 1; k < m; k++) M[k - 1][m - 1] = c_scale(out[pts[k]], -1);
    if (!m_solve(m - 1, x)) return;

    for (int e = 0; e < EDGES; e++) {
        int a, b;
        edge_ends(e, &a, &b);
        if (!joins(&c->parts[e]) || pos[a] < 0) continue;
        cplx pa = pos[a] ? x[pos[a] - 1] : c_make(0, 0);
        cplx pb = pos[b] ? x[pos[b] - 1] : c_make(0, 0);
        r->i[e] = c_sub(pa, pb);
    }
}

//---
//  Nodal analysis
//---

void circuit_solve(circuit_t const *c, double f, result_t *r)
{
    static int uf[NODES], grp[NODES], ground[NODES], idx[NODES];
    static cplx x[MAXN];
    int sources = 0, loads = 0;

    r->status = S_OK;
    r->bad_edge = -1;
    r->ref_source = -1;
    r->supplied = c_make(0, 0);
    for (int n = 0; n < NODES; n++) {
        r->v[n] = c_make(0, 0);
        r->known[n] = false;
        uf[n] = grp[n] = n;
        ground[n] = -1;
        idx[n] = -2;
    }
    for (int e = 0; e < EDGES; e++)
        r->i[e] = r->s[e] = c_make(0, 0);

    /* Points joined by wires, ammeters and closed switches are one node */
    for (int e = 0; e < EDGES; e++) {
        int a, b;
        edge_ends(e, &a, &b);
        if (joins(&c->parts[e])) uf_join(uf, a, b);
    }

    /* A source whose terminals are on the same node is short-circuited */
    for (int e = 0; e < EDGES; e++) {
        part_t const *p = &c->parts[e];
        if (!part_element(p)) continue;
        int a, b;
        edge_ends(e, &a, &b);
        if (p->kind == P_SRC) {
            if (r->ref_source < 0) r->ref_source = e;
            sources++;
            if (uf_find(uf, a) == uf_find(uf, b)) {
                r->status = S_SHORT;
                r->bad_edge = e;
            }
        }
        else loads++;
    }
    if (!sources || !loads) { r->status = S_EMPTY; return; }
    if (r->status == S_SHORT) return;

    /* Group the nodes that parts connect; each group gets its own 0 V
       reference, preferably a source's - terminal */
    for (int e = 0; e < EDGES; e++) {
        int a, b;
        edge_ends(e, &a, &b);
        if (part_element(&c->parts[e]))
            uf_join(grp, uf_find(uf, a), uf_find(uf, b));
    }
    for (int pass = 0; pass < 2; pass++)
    for (int e = 0; e < EDGES; e++) {
        part_t const *p = &c->parts[e];
        if (!part_element(p) || (pass == 0) != (p->kind == P_SRC)) continue;
        int a, b;
        edge_ends(e, &a, &b);
        int ref = uf_find(uf, p->kind == P_SRC && !p->flip ? b : a);
        int g = uf_find(grp, ref);
        if (ground[g] < 0) ground[g] = ref;
    }

    /* Number the unknown node voltages, then one current per source */
    int nv = 0;
    for (int e = 0; e < EDGES; e++) {
        part_t const *p = &c->parts[e];
        if (!part_element(p)) continue;
        int ends[2];
        edge_ends(e, &ends[0], &ends[1]);
        /* A part shorted by a wire carries nothing and adds no equation */
        if (uf_find(uf, ends[0]) == uf_find(uf, ends[1])) continue;
        for (int k = 0; k < 2; k++) {
            int s = uf_find(uf, ends[k]);
            if (idx[s] != -2) continue;
            idx[s] = ground[uf_find(grp, s)] == s ? -1 : nv++;
        }
    }
    int n = nv + sources;
    if (n > MAXN) { r->status = S_TOOBIG; return; }

    m_clear(n);
    int k = nv;
    for (int e = 0; e < EDGES; e++) {
        part_t const *p = &c->parts[e];
        if (!part_element(p)) continue;
        int a, b;
        edge_ends(e, &a, &b);
        int i = idx[uf_find(uf, a)], j = idx[uf_find(uf, b)];

        if (p->kind == P_SRC) {
            /* Unknown: current into the + terminal (out of the circuit at
               +, back in at -); equation: V+ - V- = V at its phase */
            int ip = p->flip ? j : i, in = p->flip ? i : j;
            if (ip >= 0) { M[ip][k].re += 1; M[k][ip].re += 1; }
            if (in >= 0) { M[in][k].re -= 1; M[k][in].re -= 1; }
            M[k][n] = c_polar(p->value, p->value2);
            k++;
            continue;
        }
        if (uf_find(uf, a) == uf_find(uf, b)) continue;
        stamp_y(i, j, c_div(c_make(1, 0), part_impedance(p, f)));
    }
    if (!m_solve(n, x)) { r->status = S_LOOP; return; }

    for (int pt = 0; pt < NODES; pt++) {
        int s = uf_find(uf, pt);
        if (idx[s] == -1) r->known[pt] = true;
        else if (idx[s] >= 0) {
            r->v[pt] = x[idx[s]];
            r->known[pt] = true;
        }
    }

    k = nv;
    for (int e = 0; e < EDGES; e++) {
        part_t const *p = &c->parts[e];
        if (!part_element(p)) continue;
        int a, b;
        edge_ends(e, &a, &b);
        if (p->kind == P_SRC) {
            /* x[k] flows into + and out of -, inside the source */
            cplx vs = c_polar(p->value, p->value2);
            r->i[e] = p->flip ? c_scale(x[k], -1) : x[k];
            r->s[e] = c_scale(c_mul(vs, c_conj(x[k])), -1);
            r->supplied = c_add(r->supplied, r->s[e]);
            k++;
            continue;
        }
        if (uf_find(uf, a) == uf_find(uf, b)) continue;
        cplx vab = c_sub(r->v[a], r->v[b]);
        r->i[e] = c_div(vab, part_impedance(p, f));
        r->s[e] = c_mul(vab, c_conj(r->i[e]));
    }

    /* Then the currents in the wires of each node */
    for (int pt = 0; pt < NODES; pt++)
        if (uf_find(uf, pt) == pt) wire_currents(c, r, uf, pt);
}
