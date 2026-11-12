//---
//	i18n - Pick the app's language from the calculator's OS language
//
//	Call lang_init() once at start-up (it reads the OS message language
//	through the gint world switch). Then wrap user-facing text in L():
//
//		dtext(10, 10, C_BLACK, L("Settings", "Definicoes", "Reglages"));
//
//	Only English, Portuguese and French are translated for now; every
//	other OS language falls back to the English string. The fonts are
//	ASCII only, so translations leave out accents.
//---

#ifndef CASIO_I18N
#define CASIO_I18N

/* Order matches LocalizeMessage1(1)'s language-name detection */
enum { L_EN, L_ES, L_DE, L_FR, L_PT, L_ZH, L_COUNT };

/* The detected language (L_EN until lang_init() runs) */
extern int lang_current;

/* lang_init(): Detect the OS language. Safe to call once at start-up. */
void lang_init(void);

/* L(): English string, or its Portuguese or French translation when the
   calculator is set to that language (any other uses the English one). */
#define L(en, pt, fr)  (lang_current == L_PT ? (pt) : \
                        lang_current == L_FR ? (fr) : (en))

#endif /* CASIO_I18N */
