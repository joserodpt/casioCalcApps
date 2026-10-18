#include <gint/display.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui.h"
#include "eseries.h"
#include "tools.h"
#include "i18n.h"

#define PI 3.14159265358979323846

#define NUM(lbl, u, t)  { .kind = F_NUM, .label = lbl, .unit = u, .text = t }

static double val(form_t const *f, int i)
{
    return field_value(&f->fields[i]);
}

/* Value with its nearest E-series value as a note */
static void result_e(int y, char const *label, double v, int series)
{
    char a[32], b[32], c[16];
    result(y, label, C_BLACK, "%s", fmt_si(a, v, S_OHM, 4));
    double e = eseries_nearest(series, v, false);
    if (!isnan(e)) result_note(y, "E%d %s %s", series,
        fmt_si(b, e, S_OHM, 3), fmt_dev(c, e, v));
}

//---
//  SMD codes
//---

#define SMD_COLOR C_RGB(4, 14, 14)

static char const * const *eia_letters = eia96_letter_names;

static field_t smd_fields[] = {
    { .kind = F_CODE, .label = "Code", .text = "472", .maxlen = 4 },
    { .kind = F_CODE, .label = "EIA-96", .text = "01", .maxlen = 2,
      .digits_only = true },
    { .kind = F_CHOICE, .label = "Letter", .option_count = EIA96_LETTERS,
      .choice = 5 },
    NUM("Value", S_OHM, "4k7"),
};

static void smd_code_line(int y, char const *label, char const *code,
    bool ok, bool exact, double nearest)
{
    char a[32];
    if (!ok) {
        result(y, label, C_DARK, L("not possible", "impossivel"));
        return;
    }
    result(y, label, SMD_COLOR, "%s", code);
    if (!exact) result_note(y, "nearest, %s", fmt_si(a, nearest, S_OHM, 3));
}

static void smd_results(form_t *f, int y)
{
    char a[32], code[8];
    bool exact, ok;

    if (f->sel == 0) {
        char const *t = f->fields[0].text;
        double v = smd_decode(t);
        if (isnan(v)) {
            result(y, L("Value","Valor"), C_RED, L("invalid code", "codigo invalido"));
        }
        else {
            etext(10, y, C_DARK, DTEXT_LEFT, false, L("Value","Valor"));
            etext(100, y - 2, SMD_COLOR, DTEXT_LEFT, true,
                fmt_si(a, v, S_OHM, 4));
            y += 6;
            int n = strlen(t);
            result(y + RESULT_STEP, L("Type","Tipo"), C_BLACK, "%s",
                v == 0 ? L("0 " S_OHM " jumper","ponte 0 " S_OHM) : strchr(t, 'R')
                ? L("R = decimal point","R = virgula") : n == 4 ? L("4-digit, usually 1%","4 digitos, 1%")
                : L("3-digit, usually 5%","3 digitos, 5%"));
            if (v > 0) {
                result_e(y + 2 * RESULT_STEP, L("Nearest","Mais prox."), v, 24);
                result_e(y + 3 * RESULT_STEP, "", v, 96);
            }
        }
        etext(10, 192, C_DARK, DTEXT_LEFT, false,
            L("Press . to type R (4R7 = 4.7 " S_OHM ")",
              "Prima . para R (4R7 = 4.7 " S_OHM ")"));
        return;
    }

    if (f->sel == 1 || f->sel == 2) {
        int n = atoi(f->fields[1].text);
        int m = e96_value(n);
        if (!m) {
            result(y, L("Value","Valor"), C_RED, L("code must be 01 to 96","codigo 01 a 96"));
            return;
        }
        double v = m * pow(10, eia96_letter_exp[f->fields[2].choice]);
        etext(10, y, C_DARK, DTEXT_LEFT, false, L("Value","Valor"));
        etext(100, y - 2, SMD_COLOR, DTEXT_LEFT, true,
            fmt_si(a, v, S_OHM, 4));
        y += 6;
        result(y + RESULT_STEP, L("Code","Codigo"), C_BLACK, "%02d%c", n,
            "ZYXABCDEF"[f->fields[2].choice]);
        result(y + 2 * RESULT_STEP, L("Type","Tipo"), C_BLACK, L("EIA-96, 1%% series","EIA-96, serie 1%%"));
        etext(10, 192, C_DARK, DTEXT_LEFT, false,
            L("Change the letter with left / right","Mude a letra com esq / dir"));
        return;
    }

    double v = val(f, 3);
    if (isnan(v) || v < 0) {
        result(y, L("Value","Valor"), C_RED, L("enter a resistance","introduza resistencia"));
        return;
    }
    ok = smd_encode(code, v, 2, &exact);
    smd_code_line(y, "3-digit", code, ok, exact, smd_decode(code));
    ok = smd_encode(code, v, 3, &exact);
    smd_code_line(y + RESULT_STEP, "4-digit", code, ok, exact,
        smd_decode(code));
    ok = eia96_encode(code, v, &exact);
    smd_code_line(y + 2 * RESULT_STEP, "EIA-96", code, ok, exact,
        eseries_nearest(96, v, false));
}

void smd_run(void)
{
    static form_t f = {
        .color = SMD_COLOR, .fields = smd_fields,
        .count = 4, .cols = 1, .label_w = 70, .results = smd_results,
    };
    f.title = L("SMD codes", "Codigos SMD");
    smd_fields[2].options = eia_letters;
    form_run(&f);
}

//---
//  Series / parallel
//---

#define RNET_COLOR C_RGB(3, 9, 24)

static field_t rnet_fields[] = {
    NUM("R1", S_OHM, "1k"), NUM("R2", S_OHM, "2.2k"),
    NUM("R3", S_OHM, "4.7k"), NUM("R4", S_OHM, ""),
    NUM("R5", S_OHM, ""), NUM("R6", S_OHM, ""),
    NUM("R7", S_OHM, ""), NUM("R8", S_OHM, ""),
};

static void rnet_results(form_t *f, int y)
{
    double series = 0, conductance = 0;
    int n = 0;
    bool zero = false;

    for (int i = 0; i < f->count; i++) {
        double r = val(f, i);
        if (isnan(r) || r < 0) continue;
        n++;
        series += r;
        if (r == 0) zero = true;
        else conductance += 1 / r;
    }
    if (!n) {
        result(y, "Total", C_DARK, L("enter some resistors","introduza resistencias"));
        return;
    }
    double parallel = zero ? 0 : 1 / conductance;

    result(y, L("Resistors","Resistencias"), C_BLACK, "%d", n);
    result_e(y + RESULT_STEP, L("Series","Serie"), series, 24);
    result_e(y + 2 * RESULT_STEP, L("Parallel","Paralelo"), parallel, 24);
    char a[32];
    if (!zero) result(y + 3 * RESULT_STEP, "G total", C_BLACK, "%s",
        fmt_si(a, conductance, "S", 4));
    etext(10, 192, C_DARK, DTEXT_LEFT, false,
        L("Empty fields are ignored","Campos vazios sao ignorados"));
}

void rnet_run(void)
{
    static form_t f = {
        .color = RNET_COLOR,
        .fields = rnet_fields, .count = 8, .cols = 2, .label_w = 30,
        .results = rnet_results,
    };
    f.title = L("Series / parallel", "Serie / paralelo");
    form_run(&f);
}

//---
//  Voltage divider
//---

#define DIV_COLOR C_RGB(2, 17, 6)

static field_t div_fields[] = {
    NUM("Vin", "V", "12"), NUM("R1 (top)", S_OHM, "10k"),
    NUM("R2 (bottom)", S_OHM, "4.7k"), NUM("Target Vout", "V", "3.3"),
};

static void div_results(form_t *f, int y)
{
    char a[32], b[32], c[16];
    double vin = val(f, 0), r1 = val(f, 1), r2 = val(f, 2), vt = val(f, 3);

    if (!isnan(vin) && r1 >= 0 && r2 >= 0 && r1 + r2 > 0) {
        double i = vin / (r1 + r2);
        result(y, "Vout", DIV_COLOR, "%s", fmt_si(a, vin * r2 / (r1 + r2),
            "V", 4));
        result_note(y, L("ratio %s","racio %s"), fmt_num(b, r2 / (r1 + r2), 4));
        result(y + RESULT_STEP, L("Current","Corrente"), C_BLACK, "%s",
            fmt_si(a, i, "A", 4));
        result(y + 2 * RESULT_STEP, L("Power","Potencia"), C_BLACK, "R1 %s   R2 %s",
            fmt_si(a, i * i * r1, "W", 3), fmt_si(b, i * i * r2, "W", 3));
    }
    else result(y, "Vout", C_DARK, "-");

    y += 3 * RESULT_STEP + 4;
    dline(6, y - 3, DWIDTH - 7, y - 3, C_LIGHT);
    if (isnan(vt) || isnan(vin) || isnan(r1) || vt <= 0 || vt >= vin) {
        result(y, L("Target R2","R2 alvo"), C_DARK, L("needs 0 < target < Vin","precisa 0 < alvo < Vin"));
        return;
    }
    double r2t = r1 * vt / (vin - vt);
    result(y, L("Target R2","R2 alvo"), C_BLACK, "%s", fmt_si(a, r2t, S_OHM, 4));
    for (int k = 0; k < 2; k++) {
        int series = k ? 24 : 12;
        double e = eseries_nearest(series, r2t, false);
        double vo = vin * e / (r1 + e);
        char label[16];
        sprintf(label, "  with E%d", series);
        result(y + (k + 1) * RESULT_STEP, label, C_BLACK, "%s",
            fmt_si(a, e, S_OHM, 3));
        result_note(y + (k + 1) * RESULT_STEP, "Vout %s %s",
            fmt_si(b, vo, "V", 4), fmt_dev(c, vo, vt));
    }
}

void divider_run(void)
{
    static form_t f = {
        .color = DIV_COLOR, .fields = div_fields,
        .count = 4, .cols = 1, .label_w = 108, .results = div_results,
    };
    f.title = L("Voltage divider", "Divisor de tensao");
    form_run(&f);
}

//---
//  RC / RL / LC filters
//---

#define FILT_COLOR C_RGB(17, 4, 22)

static field_t filt_fields[] = {
    NUM("R", S_OHM, "1k"), NUM("C", "F", "100n"), NUM("L", "H", "10m"),
};

static void filt_results(form_t *f, int y)
{
    char a[32], b[32];
    double r = val(f, 0), c = val(f, 1), l = val(f, 2);
    bool hr = r > 0, hc = c > 0, hl = l > 0;

    result(y, "RC  fc", FILT_COLOR, "%s", hr && hc
        ? fmt_si(a, 1 / (2 * PI * r * c), "Hz", 4) : L("needs R, C","precisa R, C"));
    if (hr && hc) result_note(y, "tau = RC = %s", fmt_si(b, r * c, "s", 4));
    y += RESULT_STEP;

    result(y, "RL  fc", FILT_COLOR, "%s", hr && hl
        ? fmt_si(a, r / (2 * PI * l), "Hz", 4) : L("needs R, L","precisa R, L"));
    if (hr && hl) result_note(y, "tau = L/R = %s", fmt_si(b, l / r, "s", 4));
    y += RESULT_STEP;

    result(y, "LC  f0", FILT_COLOR, "%s", hc && hl
        ? fmt_si(a, 1 / (2 * PI * sqrt(l * c)), "Hz", 4) : L("needs L, C","precisa L, C"));
    if (hc && hl) result_note(y, "Z0 = %s", fmt_si(b, sqrt(l / c), S_OHM, 4));
    y += RESULT_STEP;

    if (hr && hc && hl) result(y, "RLC  Q", C_BLACK, "%s",
        fmt_num(a, sqrt(l / c) / r, 3));
    y += RESULT_STEP + 4;

    etext(10, y, C_DARK, DTEXT_LEFT, false,
        "fc = 1 / (2" S_PI "RC),  fc = R / (2" S_PI "L)");
    etext(10, y + RESULT_STEP, C_DARK, DTEXT_LEFT, false,
        "-3 dB point; 63% of the step after one tau");
}

void filter_run(void)
{
    static form_t f = {
        .color = FILT_COLOR,
        .fields = filt_fields, .count = 3, .cols = 1, .label_w = 30,
        .results = filt_results,
    };
    f.title = L("RC / RL filters", "Filtros RC / RL");
    form_run(&f);
}

//---
//  555 timer
//---

#define T555_COLOR C_RGB(26, 9, 0)

static char const * const t555_modes[] = { "Astable", "Monostable" };

static field_t t555_fields[] = {
    { .kind = F_CHOICE, .label = "Mode", .options = t555_modes,
      .option_count = 2 },
    NUM("R1", S_OHM, "1k"), NUM("R2", S_OHM, "10k"), NUM("C", "F", "10" S_MU),
};

static void t555_changed(form_t *f, int index)
{
    (void)index;
    bool mono = f->fields[0].choice == 1;
    f->fields[1].label = mono ? "R" : "R1";
    f->fields[2].disabled = mono;
}

static void t555_results(form_t *f, int y)
{
    char a[32], b[32];
    double r1 = val(f, 1), r2 = val(f, 2), c = val(f, 3);

    if (f->fields[0].choice == 1) {
        if (!(r1 > 0 && c > 0)) {
            result(y, L("Pulse","Pulso"), C_DARK, L("needs R, C","precisa R, C"));
            return;
        }
        result(y, L("Pulse width","Largura pulso"), T555_COLOR, "%s",
            fmt_si(a, 1.1 * r1 * c, "s", 4));
        etext(10, y + 2 * RESULT_STEP, C_DARK, DTEXT_LEFT, false,
            "t = 1.1 R C");
        return;
    }

    if (!(r1 > 0 && r2 > 0 && c > 0)) {
        result(y, L("Frequency","Frequencia"), C_DARK, L("needs R1, R2, C","precisa R1, R2, C"));
        return;
    }
    double th = log(2) * (r1 + r2) * c;
    double tl = log(2) * r2 * c;
    result(y, L("Frequency","Frequencia"), T555_COLOR, "%s", fmt_si(a, 1 / (th + tl),
        "Hz", 4));
    result(y + RESULT_STEP, L("Period","Periodo"), C_BLACK, "%s",
        fmt_si(a, th + tl, "s", 4));
    result(y + 2 * RESULT_STEP, L("High / low","Alto / baixo"), C_BLACK, "%s / %s",
        fmt_si(a, th, "s", 4), fmt_si(b, tl, "s", 4));
    result(y + 3 * RESULT_STEP, L("Duty cycle","Ciclo"), C_BLACK, "%s%%",
        fmt_num(a, 100 * th / (th + tl), 1));
    etext(10, y + 4 * RESULT_STEP + 4, C_DARK, DTEXT_LEFT, false,
        "f = 1.44 / ((R1 + 2 R2) C)");
}

void timer555_run(void)
{
    static form_t f = {
        .color = T555_COLOR, .fields = t555_fields,
        .count = 4, .cols = 1, .label_w = 50, .results = t555_results,
        .changed = t555_changed,
    };
    f.title = L("555 timer", "Temporizador 555");
    form_run(&f);
}

//---
//  LED resistor
//---

#define LED_COLOR C_RGB(24, 3, 6)

static char const * const led_types[] = {
    "Red", "Orange", "Yellow", "Green", "Blue", "White", "Infrared",
    "Custom" };
static char const * const led_vf[] = {
    "2", "2", "2.1", "2.2", "3.2", "3.2", "1.3" };
#define LED_CUSTOM 7

static field_t led_fields[] = {
    { .kind = F_CHOICE, .label = "LED", .options = led_types,
      .option_count = 8 },
    NUM("Supply", "V", "5"), NUM("LED Vf", "V", "2"),
    NUM("Current", "A", "20m"), NUM("LEDs", "", "1"),
};

static void led_changed(form_t *f, int index)
{
    field_t *type = &f->fields[0];
    if (index == 0 && type->choice != LED_CUSTOM)
        strcpy(f->fields[2].text, led_vf[type->choice]);
    if (index == 2) type->choice = LED_CUSTOM;
}

/* Smallest common power rating with a 2x margin */
static char const *rating(double p)
{
    static char const * const names[] = {
        "1/8 W", "1/4 W", "1/2 W", "1 W", "2 W", "5 W" };
    static double const watts[] = { 0.125, 0.25, 0.5, 1, 2, 5 };
    for (int i = 0; i < 6; i++) if (2 * p <= watts[i]) return names[i];
    return "> 5 W";
}

static void led_results(form_t *f, int y)
{
    char a[32], b[32], c[32];
    double vs = val(f, 1), vf = val(f, 2), i = val(f, 3);
    double n = round(val(f, 4));

    if (isnan(n) || n < 1) n = 1;
    if (!(vs > 0 && vf > 0 && i > 0)) {
        result(y, L("Resistor","Resistencia"), C_DARK, L("needs supply, Vf, current","precisa Vcc, Vf, corrente"));
        return;
    }
    double vr = vs - n * vf;
    if (vr <= 0) {
        result(y, L("Resistor","Resistencia"), C_RED, L("supply below %s","Vcc abaixo de %s"),
            fmt_si(a, n * vf, "V", 3));
        return;
    }

    double r = vr / i;
    result(y, L("Resistor","Resistencia"), LED_COLOR, "%s", fmt_si(a, r, S_OHM, 4));
    result_note(y, "%s, %s", fmt_si(b, vr * i, "W", 3), rating(vr * i));
    for (int k = 0; k < 2; k++) {
        int series = k ? 24 : 12;
        double e = eseries_nearest(series, r, true);
        double ie = vr / e;
        char label[16];
        sprintf(label, "E%d (up)", series);
        result(y + (k + 1) * RESULT_STEP, label, C_BLACK, "%s",
            fmt_si(a, e, S_OHM, 3));
        result_note(y + (k + 1) * RESULT_STEP, "%s, %s",
            fmt_si(b, ie, "A", 3), fmt_si(c, vr * ie, "W", 2));
    }
    result(y + 3 * RESULT_STEP, L("LED power","Potencia LED"), C_BLACK, "%s",
        fmt_si(a, n * vf * i, "W", 3));
    etext(10, y + 4 * RESULT_STEP + 2, C_DARK, DTEXT_LEFT, false,
        "R = (Vs - n Vf) / I, rounded up");
}

void led_run(void)
{
    static form_t f = {
        .color = LED_COLOR, .fields = led_fields,
        .count = 5, .cols = 1, .label_w = 70, .results = led_results,
        .changed = led_changed,
    };
    f.title = L("LED resistor", "Resistencia LED");
    form_run(&f);
}
