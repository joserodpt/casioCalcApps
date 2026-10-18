#include "units.h"

#define U(n, f)      { n, f, 0 }
#define UO(n, f, o)  { n, f, o }

/* Length, base metre */
static unit_t const length[] = {
    U("km", 1000), U("m", 1), U("dm", 0.1), U("cm", 0.01), U("mm", 0.001),
    U("um", 1e-6), U("nm", 1e-9), U("mile", 1609.344), U("yd", 0.9144),
    U("ft", 0.3048), U("in", 0.0254),
};

/* Area, base square metre */
static unit_t const area[] = {
    U("km2", 1e6), U("m2", 1), U("cm2", 1e-4), U("mm2", 1e-6),
    U("ha", 1e4), U("acre", 4046.8564), U("ft2", 0.09290304),
    U("in2", 6.4516e-4),
};

/* Volume, base litre (= dm3) */
static unit_t const volume[] = {
    U("m3", 1000), U("dm3", 1), U("L", 1), U("cm3", 0.001), U("mL", 0.001),
    U("mm3", 1e-6), U("uL", 1e-6), U("gal", 3.785411784), U("floz", 0.0295735),
};

/* Mass, base kilogram */
static unit_t const mass[] = {
    U("t", 1000), U("kg", 1), U("g", 0.001), U("mg", 1e-6), U("ug", 1e-9),
    U("lb", 0.45359237), U("oz", 0.028349523),
};

/* Temperature, base kelvin (affine) */
static unit_t const temp[] = {
    UO("C", 1, 273.15), U("K", 1), UO("F", 5.0 / 9.0, 255.372222222),
};

/* Pressure, base pascal */
static unit_t const pressure[] = {
    U("Pa", 1), U("kPa", 1000), U("bar", 1e5), U("atm", 101325),
    U("mmHg", 133.322387), U("psi", 6894.757),
};

/* Energy, base joule */
static unit_t const energy[] = {
    U("J", 1), U("kJ", 1000), U("cal", 4.184), U("kcal", 4184),
    U("Wh", 3600), U("kWh", 3.6e6), U("eV", 1.602176634e-19),
};

/* Time, base second */
static unit_t const time_[] = {
    U("s", 1), U("ms", 0.001), U("us", 1e-6), U("min", 60), U("h", 3600),
    U("day", 86400), U("yr", 31557600),
};

/* Speed, base metre per second */
static unit_t const speed[] = {
    U("m/s", 1), U("km/h", 1.0 / 3.6), U("mph", 0.44704), U("kn", 0.514444),
    U("ft/s", 0.3048),
};

/* Data, base byte */
static unit_t const data[] = {
    U("bit", 0.125), U("B", 1), U("kB", 1e3), U("MB", 1e6), U("GB", 1e9),
    U("KiB", 1024), U("MiB", 1048576.0), U("GiB", 1073741824.0),
};

#define CAT(n, pt, arr) { n, pt, arr, (int)(sizeof arr / sizeof *arr) }

category_t const categories[] = {
    CAT("Length", "Comprimento", length),
    CAT("Area", "Area", area),
    CAT("Volume", "Volume", volume),
    CAT("Mass", "Massa", mass),
    CAT("Temperature", "Temperatura", temp),
    CAT("Pressure", "Pressao", pressure),
    CAT("Energy", "Energia", energy),
    CAT("Time", "Tempo", time_),
    CAT("Speed", "Velocidade", speed),
    CAT("Data", "Dados", data),
};
int const category_count = (int)(sizeof categories / sizeof *categories);

double unit_convert(category_t const *cat, int from, int to, double x)
{
    unit_t const *a = &cat->units[from];
    unit_t const *b = &cat->units[to];
    double base = x * a->factor + a->offset;
    return (base - b->offset) / b->factor;
}
