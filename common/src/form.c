#include <gint/display.h>
#include <gint/keyboard.h>
#include <stdarg.h>
#include <stdio.h>

#include "form.h"
#include "info.h"
#include "i18n.h"

#if GINT_HW_FX
#define FORM_Y      10
#define FORM_STEP   9
#define FORM_SHOWN  3       /* fields shown at once; the list scrolls */
#else
#define FORM_Y      24
#define FORM_STEP   20
#endif

static int form_cols(form_t const *f)
{
    return f->cols > 1 ? f->cols : 1;
}

static int form_rows(form_t const *f)
{
    return (f->count + form_cols(f) - 1) / form_cols(f);
}

static char const *form_title(form_t const *f)
{
    return lang_current == L_PT ? f->title_pt : f->title_en;
}

#if GINT_HW_FX

/* fx-9860G: one column of fields (FORM_SHOWN at once), results below */
static void form_draw(form_t *f)
{
    int shown = f->count < FORM_SHOWN ? f->count : FORM_SHOWN;
    int top = f->sel < shown ? 0 : f->sel - shown + 1;

    dclear(C_WHITE);
    info_title(form_title(f), f->color);

    int fx = 0, uw = 0;
    for (int i = 0; i < f->count; i++) {
        int w;
        dsize(L(f->fields[i].label_en, f->fields[i].label_pt), NULL, &w,
            NULL);
        if (w + 4 > fx) fx = w + 4;
        if (f->fields[i].unit) {
            dsize(f->fields[i].unit, NULL, &w, NULL);
            if (w + 2 > uw) uw = w + 2;
        }
    }

    for (int k = 0; k < shown; k++) {
        int i = top + k, y = FORM_Y + k * FORM_STEP;
        bool sel = i == f->sel;
        dtext(1, y + 1, sel ? C_BLACK : C_DARK,
            L(f->fields[i].label_en, f->fields[i].label_pt));
        numfield_draw(&f->fields[i].f, fx, y, DWIDTH - fx - uw - 1, sel,
            f->color);
        if (f->fields[i].unit)
            dtext(DWIDTH - uw + 1, y + 1, C_DARK, f->fields[i].unit);
    }
    /* More fields above or below: a mark at the right of the list */
    if (top > 0) dtext(DWIDTH - 4, FORM_Y - 1, C_DARK, "^");

    int ry = FORM_Y + shown * FORM_STEP + 1;
    dline(0, ry - 1, DWIDTH - 1, ry - 1, C_LIGHT);
    f->results(f, ry + 1);
    dupdate();
}

#else

static void form_draw(form_t *f)
{
    int cols = form_cols(f), rows = form_rows(f);
    int colw = DWIDTH / cols;

    dclear(C_WHITE);
    info_title(form_title(f), f->color);

    /* Fields start after the widest label (at least x+54) */
    int fx = 54;
    for (int i = 0; i < f->count; i++) {
        int w;
        dsize(L(f->fields[i].label_en, f->fields[i].label_pt), NULL, &w,
            NULL);
        if (10 + w + 8 > fx) fx = 10 + w + 8;
    }

    for (int i = 0; i < f->count; i++) {
        int x = (i / rows) * colw;
        int y = FORM_Y + (i % rows) * FORM_STEP;
        bool sel = i == f->sel;
        int bw = cols == 2 ? colw - 90 : 170;
        dtext(x + 10, y + 3, sel ? f->color : C_BLACK,
            L(f->fields[i].label_en, f->fields[i].label_pt));
        numfield_draw(&f->fields[i].f, x + fx, y, bw, sel, f->color);
        if (f->fields[i].unit)
            dtext(x + fx + bw + 4, y + 3, C_DARK, f->fields[i].unit);
    }

    int ry = FORM_Y + rows * FORM_STEP + 6;
    dline(6, ry - 4, DWIDTH - 7, ry - 4, C_LIGHT);
    f->results(f, ry);
    dupdate();
}

#endif /* GINT_HW_FX */

void form_run(form_t *f)
{
    int rows = form_rows(f);

    while (1) {
        form_draw(f);
        int key = getkey().key;

        if (key == KEY_EXIT) return;
        if (key == INFO_KEY) info_app(form_title(f), f->color);
        else if (key == KEY_UP) f->sel = (f->sel + f->count - 1) % f->count;
        else if (key == KEY_DOWN || key == KEY_EXE)
            f->sel = (f->sel + 1) % f->count;
        else if (key == KEY_LEFT && f->sel - rows >= 0) f->sel -= rows;
        else if (key == KEY_RIGHT && f->sel + rows < f->count) f->sel += rows;
        else numfield_key(&f->fields[f->sel].f, key);
    }
}

double form_value(form_t const *f, int i)
{
    return numfield_value(&f->fields[i].f);
}

int form_one_empty(form_t const *f)
{
    int idx = -1, n = 0;
    for (int i = 0; i < f->count; i++)
        if (f->fields[i].f.text[0] == '\0') { idx = i; n++; }
    return n == 1 ? idx : -1;
}

void form_result(int y, int val_x, char const *label, int color,
    char const *fmt, ...)
{
    char buf[48];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
#if GINT_HW_FX
    /* The label at the left, the value against the right edge */
    (void)val_x;
    dtext(1, y, C_DARK, label);
    dtext_opt(DWIDTH - 1, y, color, C_NONE, DTEXT_RIGHT, DTEXT_TOP, buf);
#else
    dtext(10, y, C_DARK, label);
    dtext(val_x, y, color, buf);
#endif
}
