//---
//	i18n - Pick the app's language from the calculator's OS language
//
//	Call lang_init() once at start-up (it reads the OS message language
//	through the gint world switch). Then wrap user-facing text in L():
//
//		dtext(10, 10, C_BLACK, L("Settings", "Definicoes"));
//
//	Only English and Portuguese are translated for now; every other OS
//	language falls back to the English string.
//---

#ifndef CASIO_I18N
#define CASIO_I18N

/* Order matches LocalizeMessage1(1)'s language-name detection */
enum { L_EN, L_ES, L_DE, L_FR, L_PT, L_ZH, L_COUNT };

/* The detected language (L_EN until lang_init() runs) */
extern int lang_current;

/* lang_init(): Detect the OS language. Safe to call once at start-up. */
void lang_init(void);

/* L(): English string, or its Portuguese translation when the calculator
   is set to Portuguese (any other language uses the English string). */
#define L(en, pt)  (lang_current == L_PT ? (pt) : (en))

#endif /* CASIO_I18N */
