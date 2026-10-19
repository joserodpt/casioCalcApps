#include <gint/gint.h>
#include <gint/timer.h>
#include <gint/clock.h>
#include <gint/hardware.h>
#include <gint/config.h>
#include <gint/mpu/scif.h>
#include <math.h>
#include <stdint.h>

#include "audio.h"

#define PI 3.14159265358979323846

//---
//  OS serial driver: it powers the module and routes its pins
//---

#if GINT_HW_FX
#define SYSCALL_ENTRY "0x80010070"
#else
#define SYSCALL_ENTRY "0x80020070"
#endif

/* A syscall stub, as in common/src/i18n.c */
#define SYSCALL(name, number) \
    __asm__(".text\n.weak _" #name "\n.align 1\n_" #name ":\n" \
        "\tmov.l 1f, r2\n\tmov.l 2f, r0\n\tjmp @r2\n\tnop\n" \
        ".align 2\n1:\t.long " SYSCALL_ENTRY "\n2:\t.long " #number "\n")

#if GINT_HW_FX
SYSCALL(Serial_Open, 0x0418);
SYSCALL(Serial_Close, 0x0419);
#else
SYSCALL(Serial_Open, 0x1BB7);
SYSCALL(Serial_Close, 0x1BB8);
#endif

/* mode: 0, baud (9 = 115200), parity (0 none), 8 bits (0), 1 stop bit
   (0), 0. Returns 0, or 3 if it was already open */
int Serial_Open(unsigned char const *mode);
/* mode 1: close even if bytes are still waiting to be sent */
int Serial_Close(int mode);

static int os_open(void)
{
    static unsigned char const mode[6] = { 0, 9, 0, 0, 0, 0 };
    int r = Serial_Open(mode);
    return r == 0 || r == 3;
}

static int os_close(void)
{
    Serial_Close(1);
    return 0;
}

//---
//  The SCIF of the SH7305 (gint/mpu/scif.h). SCSPTR (serial port register,
//  as on the SH7730) is the one gint leaves out: with TE cleared, SPB2IO
//  puts the TX pin under the control of SPB2DT
//---

#define SCIF       SH7305_SCIF
#define SCSPTR     (*(volatile uint16_t *)0xa4410020)
#define SPB2IO     0x0002
#define SPB2DT     0x0001
#define SCSCR_TE   0x0020
#define SCSCR_CKE  0x0003
#define FIFO_SIZE  16

#define UART_RATE  115200   /* bits per second (Serial_Open mode 9) */
#define PIN_RATE   40000    /* delta-sigma samples per second on the pin */
#define REFILL_US  400      /* the FIFO holds 16 bytes = 1.39 ms */

static bool is_open;
static out_t method;
static uint16_t saved_scscr, saved_scsptr;

/* What the timer interrupt does */
enum { K_NONE, K_UART, K_PIN_DS, K_PIN_EDGE };

#define FULL 32767

/* Shared with the timer interrupt */
static volatile struct {
    uint32_t phase, inc;        /* phase in 1/2^32 of a period per sample */
    uint32_t duty_th;           /* square: high while phase < duty_th */
    int wave, vol;              /* vol: 0 to 256 */
    int32_t acc;                /* delta-sigma error */
    uint32_t t_high, t_low;     /* K_PIN_EDGE: timer constants */
    int level;
} st;

static int16_t sine[256];
static int timer = -1;
static int kind = K_NONE;

bool audio_supported(void)
{
    return !isSH3();
}

bool audio_is_open(void)
{
    return is_open;
}

out_t audio_method(void)
{
    return method;
}

bool audio_playing(void)
{
    return kind != K_NONE;
}

//---
//  Signal
//---

static int32_t sample_now(void)
{
    uint32_t p = st.phase;
    int32_t s, q;
    switch (st.wave) {
    case WAVE_SQUARE: s = p < st.duty_th ? FULL : -FULL; break;
    case WAVE_SINE: s = sine[p >> 24]; break;
    case WAVE_TRIANGLE:
        q = p >> 16;
        s = q < 32768 ? 2 * q - 32768 : 98303 - 2 * q;
        break;
    default: s = (int32_t)(p >> 16) - 32768; break;
    }
    return s * st.vol >> 8;
}

/* One bit of a first-order delta-sigma signal; forced is -1, or the bit
   the line must have (UART start and stop bits), which is fed back too */
static int ds_bit(int forced)
{
    int32_t v = st.acc + sample_now();
    st.phase += st.inc;
    int bit = forced >= 0 ? forced : v >= 0;
    v -= bit ? FULL : -FULL;
    if (v > 4 * FULL) v = 4 * FULL;
    if (v < -4 * FULL) v = -4 * FULL;
    st.acc = v;
    return bit;
}

/* Bytes go out least significant bit first, between a start bit (0) and a
   stop bit (1). In the emulator SCFDR reads as 0: at most one FIFO's worth
   is written per call */
static int isr_uart(void)
{
    int room = FIFO_SIZE - SCIF.SCFDR.TFDC;
    for (int k = 0; k < room; k++) {
        int byte = 0;
        ds_bit(0);
        for (int b = 0; b < 8; b++) byte |= ds_bit(-1) << b;
        ds_bit(1);
        SCIF.SCFTDR = byte;
    }
    return TIMER_CONTINUE;
}

static int isr_pin_ds(void)
{
    SCSPTR = SPB2IO | ds_bit(-1);
    return TIMER_CONTINUE;
}

/* The new level lasts the constant loaded at this underflow; the one set
   here is for the level after it */
static int isr_pin_edge(void)
{
    st.level ^= 1;
    SCSPTR = SPB2IO | st.level;
    timer_reload(timer, st.level ? st.t_low : st.t_high);
    return TIMER_CONTINUE;
}

double audio_sample(wave_t wave, int duty, double phase)
{
    phase -= floor(phase);
    switch (wave) {
    case WAVE_SQUARE: return phase < duty / 100.0 ? 1 : -1;
    case WAVE_SINE: return sin(2 * PI * phase);
    case WAVE_TRIANGLE: return phase < 0.5 ? 4 * phase - 1 : 3 - 4 * phase;
    default: return 2 * phase - 1;
    }
}

//---
//  Port
//---

/* Hand the TX pin to the UART, or take it for SPB2DT */
static void configure(out_t m)
{
    uint16_t cke = saved_scscr & SCSCR_CKE;
    if (m == OUT_UART) {
        SCSPTR = 0;
        SCIF.SCSCR.word = cke | SCSCR_TE;
    }
    else {
        SCIF.SCSCR.word = cke;
        SCSPTR = SPB2IO | SPB2DT;   /* idle high, as the UART */
    }
}

bool audio_open(out_t m)
{
    if (!audio_supported()) return false;
    if (is_open) {
        if (m != method) {
            audio_silence();
            configure(m);
            method = m;
        }
        return true;
    }
    if (!gint_world_switch(GINT_CALL(os_open))) return false;

    for (int k = 0; k < 256; k++)
        sine[k] = (int16_t)(FULL * sin(2 * PI * k / 256));
    saved_scscr = SCIF.SCSCR.word;
    saved_scsptr = SCSPTR;
    configure(m);
    method = m;
    is_open = true;
    return true;
}

void audio_close(void)
{
    if (!is_open) return;
    audio_silence();
    SCSPTR = saved_scsptr;
    SCIF.SCSCR.word = saved_scscr;
    gint_world_switch(GINT_CALL(os_close));
    is_open = false;
}

//---
//  Tones
//---

/* TMU input clock: Pphi/4 (timer_configure() picks it for short delays) */
static double tmu_hz(void)
{
    return clock_freq()->Pphi_f / 4.0;
}

/* Timer constant for a period in seconds; the timer counts it plus one */
static uint32_t ticks(double seconds)
{
    double t = seconds * tmu_hz() - 1;
    return t < 1 ? 1 : (uint32_t)(t + 0.5);
}

static void set_params(double freq, wave_t wave, int duty, int volume,
    double rate)
{
    st.inc = (uint32_t)(freq / rate * 4294967296.0);
    st.duty_th = (uint32_t)(duty / 100.0 * 4294967295.0);
    st.wave = wave;
    st.vol = volume * 256 / 100;
    st.t_high = ticks(duty / 100.0 / freq);
    st.t_low = ticks((100 - duty) / 100.0 / freq);
}

void audio_play(double freq, wave_t wave, int duty, int volume)
{
    if (!is_open) return;
    if (freq < FREQ_MIN) freq = FREQ_MIN;
    if (freq > FREQ_MAX) freq = FREQ_MAX;
    if (duty < 1) duty = 1;
    if (duty > 99) duty = 99;

    int want = method == OUT_UART ? K_UART
        : wave == WAVE_SQUARE ? K_PIN_EDGE : K_PIN_DS;
    double rate = method == OUT_UART ? UART_RATE : PIN_RATE;

    if (want == kind) {
        /* Same kind of signal: the interrupt picks up the new values */
        if (kind == K_PIN_DS) rate = tmu_hz() / (ticks(1.0 / PIN_RATE) + 1);
        set_params(freq, wave, duty, volume, rate);
        return;
    }

    audio_silence();
    double period = 0;
    gint_call_t call;
    if (want == K_UART) {
        period = REFILL_US * 1e-6;
        call = GINT_CALL(isr_uart);
    }
    else if (want == K_PIN_DS) {
        period = 1.0 / PIN_RATE;
        rate = tmu_hz() / (ticks(period) + 1);
        call = GINT_CALL(isr_pin_ds);
    }
    else call = GINT_CALL(isr_pin_edge);

    st.phase = 0;
    st.acc = 0;
    set_params(freq, wave, duty, volume, rate);
    /* The square wave starts high, for t_high; then the constant set in
       the interrupt alternates t_low and t_high */
    if (want == K_PIN_EDGE) period = duty / 100.0 / freq;

    /* The first delay is in µs; from the next one on, the exact constant */
    uint64_t us = (uint64_t)(period * 1e6);
    timer = timer_configure(TIMER_TMU, us ? us : 1, call);
    if (timer < 0) return;
    if (want == K_PIN_EDGE) {
        st.level = 1;
        SCSPTR = SPB2IO | SPB2DT;
        timer_reload(timer, st.t_low);
    }
    else timer_reload(timer, ticks(period));
    kind = want;
    timer_start(timer);
}

void audio_silence(void)
{
    if (timer >= 0) timer_stop(timer);
    timer = -1;
    kind = K_NONE;
    if (is_open && method == OUT_PIN) SCSPTR = SPB2IO | SPB2DT;
}
