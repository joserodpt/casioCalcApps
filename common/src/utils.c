#include <gint/display.h>
#include <stdbool.h>
#include <gint/gray.h>

#include "utils.h"
#include "color.h"

void screen_init(void)
{
#if GINT_HW_FX
    dgray(DGRAY_ON);
#endif
}

/* scrollbar_px(): Pixel-based scrollbar */
void scrollbar_px(int view_top, int view_bottom, int range_min, int range_max,
    int range_pos, int range_view)
{
    int view_x      = DWIDTH - 5;
    int view_width  = 2;
    int view_height = view_bottom - view_top;

    /* Bring virtual range to 0..range_max */
    range_max -= range_min;
    range_pos -= range_min;

    int bar_pos    = (range_pos  * view_height + range_max/2) / range_max;
    int bar_height = (range_view * view_height + range_max/2) / range_max;

    drect(view_x, view_top + bar_pos, view_x + view_width - 1,
        view_top + bar_pos + bar_height, C_BLACK);
}

/* fkey_button_color(): A rectangular F-key with a custom background */
void fkey_button_color(int position, char const *text, int color)
{
    int width;
    dsize(text, NULL, &width, NULL);

#if GINT_HW_FX
    /* Six 20-pixel keys along the bottom row (labels of up to 3 letters).
       Black keys; a coloured (selected) one is white with a black border */
    int x = 1 + 21 * (position - 1);
    int y = DHEIGHT - 8;
    int w = 20;
    bool sel = color != C_BLACK;

    if (sel) drect_border(x, y, x + w - 1, y + 7, C_WHITE, 1, C_BLACK);
    else drect(x, y, x + w - 1, y + 7, C_BLACK);
    dtext(x + ((w - width + 1) >> 1), y + 1, sel ? C_BLACK : C_WHITE, text);
#else
    int x = 4 + 65 * (position - 1);
    int y = 207;
    int w = 63;

    dline(x + 1, y, x + w - 2, y, color);
    dline(x + 1, y + 14, x + w - 2, y + 14, color);
    drect(x, y + 1, x + w - 1, y + 13, color);

    dtext(x + ((w - width) >> 1), y + 3, C_WHITE, text);
#endif
}

/* fkey_button(): A rectangular F-key */
void fkey_button(int position, char const *text)
{
    fkey_button_color(position, text, C_BLACK);
}
