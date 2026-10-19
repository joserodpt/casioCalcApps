//---
//	info - Shared title bar and "About" screen for every app
//---

#ifndef CASIO_INFO
#define CASIO_INFO

#include <gint/display.h>
#include <gint/keyboard.h>
#include "color.h"

/* Key that opens the info screen, and the hint shown in the title bar */
#define INFO_KEY   KEY_OPTN
#define INFO_HINT  "OPTN for info"

/* Large 10x14 monospace font (charset "print"), from common/assets-cg and
   common/assets-fx */
extern font_t const font_big;

/* info_title(): Title bar with the app name and the info key hint */
void info_title(char const *title, int color);

/* info_screen(): About screen (name, subtitle, author, build date, QR code
   to the repository); returns when a key is pressed. Call it through
   info_show() so that the build date is the app's own */
void info_screen(char const *name, char const *subtitle, int color,
	char const *date, char const *time);

#define info_show(name, subtitle, color) \
	info_screen(name, subtitle, color, __DATE__, __TIME__)

/* Name of the running app, for shared screens (forms, unit converter) that
   open the info screen; menu_run() sets it */
extern char const *app_name;

#define info_app(subtitle, color) info_show(app_name, subtitle, color)

#endif /* CASIO_INFO */
