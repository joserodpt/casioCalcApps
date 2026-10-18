//---
//  units - Unit categories and conversion (affine: base = x*factor + offset)
//---

#ifndef CONV_UNITS
#define CONV_UNITS

typedef struct {
    char const *name;       /* short unit label, e.g. "cm3" */
    double factor;          /* value_in_base = x*factor + offset */
    double offset;          /* non-zero only for temperature */
} unit_t;

typedef struct {
    char const *name;       /* category name, e.g. "Volume" */
    char const *name_pt;
    unit_t const *units;
    int count;
} category_t;

extern category_t const categories[];
extern int const category_count;

/* unit_convert(): value x expressed in unit `from` of `cat`, converted to
   unit `to`. */
double unit_convert(category_t const *cat, int from, int to, double x);

#endif /* CONV_UNITS */
