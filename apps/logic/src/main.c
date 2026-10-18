#include <gint/display.h>
#include <gint/keyboard.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "utils.h"
#include "info.h"

#define VERSION "v1.0"
#define COLOR   C_RGB(14, 6, 20)
#define ONEC    C_RGB(4, 16, 8)     /* a 1 in the table / map */
#define ZEROC   C_RGB(24, 24, 25)

/* Up to 4 variables, A..D; the truth table has up to 16 rows */
#define MAX_VARS 4

//---
//  Boolean expression parser
//
//  Grammar (lowest to highest precedence):
//    or    := xor ('+' xor)*            + or |
//    xor   := and ('^' and)*            ^
//    and   := not (('*'|'.'| )  not)*   * . or juxtaposition (AB)
//    not   := '!' not | atom            ! or ' after the atom
//    atom  := var | '1' | '0' | '(' or ')'
//  A result of -1 marks a syntax error.
//---

typedef struct {
    char const *s;
    int vars;           /* bitmask of variables seen */
    int row;            /* current assignment, bit (3-i) = variable i */
    bool error;
} parser_t;

static int parse_or(parser_t *p);

static void skip_spaces(parser_t *p)
{
    while (*p->s == ' ') p->s++;
}

static int var_bit(parser_t *p, int var)
{
    p->vars |= 1 << var;
    /* Variable A is the most significant bit of the row index */
    return (p->row >> (MAX_VARS - 1 - var)) & 1;
}

static int parse_atom(parser_t *p)
{
    skip_spaces(p);
    char c = *p->s;

    if (c >= 'A' && c <= 'D') { p->s++; return var_bit(p, c - 'A'); }
    if (c >= 'a' && c <= 'd') { p->s++; return var_bit(p, c - 'a'); }
    if (c == '1') { p->s++; return 1; }
    if (c == '0') { p->s++; return 0; }
    if (c == '(') {
        p->s++;
        int v = parse_or(p);
        skip_spaces(p);
        if (*p->s == ')') p->s++;
        else p->error = true;
        return v;
    }
    p->error = true;
    return 0;
}

static int parse_not(parser_t *p)
{
    skip_spaces(p);
    if (*p->s == '!' || *p->s == '/' || *p->s == '~') {
        p->s++;
        return !parse_not(p);
    }
    int v = parse_atom(p);
    /* Postfix NOT: A' */
    while (*p->s == '\'') { p->s++; v = !v; }
    return v;
}

/* True if the next token starts another AND factor (juxtaposition) */
static bool starts_factor(char c)
{
    return (c >= 'A' && c <= 'D') || (c >= 'a' && c <= 'd')
        || c == '(' || c == '!' || c == '/' || c == '~' || c == '0'
        || c == '1';
}

static int parse_and(parser_t *p)
{
    int v = parse_not(p);
    while (1) {
        skip_spaces(p);
        char c = *p->s;
        if (c == '*' || c == '.' || c == '&') {
            p->s++;
            v = parse_not(p) & v;
        }
        else if (starts_factor(c)) {
            v = parse_not(p) & v;
        }
        else break;
    }
    return v;
}

static int parse_xor(parser_t *p)
{
    int v = parse_and(p);
    skip_spaces(p);
    while (*p->s == '^') {
        p->s++;
        v ^= parse_and(p);
        skip_spaces(p);
    }
    return v;
}

static int parse_or(parser_t *p)
{
    int v = parse_xor(p);
    skip_spaces(p);
    while (*p->s == '+' || *p->s == '|') {
        p->s++;
        v |= parse_xor(p);
        skip_spaces(p);
    }
    return v;
}

/* Evaluate expr for a given row; sets *vars to the variables used. On a
   syntax error returns -1 */
static int eval(char const *expr, int row, int *vars)
{
    parser_t p = { .s = expr, .row = row };
    int v = parse_or(&p);
    skip_spaces(&p);
    if (*p.s) p.error = true;
    if (vars) *vars = p.vars;
    return p.error ? -1 : v;
}

//---
//  Model: minterms and variable count
//---

typedef struct {
    char expr[40];
    int cursor;
    uint16_t minterms;      /* bit i set = output 1 for row i */
    int var_count;          /* 2..4 */
    bool from_expr;         /* minterms came from the expression */
    bool error;
} model_t;

static int rows_of(model_t const *m)
{
    return 1 << m->var_count;
}

/* Recompute the minterms from the expression */
static void recompute(model_t *m)
{
    if (!m->from_expr) return;
    int used = 0;
    m->minterms = 0;
    m->error = false;

    for (int r = 0; r < 16; r++) {
        int vars;
        int v = eval(m->expr, r, &vars);
        if (v < 0) { m->error = true; return; }
        used |= vars;
        if (v) m->minterms |= 1 << r;
    }
    /* Variable count: at least the highest variable used, min 2 */
    int top = 0;
    for (int i = 0; i < MAX_VARS; i++) if (used & (1 << i)) top = i + 1;
    if (top < 2) top = 2;
    m->var_count = top;

    /* Keep only minterms within the active rows, folded down */
    uint16_t mask = (1 << rows_of(m)) - 1;
    if (m->var_count < MAX_VARS) {
        /* eval used A as MSB of a 4-bit row; with fewer vars the rows
           repeat, so collapse to the low rows */
        uint16_t folded = 0;
        for (int r = 0; r < 16; r++)
            if (m->minterms & (1 << r))
                folded |= 1 << (r >> (MAX_VARS - m->var_count));
        m->minterms = folded & mask;
    }
    else m->minterms &= mask;
}

//---
//  Quine-McCluskey style simplification (<= 4 variables)
//
//  An implicant is (bits, mask): mask bit set = that variable is fixed to
//  the value in bits. We find all prime implicants, then greedily cover
//  the minterms.
//---

typedef struct { uint8_t bits, mask; } impl_t;

/* Does implicant cover row r (within var_count variables)? */
static bool covers(impl_t im, int r)
{
    return (r & im.mask) == (im.bits & im.mask);
}

/* Number of minterms, as a cost tiebreaker */
static int popcount(unsigned v)
{
    int n = 0;
    while (v) { n += v & 1; v >>= 1; }
    return n;
}

#define MAX_IMPL 32

typedef struct {
    impl_t items[MAX_IMPL];
    int count;
} cover_t;

/* All prime implicants covering only minterms (and don't-cares none here) */
static int prime_implicants(model_t const *m, impl_t out[MAX_IMPL])
{
    int vc = m->var_count;
    int full = (1 << vc) - 1;
    int n = 0;

    /* Try implicants from largest (fewest fixed vars) to smallest so that
       a smaller one is skipped when a larger already covers its rows */
    for (int fixed = 0; fixed <= vc; fixed++) {
        /* choose which variables are fixed: masks with `fixed` bits set */
        for (int mask = 0; mask <= full; mask++) {
            if (popcount(mask) != fixed) continue;
            for (int bits = 0; bits <= full; bits++) {
                if (bits & ~mask) continue;     /* free vars are 0 in bits */
                impl_t im = { bits, mask };
                /* Every covered row must be a minterm */
                bool ok = true;
                for (int r = 0; r <= full && ok; r++)
                    if (covers(im, r) && !(m->minterms & (1 << r)))
                        ok = false;
                if (!ok) continue;
                /* Prime: not contained in an already-found larger one */
                bool prime = true;
                for (int i = 0; i < n; i++)
                    if ((out[i].mask & im.mask) == out[i].mask) {
                        /* out[i] has fewer or equal fixed vars; does it
                           cover all of im's rows? */
                        bool sub = true;
                        for (int r = 0; r <= full && sub; r++)
                            if (covers(im, r) && !covers(out[i], r))
                                sub = false;
                        if (sub) { prime = false; break; }
                    }
                if (prime && n < MAX_IMPL) out[n++] = im;
            }
        }
    }
    return n;
}

/* Greedy minimal cover of the minterms */
static void minimal_cover(model_t const *m, cover_t *cov)
{
    impl_t primes[MAX_IMPL];
    int np = prime_implicants(m, primes);
    int full = (1 << m->var_count) - 1;
    uint16_t need = m->minterms;
    cov->count = 0;

    /* Essential first: a minterm covered by exactly one prime */
    for (int r = 0; r <= full; r++) {
        if (!(need & (1 << r))) continue;
        int found = -1, cnt = 0;
        for (int i = 0; i < np; i++)
            if (covers(primes[i], r)) { found = i; cnt++; }
        if (cnt == 1) {
            /* add prime `found` if not already in */
            bool have = false;
            for (int j = 0; j < cov->count; j++)
                if (cov->items[j].bits == primes[found].bits
                    && cov->items[j].mask == primes[found].mask) have = true;
            if (!have) {
                cov->items[cov->count++] = primes[found];
                for (int rr = 0; rr <= full; rr++)
                    if (covers(primes[found], rr)) need &= ~(1 << rr);
            }
        }
    }
    /* Then greedily cover the rest with the largest prime each step */
    while (need) {
        int best = -1, best_cov = 0;
        for (int i = 0; i < np; i++) {
            int c = 0;
            for (int r = 0; r <= full; r++)
                if ((need & (1 << r)) && covers(primes[i], r)) c++;
            if (c > best_cov) { best_cov = c; best = i; }
        }
        if (best < 0) break;
        cov->items[cov->count++] = primes[best];
        for (int r = 0; r <= full; r++)
            if (covers(primes[best], r)) need &= ~(1 << r);
        if (cov->count >= MAX_IMPL) break;
    }
}

/* Text of one product term, e.g. "AB'" or "1" for the all-ones implicant */
static void term_text(char *buf, impl_t im, int vc)
{
    char *p = buf;
    for (int i = 0; i < vc; i++) {
        if (!(im.mask & (1 << (vc - 1 - i)))) continue;
        *p++ = 'A' + i;
        if (!((im.bits >> (vc - 1 - i)) & 1)) *p++ = '\'';
    }
    if (p == buf) *p++ = '1';       /* covers everything */
    *p = '\0';
}

/* Full sum-of-products into buf */
static void sop_text(char *buf, size_t n, model_t const *m,
    cover_t const *cov)
{
    if (m->minterms == 0) { snprintf(buf, n, "0"); return; }
    uint16_t all = (1 << rows_of(m)) - 1;
    if ((m->minterms & all) == all) { snprintf(buf, n, "1"); return; }

    buf[0] = '\0';
    size_t len = 0;
    for (int i = 0; i < cov->count; i++) {
        char term[12];
        term_text(term, cov->items[i], m->var_count);
        int w = snprintf(buf + len, n - len, "%s%s", i ? " + " : "", term);
        if (w > 0) len += w;
        if (len >= n) break;
    }
}

//---
//  Views
//---

enum { VIEW_INPUT, VIEW_TABLE, VIEW_KMAP };

static char const vnames[MAX_VARS] = { 'A', 'B', 'C', 'D' };

/* Expression editor and the live SOP */
static void draw_input(model_t const *m, cover_t const *cov)
{
    dtext(10, 24, C_DARK, "Expression  (F(A,B,C,D) =)");

    int bx = 10, by = 40, bw = DWIDTH - 20;
    drect_border(bx, by, bx + bw, by + 20, C_WHITE, 2,
        m->error ? C_RED : COLOR);
    font_t const *old = dfont(&font_big);
    dtext(bx + 6, by + 3, m->error ? C_RED : C_BLACK, m->expr);
    int cw;
    char tmp[40];
    memcpy(tmp, m->expr, m->cursor);
    tmp[m->cursor] = '\0';
    dsize(tmp, NULL, &cw, NULL);
    drect(bx + 6 + cw, by + 3, bx + 7 + cw, by + 16, COLOR);
    dfont(old);

    int y = 74;
    dtext(10, y, C_DARK, "Operators");
    dtext(110, y, C_BLACK,
        "+ OR   (x) AND   ^ XOR   ! NOT   ' NOT");
    y += 14;
    dtext(110, C_DARK ? y : y, C_DARK,
        "AB means A AND B. Variables A B C D.");
    y += 20;

    dline(6, y - 4, DWIDTH - 7, y - 4, C_LIGHT);
    if (m->error) {
        dtext(10, y, C_RED, "Syntax error");
    }
    else {
        char sop[80];
        sop_text(sop, sizeof sop, m, cov);
        dtext(10, y, C_DARK, "Minimal SOP");
        dtext(10, y + 16, COLOR, "F =");
        font_t const *o2 = dfont(&font_big);
        dtext(44, y + 15, C_BLACK, sop);
        dfont(o2);
        y += 40;
        dprint(10, y, C_DARK, "%d variable%s, %d minterm%s, %d term%s",
            m->var_count, m->var_count > 1 ? "s" : "",
            popcount(m->minterms), popcount(m->minterms) != 1 ? "s" : "",
            cov->count, cov->count != 1 ? "s" : "");
    }

    dtext_opt(DWIDTH / 2, 186, C_DARK, C_NONE, DTEXT_CENTER, DTEXT_TOP,
        "Vars A-D: F4-F6 and X,log,ln,sin   +*^()! = ops");
}

#define T_X0   40
#define T_ROWH 11

static void draw_table(model_t const *m)
{
    int vc = m->var_count, rows = rows_of(m);
    int colw = 26;
    int x0 = (DWIDTH - (vc + 2) * colw) / 2;
    int y0 = 30;

    /* Header */
    for (int i = 0; i < vc; i++)
        dprint_opt(x0 + i * colw + colw / 2, y0, COLOR, C_NONE, DTEXT_CENTER,
            DTEXT_TOP, "%c", vnames[i]);
    dtext_opt(x0 + (vc + 1) * colw - 2, y0, C_BLACK, C_NONE, DTEXT_RIGHT,
        DTEXT_TOP, "F");
    dtext_opt(x0 - 8, y0, C_DARK, C_NONE, DTEXT_RIGHT, DTEXT_TOP, "#");
    y0 += 14;
    dline(x0 - 24, y0 - 1, x0 + (vc + 1) * colw, y0 - 1, C_LIGHT);
    drect(x0 + vc * colw - 3, y0 - 14, x0 + vc * colw - 3,
        y0 + rows * T_ROWH, C_LIGHT);

    for (int r = 0; r < rows; r++) {
        int y = y0 + r * T_ROWH;
        bool one = (m->minterms >> r) & 1;
        if (r % 2) drect(x0 - 24, y, x0 + (vc + 1) * colw, y + T_ROWH - 1,
            C_RGB(29, 29, 30));
        dprint_opt(x0 - 8, y + 1, C_DARK, C_NONE, DTEXT_RIGHT, DTEXT_TOP,
            "%d", r);
        for (int i = 0; i < vc; i++) {
            int bit = (r >> (vc - 1 - i)) & 1;
            dprint_opt(x0 + i * colw + colw / 2, y + 1, C_BLACK, C_NONE,
                DTEXT_CENTER, DTEXT_TOP, "%d", bit);
        }
        dprint_opt(x0 + (vc + 1) * colw - 2, y + 1, one ? ONEC : C_LIGHT,
            C_NONE, DTEXT_RIGHT, DTEXT_TOP, "%d", one);
    }

    dtext_opt(DWIDTH / 2, 30 + 14 + rows * T_ROWH + 6, C_DARK, C_NONE,
        DTEXT_CENTER, DTEXT_TOP, "EXE on a row toggles F (minterm mode)");
}

/* Gray code order so neighbours differ in one bit */
static int const gray2[4] = { 0, 1, 3, 2 };

/* K-map cell size and origin */
#define K_CELL 40
#define K_X    150
#define K_Y    50

/* Row index for K-map cell (gr, gc) given variable layout */
static int kmap_row(model_t const *m, int gr, int gc, int *cols)
{
    int vc = m->var_count;
    if (vc == 2) { *cols = 2; return (gray2[gr] << 1) | gray2[gc]; }
    if (vc == 3) { *cols = 4; return (gray2[gr] << 2) | gray2[gc]; }
    *cols = 4;
    return (gray2[gr] << 2) | gray2[gc];
}

static void draw_kmap(model_t const *m, cover_t const *cov)
{
    int vc = m->var_count;
    int rows_n = vc == 2 ? 2 : vc == 3 ? 2 : 4;
    int cols_n = vc == 2 ? 2 : 4;
    /* 4-variable maps use smaller cells so they fit above the result */
    int cell = vc == 4 ? 34 : 40;
    int x0 = K_X, y0 = vc == 4 ? 46 : 54;
    int gw = cols_n * cell, gh = rows_n * cell;

    /* Top bits label the columns, the remaining bits the rows */
    char const *coltext = vc == 2 ? "B" : vc == 3 ? "BC" : "CD";
    char const *rowtext = vc == 2 ? "A" : vc == 3 ? "A" : "AB";
    dtext_opt(x0 + gw / 2, y0 - 28, COLOR, C_NONE, DTEXT_CENTER, DTEXT_TOP,
        coltext);
    dtext_opt(x0 - 32, y0 + gh / 2 - 6, COLOR, C_NONE, DTEXT_LEFT, DTEXT_TOP,
        rowtext);

    /* Gray-code header values */
    for (int c = 0; c < cols_n; c++) {
        int gv = gray2[c] & (cols_n == 2 ? 1 : 3);
        char lab[4];
        if (cols_n == 2) sprintf(lab, "%d", gv & 1);
        else sprintf(lab, "%d%d", (gv >> 1) & 1, gv & 1);
        dtext_opt(x0 + c * cell + cell / 2, y0 - 15, C_DARK, C_NONE,
            DTEXT_CENTER, DTEXT_TOP, lab);
    }
    for (int r = 0; r < rows_n; r++) {
        int gv = gray2[r] & (rows_n == 2 ? 1 : 3);
        char lab[4];
        if (rows_n == 2) sprintf(lab, "%d", gv & 1);
        else sprintf(lab, "%d%d", (gv >> 1) & 1, gv & 1);
        dtext_opt(x0 - 8, y0 + r * cell + cell / 2 - 6, C_DARK, C_NONE,
            DTEXT_RIGHT, DTEXT_TOP, lab);
    }

    /* Cells */
    for (int r = 0; r < rows_n; r++)
        for (int c = 0; c < cols_n; c++) {
            int cols, row = kmap_row(m, r, c, &cols);
            int x = x0 + c * cell, y = y0 + r * cell;
            bool one = (m->minterms >> row) & 1;
            drect(x + 1, y + 1, x + cell - 1, y + cell - 1,
                one ? C_RGB(10, 24, 14) : C_WHITE);
            drect_border(x, y, x + cell, y + cell, C_NONE, 1, C_LIGHT);
            dprint_opt(x + cell - 4, y + cell - 12, C_LIGHT, C_NONE,
                DTEXT_RIGHT, DTEXT_TOP, "%d", row);
            font_t const *old = dfont(&font_big);
            dtext_opt(x + cell / 2 - 3, y + 10, one ? C_WHITE : C_LIGHT,
                C_NONE, DTEXT_LEFT, DTEXT_TOP, one ? "1" : "0");
            dfont(old);
        }

    /* Group outlines, one colour each (inset so overlaps are visible) */
    static int const gcolors[] = {
        C_RGB(31, 6, 6), C_RGB(6, 10, 31), C_RGB(31, 18, 0),
        C_RGB(20, 4, 24), C_RGB(0, 18, 18), C_RGB(10, 10, 10) };
    for (int i = 0; i < cov->count; i++) {
        int color = gcolors[i % 6];
        int inset = 3 + (i % 3) * 2;
        for (int r = 0; r < rows_n; r++)
            for (int c = 0; c < cols_n; c++) {
                int cols, row = kmap_row(m, r, c, &cols);
                if (!covers(cov->items[i], row)) continue;
                int x = x0 + c * cell, y = y0 + r * cell;
                /* Draw edges only where the neighbour is not in the group */
                int rr[4], cc[4];
                rr[0] = (r - 1 + rows_n) % rows_n; cc[0] = c;   /* up */
                rr[1] = (r + 1) % rows_n; cc[1] = c;            /* down */
                rr[2] = r; cc[2] = (c - 1 + cols_n) % cols_n;   /* left */
                rr[3] = r; cc[3] = (c + 1) % cols_n;            /* right */
                bool nb[4];
                for (int k = 0; k < 4; k++) {
                    int cl, nrow = kmap_row(m, rr[k], cc[k], &cl);
                    nb[k] = covers(cov->items[i], nrow);
                }
                int xl = x + inset, yt = y + inset;
                int xr = x + cell - inset, yb = y + cell - inset;
                if (!nb[0]) drect(xl, yt, xr, yt + 1, color);
                if (!nb[1]) drect(xl, yb - 1, xr, yb, color);
                if (!nb[2]) drect(xl, yt, xl + 1, yb, color);
                if (!nb[3]) drect(xr - 1, yt, xr, yb, color);
            }
    }

    /* Legend: one coloured term per group, down the left side */
    dtext(10, 34, C_DARK, "Groups");
    if (cov->count == 0) dtext(10, 52, C_LIGHT, "none");
    for (int i = 0; i < cov->count && i < 6; i++) {
        char term[12];
        term_text(term, cov->items[i], m->var_count);
        int ly = 52 + i * 16;
        drect(10, ly + 2, 24, ly + 10, gcolors[i % 6]);
        dtext(30, ly, C_BLACK, term);
    }

    /* Simplified result along the bottom */
    char sop[80];
    sop_text(sop, sizeof sop, m, cov);
    int sy = 190;
    dline(6, sy - 5, DWIDTH - 7, sy - 5, C_LIGHT);
    dtext(10, sy, COLOR, "F =");
    font_t const *old = dfont(&font_big);
    dtext(46, sy - 1, C_BLACK, sop);
    dfont(old);
}

//---
//  Input
//---

static void insert(model_t *m, char c)
{
    int len = strlen(m->expr);
    if (len >= (int)sizeof m->expr - 1) return;
    memmove(m->expr + m->cursor + 1, m->expr + m->cursor, len - m->cursor + 1);
    m->expr[m->cursor++] = c;
    m->from_expr = true;
    recompute(m);
}

static void backspace(model_t *m)
{
    if (m->cursor == 0) return;
    int len = strlen(m->expr);
    memmove(m->expr + m->cursor - 1, m->expr + m->cursor, len - m->cursor + 1);
    m->cursor--;
    recompute(m);
}

/* SHIFT + key for operators that have no dedicated key */
static char shifted_op(int key)
{
    switch (key) {
        case KEY_ADD: return '+';
        case KEY_MUL: return '*';
        case KEY_POWER: return '^';
        case KEY_LEFTP: return '(';
        case KEY_RIGHTP: return ')';
        case KEY_DIV: return '!';
        case KEY_DOT: return '\'';
        default: return 0;
    }
}

static void draw(model_t *m, cover_t const *cov, int view)
{
    static char const * const titles[] = {
        "Boolean expression", "Truth table", "Karnaugh map" };

    dclear(C_WHITE);
    info_title(titles[view], COLOR);

    if (view == VIEW_INPUT) draw_input(m, cov);
    else if (view == VIEW_TABLE) draw_table(m);
    else draw_kmap(m, cov);

    static char const * const labels[] = { "EDIT", "TABLE", "KMAP" };
    for (int i = 0; i < 3; i++) {
        if (i == view) fkey_button_color(i + 1, labels[i], COLOR);
        else fkey_button(i + 1, labels[i]);
    }
    if (view == VIEW_INPUT) {
        fkey_button(4, "A");
        fkey_button(5, "B");
        fkey_button(6, "C");
    }
    dupdate();
}

int main(void)
{
    static model_t m = { .expr = "AB + C", .cursor = 5, .from_expr = true };
    static cover_t cov;
    int view = VIEW_INPUT;
    int table_sel = 0;

    recompute(&m);

    while (1) {
        minimal_cover(&m, &cov);
        draw(&m, &cov, view);

        int key = getkey().key;

        if (key == KEY_EXIT) return 1;
        if (key == INFO_KEY) {
            info_show("Logic", "Truth table / K-map " VERSION, COLOR);
            continue;
        }
        if (key == KEY_F1) { view = VIEW_INPUT; continue; }
        if (key == KEY_F2) { view = VIEW_TABLE; continue; }
        if (key == KEY_F3) { view = VIEW_KMAP; continue; }

        if (view == VIEW_INPUT) {
            char op = shifted_op(key);
            if (key == KEY_F4) insert(&m, 'A');
            else if (key == KEY_F5) insert(&m, 'B');
            else if (key == KEY_F6) insert(&m, 'C');
            /* Letter row X,θ,T / log / ln / sin = A B C D */
            else if (key == KEY_XOT) insert(&m, 'A');
            else if (key == KEY_LOG) insert(&m, 'B');
            else if (key == KEY_LN) insert(&m, 'C');
            else if (key == KEY_SIN) insert(&m, 'D');
            else if (key == KEY_LEFT && m.cursor > 0) m.cursor--;
            else if (key == KEY_RIGHT && m.cursor < (int)strlen(m.expr))
                m.cursor++;
            else if (key == KEY_DEL) backspace(&m);
            else if (key == KEY_ACON) { m.expr[0] = '\0'; m.cursor = 0;
                recompute(&m); }
            else if (key == KEY_ADD) insert(&m, '+');
            else if (key == KEY_MUL) insert(&m, '*');
            else if (key == KEY_POWER) insert(&m, '^');
            else if (key == KEY_LEFTP) insert(&m, '(');
            else if (key == KEY_RIGHTP) insert(&m, ')');
            else if (op) insert(&m, op);
            continue;
        }

        if (view == VIEW_TABLE) {
            int rows = rows_of(&m);
            if (key == KEY_UP) table_sel = (table_sel + rows - 1) % rows;
            else if (key == KEY_DOWN) table_sel = (table_sel + 1) % rows;
            else if (key == KEY_EXE) {
                m.from_expr = false;
                m.minterms ^= 1 << table_sel;
            }
            else if (key == KEY_F5) {    /* add/remove a variable */
                if (m.var_count < MAX_VARS) m.var_count++;
            }
            else if (key == KEY_F6) {
                if (m.var_count > 2) {
                    m.var_count--;
                    m.minterms &= (1 << rows_of(&m)) - 1;
                }
            }
            continue;
        }
    }
}
