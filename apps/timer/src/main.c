#include <gint/display.h>
#include <gint/keyboard.h>
#include <gint/rtc.h>
#include <gint/timer.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "utils.h"
#include "info.h"
#include "version.h"
#include "i18n.h"

#define SW_COLOR  C_RGB(3, 9, 24)
#define TICKS     128                   /* RTC ticks per second */
#define DAY_TICKS (TICKS * 86400u)
#define FRAME_US  50000                 /* screen refresh while waiting */
#define L_DAYS { L("Sun","Dom"), L("Mon","Seg"), L("Tue","Ter"), \
    L("Wed","Qua"), L("Thu","Qui"), L("Fri","Sex"), L("Sat","Sab") }
#define MAX_LAPS  99

//---
//  Clock
//---

/* RTC ticks, continuing past midnight (rtc_ticks() wraps around) */
static uint32_t now(void)
{
    static uint32_t last, offset;
    uint32_t t = rtc_ticks();
    if (t < last) offset += DAY_TICKS;
    last = t;
    return t + offset;
}

/* A stopwatch-like accumulator of running time */
typedef struct {
    bool running;
    uint32_t start;         /* tick when it was last started */
    uint32_t accum;         /* ticks accumulated before that */
} chrono_t;

static uint32_t chrono_ticks(chrono_t const *c)
{
    return c->accum + (c->running ? now() - c->start : 0);
}

static void chrono_toggle(chrono_t *c)
{
    if (c->running) c->accum += now() - c->start;
    else c->start = now();
    c->running = !c->running;
}

static void chrono_reset(chrono_t *c)
{
    c->running = false;
    c->accum = 0;
}

/* "mm:ss.cc" below an hour, "h:mm:ss" above */
static char *fmt_time(char *buf, uint32_t ticks, bool hundredths)
{
    uint32_t cs = ticks * 100 / TICKS;
    uint32_t s = ticks / TICKS;
    if (s >= 3600)
        sprintf(buf, "%u:%02u:%02u", (unsigned)(s / 3600),
            (unsigned)(s / 60 % 60), (unsigned)(s % 60));
    else if (hundredths)
        sprintf(buf, "%02u:%02u.%02u", (unsigned)(s / 60),
            (unsigned)(s % 60), (unsigned)(cs % 100));
    else
        sprintf(buf, "%02u:%02u", (unsigned)(s / 60), (unsigned)(s % 60));
    return buf;
}

//---
//  Large text: font_big scaled up
//---

/* Draw text with font_big, each pixel as a scale x scale block, centred
   on cx. The text is first rendered at the top left of the VRAM and read
   back, so call this before drawing anything else in that area */
static void big_text(int cx, int y, int color, char const *text, int scale)
{
    static uint8_t bits[14][120];
    font_t const *old = dfont(&font_big);
    int w, h;
    dsize(text, NULL, &w, &h);
    if (w > 120) w = 120;
    if (h > 14) h = 14;

    drect(0, 0, w + 1, h + 1, C_WHITE);
    dtext(0, 0, C_BLACK, text);
    for (int r = 0; r < h; r++)
        for (int c = 0; c < w; c++)
            bits[r][c] = dgetpixel(c, r) == C_BLACK;
    drect(0, 0, w + 1, h + 1, C_WHITE);
    dfont(old);

    int x0 = cx - w * scale / 2;
    for (int r = 0; r < h; r++)
        for (int c = 0; c < w; c++)
            if (bits[r][c])
                drect(x0 + c * scale, y + r * scale,
                    x0 + (c + 1) * scale - 1, y + (r + 1) * scale - 1, color);
}

//---
//  Stopwatch
//---

typedef struct {
    chrono_t chrono;
    uint32_t laps[MAX_LAPS];    /* total time at each lap */
    int lap_count;
} stopwatch_t;

static void sw_lap(stopwatch_t *sw)
{
    if (!sw->chrono.running || sw->lap_count >= MAX_LAPS) return;
    sw->laps[sw->lap_count++] = chrono_ticks(&sw->chrono);
}

static void sw_reset(stopwatch_t *sw)
{
    chrono_reset(&sw->chrono);
    sw->lap_count = 0;
}

#define LAPS_SHOWN 5

static void sw_draw_big(stopwatch_t const *sw)
{
    char buf[16];
    uint32_t t = chrono_ticks(&sw->chrono);
    big_text(DWIDTH / 2, 44, sw->chrono.running ? C_BLACK : C_DARK,
        fmt_time(buf, t, true), 4);
}

static void sw_draw(stopwatch_t const *sw)
{
    char a[16], b[16];
    uint32_t t = chrono_ticks(&sw->chrono);

    dtext(10, 20, SW_COLOR, sw->chrono.running ? L("Running", "A contar")
        : t ? L("Stopped", "Parado") : L("Ready", "Pronto"));
    dtext_opt(DWIDTH / 2, 106, C_DARK, C_NONE, DTEXT_CENTER, DTEXT_TOP,
        sw->chrono.running ? L("EXE: stop   F3: lap", "EXE: parar   F3: volta")
        : L("EXE: start   F4 / AC: reset", "EXE: iniciar   F4 / AC: repor"));

    if (!sw->lap_count) return;

    /* Fastest and slowest laps are coloured */
    int fast = 0, slow = 0;
    for (int i = 1; i < sw->lap_count; i++) {
        uint32_t d = sw->laps[i] - sw->laps[i - 1];
        if (d < sw->laps[fast] - (fast ? sw->laps[fast - 1] : 0)) fast = i;
        if (d > sw->laps[slow] - (slow ? sw->laps[slow - 1] : 0)) slow = i;
    }

    int y = 124;
    dline(6, y - 4, DWIDTH - 7, y - 4, C_LIGHT);
    dtext(10, y, C_DARK, L("Lap", "Volta"));
    dtext(110, y, C_DARK, L("Lap time", "Tempo"));
    dtext(250, y, C_DARK, "Total");
    for (int k = 0; k < LAPS_SHOWN && k < sw->lap_count; k++) {
        int i = sw->lap_count - 1 - k;
        uint32_t d = sw->laps[i] - (i ? sw->laps[i - 1] : 0);
        int fg = sw->lap_count < 2 ? C_BLACK : i == fast ? C_RGB(2, 17, 6)
            : i == slow ? C_RGB(26, 4, 4) : C_BLACK;
        y += 13;
        dprint(10, y, C_BLACK, "%d", i + 1);
        dtext(110, y, fg, fmt_time(a, d, true));
        dtext(250, y, C_DARK, fmt_time(b, sw->laps[i], true));
    }
}

//---
//  Pomodoro
//---

enum { PH_WORK, PH_SHORT, PH_LONG };

static char const *phase_names[3];
static void phase_names_init(void)
{
    phase_names[PH_WORK]  = L("Focus", "Foco");
    phase_names[PH_SHORT] = L("Short break", "Pausa curta");
    phase_names[PH_LONG]  = L("Long break", "Pausa longa");
}
static int phase_color(int phase)
{
    return phase == PH_WORK ? C_RGB(26, 4, 4)
        : phase == PH_SHORT ? C_RGB(2, 17, 6) : C_RGB(3, 9, 24);
}

static int const work_choices[] = { 15, 20, 25, 30, 45, 50, 60 };
static int const break_choices[] = { 3, 5, 10, 15 };
#define SESSIONS 4                  /* focus sessions before a long break */

typedef struct {
    chrono_t chrono;
    int phase;
    int work_i, break_i;            /* indices in the choices above */
    int done;                       /* focus sessions completed */
    bool alarm;                     /* the last phase just ended */
} pomodoro_t;

static uint32_t phase_ticks(pomodoro_t const *p)
{
    int minutes = p->phase == PH_WORK ? work_choices[p->work_i]
        : p->phase == PH_SHORT ? break_choices[p->break_i]
        : 3 * break_choices[p->break_i];
    return (uint32_t)minutes * 60 * TICKS;
}

static uint32_t remaining(pomodoro_t const *p)
{
    uint32_t total = phase_ticks(p), t = chrono_ticks(&p->chrono);
    return t >= total ? 0 : total - t;
}

/* Go to the next phase, stopped */
static void next_phase(pomodoro_t *p)
{
    if (p->phase == PH_WORK) {
        p->done++;
        p->phase = (p->done % SESSIONS == 0) ? PH_LONG : PH_SHORT;
    }
    else p->phase = PH_WORK;
    chrono_reset(&p->chrono);
}

/* Called every frame: end the phase when its time is up */
static void pomo_update(pomodoro_t *p)
{
    if (p->chrono.running && remaining(p) == 0) {
        next_phase(p);
        p->alarm = true;
    }
}

static void pomo_draw_big(pomodoro_t const *p)
{
    char buf[16];
    /* Round up so that 25:00 shows until a full second has passed */
    uint32_t r = remaining(p) + TICKS - 1;
    big_text(DWIDTH / 2, 48, phase_color(p->phase),
        fmt_time(buf, r, false), 5);
}

static void pomo_draw(pomodoro_t const *p, bool blink)
{
    int color = phase_color(p->phase);
    uint32_t total = phase_ticks(p);
    uint32_t elapsed = total - remaining(p);

    dtext(10, 20, color, phase_names[p->phase]);
    dtext_opt(DWIDTH / 2, 30, color, C_NONE, DTEXT_CENTER, DTEXT_TOP,
        p->chrono.running ? "" : p->alarm ? "" : chrono_ticks(&p->chrono)
        ? L("Paused", "Em pausa") : "");

    /* Progress bar */
    int x1 = 40, x2 = DWIDTH - 41, y = 126;
    drect_border(x1, y, x2, y + 9, C_WHITE, 1, color);
    int fill = (int)((uint64_t)(x2 - x1 - 3) * elapsed / total);
    if (fill > 0) drect(x1 + 2, y + 2, x1 + 2 + fill - 1, y + 7, color);

    /* Sessions in the current set */
    y = 146;
    int in_set = p->done % SESSIONS;
    if (p->done && in_set == 0 && p->phase == PH_LONG) in_set = SESSIONS;
    for (int i = 0; i < SESSIONS; i++) {
        int cx = DWIDTH / 2 - 36 + 24 * i;
        if (i < in_set) dcircle(cx, y + 5, 6, phase_color(PH_WORK),
            phase_color(PH_WORK));
        else dcircle(cx, y + 5, 6, C_WHITE, C_LIGHT);
    }
    dprint_opt(DWIDTH - 10, y, C_DARK, C_NONE, DTEXT_RIGHT, DTEXT_TOP,
        L("%d done", "%d feitos"), p->done);
    dprint(10, y, C_DARK, "%d / %d / %d min", work_choices[p->work_i],
        break_choices[p->break_i], 3 * break_choices[p->break_i]);

    y = 170;
    if (p->alarm) {
        int bg = blink ? color : C_WHITE;
        int fg = blink ? C_WHITE : color;
        drect(6, y - 3, DWIDTH - 7, y + 26, bg);
        dtext_opt(DWIDTH / 2, y, fg, C_NONE, DTEXT_CENTER, DTEXT_TOP,
            L("Time's up!", "Tempo esgotado!"));
        dprint_opt(DWIDTH / 2, y + 13, fg, C_NONE, DTEXT_CENTER, DTEXT_TOP,
            L("EXE: start %s", "EXE: iniciar %s"), phase_names[p->phase]);
    }
    else dtext_opt(DWIDTH / 2, y + 6, C_DARK, C_NONE, DTEXT_CENTER,
        DTEXT_TOP, p->chrono.running ? L("EXE: pause   F3: skip", "EXE: pausar   F3: saltar")
        : L("EXE: start   F3: skip   F4: reset", "EXE: iniciar  F3: saltar  F4: repor"));
}

//---
//  Main loop
//---

enum { MODE_STOPWATCH, MODE_POMODORO };

static void draw_clock(void)
{
    char const * const days[] = L_DAYS;
    rtc_time_t t;
    rtc_get_time(&t);   /* month is the raw register, 1..12 */
    dprint_opt(DWIDTH - 10, 20, C_DARK, C_NONE, DTEXT_RIGHT, DTEXT_TOP,
        "%s %02d/%02d  %02d:%02d:%02d", days[t.week_day % 7], t.month_day,
        t.month, t.hours, t.minutes, t.seconds);
}

static void draw(int mode, stopwatch_t const *sw, pomodoro_t const *p)
{
    char label[12];
    int color = mode == MODE_STOPWATCH ? SW_COLOR : phase_color(p->phase);

    dclear(C_WHITE);
    /* The large digits use the top-left corner as scratch space */
    if (mode == MODE_STOPWATCH) sw_draw_big(sw);
    else pomo_draw_big(p);

    info_title(mode == MODE_STOPWATCH ? L("Stopwatch", "Cronometro") : "Pomodoro", color);
    draw_clock();

    if (mode == MODE_STOPWATCH) sw_draw(sw);
    else pomo_draw(p, (now() / (TICKS / 2)) % 2);

    if (mode == MODE_STOPWATCH) {
        fkey_button_color(1, L("WATCH", "CRONO"), color);
        fkey_button(2, "POMO");
        fkey_button(3, L("LAP", "VOLTA"));
        fkey_button(4, L("RESET", "REPOR"));
    }
    else {
        fkey_button(1, L("WATCH", "CRONO"));
        fkey_button_color(2, "POMO", color);
        fkey_button(3, L("SKIP", "SALTAR"));
        fkey_button(4, L("RESET", "REPOR"));
        sprintf(label, "%d MIN", work_choices[p->work_i]);
        fkey_button(5, label);
        sprintf(label, "%d MIN", break_choices[p->break_i]);
        fkey_button(6, label);
    }
    dupdate();
}

/* getkey() that returns 0 after FRAME_US so the clock keeps moving */
static int getkey_frame(void)
{
    volatile int flag = 0;
    int t = timer_configure(TIMER_ANY, FRAME_US,
        GINT_CALL(timer_timeout, (void *)&flag));
    if (t < 0) return getkey().key;
    timer_start(t);
    key_event_t ev = getkey_opt(GETKEY_DEFAULT, &flag);
    timer_stop(t);
    return ev.type == KEYEV_NONE ? 0 : ev.key;
}

int main(void)
{
    static stopwatch_t sw;
    static pomodoro_t p = { .phase = PH_WORK, .work_i = 2, .break_i = 1 };
    int mode = MODE_STOPWATCH;

    lang_init();
    phase_names_init();

    while (1) {
        pomo_update(&p);
        draw(mode, &sw, &p);

        int key = getkey_frame();
        if (!key) continue;

        if (key == KEY_EXIT) return 1;
        if (key == INFO_KEY) {
            info_show("Timer", L("Stopwatch / Pomodoro " APPS_VERSION,
                "Cronometro / Pomodoro " APPS_VERSION),
                mode == MODE_STOPWATCH ? SW_COLOR : phase_color(p.phase));
            continue;
        }
        if (key == KEY_F1) { mode = MODE_STOPWATCH; continue; }
        if (key == KEY_F2) { mode = MODE_POMODORO; continue; }

        if (mode == MODE_STOPWATCH) {
            if (key == KEY_EXE) chrono_toggle(&sw.chrono);
            else if (key == KEY_F3) sw_lap(&sw);
            else if (key == KEY_F4 || key == KEY_ACON) sw_reset(&sw);
            continue;
        }

        if (key == KEY_EXE) {
            p.alarm = false;
            chrono_toggle(&p.chrono);
        }
        else if (key == KEY_F3) {
            p.alarm = false;
            next_phase(&p);
        }
        else if (key == KEY_F4 || key == KEY_ACON) {
            p.alarm = false;
            chrono_reset(&p.chrono);
        }
        else if (key == KEY_F5)
            p.work_i = (p.work_i + 1) % (int)(sizeof work_choices
                / sizeof *work_choices);
        else if (key == KEY_F6)
            p.break_i = (p.break_i + 1) % (int)(sizeof break_choices
                / sizeof *break_choices);
    }
}
