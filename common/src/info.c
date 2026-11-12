#include <gint/display.h>
#include <gint/keyboard.h>

#include "info.h"
#include "i18n.h"

#define AUTHOR      "joserodpt"
#define REPO_LINE1  "github.com/joserodpt/"
#define REPO_LINE2  "casioCalcApps"

char const *app_name = "";

#if GINT_HW_FX

/* fx-9860G: a 9-pixel title bar (content starts at y = 10), and a
   text-only About screen */

void info_title(char const *title, int color)
{
    (void)color;
    drect(0, 0, DWIDTH - 1, 8, C_BLACK);
    /* A long title is cut short before "OPTN" */
    struct dwindow clip = { 0, 0, DWIDTH - 28, DHEIGHT };
    struct dwindow old = dwindow_set(clip);
    dtext(2, 1, C_WHITE, title);
    dwindow_set(old);
    dtext_opt(DWIDTH - 2, 1, C_LIGHT, C_NONE, DTEXT_RIGHT, DTEXT_TOP,
        "OPTN");
}

void info_screen(char const *name, char const *subtitle, int color,
    char const *date, char const *time)
{
    (void)color;
    (void)time;
    dclear(C_WHITE);
    info_title(L("About", "Sobre", "A propos"), C_BLACK);

    font_t const *old = dfont(&font_big);
    dtext(2, 11, C_BLACK, name);
    dfont(old);
    if (subtitle) dtext(2, 27, C_DARK, subtitle);
    dprint(2, 36, C_BLACK, AUTHOR " (C) %s", date + 7);
    dtext(2, 46, C_DARK, REPO_LINE1);
    dtext(2, 55, C_DARK, REPO_LINE2);
    dupdate();

    getkey();
}

#else

extern bopti_image_t const img_qr_repo;

/* info_title(): Title bar with the app name and the info key hint */
void info_title(char const *title, int color)
{
    drect(0, 0, DWIDTH - 1, 15, color);
    dtext(6, 3, C_WHITE, title);
    dtext_opt(DWIDTH - 6, 3, C_WHITE, C_NONE, DTEXT_RIGHT, DTEXT_TOP,
        L("OPTN for info", "OPTN p/ info", "OPTN: infos"));
}

/* info_screen(): About screen; returns when a key is pressed */
void info_screen(char const *name, char const *subtitle, int color,
    char const *date, char const *time)
{
    int const x = 158;

    dclear(C_WHITE);
    drect(0, 0, DWIDTH - 1, 15, color);
    dtext(6, 3, C_WHITE, L("About", "Sobre", "A propos"));

    dimage(14, 36, &img_qr_repo);

    font_t const *old = dfont(&font_big);
    dtext(x, 34, color, name);
    dfont(old);
    if (subtitle) dtext(x, 54, C_DARK, subtitle);

    /* date is __DATE__ ("Mmm dd yyyy"), so the year starts at offset 7 */
    dprint(x, 78, C_BLACK, L("Made by " AUTHOR " (C) %s",
        "Feito por " AUTHOR " (C) %s", "Par " AUTHOR " (C) %s"), date + 7);
    dprint(x, 92, C_DARK, L("Built %s %s", "Compilado %s %s", "Compile %s %s"),
        date, time);

    dtext(x, 116, C_DARK, L("Source code:", "Codigo fonte:",
        "Code source :"));
    dtext(x, 130, C_BLACK, REPO_LINE1);
    dtext(x, 144, C_BLACK, REPO_LINE2);

    dtext_opt(DWIDTH / 2, 196, C_DARK, C_NONE, DTEXT_CENTER, DTEXT_TOP,
        L("Press any key to go back", "Prima uma tecla para voltar",
          "Appuyez sur une touche"));
    dupdate();

    getkey();
}

#endif /* GINT_HW_FX */
