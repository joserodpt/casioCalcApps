//---
//  ui - Text with extra symbols, SI formatting and input forms
//---

#ifndef ELEC_UI
#define ELEC_UI

#include <gint/display.h>
#include <stdbool.h>

/* Symbols missing from the fonts, drawn by etext() as small bitmaps */
#define CH_OHM    '\x01'
#define CH_MU     '\x02'
#define CH_PM     '\x03'
#define CH_TIMES  '\x04'
#define CH_PI     '\x05'
#define S_OHM     "\x01"
#define S_MU      "\x02"
#define S_PM      "\x03"
#define S_TIMES   "\x04"
#define S_PI      "\x05"

/* etext(): Text that may contain the symbols above; big selects font_big */
void etext(int x, int y, int fg, int halign, bool big, char const *s);
int etext_width(char const *s, bool big);
void eprint(int x, int y, int fg, int halign, char const *fmt, ...);

/* fmt_si(): "4.7 kΩ" with `sig` significant digits; "-" for NaN.
   buf must hold at least 32 bytes */
char *fmt_si(char *buf, double v, char const *unit, int sig);

/* fmt_num(): v with at most dec decimals, trailing zeros removed. Does
   not rely on printf's floating-point support. buf: 32 bytes */
char *fmt_num(char *buf, double v, int dec);

/* fmt_dev(): Deviation of actual from target, "(exact)" or "(+2.1%)" */
char *fmt_dev(char *buf, double actual, double target);

/* efkey(): F-key button whose label may contain symbols */
void efkey(int position, char const *text, int color);

//---
//  Input fields
//---

typedef enum { F_NUM, F_CODE, F_CHOICE } field_kind_t;

typedef struct {
    field_kind_t kind;
    char const *label;
    char const *unit;           /* F_NUM: unit shown after the box */
    char text[16];              /* F_NUM, F_CODE: what was typed */
    int maxlen;                 /* F_CODE: maximum length */
    bool digits_only;           /* F_CODE: no 'R' */
    char const * const *options;    /* F_CHOICE */
    int option_count;
    int choice;
    bool disabled;
} field_t;

/* field_value(): Value typed in an F_NUM field, NaN if empty or invalid.
   Accepts SI prefixes (p n µ m k M G), "4k7" and "1E-6" */
double field_value(field_t const *f);
/* field_set(): Write v into an F_NUM field ("4.7k") */
void field_set(field_t *f, double v);
/* field_key(): Apply a key to the field; returns true if it was used */
bool field_key(field_t *f, int key);
void field_box(field_t const *f, int x, int y, int w, bool selected,
    int color);
/* field_fkeys(): F-key bar for a field (SI prefixes for F_NUM) */
void field_fkeys(field_t const *f, int color);

//---
//  Forms: a list of fields with live results below
//---

typedef struct form form_t;
struct form {
    char const *title;
    int color;
    field_t *fields;
    int count;
    int cols;                   /* fields are laid out in columns */
    int label_w;
    void (*results)(form_t *f, int y);
    void (*changed)(form_t *f, int index);  /* optional */
    int sel;
};

void form_run(form_t *f);

/* result(): Result line, label at the left and value at x = 100 (further
   right if the label is longer) */
void result(int y, char const *label, int color, char const *fmt, ...);
/* result_note(): Extra information at the right of a result line */
void result_note(int y, char const *fmt, ...);

#define RESULT_STEP 14

#endif /* ELEC_UI */
