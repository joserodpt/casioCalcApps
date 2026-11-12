#include <gint/display.h>
#include <gint/keyboard.h>
#include <gint/timer.h>
#include <gint/clock.h>
#include <gint/rtc.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "menu.h"
#include "ui.h"
#include "numfield.h"
#include "utils.h"
#include "keys.h"
#include "info.h"
#include "version.h"
#include "i18n.h"
#include "usb_screen.h"

#define COLOR       C_RGB(20, 4, 20)
#define PIANO_COLOR C_RGB(4, 10, 24)
#define SWEEP_COLOR C_RGB(24, 10, 0)
#define OUT_COLOR   C_RGB(8, 12, 14)
#define MELODY_COLOR C_RGB(22, 4, 10)
#define PLAY_COLOR  C_RGB(2, 18, 6)
#define SOUND_SPEED 343.0               /* m/s, in air at 20 °C */

#if GINT_HW_FX
#define FK(cg, fx) fx
#else
#define FK(cg, fx) cg
#endif

/* The tone: shared by the screens */
static struct {
    double freq;
    int wave, duty, volume;
    out_t method;
} cfg = { 440, WAVE_SQUARE, 50, 100, OUT_UART };

static int const duties[] = { 50, 25, 10 };
static int const volumes[] = { 100, 50, 25 };
#define DUTIES  (int)(sizeof duties / sizeof *duties)
#define VOLUMES (int)(sizeof volumes / sizeof *volumes)

/* A one-off message on the status line (port errors) */
static char const *message;

#if !GINT_HW_FX
static char const *wave_name(int w)
{
    switch (w) {
    case WAVE_SQUARE: return L("square", "quadrada", "carree");
    case WAVE_SINE: return L("sine", "seno", "sinus");
    case WAVE_TRIANGLE: return L("triangle", "triangulo", "triangle");
    default: return L("sawtooth", "dente de serra", "dent de scie");
    }
}
#endif

static char const *method_name(int m)
{
    return m == OUT_UART ? "UART" : L("pin", "pino", "broche");
}

/* Open the port if needed; false (and a message) if it can't be */
static bool port_ready(void)
{
    if (!audio_supported()) {
        message = L("Not on this model (SH3 serial port)",
            "Nao neste modelo (porta serie SH3)",
            "Pas sur ce modele (port serie SH3)");
        return false;
    }
    if (!audio_open(cfg.method)) {
        message = L("Could not open the 3-pin port",
            "Nao foi possivel abrir a porta de 3 pinos",
            "Impossible d'ouvrir le port 3 broches");
        return false;
    }
    return true;
}

static void play(double freq)
{
    if (port_ready()) audio_play(freq, cfg.wave, cfg.duty, cfg.volume);
}

static double clamp_freq(double f)
{
    return f < FREQ_MIN ? FREQ_MIN : f > FREQ_MAX ? FREQ_MAX : f;
}

//---
//  Text helpers
//---

#if GINT_HW_FX
/* "250", "12.5k": a frequency in few characters (fx-9860G) */
static char *fmt_short_hz(char *buf, double f)
{
    if (f < 1000) return fmt_num(buf, f, 0);
    fmt_num(buf, f / 1000, 1);
    strcat(buf, "k");
    return buf;
}
#endif

static char const *const note_names[12] = { "C", "C#", "D", "D#", "E", "F",
    "F#", "G", "G#", "A", "A#", "B" };

static double midi_freq(int midi)
{
    return 440 * pow(2, (midi - 69) / 12.0);
}

/* "A4 +0 cents" (short: "A4 +0c") */
static char *fmt_note(char *buf, double f, bool shrt)
{
    double n = 12 * log(f / 440) / log(2) + 69;
    int m = (int)floor(n + 0.5);
    int cents = (int)floor((n - m) * 100 + 0.5);
    sprintf(buf, "%s%d %s%d%s", note_names[(m % 12 + 12) % 12], m / 12 - 1,
        cents >= 0 ? "+" : "", cents, shrt ? "c"
        : L(" cents", " cents", " cents"));
    return buf;
}

/* "440 Hz", "261.63 Hz" */
static char *fmt_hz(char *buf, double f)
{
    fmt_num(buf, f, f < 1000 ? 2 : 1);
    strcat(buf, " Hz");
    return buf;
}

/* Text in font_big, each pixel a scale x scale block, centred on cx. It is
   first drawn at the top left of the VRAM and read back: call it before
   drawing anything there (the title bar comes after it) */
static void big_text(int cx, int y, int color, char const *text, int scale)
{
    static uint8_t bits[14][160];
    font_t const *old = dfont(&font_big);
    if (scale == 1) {
        dtext_opt(cx, y, color, C_NONE, DTEXT_CENTER, DTEXT_TOP, text);
        dfont(old);
        return;
    }
    int w, h;
    dsize(text, NULL, &w, &h);
    if (w > 160) w = 160;
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

/* Two periods of the waveform in a box */
static void draw_wave(int x1, int y1, int x2, int y2, int wave, int color)
{
    int mid = (y1 + y2) / 2;
    int amp = (y2 - y1) / 2 - 2;
    /* A square wave on the pin is always at full amplitude */
    if (!(wave == WAVE_SQUARE && cfg.method == OUT_PIN))
        amp = amp * cfg.volume / 100;

    drect_border(x1, y1, x2, y2, C_WHITE, 1, C_LIGHT);
    for (int x = x1 + 2; x < x2 - 1; x += 4) dpixel(x, mid, C_LIGHT);

    int px = -1, py = 0, w = x2 - x1 - 4;
    for (int x = x1 + 2; x <= x2 - 2; x++) {
        double v = audio_sample(wave, cfg.duty, 2.0 * (x - x1 - 2) / w);
        int y = mid - (int)(v * amp + (v < 0 ? -0.5 : 0.5));
        if (px >= 0) dline(px, py, x, y, color);
        px = x;
        py = y;
    }
}

/* Status line: playing or not, or the message */
static void draw_status(int x, int y, char const *idle)
{
    char line[64];
    if (message) {
        dtext(x, y, GINT_HW_FX ? C_BLACK : C_RED, message);
        return;
    }
    if (!audio_playing()) {
        dtext(x, y, C_DARK, idle);
        return;
    }
#if GINT_HW_FX
    sprintf(line, L("PLAYING (%s)", "A TOCAR (%s)", "LECTURE (%s)"),
        method_name(cfg.method));
    dtext(x, y, C_BLACK, line);
#else
    drect(x, y + 1, x + 7, y + 8, PLAY_COLOR);
    sprintf(line, L("Playing on the 3-pin port (%s)",
        "A tocar na porta de 3 pinos (%s)",
        "Lecture sur le port 3 broches (%s)"), method_name(cfg.method));
    dtext(x + 12, y, PLAY_COLOR, line);
#endif
}

/* getkey() that gives up after `us`; type KEYEV_NONE then */
static key_event_t getkey_ev_for(int us)
{
    volatile int flag = 0;
    int t = timer_configure(TIMER_ANY, us,
        GINT_CALL(timer_timeout, (void *)&flag));
    if (t < 0) return getkey();
    timer_start(t);
    key_event_t ev = getkey_opt(GETKEY_DEFAULT, &flag);
    timer_stop(t);
    return ev;
}

/* The same, for the key only; 0 if none */
static int getkey_for(int us)
{
    key_event_t ev = getkey_ev_for(us);
    return ev.type == KEYEV_NONE ? 0 : ev.key;
}

//---
//  Melodies
//---

/* A MIDI note (0: a rest) and its length in units of the melody */
typedef struct { uint8_t midi, len; } note_t;

typedef struct {
    char const *name, *origin_en, *origin_pt, *origin_fr;
    int unit_ms;
    note_t const *notes;
    int count;
} melody_t;

/* Lengths in sixteenths: 4 is a quarter note */
static note_t const birthday[] = {
    {67,3},{67,1},{69,4},{67,4},{72,4},{71,8},
    {67,3},{67,1},{69,4},{67,4},{74,4},{72,8},
    {67,3},{67,1},{79,4},{76,4},{72,4},{71,4},{69,8},
    {77,3},{77,1},{76,4},{72,4},{74,4},{72,12},
};
static note_t const ode[] = {
    {76,4},{76,4},{77,4},{79,4},{79,4},{77,4},{76,4},{74,4},
    {72,4},{72,4},{74,4},{76,4},{76,6},{74,2},{74,8},
    {76,4},{76,4},{77,4},{79,4},{79,4},{77,4},{76,4},{74,4},
    {72,4},{72,4},{74,4},{76,4},{74,6},{72,2},{72,8},
};
static note_t const elise[] = {
    {76,1},{75,1},{76,1},{75,1},{76,1},{71,1},{74,1},{72,1},
    {69,2},{0,1},{60,1},{64,1},{69,1},{71,2},{0,1},{64,1},{68,1},{71,1},
    {72,2},{0,1},{64,1},{76,1},{75,1},
    {76,1},{75,1},{76,1},{71,1},{74,1},{72,1},
    {69,2},{0,1},{60,1},{64,1},{69,1},{71,2},{0,1},{64,1},{72,1},{71,1},
    {69,6},
};
static note_t const jingle[] = {
    {76,4},{76,4},{76,8},{76,4},{76,4},{76,8},
    {76,4},{79,4},{72,6},{74,2},{76,16},
    {77,4},{77,4},{77,6},{77,2},{77,4},{76,4},{76,4},{76,2},{76,2},
    {76,4},{74,4},{74,4},{76,4},{74,8},{79,8},
    {76,4},{76,4},{76,8},{76,4},{76,4},{76,8},
    {76,4},{79,4},{72,6},{74,2},{76,16},
    {77,4},{77,4},{77,6},{77,2},{77,4},{76,4},{76,4},{76,2},{76,2},
    {79,4},{79,4},{77,4},{74,4},{72,16},
};
static note_t const twinkle[] = {
    {72,4},{72,4},{79,4},{79,4},{81,4},{81,4},{79,8},
    {77,4},{77,4},{76,4},{76,4},{74,4},{74,4},{72,8},
    {79,4},{79,4},{77,4},{77,4},{76,4},{76,4},{74,8},
    {79,4},{79,4},{77,4},{77,4},{76,4},{76,4},{74,8},
    {72,4},{72,4},{79,4},{79,4},{81,4},{81,4},{79,8},
    {77,4},{77,4},{76,4},{76,4},{74,4},{74,4},{72,8},
};
static note_t const korobeiniki[] = {
    {76,4},{71,2},{72,2},{74,4},{72,2},{71,2},
    {69,4},{69,2},{72,2},{76,4},{74,2},{72,2},
    {71,6},{72,2},{74,4},{76,4},{72,4},{69,4},{69,8},
    {0,2},{74,4},{77,2},{81,4},{79,2},{77,2},
    {76,6},{72,2},{76,4},{74,2},{72,2},
    {71,4},{71,2},{72,2},{74,4},{76,4},{72,4},{69,4},{69,8},
};
/* From the official 1957 version, in E flat as it is sung */
static note_t const portuguesa[] = {
    {70,1},{67,3},{63,1},
    {72,8},{70,7},{72,1},
    {70,4},{68,4},{67,3},{63,1},{63,3},{63,1},
    {67,8},{69,3},{70,1},{72,3},{74,1},
    {70,12},{70,3},{74,1},
    {77,8},{74,4},{70,2},{68,2},
    {68,8},{67,4},{70,3},{67,1},
    {63,8},{63,2},{65,2},{67,2},{70,2},
    {65,10},{0,6},
    {65,6},{67,2},{63,4},{63,2},{67,2},
    {68,6},{70,2},{67,4},{67,2},{70,2},
    {75,6},{70,2},{70,2},{73,4},{71,2},
    {70,4},{65,8},{0,2},{68,2},
    {66,4},{70,4},{70,4},{68,2},{66,2},
    {65,12},{0,2},{68,2},
    {66,4},{70,3},{70,1},{70,4},{68,2},{66,2},
    {70,3},{70,1},{70,6},{0,5},{70,1},
    {75,8},{74,4},{0,3},{68,1},
    {72,8},{70,4},{63,2},{65,2},
    {67,4},{67,4},{70,4},{67,4},
    {65,10},{0,5},{70,1},
    {75,8},{74,4},{0,3},{68,1},
    {72,8},{70,4},{67,1},{70,2},{75,1},
    {79,8},{77,7},{75,1},
    {75,10},{67,2},{68,2},{70,2},
    {72,4},{72,4},{70,4},{68,2},{70,2},
    {75,16},
};
/* The Nutcracker: Dance of the Sugar Plum Fairy, the celesta's theme */
static note_t const sugarplum[] = {
    {0,4},{79,2},{76,2},{79,4},{78,4},
    {75,4},{76,4},{74,2},{74,2},{74,4},
    {73,2},{73,2},{73,4},{72,2},{72,2},{72,4},
    {71,2},{76,2},{72,2},{76,2},{71,4},{64,1},{62,1},{60,1},{59,1},
    {58,4},{67,2},{64,2},{67,4},{66,4},
    {72,4},{71,4},{79,2},{79,2},{79,4},
    {78,2},{78,2},{78,4},{76,2},{76,2},{76,4},
    {75,2},{78,2},{76,2},{78,2},{74,4},{71,1},{69,1},{67,1},{66,1},
    {64,4},{79,2},{76,2},{79,4},{78,4},
    {75,4},{76,4},{74,2},{74,2},{74,4},
    {73,2},{73,2},{73,4},{72,2},{72,2},{72,4},
    {71,2},{76,2},{72,2},{76,2},{71,4},{67,1},{66,1},{64,1},{62,1},
    {61,4},{76,2},{73,2},{76,4},{75,4},
    {59,4},{74,2},{71,2},{74,4},{73,4},
    {57,4},{72,2},{69,2},{72,4},{71,4},
    {0,4},{59,1},{63,1},{66,1},{71,1},{64,8},
};

/* Ragtime: the introduction and the first strain */
static note_t const entertainer[] = {
    {86,1},{88,1},{84,1},{81,2},{83,1},{79,2},{74,1},{76,1},{72,1},{69,2},
    {71,1},{67,2},{62,1},{64,1},{60,4},{0,8},{62,1},{63,1},{64,1},{72,2},
    {64,1},{72,2},{64,1},{72,6},{84,1},{86,1},{87,1},{88,1},{84,1},{86,1},
    {88,2},{83,1},{86,2},{84,6},{62,1},{63,1},{64,1},{72,2},{64,1},{72,2},
    {64,1},{72,7},{81,1},{79,1},{78,1},{81,1},{84,1},{88,2},{86,1},{84,1},
    {81,1},{86,6},{62,1},{63,1},{64,1},{72,2},{64,1},{72,2},{64,1},{72,6},
    {84,1},{86,1},{87,1},{88,1},{84,1},{86,1},{88,2},{83,1},{86,2},{84,6},
    {84,1},{86,1},{88,1},{84,1},{86,1},{88,2},{84,1},{86,1},{84,1},{88,1},
    {84,1},{86,1},{88,2},{84,1},{86,1},{84,1},{88,1},{84,1},{86,1},{88,2},
    {83,1},{86,2},{84,8},
};

/* Played three times, faster each time, as the piece speeds up */
static note_t const mountain[] = {
    {71,4},{73,4},{74,4},{76,4},{78,4},{74,4},{78,8},{77,4},{73,4},{77,8},
    {76,4},{72,4},{76,8},{71,4},{73,4},{74,4},{76,4},{78,4},{74,4},{78,4},
    {83,4},{81,4},{78,4},{74,4},{78,4},{81,8},{0,8},{78,4},{80,4},{82,4},
    {83,4},{85,4},{82,4},{85,8},{86,4},{82,4},{86,8},{85,4},{82,4},{85,8},
    {78,4},{80,4},{82,4},{83,4},{85,4},{82,4},{85,8},{86,4},{82,4},{86,8},
    {85,8},{0,8},{71,3},{73,3},{74,3},{76,3},{78,3},{74,3},{78,6},{77,3},
    {73,3},{77,6},{76,3},{72,3},{76,6},{71,3},{73,3},{74,3},{76,3},{78,3},
    {74,3},{78,3},{83,3},{81,3},{78,3},{74,3},{78,3},{81,6},{0,6},{78,3},
    {80,3},{82,3},{83,3},{85,3},{82,3},{85,6},{86,3},{82,3},{86,6},{85,3},
    {82,3},{85,6},{78,3},{80,3},{82,3},{83,3},{85,3},{82,3},{85,6},{86,3},
    {82,3},{86,6},{85,6},{0,6},{71,2},{73,2},{74,2},{76,2},{78,2},{74,2},
    {78,4},{77,2},{73,2},{77,4},{76,2},{72,2},{76,4},{71,2},{73,2},{74,2},
    {76,2},{78,2},{74,2},{78,2},{83,2},{81,2},{78,2},{74,2},{78,2},{81,4},
    {0,4},{78,2},{80,2},{82,2},{83,2},{85,2},{82,2},{85,4},{86,2},{82,2},
    {86,4},{85,2},{82,2},{85,4},{78,2},{80,2},{82,2},{83,2},{85,2},{82,2},
    {85,4},{86,2},{82,2},{86,4},{85,4},{0,4},{71,8},
};

/* The opening: two motifs and their fermatas */
static note_t const fifth[] = {
    {0,2},{67,2},{67,2},{67,2},{63,20},{0,4},{65,2},{65,2},{65,2},{62,28},
};

/* The galop of the finale */
static note_t const tell[] = {
    {74,2},{74,2},{74,4},{74,2},{74,2},{74,4},{74,2},{74,2},{79,4},{81,4},
    {83,4},{74,2},{74,2},{74,4},{74,2},{74,2},{79,4},{83,4},{81,4},{78,4},
    {74,4},{74,2},{74,2},{74,4},{74,2},{74,2},{74,4},{74,2},{74,2},{79,4},
    {81,4},{83,4},{79,2},{83,2},{86,10},{84,2},{83,2},{81,2},{79,4},{83,4},
    {79,8},
};

static note_t const cancan[] = {
    {72,8},{74,2},{77,2},{76,2},{74,2},{79,4},{79,4},{79,2},{81,2},{76,2},
    {77,2},{74,4},{74,4},{74,2},{77,2},{76,2},{74,2},{72,2},{84,2},{83,2},
    {81,2},{79,2},{77,2},{76,2},{74,2},{72,4},{72,4},{74,2},{77,2},{76,2},
    {74,2},{79,4},{79,4},{79,2},{81,2},{76,2},{77,2},{74,4},{74,4},{74,2},
    {77,2},{76,2},{74,2},{72,2},{79,2},{74,2},{76,2},{72,4},{84,4},
};

/* The trumpets' fanfare and the theme. Lengths in twelfths of a quarter
   note, for the triplets */
static note_t const wedding[] = {
    {72,4},{72,4},{72,4},{72,36},{72,4},{72,4},{72,4},{72,36},{72,4},{72,4},
    {72,4},{76,12},{76,4},{76,4},{76,4},{76,12},{76,4},{76,4},{76,4},
    {79,12},{79,4},{79,4},{79,4},{79,24},{84,24},{83,18},{78,6},{81,12},
    {79,12},{77,12},{74,12},{72,24},{74,12},{67,9},{74,3},{76,30},{72,6},
    {76,6},{79,6},{84,24},{83,18},{78,6},{81,12},{79,12},{77,12},{74,12},
    {72,24},{76,12},{74,9},{76,3},{74,24},{72,36},
};

/* The oboe's theme */
static note_t const swan[] = {
    {78,8},{71,2},{73,2},{74,2},{76,2},{78,6},{74,2},{78,6},{74,2},
    {78,6},{71,2},{74,2},{71,2},{67,2},{74,2},{71,16},
};

static note_t const wish[] = {
    {74,4},{79,4},{79,2},{81,2},{79,2},{78,2},{76,4},{76,4},{76,4},{81,4},
    {81,2},{83,2},{81,2},{79,2},{78,4},{74,4},{74,4},{83,4},{83,2},{84,2},
    {83,2},{81,2},{79,4},{76,4},{74,2},{74,2},{76,4},{81,4},{78,4},{79,8},
    {74,4},{79,4},{79,4},{79,4},{78,8},{78,4},{79,4},{78,4},{76,4},{74,8},
    {74,4},{83,4},{81,4},{79,4},{86,4},{74,4},{74,2},{74,2},{76,4},{81,4},
    {78,4},{79,12},
};

static note_t const marseillaise[] = {
    {67,1},{67,3},{67,1},{72,4},{72,4},{74,4},{74,4},{79,6},{77,2},{76,2},
    {0,1},{72,1},{76,3},{72,1},{69,4},{77,8},{74,3},{71,1},{72,8},{0,4},
    {72,3},{74,1},{76,4},{76,4},{76,4},{77,3},{76,1},{76,4},{74,2},{0,6},
    {74,3},{76,1},{77,4},{77,4},{77,4},{79,3},{77,1},{77,4},{76,4},{0,4},
    {79,3},{79,1},{79,4},{76,3},{72,1},{79,4},{76,3},{72,1},{67,8},{0,3},
    {67,1},{67,3},{67,1},{71,6},{74,2},{77,6},{74,2},{74,2},{72,2},{72,2},
    {0,6},{68,4},{67,4},{72,3},{72,1},{72,4},{71,3},{72,1},{74,8},{0,4},
    {74,4},{75,6},{75,2},{75,2},{75,2},{77,2},{79,2},{74,8},{0,4},{75,2},
    {74,2},{72,4},{72,4},{75,4},{74,3},{72,1},{72,4},{71,2},{0,9},{79,1},
    {79,10},{76,2},{72,3},{72,1},{74,8},{0,7},{79,1},{79,10},{76,2},{72,3},
    {72,1},{74,8},{0,4},{67,4},{72,8},{0,4},{72,4},{76,8},{0,8},{77,8},
    {79,6},{81,2},{71,8},{0,7},{81,1},{79,10},{76,2},{77,2},{74,2},{72,8},
};

#define MELODY(name, en, pt, fr, ms, notes) \
    { name, en, pt, fr, ms, notes, (int)(sizeof notes / sizeof *notes) }
static melody_t const melodies[] = {
    MELODY("Happy Birthday", "Hill sisters, 1893", "Irmas Hill, 1893",
        "Soeurs Hill, 1893", 150, birthday),
    MELODY("Ode to Joy", "Beethoven, 1824", "Beethoven, 1824",
        "Beethoven, 1824", 125, ode),
    MELODY("Fur Elise", "Beethoven, 1810", "Beethoven, 1810",
        "Beethoven, 1810", 140, elise),
    MELODY("Jingle Bells", "Pierpont, 1857", "Pierpont, 1857",
        "Pierpont, 1857", 95, jingle),
    MELODY("Twinkle Twinkle", "French tune, 1761", "Franca, 1761",
        "Air francais, 1761", 140, twinkle),
    MELODY("Korobeiniki", "Russian folk song", "Cancao russa",
        "Chant russe", 100, korobeiniki),
    MELODY("A Portuguesa", "Keil, 1890", "Keil, 1890", "Keil, 1890", 160,
        portuguesa),
    MELODY("Sugar Plum Fairy", "Tchaikovsky, 1892", "Tchaikovsky, 1892",
        "Tchaikovsky, 1892", 160, sugarplum),
    MELODY("The Entertainer", "Joplin, 1902", "Joplin, 1902",
        "Joplin, 1902", 160, entertainer),
    MELODY("Mountain King", "Grieg, 1875", "Grieg, 1875", "Grieg, 1875", 65,
        mountain),
    MELODY("Beethoven's 5th", "Beethoven, 1808", "Beethoven, 1808",
        "Beethoven, 1808", 70, fifth),
    MELODY("William Tell", "Rossini, 1829", "Rossini, 1829",
        "Rossini, 1829", 60, tell),
    MELODY("Can-Can", "Offenbach, 1858", "Offenbach, 1858",
        "Offenbach, 1858", 75, cancan),
    MELODY("Wedding March", "Mendelssohn, 1842", "Mendelssohn, 1842",
        "Mendelssohn, 1842", 38, wedding),
    MELODY("Swan Lake", "Tchaikovsky, 1876", "Tchaikovsky, 1876",
        "Tchaikovsky, 1876", 120, swan),
    MELODY("Merry Christmas", "English carol", "Cancao inglesa",
        "Chant anglais", 90, wish),
    MELODY("La Marseillaise", "Rouget de Lisle", "Rouget de Lisle",
        "Rouget de Lisle", 130, marseillaise),
};
#define MELODIES (int)(sizeof melodies / sizeof *melodies)

static void melody_draw(int m, int i)
{
    char a[32];
    melody_t const *mel = &melodies[m];
    int midi = mel->notes[i].midi;

    dclear(C_WHITE);
    if (midi) sprintf(a, "%s%d", note_names[midi % 12], midi / 12 - 1);
    else strcpy(a, "-");
#if GINT_HW_FX
    big_text(DWIDTH / 2, 10, midi ? C_BLACK : C_LIGHT, a, 1);
    info_title(mel->name, MELODY_COLOR);
    dtext(1, 26, C_DARK, midi ? fmt_hz(a, midi_freq(midi)) : L("rest",
        "pausa", "silence"));
    dprint_opt(DWIDTH - 1, 26, C_DARK, C_NONE, DTEXT_RIGHT, DTEXT_TOP,
        "%d/%d", i + 1, mel->count);
    drect_border(0, 35, DWIDTH - 1, 41, C_WHITE, 1, C_BLACK);
    drect(1, 36, 1 + (DWIDTH - 3) * (i + 1) / mel->count, 40, C_DARK);
    if (message) dtext(1, 46, C_BLACK, message);
    else dtext(1, 46, C_BLACK, L("UP/DOWN: tune", "CIMA/BX: musica",
        "HAUT/BAS: air"));
    dtext(1, 55, C_DARK, L("Other key: stop", "Outra tecla: parar",
        "Autre touche: arret"));
#else
    char line[96];
    big_text(DWIDTH / 2, 30, midi ? C_BLACK : C_LIGHT, a, 3);
    info_title(mel->name, MELODY_COLOR);
    dtext_opt(DWIDTH / 2, 86, C_DARK, C_NONE, DTEXT_CENTER, DTEXT_TOP,
        midi ? fmt_hz(a, midi_freq(midi)) : L("rest", "pausa", "silence"));

    int x1 = 20, x2 = DWIDTH - 21, y = 108;
    int x = x1 + 1 + (x2 - x1 - 2) * (i + 1) / mel->count;
    drect_border(x1, y, x2, y + 14, C_WHITE, 1, C_DARK);
    drect(x1 + 1, y + 1, x, y + 13, C_RGB(31, 20, 24));
    sprintf(line, L("Note %d of %d", "Nota %d de %d", "Note %d sur %d"),
        i + 1, mel->count);
    dtext(x1, y + 20, C_DARK, line);
    sprintf(line, L("Wave: %s", "Onda: %s", "Onde: %s"), wave_name(cfg.wave));
    dtext_opt(x2, y + 20, C_DARK, C_NONE, DTEXT_RIGHT, DTEXT_TOP, line);

    if (message) dtext(10, 160, C_RED, message);
    else {
        drect(10, 161, 17, 168, PLAY_COLOR);
        sprintf(line, L("Playing on the 3-pin port (%s)",
            "A tocar na porta de 3 pinos (%s)",
        "Lecture sur le port 3 broches (%s)"), method_name(cfg.method));
        dtext(22, 160, PLAY_COLOR, line);
    }
    dtext(10, 180, C_DARK, L("UP/DOWN: previous/next tune    Any key: stop",
        "CIMA/BAIXO: musica anterior/seguinte    Outra tecla: parar",
        "HAUT/BAS: air precedent/suivant    Autre touche: arret"));
#endif
    dupdate();
}

/* Play melody m with the tone's wave, duty and volume. UP/DOWN goes to the
   previous/next one; any other key stops. Returns the last melody played */
static int melody_run(int m)
{
    int last = m;
    message = NULL;
    port_ready();

    while (m >= 0) {
        melody_t const *mel = &melodies[m];
        int next = -1;
        for (int i = 0; i < mel->count && next < 0; i++) {
            note_t n = mel->notes[i];
            int ms = n.len * mel->unit_ms;
            /* A short silence between notes, so repeated ones are heard */
            int gap = n.midi ? (ms >= 120 ? 30 : ms / 4) : 0;

            audio_silence();
            melody_draw(m, i);
            if (n.midi && audio_is_open())
                audio_play(midi_freq(n.midi), cfg.wave, cfg.duty, cfg.volume);

            key_event_t ev = getkey_ev_for((ms - gap) * 1000);
            if (ev.type == KEYEV_NONE && gap) {
                audio_silence();
                ev = getkey_ev_for(gap * 1000);
            }
            if (ev.type == KEYEV_NONE) continue;
            if (ev.key == KEY_UP) next = (m + MELODIES - 1) % MELODIES;
            else if (ev.key == KEY_DOWN) next = (m + 1) % MELODIES;
            else next = MELODIES;
        }
        m = next < MELODIES ? next : -1;
        if (m >= 0) last = m;
    }
    audio_silence();
    message = NULL;
    return last;
}

#if !GINT_HW_FX
/* "0:13": how long a melody plays */
static char *fmt_length(char *buf, melody_t const *mel)
{
    int ms = 0;
    for (int i = 0; i < mel->count; i++) ms += mel->notes[i].len * mel->unit_ms;
    int s = (ms + 500) / 1000;
    sprintf(buf, "%d:%02d", s / 60, s % 60);
    return buf;
}
#endif

#if GINT_HW_FX
#define MEL_ROWS 5
#else
#define MEL_ROWS 9
#endif

/* top: the first melody shown; the list scrolls to keep sel in view */
static void melodies_draw(int sel, int top)
{
    char line[64];
    dclear(C_WHITE);
    info_title(L("Melodies", "Melodias", "Melodies"), MELODY_COLOR);

    for (int k = 0; k < MEL_ROWS && top + k < MELODIES; k++) {
        int i = top + k;
        melody_t const *mel = &melodies[i];
        bool on = i == sel;
#if GINT_HW_FX
        int y = 10 + 9 * k;
        if (on) drect(0, y - 1, DWIDTH - 4, y + 7, C_BLACK);
        dprint(2, y, on ? C_WHITE : C_DARK, "%d", i + 1);
        dtext(14, y, on ? C_WHITE : C_BLACK, mel->name);
#else
        int y = 22 + 18 * k;
        if (on) drect(0, y - 3, DWIDTH - 8, y + 13, C_RGB(31, 26, 28));
        dprint_opt(28, y, on ? MELODY_COLOR : C_DARK, C_NONE, DTEXT_RIGHT,
            DTEXT_TOP, "%d", i + 1);
        dtext(40, y, on ? MELODY_COLOR : C_BLACK, mel->name);
        dtext(200, y, C_DARK, L(mel->origin_en, mel->origin_pt,
            mel->origin_fr));
        char a[16];
        dtext_opt(DWIDTH - 14, y, C_DARK, C_NONE, DTEXT_RIGHT, DTEXT_TOP,
            fmt_length(a, mel));
#endif
    }

    /* Scroll bar */
#if GINT_HW_FX
    int y1 = 9, y2 = 53, x = DWIDTH - 2;
#else
    int y1 = 19, y2 = 180, x = DWIDTH - 4;
#endif
    int h = y2 - y1;
    drect(x, y1, x + (GINT_HW_FX ? 1 : 2), y2, C_LIGHT);
    drect(x, y1 + h * top / MELODIES, x + (GINT_HW_FX ? 1 : 2),
        y1 + h * (top + MEL_ROWS) / MELODIES - 1, MELODY_COLOR);

#if GINT_HW_FX
    dtext(1, 56, C_DARK, L("EXE: play", "EXE: tocar", "EXE: jouer"));
    static char const *const shrt[] = { "SQR", "SIN", "TRI", "SAW" };
    sprintf(line, "<> %s", shrt[cfg.wave]);
    dtext_opt(DWIDTH - 1, 56, C_DARK, C_NONE, DTEXT_RIGHT, DTEXT_TOP, line);
#else
    drect(0, 183, DWIDTH - 1, 183, C_LIGHT);
    sprintf(line, L("Wave: %s", "Onda: %s", "Onde: %s"), wave_name(cfg.wave));
    if (cfg.wave == WAVE_SQUARE)
        sprintf(line + strlen(line), L(", %d%% duty", ", ciclo %d%%",
            ", rapport %d%%"), cfg.duty);
    dtext_opt(DWIDTH - 10, 190, C_DARK, C_NONE, DTEXT_RIGHT, DTEXT_TOP, line);
    dtext(10, 190, C_DARK, L("EXE: play", "EXE: tocar", "EXE: jouer"));
    dtext(10, 205, C_DARK, L("UP/DOWN: choose  LEFT/RIGHT: wave  EXIT: back",
        "CIMA/BAIXO: escolher  ESQ/DIR: onda  EXIT: sair",
        "HAUT/BAS: choisir  G/D: onde  EXIT: retour"));
#endif
    dupdate();
}

static void melodies_run(void)
{
    static int sel = 0, top = 0;
    message = NULL;

    while (1) {
        if (sel < top) top = sel;
        if (sel >= top + MEL_ROWS) top = sel - MEL_ROWS + 1;
        melodies_draw(sel, top);
        int key = getkey().key;

        if (key == KEY_EXIT) break;
        if (key == INFO_KEY)
            info_app(L("Melodies " APPS_VERSION, "Melodias " APPS_VERSION,
                "Melodies " APPS_VERSION),
                MELODY_COLOR);
        else if (key == KEY_UP) sel = (sel + MELODIES - 1) % MELODIES;
        else if (key == KEY_DOWN) sel = (sel + 1) % MELODIES;
        else if (key == KEY_LEFT) cfg.wave = (cfg.wave + WAVES - 1) % WAVES;
        else if (key == KEY_RIGHT) cfg.wave = (cfg.wave + 1) % WAVES;
        else if (key == KEY_EXE) sel = melody_run(sel);
    }
    audio_silence();
}

//---
//  Tone generator
//---

static double const steps[] = { 0.1, 1, 10, 100, 1000 };
#define STEPS (int)(sizeof steps / sizeof *steps)

static void tone_fkeys(void)
{
    char label[16];
    static char const *const cg[] = { "SQUARE", "SINE", "TRI", "SAW" };
    static char const *const fx[] = { "SQR", "SIN", "TRI", "SAW" };
    for (int w = 0; w < WAVES; w++) {
        char const *s = GINT_HW_FX ? fx[w] : cg[w];
        if (w == cfg.wave) fkey_button_color(w + 1, s, COLOR);
        else fkey_button(w + 1, s);
    }
    if (cfg.wave == WAVE_SQUARE) {
        sprintf(label, FK(L("DUTY %d%%", "CICLO %d%%",
            "CYCLE %d%%"), "%d%%"), cfg.duty);
        fkey_button(5, label);
    }
    sprintf(label, FK("VOL %d%%", "VOL"), cfg.volume);
    fkey_button(6, label);
}

static void tone_draw(int step, numfield_t const *typing)
{
    char a[32], b[32], c[32], line[96];
    double f = cfg.freq;

    dclear(C_WHITE);
#if GINT_HW_FX
    if (!typing) big_text(DWIDTH / 2, 10, C_BLACK, fmt_hz(a, f), 1);
    info_title(L("Tone generator", "Gerador de tons", "Generateur de son"),
        COLOR);
    if (typing) {
        dtext(1, 13, C_BLACK, L("Freq:", "Freq:", "Freq:"));
        numfield_draw(typing, 30, 12, 76, true, COLOR);
        dtext(108, 13, C_DARK, "Hz");
        dtext(1, 26, C_DARK, L("EXE: set  EXIT: back",
            "EXE: ok  EXIT: sair", "EXE: ok  EXIT: retour"));
    }
    else {
        dtext(1, 26, C_BLACK, fmt_note(a, f, true));
        dtext_opt(DWIDTH - 1, 26, C_DARK, C_NONE, DTEXT_RIGHT, DTEXT_TOP,
            fmt_si(b, 1 / f, "s", 3));
    }
    draw_wave(0, 34, DWIDTH - 1, 46, cfg.wave, C_BLACK);
    draw_status(1, 48, L("EXE: play", "EXE: tocar", "EXE: jouer"));
    if (!message && !audio_playing()) {
        sprintf(line, "+-%s", fmt_num(c, steps[step], 1));
        dtext_opt(DWIDTH - 1, 48, C_DARK, C_NONE, DTEXT_RIGHT, DTEXT_TOP, line);
    }
#else
    if (!typing) big_text(DWIDTH / 2, 30, C_BLACK, fmt_hz(a, f), 3);
    info_title(L("Tone generator", "Gerador de tons", "Generateur de son"),
        COLOR);
    if (typing) {
        dtext(30, 47, COLOR, L("New frequency", "Nova frequencia",
            "Nouv. frequence"));
        numfield_draw(typing, 150, 44, 140, true, COLOR);
        dtext(298, 47, C_DARK, "Hz");
        dtext_opt(DWIDTH / 2, 68, C_DARK, C_NONE, DTEXT_CENTER, DTEXT_TOP,
            L("EXE: set    EXIT: cancel", "EXE: definir    EXIT: cancelar",
            "EXE: valider    EXIT: annuler"));
    }
    dtext(10, 84, COLOR, fmt_note(a, f, false));
    sprintf(line, "%s", wave_name(cfg.wave));
    if (cfg.wave == WAVE_SQUARE)
        sprintf(line + strlen(line), L(", %d%% duty", ", ciclo %d%%",
            ", rapport %d%%"), cfg.duty);
    dtext_opt(DWIDTH - 10, 84, C_DARK, C_NONE, DTEXT_RIGHT, DTEXT_TOP, line);
    sprintf(line, L("Period %s", "Periodo %s", "Periode %s"),
        fmt_si(b, 1 / f, "s", 4));
    dtext(10, 98, C_DARK, line);
    sprintf(line, L("Wavelength in air %s", "Comprimento no ar %s",
        "Long. d'onde (air) %s"),
        fmt_si(c, SOUND_SPEED / f, "m", 3));
    dtext_opt(DWIDTH - 10, 98, C_DARK, C_NONE, DTEXT_RIGHT, DTEXT_TOP, line);

    draw_wave(10, 114, DWIDTH - 11, 151, cfg.wave, COLOR);

    draw_status(10, 157, L("Stopped.  EXE: play", "Parado.  EXE: tocar",
        "Arrete.  EXE: jouer"));
    sprintf(line, L("Step %s", "Passo %s", "Pas %s"), fmt_hz(a, steps[step]));
    dtext_opt(DWIDTH - 10, 157, C_DARK, C_NONE, DTEXT_RIGHT, DTEXT_TOP, line);
    dtext(10, 174, C_DARK, L("UP/DOWN: +/- step    LEFT/RIGHT: step size",
        "CIMA/BAIXO: +/- passo    ESQ/DIR: passo",
        "HAUT/BAS: +/- pas    G/D: taille du pas"));
    dtext(10, 188, C_DARK, L("+/-: semitone    0-9: type a frequency",
        "+/-: meio-tom    0-9: escrever a frequencia",
        "+/-: demi-ton    0-9: saisir une frequence"));
#endif
    tone_fkeys();
    dupdate();
}

static void tone_run(void)
{
    static int step = 2;
    numfield_t typed;
    bool typing = false;
    message = NULL;

    while (1) {
        tone_draw(step, typing ? &typed : NULL);
        int key = getkey().key;
        message = NULL;
        bool changed = false;

        if (typing) {
            if (key == KEY_EXIT) typing = false;
            else if (key == KEY_EXE) {
                double v = numfield_value(&typed);
                if (v >= FREQ_MIN && v <= FREQ_MAX) {
                    cfg.freq = v;
                    changed = true;
                    typing = false;
                }
                else message = L("From 20 Hz to 20000 Hz",
                    "De 20 Hz a 20000 Hz", "De 20 Hz a 20000 Hz");
            }
            else numfield_key(&typed, key);
        }
        else if (key == KEY_EXIT) {
            audio_silence();
            return;
        }
        else if (key == INFO_KEY)
            info_app(L("Tone generator " APPS_VERSION,
                "Gerador de tons " APPS_VERSION,
                "Generateur de son " APPS_VERSION), COLOR);
        else if (key == KEY_EXE) {
            if (audio_playing()) audio_silence();
            else play(cfg.freq);
        }
        else if (key == KEY_UP || key == KEY_DOWN) {
            /* Back on the step's grid, then one step */
            double s = steps[step];
            double f = floor(cfg.freq / s + 0.5) * s;
            cfg.freq = clamp_freq(f + (key == KEY_UP ? s : -s));
            changed = true;
        }
        else if (key == KEY_LEFT && step < STEPS - 1) step++;
        else if (key == KEY_RIGHT && step > 0) step--;
        else if (key == KEY_PLUS || key == KEY_MINUS) {
            cfg.freq = clamp_freq(cfg.freq * pow(2, (key == KEY_PLUS ? 1 : -1)
                / 12.0));
            changed = true;
        }
        else if (key >= KEY_F1 && key <= KEY_F4) {
            cfg.wave = key - KEY_F1;
            changed = true;
        }
        else if (key == KEY_F5 && cfg.wave == WAVE_SQUARE) {
            int i = 0;
            while (i < DUTIES && duties[i] != cfg.duty) i++;
            cfg.duty = duties[(i + 1) % DUTIES];
            changed = true;
        }
        else if (key == KEY_F6) {
            int i = 0;
            while (i < VOLUMES && volumes[i] != cfg.volume) i++;
            cfg.volume = volumes[(i + 1) % VOLUMES];
            changed = true;
        }
        else if (key_digit(key) >= 0 || key == KEY_DOT) {
            memset(&typed, 0, sizeof typed);
            numfield_key(&typed, key);
            typing = true;
        }

        if (changed && audio_playing()) play(cfg.freq);
    }
}

//---
//  Piano: notes while their key is held
//---

/* White keys on 1-8 (C to C), black keys on F1-F5 */
static int const white_keys[8] = { KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6,
    KEY_7, KEY_8 };
static int const white_semis[8] = { 0, 2, 4, 5, 7, 9, 11, 12 };
static int const black_keys[5] = { KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5 };
static int const black_semis[5] = { 1, 3, 6, 8, 10 };
/* A black key sits after this white key */
static int const black_after[5] = { 0, 1, 3, 4, 5 };

#if GINT_HW_FX
#define PK_X 4
#define PK_Y 19
#define PK_W 15
#define PK_H 28
#define PK_BW 9
#define PK_BH 16
#else
#define PK_X 22
#define PK_Y 58
#define PK_W 44
#define PK_H 110
#define PK_BW 26
#define PK_BH 66
#endif

/* semi: the key held (-1: none); last: the last note played, as a MIDI
   number (-1: none yet), shown until the next one */
static void piano_draw(int octave, int semi, int last)
{
    char a[32], line[64];
    dclear(C_WHITE);
    info_title("Piano", PIANO_COLOR);

    for (int k = 0; k < 8; k++) {
        int x = PK_X + k * PK_W;
        bool on = semi == white_semis[k];
        drect_border(x, PK_Y, x + PK_W, PK_Y + PK_H, on ? (GINT_HW_FX ? C_LIGHT
            : C_RGB(20, 26, 31)) : C_WHITE, 1, C_BLACK);
        dprint_opt(x + PK_W / 2 + 1, PK_Y + PK_H - (GINT_HW_FX ? 8 : 16),
            on ? PIANO_COLOR : C_DARK, C_NONE, DTEXT_CENTER, DTEXT_TOP,
            "%d", k + 1);
    }
    for (int k = 0; k < 5; k++) {
        int x = PK_X + (black_after[k] + 1) * PK_W - PK_BW / 2;
        bool on = semi == black_semis[k];
        drect(x, PK_Y, x + PK_BW, PK_Y + PK_BH, on ? (GINT_HW_FX ? C_DARK
            : PIANO_COLOR) : C_BLACK);
#if !GINT_HW_FX
        dprint_opt(x + PK_BW / 2 + 1, PK_Y + PK_BH - 14, C_WHITE, C_NONE,
            DTEXT_CENTER, DTEXT_TOP, "F%d", k + 1);
#endif
    }

    if (last >= 0) sprintf(line, "%s%d  %s", note_names[last % 12],
        last / 12 - 1, fmt_hz(a, midi_freq(last)));
#if GINT_HW_FX
    if (last < 0) strcpy(line, L("UP/DOWN: octave",
        "CIMA/BX: oitava", "HAUT/BAS: octave"));
    dtext(1, 10, semi >= 0 ? C_BLACK : C_DARK, line);
    dprint_opt(DWIDTH - 1, 10, C_DARK, C_NONE, DTEXT_RIGHT, DTEXT_TOP,
        L("Oct %d", "Oit %d", "Oct %d"), octave);
    if (message) dtext(1, 48, C_BLACK, message);
    /* No room for labels on the black keys: the F-keys that play them
       carry their notes, the one held shown selected */
    for (int k = 0; k < 5; k++)
        fkey_button_color(k + 1, note_names[black_semis[k]],
            semi == black_semis[k] ? PIANO_COLOR : C_BLACK);
    fkey_button(6, "WAV");
#else
    sprintf(line, L("Octave %d (C%d to C%d)    UP/DOWN: change",
        "Oitava %d (C%d a C%d)    CIMA/BAIXO: mudar",
        "Octave %d (C%d a C%d)    HAUT/BAS: changer"), octave, octave,
        octave + 1);
    dtext(10, 22, C_DARK, line);
    if (last >= 0) {
        char note[64];
        sprintf(note, "%s%d   %s", note_names[last % 12], last / 12 - 1,
            fmt_hz(a, midi_freq(last)));
        dtext(10, 37, semi >= 0 ? PIANO_COLOR : C_DARK, note);
    }
    sprintf(line, L("Wave: %s", "Onda: %s", "Onde: %s"), wave_name(cfg.wave));
    dtext_opt(DWIDTH - 10, 37, C_DARK, C_NONE, DTEXT_RIGHT, DTEXT_TOP, line);
    if (message) dtext(10, 176, C_RED, message);
    else dtext(10, 176, C_DARK, L("Hold 1-8: white keys    F1-F5: black keys",
        "Mantenha 1-8: brancas    F1-F5: pretas",
        "Maintenir 1-8: blanches    F1-F5: noires"));
    dtext(10, 190, C_DARK, L("F6: waveform    EXIT: back",
        "F6: forma de onda    EXIT: voltar",
        "F6: forme d'onde    EXIT: retour"));
    fkey_button(6, L("WAVE", "ONDA", "ONDE"));
#endif
    dupdate();
}

/* The note whose key is held, as semitones above C; -1 if none */
static int held_note(void)
{
    for (int k = 0; k < 5; k++) if (keydown(black_keys[k])) return black_semis[k];
    for (int k = 0; k < 8; k++) if (keydown(white_keys[k])) return white_semis[k];
    return -1;
}

static void piano_run(void)
{
    static int octave = 4, last = -1;
    int semi = -1, drawn_semi = -2, drawn_octave = -1, drawn_wave = -1;
    /* Without the port the keys still show the notes */
    message = NULL;
    port_ready();

    while (1) {
        key_event_t ev;
        bool quit = false;
        while ((ev = pollevent()).type != KEYEV_NONE) {
            if (ev.type != KEYEV_DOWN) continue;
            if (ev.key == KEY_EXIT) quit = true;
            else if (ev.key == KEY_UP && octave < 7) octave++;
            else if (ev.key == KEY_DOWN && octave > 1) octave--;
            else if (ev.key == KEY_F6) cfg.wave = (cfg.wave + 1) % WAVES;
            else if (ev.key == INFO_KEY) {
                audio_silence();
                info_app(L("Piano " APPS_VERSION, "Piano " APPS_VERSION,
                    "Piano " APPS_VERSION),
                    PIANO_COLOR);
                drawn_semi = -2;
            }
        }
        if (quit) break;

        int now = held_note();
        if (now != semi || octave != drawn_octave || cfg.wave != drawn_wave) {
            if (now < 0) audio_silence();
            else if (audio_is_open())
                audio_play(midi_freq(12 * (octave + 1) + now), cfg.wave,
                    cfg.duty, cfg.volume);
            semi = now;
            if (now >= 0) last = 12 * (octave + 1) + now;
        }
        if (semi != drawn_semi || octave != drawn_octave
            || cfg.wave != drawn_wave) {
            piano_draw(octave, semi, last);
            drawn_semi = semi;
            drawn_octave = octave;
            drawn_wave = cfg.wave;
        }
        sleep_us(10000);
    }
    audio_silence();
}

//---
//  Sweep
//---

enum { END_STOP, END_REPEAT, END_BOUNCE, ENDS };

static struct {
    double from, to, seconds;
    int log, end;
} sw = { 100, 10000, 5, 1, END_REPEAT };

static char const *end_name(int e)
{
    if (e == END_STOP) return L("stop", "parar", "arreter");
    if (e == END_REPEAT) return L("repeat", "repetir", "repeter");
    return L("go back", "voltar", "retour");
}

static double sweep_freq(double p)
{
    if (sw.log) return sw.from * pow(sw.to / sw.from, p);
    return sw.from + (sw.to - sw.from) * p;
}

static void sweep_results(eform_t *f, int y)
{
    char a[32];
    double from = field_value(&f->fields[0]), to = field_value(&f->fields[1]);
    double t = field_value(&f->fields[2]);
    if (from >= FREQ_MIN && to >= FREQ_MIN && from <= FREQ_MAX
        && to <= FREQ_MAX && t > 0) {
        if (f->fields[3].choice)
            result(y, L("Rate", "Ritmo", "Vitesse"), C_BLACK, L("%s octaves/s",
                "%s oitavas/s", "%s octaves/s"),
                fmt_num(a, fabs(log(to / from) / log(2)) / t, 2));
        else result(y, L("Rate", "Ritmo", "Vitesse"), C_BLACK, "%s/s",
            fmt_hz(a, fabs(to - from) / t));
    }
    else result(y, L("Range", "Intervalo", "Plage"), C_DARK, "20 Hz - 20 kHz");
    result_note(y + 2 * RESULT_STEP, L("EXIT: back", "EXIT: voltar",
        "EXIT: retour"));
}

static void sweep_edit(void)
{
    static field_t fields[5];
    static char const *scales[2], *ends[ENDS];
    memset(fields, 0, sizeof fields);
    scales[0] = L("linear", "linear", "lineaire");
    scales[1] = L("logarithmic", "logaritmica",
        "logarithmique");
    for (int e = 0; e < ENDS; e++) ends[e] = end_name(e);

    fields[0] = (field_t){ .kind = F_NUM, .label = L("From", "De", "De"),
        .unit = "Hz" };
    fields[1] = (field_t){ .kind = F_NUM, .label = L("To", "Ate", "A"),
        .unit = "Hz" };
    fields[2] = (field_t){ .kind = F_NUM, .label = L("Time", "Tempo", "Duree"),
        .unit = "s" };
    fields[3] = (field_t){ .kind = F_CHOICE, .label = L("Scale", "Escala",
        "Echelle"),
        .options = scales, .option_count = 2, .choice = sw.log };
    fields[4] = (field_t){ .kind = F_CHOICE, .label = L("At the end",
        "No fim", "A la fin"),
        .options = ends, .option_count = ENDS, .choice = sw.end };
    field_set(&fields[0], sw.from);
    field_set(&fields[1], sw.to);
    field_set(&fields[2], sw.seconds);

    eform_t f = { .title = L("Sweep settings", "Definicoes do varrimento",
        "Reglages du balayage"),
        .color = SWEEP_COLOR, .fields = fields, .count = 5, .cols = 1,
        .label_w = 110, .results = sweep_results };
    eform_run(&f);

    double from = field_value(&fields[0]), to = field_value(&fields[1]);
    double t = field_value(&fields[2]);
    if (from >= FREQ_MIN && from <= FREQ_MAX) sw.from = from;
    if (to >= FREQ_MIN && to <= FREQ_MAX) sw.to = to;
    if (t >= 0.1 && t <= 3600) sw.seconds = t;
    sw.log = fields[3].choice;
    sw.end = fields[4].choice;
}

/* Elapsed RTC ticks (128 per second), across midnight */
static uint32_t ticks_since(uint32_t start)
{
    uint32_t now = rtc_ticks();
    return now >= start ? now - start : now + 128u * 86400 - start;
}

static void sweep_draw(double p, double f)
{
    char a[32], b[32], line[96];
    dclear(C_WHITE);
#if GINT_HW_FX
    big_text(DWIDTH / 2, 10, C_BLACK, fmt_hz(a, f), 1);
    info_title(L("Sweep", "Varrimento", "Balayage"), SWEEP_COLOR);
    drect_border(0, 27, DWIDTH - 1, 33, C_WHITE, 1, C_BLACK);
    drect(1, 28, 1 + (int)(p * (DWIDTH - 3)), 32, C_DARK);
    sprintf(line, "%s > %s", fmt_short_hz(a, sw.from), fmt_short_hz(b, sw.to));
    dtext(1, 36, C_DARK, line);
    dtext_opt(DWIDTH - 1, 36, C_DARK, C_NONE, DTEXT_RIGHT, DTEXT_TOP,
        sw.log ? "log" : "lin");
    draw_status(1, 46, L("EXE: start", "EXE: iniciar", "EXE: lancer"));
    fkey_button(1, L("SET", "DEF", "REG"));
#else
    big_text(DWIDTH / 2, 30, C_BLACK, fmt_hz(a, f), 3);
    info_title(L("Sweep", "Varrimento", "Balayage"), SWEEP_COLOR);
    dtext_opt(DWIDTH / 2, 86, C_DARK, C_NONE, DTEXT_CENTER, DTEXT_TOP,
        fmt_note(a, f, false));

    int x1 = 20, x2 = DWIDTH - 21, y = 108;
    drect_border(x1, y, x2, y + 14, C_WHITE, 1, C_DARK);
    drect(x1 + 1, y + 1, x1 + 1 + (int)(p * (x2 - x1 - 2)), y + 13,
        C_RGB(31, 24, 14));
    int mx = x1 + 1 + (int)(p * (x2 - x1 - 2));
    drect(mx - 1, y - 3, mx + 1, y + 17, SWEEP_COLOR);
    dtext(x1, y + 20, C_DARK, fmt_hz(a, sw.from));
    dtext_opt(x2, y + 20, C_DARK, C_NONE, DTEXT_RIGHT, DTEXT_TOP,
        fmt_hz(b, sw.to));
    sprintf(line, L("%s s, %s, then %s", "%s s, %s, depois %s",
        "%s s, %s, puis %s"),
        fmt_num(a, sw.seconds, 2), sw.log ? L("logarithmic", "logaritmico",
        "logarithmique") : L("linear", "linear", "lineaire"),
        end_name(sw.end));
    dtext_opt(DWIDTH / 2, y + 20, C_DARK, C_NONE, DTEXT_CENTER, DTEXT_TOP, line);

    draw_status(10, 160, L("Stopped.  EXE: start", "Parado.  EXE: iniciar",
        "Arrete.  EXE: lancer"));
    sprintf(line, L("Wave: %s", "Onda: %s", "Onde: %s"), wave_name(cfg.wave));
    dtext_opt(DWIDTH - 10, 160, C_DARK, C_NONE, DTEXT_RIGHT, DTEXT_TOP, line);
    dtext(10, 180, C_DARK, L("F1: settings    F6: waveform    EXIT: back",
        "F1: definicoes    F6: forma de onda    EXIT: voltar",
        "F1: reglages    F6: forme d'onde    EXIT: retour"));
    fkey_button(1, L("SET", "DEFIN.", "REGL."));
#endif
    fkey_button(6, FK(L("WAVE", "ONDA", "ONDE"), "WAV"));
    dupdate();
}

static void sweep_run(void)
{
    bool running = false;
    uint32_t start = 0;
    double p = 0;
    message = NULL;

    while (1) {
        if (running) {
            double t = ticks_since(start) / 128.0 / sw.seconds;
            if (sw.end == END_STOP && t >= 1) {
                p = 1;
                running = false;
                audio_silence();
            }
            else if (sw.end == END_BOUNCE) {
                t = fmod(t, 2);
                p = t > 1 ? 2 - t : t;
            }
            else p = fmod(t, 1);
            if (running) audio_play(sweep_freq(p), cfg.wave, cfg.duty,
                cfg.volume);
        }
        sweep_draw(p, sweep_freq(p));

        int key = running ? getkey_for(20000) : getkey().key;
        if (!key) continue;
        message = NULL;

        if (key == KEY_EXIT) break;
        if (key == INFO_KEY) {
            info_app(L("Sweep " APPS_VERSION, "Varrimento " APPS_VERSION,
                "Balayage " APPS_VERSION),
                SWEEP_COLOR);
            if (running) start = rtc_ticks() - (uint32_t)(p * sw.seconds * 128);
        }
        else if (key == KEY_EXE) {
            if (running) {
                running = false;
                audio_silence();
            }
            else if (port_ready()) {
                running = true;
                p = 0;
                start = rtc_ticks();
            }
        }
        else if (key == KEY_F1) {
            running = false;
            audio_silence();
            sweep_edit();
            p = 0;
        }
        else if (key == KEY_F6) cfg.wave = (cfg.wave + 1) % WAVES;
    }
    audio_silence();
}

//---
//  Output and wiring
//---

static void output_draw(void)
{
    dclear(C_WHITE);
    info_title(FK(L("Output and wiring", "Saida e ligacoes",
        "Sortie et cablage"), L("Output", "Saida", "Sortie")), OUT_COLOR);
#if GINT_HW_FX
    dtext(1, 10, C_BLACK, cfg.method == OUT_UART ? L("> UART 115200 bd",
        "> UART 115200 bd", "> UART 115200 bd")
        : L("> TX pin (SCSPTR)", "> Pino TX (SCSPTR)",
        "> Broche TX (SCSPTR)"));
    dtext(1, 19, C_DARK, L("Tip TX  Ring RX", "Ponta TX  Anel RX",
        "Pointe TX  Bague RX"));
    dtext(1, 28, C_DARK, L("Sleeve GND (0 V)", "Manga GND (0 V)",
        "Corps GND (0 V)"));
    dtext(1, 37, C_DARK, L("TX-1k-headphones", "TX-1k-auscultad.",
        "TX-1k-casque"));
    draw_status(1, 47, L("F6: test 440 Hz", "F6: teste 440 Hz",
        "F6: test 440 Hz"));
    fkey_button_color(1, "URT", cfg.method == OUT_UART ? OUT_COLOR : C_BLACK);
    fkey_button_color(2, L("PIN", "PIN", "BRO"),
        cfg.method == OUT_PIN ? OUT_COLOR : C_BLACK);
    fkey_button(6, L("TST", "TST", "TST"));
#else
    static char const *const names_en[] = { "UART stream", "TX pin" };
    static char const *const names_pt[] = { "Fluxo UART", "Pino TX" };
    static char const *const names_fr[] = { "Flux UART", "Broche TX" };
    static char const *const desc_en[] = {
        "Bytes at 115200 baud, each bit one sample",
        "The pin set high and low: exact square waves" };
    static char const *const desc_pt[] = {
        "Bytes a 115200 baud, cada bit uma amostra",
        "O pino ligado e desligado: ondas quadradas exatas" };
    static char const *const desc_fr[] = {
        "Octets a 115200 bauds, un bit par echantillon",
        "Broche haute et basse: ondes carrees exactes" };
    for (int m = 0; m < OUTS; m++) {
        int y = 22 + 28 * m;
        bool sel = m == (int)cfg.method;
        if (sel) drect(0, y - 2, DWIDTH - 1, y + 24, C_RGB(28, 29, 31));
        dcircle(16, y + 6, 5, C_WHITE, sel ? OUT_COLOR : C_DARK);
        if (sel) dcircle(16, y + 6, 2, OUT_COLOR, C_NONE);
        dprint(28, y, sel ? OUT_COLOR : C_BLACK, "F%d  %s", m + 1,
            L(names_en[m], names_pt[m], names_fr[m]));
        dtext(48, y + 12, C_DARK, L(desc_en[m], desc_pt[m], desc_fr[m]));
    }

    /* The 2.5 mm plug, tip to the right */
    int y = 86, metal = C_RGB(26, 26, 27);
    drect(20, y, 60, y + 16, C_RGB(18, 18, 20));                  /* body */
    drect(61, y + 3, 190, y + 13, metal);                         /* sleeve */
    drect(191, y + 3, 195, y + 13, C_BLACK);
    drect(196, y + 3, 250, y + 13, metal);                        /* ring */
    drect(251, y + 3, 255, y + 13, C_BLACK);
    drect(256, y + 3, 326, y + 13, metal);                        /* tip */
    dline(327, y + 4, 332, y + 8, C_DARK);
    dline(327, y + 12, 332, y + 8, C_DARK);
    dtext_opt(125, y + 20, C_DARK, C_NONE, DTEXT_CENTER, DTEXT_TOP,
        L("Sleeve: GND", "Manga: GND", "Corps: GND"));
    dtext_opt(223, y + 20, C_DARK, C_NONE, DTEXT_CENTER, DTEXT_TOP,
        L("Ring: RX", "Anel: RX", "Bague: RX"));
    dtext_opt(291, y + 20, OUT_COLOR, C_NONE, DTEXT_CENTER, DTEXT_TOP,
        L("Tip: TX", "Ponta: TX", "Pointe: TX"));

    etext(10, 130, C_BLACK, DTEXT_LEFT, false, L("Tip (TX) -- 1 k" S_OHM
        " -- headphones or amplifier", "Ponta (TX) -- 1 k" S_OHM
        " -- auscultadores ou amplif.", "Pointe (TX) -- 1 k" S_OHM
        " -- casque ou ampli"));
    etext(10, 144, C_BLACK, DTEXT_LEFT, false, L("Sleeve (GND) -- the other "
        "wire", "Manga (GND) -- o outro fio", "Corps (GND) -- l'autre fil"));
    etext(10, 158, C_DARK, DTEXT_LEFT, false, L("3.3 V logic: never a speaker "
        "straight on it.", "Logica de 3,3 V: nunca um altifalante direto.",
        "Logique 3,3 V: jamais de haut-parleur direct."));
    etext(10, 172, C_DARK, DTEXT_LEFT, false, L("Silent? Some cables swap the "
        "tip and the ring.", "Sem som? Alguns cabos trocam ponta e anel.",
        "Muet? Certains cables inversent pointe et bague."));
    draw_status(10, 189, L("F6: test tone (440 Hz)",
        "F6: tom de teste (440 Hz)", "F6: son de test (440 Hz)"));
    fkey_button_color(1, "UART", cfg.method == OUT_UART ? OUT_COLOR : C_BLACK);
    fkey_button_color(2, L("PIN", "PINO", "BROCHE"),
        cfg.method == OUT_PIN ? OUT_COLOR : C_BLACK);
    fkey_button(6, L("TEST", "TESTE", "TEST"));
#endif
    dupdate();
}

static void output_run(void)
{
    message = NULL;
    while (1) {
        output_draw();
        int key = getkey().key;
        message = NULL;
        out_t m = cfg.method;

        if (key == KEY_EXIT) break;
        if (key == INFO_KEY)
            info_app(L("Output " APPS_VERSION, "Saida " APPS_VERSION,
                "Sortie " APPS_VERSION), OUT_COLOR);
        else if (key == KEY_F1 || key == KEY_F2) m = key - KEY_F1;
        else if (key == KEY_UP || key == KEY_DOWN) m = !m;
        else if (key == KEY_F6) {
            if (audio_playing()) audio_silence();
            else if (port_ready())
                audio_play(440, cfg.wave, cfg.duty, cfg.volume);
        }

        if (m != cfg.method) {
            bool was = audio_playing();
            cfg.method = m;
            if (audio_is_open()) audio_open(m);
            if (was) audio_play(440, cfg.wave, cfg.duty, cfg.volume);
        }
    }
    audio_silence();
}

//---
//  Main menu
//---

static menu_item_t const items[] = {
    { "Tone generator", "Gerador de tons", "Generateur de son",
      "Four waveforms, from 20 Hz to 20 kHz",
      "Quatro formas de onda, de 20 Hz a 20 kHz",
      "Quatre formes d'onde, de 20 Hz a 20 kHz", tone_run, COLOR },
    { "Piano", "Piano", "Piano", "Hold the number keys to play notes",
      "Mantenha as teclas numericas para tocar",
      "Maintenez les touches numeriques", piano_run, PIANO_COLOR },
    { "Sweep", "Varrimento", "Balayage", "Glide from one frequency to another",
      "Deslize de uma frequencia para outra",
      "Glisser d'une frequence a une autre", sweep_run, SWEEP_COLOR },
    { "Melodies", "Melodias", "Melodies", "Classics, carols and anthems",
      "Classicos, natal e hinos", "Classiques, Noel et hymnes", melodies_run,
      MELODY_COLOR },
    { "Output and wiring", "Saida e ligacoes", "Sortie et cablage",
      "UART or pin, and how to wire the 3-pin jack",
      "UART ou pino, e como ligar a ficha de 3 pinos",
      "UART ou broche, cabler la prise 3 broches", output_run, OUT_COLOR },
};

static menu_t const menu = { "Tone", "Som", "Son",
    "Tone generator " APPS_VERSION, "Gerador de tons " APPS_VERSION,
    "Generateur de son " APPS_VERSION, COLOR, items,
    (int)(sizeof items / sizeof *items) };

int main(void)
{
    lang_init();
    usb_screen_init();
    screen_init();

    menu_run(&menu);
    audio_close();
    return 1;
}
