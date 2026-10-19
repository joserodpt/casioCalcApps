//---
//	color - The fx-CG colour names on both families
//
//	On the fx-CG (colour) this only includes <gint/display.h>. On the
//	fx-9860G (mono) gint has just C_WHITE, C_LIGHT, C_DARK and C_BLACK
//	(the greys need the grey engine: see screen_init()), so C_RGB() and
//	the named colours become the nearest of these four by brightness.
//	They stay constant expressions, for static tables.
//---

#ifndef CASIO_COLOR
#define CASIO_COLOR

#include <gint/display.h>
#include <gint/config.h>

#if GINT_HW_FX

/* Brightness of a 5-bit-per-channel colour, 0 (black) to 310 (white) */
#define C_LUMA_(r, g, b)  (3 * (r) + 6 * (g) + (b))

#define C_RGB(r, g, b) \
	(C_LUMA_(r, g, b) > 240 ? C_WHITE : \
	 C_LUMA_(r, g, b) > 150 ? C_LIGHT : \
	 C_LUMA_(r, g, b) > 50  ? C_DARK  : C_BLACK)

#define C_RED      C_BLACK
#define C_GREEN    C_BLACK
#define C_BLUE     C_BLACK

#endif /* GINT_HW_FX */

/* screen_init(): Turn on the fx-9860G grey engine (four shades); does
   nothing on the fx-CG. Call it once at start-up, after lang_init() */
void screen_init(void);

#endif /* CASIO_COLOR */
