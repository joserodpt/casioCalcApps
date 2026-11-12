//---
//	units - Unit categories, conversion (affine: base = x*factor + offset)
//	        and the unit converter screen
//---

#ifndef CASIO_UNITS
#define CASIO_UNITS

typedef struct {
    char const *name;       /* short unit label, e.g. "cm3" */
    double factor;          /* value_in_base = x*factor + offset */
    double offset;          /* non-zero only for temperature */
} unit_t;

typedef struct {
    char const *name;       /* category name, e.g. "Volume" */
    char const *name_pt;
    char const *name_fr;
    unit_t const *units;
    int count;
} category_t;

extern category_t const categories[];
extern int const category_count;

/* unit_convert(): value x expressed in unit `from` of `cat`, converted to
   unit `to`. */
double unit_convert(category_t const *cat, int from, int to, double x);

/* units_screen(): Unit converter starting on category `cat` (index in
   categories[]); returns on EXIT */
void units_screen(int cat, int color);

/* units_screen_in(): The same over an app's own `count` categories (at
   most 12 units each on the fx-CG) */
void units_screen_in(category_t const *cats, int count, int cat, int color);

#endif /* CASIO_UNITS */
