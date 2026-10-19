#include <gint/display.h>
#include <gint/keyboard.h>
#include <stdarg.h>
#include <stdio.h>

#include "form.h"
#include "info.h"
#include "i18n.h"

#define FORM_Y      24
#define FORM_STEP   20

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
    dtext(10, y, C_DARK, label);
    dtext(val_x, y, color, buf);
}
