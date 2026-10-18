#include <gint/display.h>
#include <gint/keyboard.h>

#include "info.h"

#define AUTHOR      "joserodpt"
#define REPO_LINE1  "github.com/joserodpt/"
#define REPO_LINE2  "casioPrizmApps"

extern bopti_image_t const img_qr_repo;

/* info_title(): Title bar with the app name and the info key hint */
void info_title(char const *title, int color)
{
    drect(0, 0, DWIDTH - 1, 15, color);
    dtext(6, 3, C_WHITE, title);
    dtext_opt(DWIDTH - 6, 3, C_WHITE, C_NONE, DTEXT_RIGHT, DTEXT_TOP,
        INFO_HINT);
}

/* info_screen(): About screen; returns when a key is pressed */
void info_screen(char const *name, char const *subtitle, int color,
    char const *date, char const *time)
{
    int const x = 172;

    dclear(C_WHITE);
    drect(0, 0, DWIDTH - 1, 15, color);
    dtext(6, 3, C_WHITE, "About");

    dimage(20, 36, &img_qr_repo);

    font_t const *old = dfont(&font_big);
    dtext(x, 34, color, name);
    dfont(old);
    if (subtitle) dtext(x, 54, C_DARK, subtitle);

    /* date is __DATE__ ("Mmm dd yyyy"), so the year starts at offset 7 */
    dprint(x, 78, C_BLACK, "Made by " AUTHOR " (C) %s", date + 7);
    dprint(x, 92, C_DARK, "Built %s %s", date, time);

    dtext(x, 116, C_DARK, "Source code:");
    dtext(x, 130, C_BLACK, REPO_LINE1);
    dtext(x, 144, C_BLACK, REPO_LINE2);

    dtext_opt(DWIDTH / 2, 196, C_DARK, C_NONE, DTEXT_CENTER, DTEXT_TOP,
        "Press any key to go back");
    dupdate();

    getkey();
}
