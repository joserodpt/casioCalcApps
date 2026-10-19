#include <gint/display.h>
#include <gint/keyboard.h>

#include "menu.h"
#include "info.h"
#include "i18n.h"
#include "keys.h"

#if GINT_HW_FX

/* fx-9860G: one 9-pixel row per tool, up to 6 at once, scrolling */
#define ROWS 6

static void menu_draw(menu_t const *m, int sel)
{
    int top = sel < ROWS ? 0 : sel - ROWS + 1;

    dclear(C_WHITE);
    info_title(L(m->app, m->app_pt), m->color);
    for (int k = 0; k < ROWS && top + k < m->count; k++) {
        int i = top + k, y = 10 + 9 * k;
        menu_item_t const *it = &m->items[i];
        bool s = i == sel;
        if (s) drect(0, y - 1, DWIDTH - 1, y + 7, C_BLACK);
        dprint(2, y, s ? C_WHITE : C_DARK, "%d", i + 1);
        dtext(10, y, s ? C_WHITE : C_BLACK, L(it->name_en, it->name_pt));
    }
    dupdate();
}

#else

/* Up to 6 items get roomy 30-pixel rows; 7 need tighter 26-pixel ones */
typedef struct { int top, step, highlight, pad, box; } layout_t;
static layout_t const roomy = { 22, 30, 27, 4, 20 };
static layout_t const tight = { 20, 26, 24, 3, 18 };

static void menu_draw(menu_t const *m, int sel)
{
    layout_t const *l = m->count * roomy.step + roomy.top <= DHEIGHT
        ? &roomy : &tight;

    dclear(C_WHITE);
    info_title(L(m->app, m->app_pt), m->color);

    for (int i = 0; i < m->count; i++) {
        menu_item_t const *it = &m->items[i];
        int color = it->color;
        int y = l->top + i * l->step;
        int ty = y + l->pad + 5;

        if (i == sel) drect(0, y, DWIDTH - 1, y + l->highlight,
            C_RGB(28, 29, 31));
        drect(8, y + l->pad, 8 + l->box, y + l->pad + l->box, color);
        dprint_opt(8 + l->box / 2, ty, C_WHITE, C_NONE, DTEXT_CENTER,
            DTEXT_TOP, "%d", i + 1);
        dtext(18 + l->box, y + 3, i == sel ? color : C_BLACK,
            L(it->name_en, it->name_pt));
        dtext(18 + l->box, y + l->pad + 11, C_DARK,
            L(it->desc_en, it->desc_pt));
        if (i == sel) dtext_opt(DWIDTH - 10, ty, color, C_NONE,
            DTEXT_RIGHT, DTEXT_TOP, ">");
    }
    dupdate();
}

#endif /* GINT_HW_FX */

void menu_run(menu_t const *m)
{
    int sel = 0;
    app_name = L(m->app, m->app_pt);

    while (1) {
        menu_draw(m, sel);
        int key = getkey().key;
        int open = key_digit(key) - 1;

        if (key == KEY_EXIT) return;
        if (key == INFO_KEY)
            info_show(app_name, L(m->about_en, m->about_pt), m->color);
        else if (key == KEY_UP) sel = (sel + m->count - 1) % m->count;
        else if (key == KEY_DOWN) sel = (sel + 1) % m->count;
        else if (key == KEY_EXE || key == KEY_RIGHT) open = sel;

        if (open >= 0 && open < m->count) {
            sel = open;
            m->items[open].run();
        }
    }
}
