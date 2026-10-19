#include <math.h>
#include <string.h>

#include "circuit.h"

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

double part_resistance(part_t const *p)
{
    if (p->kind == P_LAMP) return p->value * p->value / p->value2;
    return p->value;
}

part_t part_default(kind_t kind)
{
    part_t p = { .kind = kind };
    if (kind == P_RES) p.value = 100;
    if (kind == P_BATT) p.value = 9;
    if (kind == P_LAMP) { p.value = 6; p.value2 = 3; }
    if (kind == P_SWITCH) p.closed = 1;
    return p;
}

/* Parts that join their two points into one node */
static bool joins(part_t const *p)
{
    return p->kind == P_WIRE || (p->kind == P_SWITCH && p->closed);
}

/* Parts that go into the equations */
static bool element(part_t const *p)
{
    return p->kind == P_RES || p->kind == P_BATT || p->kind == P_LAMP;
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
//  Linear solver: Gaussian elimination with partial pivoting
//---

#define MAXN 72
static double M[MAXN][MAXN + 1];

static void m_clear(int n)
{
    for (int r = 0; r < n; r++)
        memset(M[r], 0, (n + 1) * sizeof(double));
}

/* Solve M[0..n-1][0..n-1] x = M[.][n]; false if (nearly) singular */
static bool m_solve(int n, double *x)
{
    double scale = 0;
    for (int r = 0; r < n; r++)
        for (int c = 0; c < n; c++)
            if (fabs(M[r][c]) > scale) scale = fabs(M[r][c]);

    for (int c = 0; c < n; c++) {
        int p = c;
        for (int r = c + 1; r < n; r++)
            if (fabs(M[r][c]) > fabs(M[p][c])) p = r;
        if (fabs(M[p][c]) <= 1e-13 * scale) return false;
        if (p != c) {
            for (int k = c; k <= n; k++) {
                double t = M[c][k];
                M[c][k] = M[p][k];
                M[p][k] = t;
            }
        }
        for (int r = c + 1; r < n; r++) {
            double f = M[r][c] / M[c][c];
            if (f == 0) continue;
            for (int k = c; k <= n; k++) M[r][k] -= f * M[c][k];
        }
    }
    for (int r = n - 1; r >= 0; r--) {
        double s = M[r][n];
        for (int k = r + 1; k < n; k++) s -= M[r][k] * x[k];
        x[r] = s / M[r][r];
    }
    return true;
}

//---
//  Wire currents
//
//  Wires have no resistance, so the nodal solution gives no current for
//  them. Each node is solved again as a network of equal small resistors
//  fed by the currents the parts take in and out at each point: that is
//  how the current really splits between parallel wires.
//---

static void wire_currents(circuit_t const *c, result_t *r, int const *uf,
    int node)
{
    static int pos[NODES];
    static int pts[NODES];
    static double out[NODES];
    static double x[MAXN];
    int m = 0;

    /* Points of this node that touch a wire */
    memset(out, 0, sizeof out);
    for (int n = 0; n < NODES; n++) pos[n] = -1;
    for (int e = 0; e < EDGES; e++) {
        int a, b;
        edge_ends(e, &a, &b);
        part_t const *p = &c->parts[e];
        if (element(p)) {
            out[a] += r->i[e];
            out[b] -= r->i[e];
        }
        if (!joins(p) || uf_find((int *)uf, a) != node) continue;
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
        int i = pos[a] - 1, j = pos[b] - 1;
        if (i >= 0) M[i][i] += 1;
        if (j >= 0) M[j][j] += 1;
        if (i >= 0 && j >= 0) { M[i][j] -= 1; M[j][i] -= 1; }
    }
    for (int k = 1; k < m; k++) M[k - 1][m - 1] = -out[pts[k]];
    if (!m_solve(m - 1, x)) return;

    for (int e = 0; e < EDGES; e++) {
        int a, b;
        edge_ends(e, &a, &b);
        if (!joins(&c->parts[e]) || pos[a] < 0) continue;
        double pa = pos[a] ? x[pos[a] - 1] : 0;
        double pb = pos[b] ? x[pos[b] - 1] : 0;
        r->i[e] = pa - pb;
    }
}

//---
//  Nodal analysis
//---

void circuit_solve(circuit_t const *c, result_t *r)
{
    static int uf[NODES], grp[NODES], ground[NODES], idx[NODES];
    static double x[MAXN];
    int elements = 0;

    r->status = S_OK;
    r->bad_edge = -1;
    r->supplied = 0;
    for (int n = 0; n < NODES; n++) {
        r->v[n] = NAN;
        uf[n] = grp[n] = n;
        ground[n] = -1;
        idx[n] = -2;
    }
    for (int e = 0; e < EDGES; e++) r->i[e] = r->p[e] = 0;

    /* Points joined by wires and closed switches form one node */
    for (int e = 0; e < EDGES; e++) {
        int a, b;
        edge_ends(e, &a, &b);
        if (joins(&c->parts[e])) uf_join(uf, a, b);
    }

    /* A battery whose terminals are on the same node is short-circuited */
    for (int e = 0; e < EDGES; e++) {
        part_t const *p = &c->parts[e];
        if (!element(p)) continue;
        elements++;
        int a, b;
        edge_ends(e, &a, &b);
        if (p->kind == P_BATT && uf_find(uf, a) == uf_find(uf, b)) {
            r->status = S_SHORT;
            r->bad_edge = e;
        }
    }
    if (!elements) { r->status = S_EMPTY; return; }
    if (r->status == S_SHORT) return;

    /* Group the nodes that parts connect; each group gets its own 0 V
       reference, preferably a battery's - terminal */
    for (int e = 0; e < EDGES; e++) {
        int a, b;
        edge_ends(e, &a, &b);
        if (element(&c->parts[e]))
            uf_join(grp, uf_find(uf, a), uf_find(uf, b));
    }
    for (int pass = 0; pass < 2; pass++)
    for (int e = 0; e < EDGES; e++) {
        part_t const *p = &c->parts[e];
        if (!element(p) || (pass == 0) != (p->kind == P_BATT)) continue;
        int a, b;
        edge_ends(e, &a, &b);
        int ref = uf_find(uf, p->kind == P_BATT && !p->flip ? b : a);
        int g = uf_find(grp, ref);
        if (ground[g] < 0) ground[g] = ref;
    }

    /* Number the unknown node voltages, then one current per battery */
    int nv = 0, nb = 0;
    for (int e = 0; e < EDGES; e++) {
        part_t const *p = &c->parts[e];
        if (!element(p)) continue;
        if (p->kind == P_BATT) nb++;
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
    int n = nv + nb;
    if (n > MAXN) { r->status = S_TOOBIG; return; }

    m_clear(n);
    int k = nv;
    for (int e = 0; e < EDGES; e++) {
        part_t const *p = &c->parts[e];
        if (!element(p)) continue;
        int a, b;
        edge_ends(e, &a, &b);
        int i = idx[uf_find(uf, a)], j = idx[uf_find(uf, b)];

        if (p->kind == P_BATT) {
            /* Unknown: current into the + terminal (out of the circuit at
               +, back in at -); equation: V+ - V- = V */
            int ip = p->flip ? j : i, in = p->flip ? i : j;
            if (ip >= 0) { M[ip][k] += 1; M[k][ip] += 1; }
            if (in >= 0) { M[in][k] -= 1; M[k][in] -= 1; }
            M[k][n] = p->value;
            k++;
            continue;
        }
        if (uf_find(uf, a) == uf_find(uf, b)) continue;
        double g = 1 / part_resistance(p);
        if (i >= 0) M[i][i] += g;
        if (j >= 0) M[j][j] += g;
        if (i >= 0 && j >= 0) { M[i][j] -= g; M[j][i] -= g; }
    }
    if (!m_solve(n, x)) { r->status = S_LOOP; return; }

    for (int pt = 0; pt < NODES; pt++) {
        int s = uf_find(uf, pt);
        if (idx[s] == -1) r->v[pt] = 0;
        else if (idx[s] >= 0) r->v[pt] = x[idx[s]];
    }

    k = nv;
    for (int e = 0; e < EDGES; e++) {
        part_t const *p = &c->parts[e];
        if (!element(p)) continue;
        int a, b;
        edge_ends(e, &a, &b);
        if (p->kind == P_BATT) {
            /* x[k] flows into + and out of -, inside the battery */
            r->i[e] = p->flip ? -x[k] : x[k];
            r->p[e] = -x[k] * p->value;
            r->supplied += r->p[e];
            k++;
        }
        else {
            double R = part_resistance(p);
            if (uf_find(uf, a) == uf_find(uf, b)) continue;
            r->i[e] = (r->v[a] - r->v[b]) / R;
            r->p[e] = r->i[e] * r->i[e] * R;
        }
    }

    /* Then the currents in the wires of each node */
    for (int pt = 0; pt < NODES; pt++)
        if (uf_find(uf, pt) == pt) wire_currents(c, r, uf, pt);
}
