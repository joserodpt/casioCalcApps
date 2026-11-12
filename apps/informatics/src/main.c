/* Informatics: a toolkit for computing classes. Data-size and data-rate
   conversions, transfer times, raw media sizes, IEEE 754 floats bit by
   bit, Unix time, CPU performance (CPI, MIPS, Amdahl's law) and a list of
   constants and limits. */

#include <gint/display.h>
#include <gint/keyboard.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "utils.h"
#include "keys.h"
#include "info.h"
#include "version.h"
#include "i18n.h"
#include "fmt.h"
#include "form.h"
#include "menu.h"
#include "numfield.h"
#include "units.h"
#include "usb_screen.h"

#define COLOR   C_RGB(2, 16, 12)    /* teal */
#define LIGHTC  C_RGB(28, 29, 31)   /* selected row */

_Static_assert(sizeof(double) == 8, "IEEE 754 screen needs a 64-bit double");

/* A result line: label at x=10, value at x=130 */
#define res(y, label, color, ...) \
    form_result(y, 130, label, color, __VA_ARGS__)

static double fv(form_t *f, int i) { return form_value(f, i); }

/* A whole number of up to 19 digits (fmt_g turns big ones scientific) */
static char *int_text(char *buf, long long v)
{
    char tmp[24];
    int n = 0;
    unsigned long long u = v < 0 ? 0ull - (unsigned long long)v : (unsigned long long)v;
    do { tmp[n++] = '0' + u % 10; u /= 10; } while (u);
    char *p = buf;
    if (v < 0) *p++ = '-';
    while (n) *p++ = tmp[--n];
    *p = 0;
    return buf;
}

/* Bytes in binary units ("1.5 MiB"), then decimal ones ("1.57 MB") */
static char *size_text(char *buf, double bytes)
{
    static char const * const bin[] = { "B", "KiB", "MiB", "GiB", "TiB", "PiB" };
    static char const * const dec[] = { "B", "kB", "MB", "GB", "TB", "PB" };
    char a[32], b[32];
    int i = 0, j = 0;
    double x = bytes, y = bytes;
    while (x >= 1024 && i < 5) { x /= 1024; i++; }
    while (y >= 1000 && j < 5) { y /= 1000; j++; }
    if (i == 0) sprintf(buf, "%s B", fmt_g(a, bytes, 6));
    else sprintf(buf, "%s %s (%s %s)", fmt_g(a, x, 4), bin[i],
        fmt_g(b, y, 4), dec[j]);
    return buf;
}

/* Seconds as "1 h 02 min 03 s" from a minute up, else in s/ms/us/ns */
static char *time_text(char *buf, double s)
{
    char a[32];
    if (isnan(s) || isinf(s)) return strcpy(buf, "-");
    if (s >= 60) {
        double days = floor(s / 86400);
        long r = (long)floor(s - days * 86400);
        int h = r / 3600, m = r / 60 % 60, sec = r % 60;
        if (days >= 1)
            sprintf(buf, "%s d %d h %02d min", fmt_g(a, days, 6), h, m);
        else if (h) sprintf(buf, "%d h %02d min %02d s", h, m, sec);
        else sprintf(buf, "%d min %02d s", m, sec);
        return buf;
    }
    if (s >= 1 || s == 0) sprintf(buf, "%s s", fmt_g(a, s, 5));
    else if (s >= 1e-3) sprintf(buf, "%s ms", fmt_g(a, s * 1e3, 5));
    else if (s >= 1e-6) sprintf(buf, "%s us", fmt_g(a, s * 1e6, 5));
    else sprintf(buf, "%s ns", fmt_g(a, s * 1e9, 5));
    return buf;
}

static void hint(int y, char const *en, char const *pt, char const *fr)
{
    dtext(10, y, C_DARK, L(en, pt, fr));
}

//---
//  Unit converter
//---

#define U(n, f) { n, f, 0 }

/* Data size, base byte */
static unit_t const size_units[] = {
    U("bit", 0.125), U("B", 1), U("kB", 1e3), U("MB", 1e6), U("GB", 1e9),
    U("TB", 1e12), U("KiB", 1024.0), U("MiB", 1048576.0),
    U("GiB", 1073741824.0), U("TiB", 1099511627776.0),
};

/* Data rate, base bit per second */
static unit_t const rate_units[] = {
    U("bit/s", 1), U("kbit/s", 1e3), U("Mbit/s", 1e6), U("Gbit/s", 1e9),
    U("B/s", 8), U("kB/s", 8e3), U("MB/s", 8e6), U("GB/s", 8e9),
    U("KiB/s", 8192.0), U("MiB/s", 8388608.0),
};

/* Time, base second */
static unit_t const time_units[] = {
    U("ns", 1e-9), U("us", 1e-6), U("ms", 1e-3), U("s", 1), U("min", 60),
    U("h", 3600), U("day", 86400), U("week", 604800), U("year", 31557600),
};

/* Frequency, base hertz */
static unit_t const freq_units[] = {
    U("Hz", 1), U("kHz", 1e3), U("MHz", 1e6), U("GHz", 1e9),
};

#define CAT(n, pt, fr, arr) \
    { n, pt, fr, arr, (int)(sizeof arr / sizeof *arr) }

static category_t const cats[] = {
    CAT("Data size", "Tamanho de dados", "Taille de donnees", size_units),
    CAT("Data rate", "Taxa de dados", "Debit de donnees", rate_units),
    CAT("Time", "Tempo", "Temps", time_units),
    CAT("Frequency", "Frequencia", "Frequence", freq_units),
};

static void units_run(void)
{
    units_screen_in(cats, (int)(sizeof cats / sizeof *cats), 0, COLOR);
}

//---
//  Transfer time: size / speed, leave one field empty
//---

static void transfer_results(form_t *f, int y)
{
    double size = fv(f, 0), speed = fv(f, 1), t = fv(f, 2);
    int empty = form_one_empty(f);
    char b[48];

    if (empty < 0) {
        hint(y, "Leave one field empty to solve it.",
            "Deixe um campo vazio para o calcular.",
            "Videz un champ pour le calculer.");
        hint(y + 16, "Size in MB (10^6 bytes), speed in Mbit/s.",
            "Tamanho em MB (10^6 bytes), velocidade em Mbit/s.",
            "Taille en MB (10^6 octets), debit en Mbit/s.");
        return;
    }
    /* MB * 8 = Mbit */
    if (empty == 0) size = speed * t / 8;
    else if (empty == 1) speed = t > 0 ? size * 8 / t : NAN;
    else t = speed > 0 ? size * 8 / speed : NAN;
    if (isnan(size) || isnan(speed) || isnan(t)) {
        res(y, L("Result", "Resultado", "Resultat"), C_RED, "-");
        return;
    }

    char const *names[3] = { L("Size", "Tamanho", "Taille"),
        L("Speed", "Velocidade", "Debit"), L("Time", "Tempo", "Temps") };
    if (empty == 0) res(y, names[0], COLOR, "%s MB", fmt_g(b, size, 6));
    else if (empty == 1) res(y, names[1], COLOR, "%s Mbit/s",
        fmt_g(b, speed, 6));
    else res(y, names[2], COLOR, "%s", time_text(b, t));
    y += 22;
    if (empty != 2) {
        res(y, L("Time", "Tempo", "Temps"), C_BLACK, "%s", time_text(b, t));
        y += 16;
    }
    res(y, L("Size", "Tamanho", "Taille"), C_BLACK, "%s",
        size_text(b, size * 1e6));
    y += 16;
    char a[32];
    res(y, L("Speed", "Velocidade", "Debit"), C_BLACK, "%s MB/s  (%s MiB/s)",
        fmt_g(a, speed / 8, 5), fmt_g(b, speed * 1e6 / 8 / 1048576, 5));
}

static void transfer_screen(void)
{
    static form_t f = {
        .title_en = "Transfer time", .title_pt = "Tempo de transferencia",
        .title_fr = "Temps de transfert",
        .color = COLOR, .count = 3, .cols = 1,
        .fields = {
            { "Size", "Tamanho", "Taille", "MB", { "700" } },
            { "Speed", "Velocidade", "Debit", "Mbit/s", { "100" } },
            { "Time", "Tempo", "Temps", "s" },
        },
        .results = transfer_results,
    };
    form_run(&f);
}

//---
//  Raw media sizes: image / video frames, PCM audio
//---

static void media_results(form_t *f, int y)
{
    double w = fv(f, 0), h = fv(f, 1), bpp = fv(f, 2), fps = fv(f, 3),
        rate = fv(f, 4), bits = fv(f, 5), ch = fv(f, 6), t = fv(f, 7);
    char a[48], b[32];

    double frame = w * h * bpp / 8;     /* NaN if any is missing */
    if (!isnan(frame)) {
        res(y, L("Image", "Imagem", "Image"), COLOR, "%s", size_text(a, frame));
        y += 16;
        if (bpp >= 1 && bpp <= 64)
            res(y, L("Colours", "Cores", "Couleurs"), C_BLACK, "2^%d = %s",
                (int)bpp, bpp <= 62 ? int_text(b, 1ll << (int)bpp)
                : fmt_g(b, pow(2, bpp), 6));
        y += 16;
    }
    else { hint(y, "Image: width x height x bits/px",
        "Imagem: largura x altura x bits/px",
        "Image: largeur x hauteur x bits/px"); y += 32; }

    if (!isnan(frame) && !isnan(fps)) {
        res(y, L("Video bitrate", "Debito video", "Debit video"), COLOR,
            "%s Mbit/s", fmt_g(b, frame * 8 * fps / 1e6, 5));
        y += 16;
        if (!isnan(t)) {
            res(y, L("Video size", "Tamanho video", "Taille video"), C_BLACK,
                "%s", size_text(a, frame * fps * t));
            y += 16;
        }
    }
    y += 6;
    double bps = rate * bits * ch;
    if (!isnan(bps)) {
        res(y, L("Audio bitrate", "Debito audio", "Debit audio"), COLOR,
            "%s kbit/s", fmt_g(b, bps / 1e3, 6));
        y += 16;
        if (!isnan(t)) res(y, L("Audio size", "Tamanho audio",
            "Taille audio"), C_BLACK, "%s", size_text(a, bps * t / 8));
    }
    else hint(y, "Audio: rate x bits x channels",
        "Audio: taxa x bits x canais", "Audio: freq. x bits x canaux");
}

static void media_screen(void)
{
    static form_t f = {
        .title_en = "Media sizes (uncompressed)",
        .title_pt = "Tamanho de media (sem compressao)",
        .title_fr = "Taille media (non compresse)",
        .color = COLOR, .count = 8, .cols = 2,
        .fields = {
            { "Width", "Largura", "Largeur", "px", { "1920" } },
            { "Height", "Altura", "Hauteur", "px", { "1080" } },
            { "Bits/px", "Bits/px", "Bits/px", "", { "24" } },
            { "FPS", "FPS", "FPS", "", { "30" } },
            { "Rate", "Taxa", "Freq.", "Hz", { "44100" } },
            { "Bits", "Bits", "Bits", "", { "16" } },
            { "Chan.", "Canais", "Canaux", "", { "2" } },
            { "Time", "Tempo", "Temps", "s", { "60" } },
        },
        .results = media_results,
    };
    form_run(&f);
}

//---
//  IEEE 754: a number as a float (32-bit) and a double (64-bit)
//---

typedef union { float f; uint32_t u; } f32_t;
typedef union { double d; uint64_t u; } f64_t;

static char const *float_class(int exp, uint64_t mant, int max_exp)
{
    if (exp == max_exp)
        return mant ? "NaN" : L("infinity", "infinito", "infini");
    if (exp == 0) return mant ? L("subnormal", "subnormal", "sous-normal")
        : L("zero", "zero", "zero");
    return L("normal", "normal", "normal");
}

/* The 32 bits in boxes: sign, exponent, mantissa in their colours */
static void float_bits(int y, uint32_t u)
{
    static int const fill[3] = { C_RGB(31, 20, 20), C_RGB(20, 30, 20),
        C_RGB(20, 26, 31) };
    int x = 14;
    for (int i = 31; i >= 0; i--) {
        int part = i == 31 ? 0 : i >= 23 ? 1 : 2;
        if (i == 30 || i == 22) x += 4;
        drect(x, y, x + 9, y + 13, fill[part]);
        dtext_opt(x + 5, y + 7, C_BLACK, C_NONE, DTEXT_CENTER, DTEXT_MIDDLE,
            (u >> i) & 1 ? "1" : "0");
        x += 11;
    }
}

static void ieee_screen(void)
{
    numfield_t val = { "0.1" };
    char hex[9] = "";
    bool hex_mode = false;

    while (1) {
        f32_t f;
        double x;
        if (hex_mode) {
            f.u = 0;
            for (char *p = hex; *p; p++)
                f.u = f.u << 4 | (*p <= '9' ? *p - '0' : *p - 'A' + 10);
            x = f.f;
        }
        else {
            x = numfield_value(&val);
            f.f = (float)x;
        }
        bool have = hex_mode ? hex[0] != 0 : !isnan(x);
        f64_t d = { .d = x };

        dclear(C_WHITE);
        info_title("IEEE 754", COLOR);
        char a[48], b[48];

        dtext(10, 24, C_BLACK, hex_mode
            ? L("Float bits", "Bits float", "Bits float")
            : L("Value", "Valor", "Valeur"));
        if (hex_mode) {
            numfield_t box;
            sprintf(box.text, "0x%s", hex);
            numfield_draw(&box, 90, 21, 180, true, COLOR);
        }
        else numfield_draw(&val, 90, 21, 180, true, COLOR);

        if (have) {
            int e = f.u >> 23 & 0xFF;
            uint32_t m = f.u & 0x7FFFFF;
            dtext(10, 46, COLOR, "float (32-bit)");
            dprint(130, 46, C_BLACK, "0x%08X   %s", (unsigned)f.u,
                float_class(e, m, 0xFF));
            float_bits(60, f.u);
            if (e && e != 0xFF)
                sprintf(a, "%d - 127 = %d", e, e - 127);
            else sprintf(a, "%d", e);
            dprint(10, 80, C_RGB(22, 4, 4), L("sign %d", "sinal %d",
                "signe %d"), (int)(f.u >> 31));
            dprint(70, 80, C_RGB(2, 16, 2), L("exponent %s", "expoente %s",
                "exposant %s"), a);
            dprint_opt(DWIDTH - 10, 80, C_RGB(2, 10, 24), C_NONE,
                DTEXT_RIGHT, DTEXT_TOP, "mant. 0x%06X", (unsigned)m);
            res(98, L("Stored", "Guardado", "Stocke"), C_BLACK, "%s",
                fmt_g(a, (double)f.f, 9));
            if (!hex_mode && e != 0xFF)
                res(114, L("Error", "Erro", "Erreur"), C_BLACK, "%s",
                    fmt_g(a, (double)f.f - x, 3));

            int de = d.u >> 52 & 0x7FF;
            uint64_t dm = d.u & 0xFFFFFFFFFFFFFull;
            dtext(10, 138, COLOR, "double (64-bit)");
            dprint(130, 138, C_BLACK, "0x%08X%08X   %s",
                (unsigned)(d.u >> 32), (unsigned)d.u,
                float_class(de, dm, 0x7FF));
            if (de && de != 0x7FF) sprintf(b, "%d - 1023 = %d", de, de - 1023);
            else sprintf(b, "%d", de);
            res(154, L("Sign, exponent", "Sinal, expoente", "Signe, exposant"),
                C_BLACK, "%d,  %s", (int)(d.u >> 63), b);
            res(170, L("Stored", "Guardado", "Stocke"), C_BLACK, "%s",
                fmt_g(a, d.d, 15));
        }
        else hint(46, "Type a number, or press F2 to type the bits.",
            "Escreva um numero, ou F2 para escrever os bits.",
            "Entrez un nombre, ou F2 pour entrer les bits.");

        fkey_button_color(1, "DEC", hex_mode ? C_BLACK : COLOR);
        fkey_button_color(2, "HEX", hex_mode ? COLOR : C_BLACK);
        dupdate();

        int key = getkey().key;
        int n = strlen(hex);
        if (key == KEY_EXIT) return;
        if (key == INFO_KEY) info_app("IEEE 754", COLOR);
        else if (key == KEY_F1 && hex_mode) {
            /* keep the number: its float value, as text */
            hex_mode = false;
            if (have) fmt_g(val.text, (double)f.f, 9);
        }
        else if (key == KEY_F2 && !hex_mode) {
            hex_mode = true;
            if (have) sprintf(hex, "%08X", (unsigned)f.u);
        }
        else if (hex_mode) {
            if (key_hex(key) >= 0 && n < 8) {
                hex[n] = "0123456789ABCDEF"[key_hex(key)];
                hex[n + 1] = 0;
            }
            else if (key == KEY_DEL && n) hex[n - 1] = 0;
            else if (key == KEY_ACON) hex[0] = 0;
        }
        else numfield_key(&val, key);
    }
}

//---
//  Unix time <-> UTC date (Howard Hinnant's civil-date algorithms)
//---

static long long days_from_civil(long long y, int m, int d)
{
    y -= m <= 2;
    long long era = (y >= 0 ? y : y - 399) / 400;
    long long yoe = y - era * 400;
    long long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

static void civil_from_days(long long z, long long *y, int *m, int *d)
{
    z += 719468;
    long long era = (z >= 0 ? z : z - 146096) / 146097;
    long long doe = z - era * 146097;
    long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    long long mp = (5 * doy + 2) / 153;
    *d = (int)(doy - (153 * mp + 2) / 5 + 1);
    *m = (int)(mp < 10 ? mp + 3 : mp - 9);
    *y = yoe + era * 400 + (*m <= 2);
}

static char const *weekday(long long days)
{
    static char const * const en[] = { "Thursday", "Friday", "Saturday",
        "Sunday", "Monday", "Tuesday", "Wednesday" };
    static char const * const pt[] = { "quinta-feira", "sexta-feira",
        "sabado", "domingo", "segunda-feira", "terca-feira", "quarta-feira" };
    static char const * const fr[] = { "jeudi", "vendredi", "samedi",
        "dimanche", "lundi", "mardi", "mercredi" };
    int i = (int)(((days % 7) + 7) % 7);    /* 1970-01-01 was a Thursday */
    return L(en[i], pt[i], fr[i]);
}

static void unix_results(form_t *f, int y)
{
    double ts = fv(f, 0), Y = fv(f, 1), M = fv(f, 2), D = fv(f, 3),
        h = fv(f, 4), mi = fv(f, 5), s = fv(f, 6);
    char a[32], b[32];
    long long t;

    if (!isnan(ts)) {
        if (fabs(ts) > 1e15) { res(y, "UTC", C_RED, "-"); return; }
        t = (long long)floor(ts);
    }
    else if (!isnan(Y) && !isnan(M) && !isnan(D)) {
        if (M < 1 || M > 12 || D < 1 || D > 31 || fabs(Y) > 1e7) {
            res(y, L("Date", "Data", "Date"), C_RED,
                L("invalid", "invalida", "invalide"));
            return;
        }
        t = days_from_civil((long long)Y, (int)M, (int)D) * 86400
            + (long long)(isnan(h) ? 0 : h) * 3600
            + (long long)(isnan(mi) ? 0 : mi) * 60
            + (long long)(isnan(s) ? 0 : s);
        res(y, L("Unix time", "Tempo Unix", "Heure Unix"), COLOR, "%s s",
            int_text(a, t));
        y += 18;
    }
    else {
        hint(y, "Enter a Unix time (seconds since 1970),",
            "Escreva um tempo Unix (segundos desde 1970),",
            "Entrez une heure Unix (s depuis 1970),");
        hint(y + 16, "or leave it empty and enter a UTC date.",
            "ou deixe-o vazio e escreva uma data UTC.",
            "ou videz-la et entrez une date UTC.");
        return;
    }

    long long days = t >= 0 ? t / 86400 : -((-t + 86399) / 86400);
    long long secs = t - days * 86400, year;
    int mon, day;
    civil_from_days(days, &year, &mon, &day);
    res(y, "UTC", isnan(ts) ? C_BLACK : COLOR, "%s-%02d-%02d %02d:%02d:%02d",
        int_text(a, year), mon, day, (int)(secs / 3600),
        (int)(secs / 60 % 60), (int)(secs % 60));
    y += 16;
    res(y, L("Weekday", "Dia da semana", "Jour de semaine"), C_BLACK, "%s",
        weekday(days));
    y += 16;
    res(y, L("Milliseconds", "Milissegundos", "Millisecondes"), C_BLACK, "%s",
        int_text(b, t * 1000));
    y += 16;
    bool fits = t >= -2147483648ll && t <= 2147483647ll;
    res(y, L("Signed 32-bit", "32 bits c/ sinal", "32 bits signe"),
        fits ? C_BLACK : C_RED, "%s", fits
        ? L("fits (until 2038-01-19 03:14:07)",
            "cabe (ate 2038-01-19 03:14:07)",
            "ok jusqu'au 2038-01-19 03:14:07")
        : L("overflows", "excede", "deborde"));
}

static void unix_screen(void)
{
    static form_t f = {
        .title_en = "Unix time (UTC)", .title_pt = "Tempo Unix (UTC)",
        .title_fr = "Heure Unix (UTC)",
        .color = COLOR, .count = 7, .cols = 2,
        .fields = {
            { "Unix", "Unix", "Unix", "s", { "1700000000" } },
            { "Year", "Ano", "Annee", "" }, { "Month", "Mes", "Mois", "" },
            { "Day", "Dia", "Jour", "" }, { "Hour", "Hora", "Heure", "" },
            { "Min", "Min", "Min", "" }, { "Sec", "Seg", "Sec", "" },
        },
        .results = unix_results,
    };
    form_run(&f);
}

//---
//  CPU performance: time = IC x CPI / f, MIPS, Amdahl's law
//---

static void cpu_results(form_t *f, int y)
{
    double ic = fv(f, 0), cpi = fv(f, 1), mhz = fv(f, 2), p = fv(f, 3),
        n = fv(f, 4);
    char a[32], b[32];

    if (!isnan(ic) && !isnan(cpi) && !isnan(mhz) && mhz > 0) {
        res(y, L("CPU time", "Tempo CPU", "Temps CPU"), COLOR, "%s  (%s %s)",
            time_text(a, ic * cpi / (mhz * 1e6)), fmt_g(b, ic * cpi, 4),
            L("cycles", "ciclos", "cycles"));
        y += 16;
    }
    if (!isnan(cpi) && !isnan(mhz) && cpi > 0) {
        res(y, "MIPS", C_BLACK, "%s", fmt_g(a, mhz / cpi, 6));
        y += 16;
    }
    y += 6;
    if (!isnan(p) && !isnan(n) && p >= 0 && p <= 100 && n >= 1) {
        double fr = p / 100, sp = 1 / ((1 - fr) + fr / n);
        res(y, L("Speed-up", "Aceleracao", "Acceleration"), COLOR,
            "%sx  (%s%% %s)",
            fmt_g(a, sp, 4), fmt_g(b, sp / n * 100, 3),
            L("efficiency", "eficiencia", "efficacite"));
        y += 16;
        res(y, L("Limit (N = inf)", "Limite (N = inf)", "Limite (N = inf)"),
            C_BLACK, "%s", fr < 1 ? fmt_g(a, 1 / (1 - fr), 4) : "inf");
        y += 16;
    }
    dtext(10, y + 4, C_DARK, "T = IC x CPI / f     S = 1 / ((1 - p) + p / N)");
}

static void cpu_screen(void)
{
    static form_t f = {
        .title_en = "CPU performance", .title_pt = "Desempenho do CPU",
        .title_fr = "Performance CPU",
        .color = COLOR, .count = 5, .cols = 1,
        .fields = {
            { "Instructions", "Instrucoes", "Instructions", "", { "1E9" } },
            { "CPI", "CPI", "CPI", "", { "1.5" } },
            { "Clock", "Relogio", "Horloge", "MHz", { "2000" } },
            { "Parallel (p)", "Paralelo (p)", "Parallele (p)", "%", { "90" } },
            { "Cores (N)", "Nucleos (N)", "Coeurs (N)", "", { "8" } },
        },
        .results = cpu_results,
    };
    form_run(&f);
}

//---
//  Constants and limits
//---

typedef struct {
    char const *name, *name_pt, *name_fr, *sym, *value;
} constant_t;

static constant_t const constants[] = {
    { "2 to the 8", "2 elevado a 8", "2 puissance 8", "2^8", "256" },
    { "2 to the 10 (kibi)", "2 elevado a 10 (kibi)", "2 puissance 10 (kibi)",
      "2^10", "1 024" },
    { "2 to the 16", "2 elevado a 16", "2 puissance 16", "2^16", "65 536" },
    { "2 to the 20 (mebi)", "2 elevado a 20 (mebi)", "2 puissance 20 (mebi)",
      "2^20", "1 048 576" },
    { "2 to the 30 (gibi)", "2 elevado a 30 (gibi)", "2 puissance 30 (gibi)",
      "2^30", "1 073 741 824" },
    { "2 to the 32", "2 elevado a 32", "2 puissance 32",
      "2^32", "4 294 967 296" },
    { "2 to the 64", "2 elevado a 64", "2 puissance 64",
      "2^64", "18 446 744 073 709 551 616" },
    { "Signed 8-bit range", "Gama 8 bits c/ sinal", "Plage 8 bits signe",
      "int8", "-128 .. 127" },
    { "Unsigned 8-bit max.", "Max. 8 bits s/ sinal", "Max. 8 bits non signe",
      "uint8", "255" },
    { "Signed 16-bit range", "Gama 16 bits c/ sinal", "Plage 16 bits signe",
      "int16", "-32 768 .. 32 767" },
    { "Unsigned 16-bit max.", "Max. 16 bits s/ sinal", "Max. 16 bits non signe",
      "uint16", "65 535" },
    { "Signed 32-bit min.", "Min. 32 bits c/ sinal", "Min. 32 bits signe",
      "INT32_MIN", "-2 147 483 648" },
    { "Signed 32-bit max.", "Max. 32 bits c/ sinal", "Max. 32 bits signe",
      "INT32_MAX", "2 147 483 647" },
    { "Unsigned 32-bit max.", "Max. 32 bits s/ sinal", "Max. 32 bits non signe",
      "UINT32_MAX", "4 294 967 295" },
    { "Signed 64-bit min.", "Min. 64 bits c/ sinal", "Min. 64 bits signe",
      "INT64_MIN", "-9 223 372 036 854 775 808" },
    { "Signed 64-bit max.", "Max. 64 bits c/ sinal", "Max. 64 bits signe",
      "INT64_MAX", "9 223 372 036 854 775 807" },
    { "Unsigned 64-bit max.", "Max. 64 bits s/ sinal", "Max. 64 bits non signe",
      "UINT64_MAX", "18 446 744 073 709 551 615" },
    { "Float largest", "Maior float", "Plus grand float",
      "FLT_MAX", "3.40282347e38" },
    { "Float smallest normal", "Menor float normal", "Plus petit float normal",
      "FLT_MIN", "1.17549435e-38" },
    { "Float epsilon", "Epsilon do float", "Epsilon du float",
      "FLT_EPS", "1.19209290e-7 (2^-23)" },
    { "Float precision", "Precisao do float", "Precision du float",
      "FLT_DIG", "24 bits, ~7 digits" },
    { "Double largest", "Maior double", "Plus grand double",
      "DBL_MAX", "1.7976931348623157e308" },
    { "Double smallest normal", "Menor double normal",
      "Plus petit double normal", "DBL_MIN", "2.2250738585072014e-308" },
    { "Double epsilon", "Epsilon do double", "Epsilon du double",
      "DBL_EPS", "2.220446049250313e-16 (2^-52)" },
    { "Double precision", "Precisao do double", "Precision du double",
      "DBL_DIG", "53 bits, ~15-16 digits" },
    { "Unix epoch", "Epoca Unix", "Epoque Unix",
      "t = 0", "1970-01-01 00:00:00 UTC" },
    { "Year 2038 problem", "Problema do ano 2038", "Probleme de l'an 2038",
      "2^31-1 s", "2038-01-19 03:14:07 UTC" },
    { "Ethernet MTU", "MTU Ethernet", "MTU Ethernet", "MTU", "1500 B" },
    { "IPv4 header (min.)", "Cabecalho IPv4 (min.)", "En-tete IPv4 (min.)",
      "IPv4", "20 B" },
    { "IPv6 header", "Cabecalho IPv6", "En-tete IPv6", "IPv6", "40 B" },
    { "TCP header (min.)", "Cabecalho TCP (min.)", "En-tete TCP (min.)",
      "TCP", "20 B" },
    { "UDP header", "Cabecalho UDP", "En-tete UDP", "UDP", "8 B" },
    { "Light in optical fibre", "Luz em fibra otica",
      "Lumiere en fibre optique", "c/n", "~2e8 m/s (5 us/km)" },
};
#define CONST_COUNT (int)(sizeof constants / sizeof *constants)
#define CONST_ROWS  11

static void constants_screen(void)
{
    int sel = 0, top = 0;
    while (1) {
        dclear(C_WHITE);
        info_title(L("Constants & limits", "Constantes e limites",
            "Constantes et limites"), COLOR);

        if (sel < top) top = sel;
        if (sel >= top + CONST_ROWS) top = sel - CONST_ROWS + 1;
        for (int r = 0; r < CONST_ROWS && top + r < CONST_COUNT; r++) {
            int i = top + r, y = 22 + r * 16;
            bool s = i == sel;
            if (s) drect(4, y - 1, DWIDTH - 9, y + 13, LIGHTC);
            dtext(10, y, s ? COLOR : C_BLACK, L(constants[i].name,
                constants[i].name_pt, constants[i].name_fr));
            dtext(290, y, s ? COLOR : C_DARK, constants[i].sym);
        }
        scrollbar_px(22, 22 + CONST_ROWS * 16, 0, CONST_COUNT, top,
            CONST_ROWS);

        dline(6, 196, DWIDTH - 7, 196, C_LIGHT);
        dprint(10, 200, COLOR, "%s = %s", constants[sel].sym,
            constants[sel].value);
        dupdate();

        int key = getkey().key;
        if (key == KEY_EXIT) return;
        if (key == INFO_KEY)
            info_app(L("Constants & limits", "Constantes e limites",
                "Constantes et limites"), COLOR);
        else if (key == KEY_UP) sel = (sel + CONST_COUNT - 1) % CONST_COUNT;
        else if (key == KEY_DOWN) sel = (sel + 1) % CONST_COUNT;
    }
}

//---
//  Menu
//---

static menu_item_t const tools[] = {
    { "Unit converter", "Conversor de unidades", "Convertisseur d'unites",
      "Data size, data rate, time, frequency",
      "Tamanho, taxa de dados, tempo, freq.",
      "Taille, debit, temps, frequence", units_run, COLOR },
    { "Transfer time", "Tempo de transferencia", "Temps de transfert",
      "Size, speed, time: solve the missing one",
      "Tamanho, velocidade, tempo",
      "Taille, debit, temps: trouve le manquant", transfer_screen, COLOR },
    { "Media sizes", "Tamanho de media", "Tailles de media",
      "Raw image, video and audio sizes",
      "Imagem, video e audio sem compressao",
      "Image, video et audio non compresses", media_screen, COLOR },
    { "IEEE 754", "IEEE 754", "IEEE 754",
      "A number as float and double, bit by bit",
      "Um numero em float e double, bit a bit",
      "Nombre en float et double, bit a bit", ieee_screen, COLOR },
    { "Unix time", "Tempo Unix", "Heure Unix",
      "Timestamp <-> UTC date, weekday, 2038",
      "Timestamp <-> data UTC, dia, 2038",
      "Timestamp <-> date UTC, jour, 2038", unix_screen, COLOR },
    { "CPU performance", "Desempenho do CPU", "Performance CPU",
      "CPU time, MIPS, Amdahl's law",
      "Tempo de CPU, MIPS, lei de Amdahl",
      "Temps CPU, MIPS, loi d'Amdahl", cpu_screen, COLOR },
    { "Constants & limits", "Constantes e limites", "Constantes et limites",
      "Powers of 2, integer and float limits...",
      "Potencias de 2, limites de inteiros...",
      "Puissances de 2, limites d'entiers...", constants_screen, COLOR },
};

static menu_t const menu = {
    "Informatics", "Informatica", "Informatique",
    "Informatics toolkit " APPS_VERSION,
    "Ferramentas de informatica " APPS_VERSION,
    "Outils d'informatique " APPS_VERSION, COLOR,
    tools, (int)(sizeof tools / sizeof *tools),
};

int main(void)
{
    lang_init();
    usb_screen_init();
    menu_run(&menu);
    return 1;
}
