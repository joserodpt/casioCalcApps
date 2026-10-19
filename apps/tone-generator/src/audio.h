//---
//  audio - Tones on the 3-pin serial port (the 2.5 mm link jack)
//
//  The port has a single digital output (TX, 0 V or 3.3 V), so every
//  waveform is made of on/off levels. The OS opens the port (Serial_Open,
//  through the gint world switch), then gint drives it in one of two ways:
//
//  OUT_UART  Bytes are streamed into the serial FIFO at 115200 baud. Each
//            bit of the stream is one sample of a 1-bit delta-sigma
//            signal, start and stop bits included. Works whenever the OS
//            serial driver does.
//  OUT_PIN   The TX pin is set high or low directly (SCSPTR). A square wave
//            gets exact edges from a timer; the other waveforms use the
//            same delta-sigma signal at 40 kHz.
//---

#ifndef TONE_AUDIO
#define TONE_AUDIO

#include <stdbool.h>

typedef enum { WAVE_SQUARE, WAVE_SINE, WAVE_TRIANGLE, WAVE_SAW,
    WAVES } wave_t;
typedef enum { OUT_UART, OUT_PIN, OUTS } out_t;

#define FREQ_MIN 20.0
#define FREQ_MAX 20000.0

/* audio_supported(): false on the SH3 models, whose serial port is a
   different module */
bool audio_supported(void);

/* audio_open(): Open the port for this method; true if it worked */
bool audio_open(out_t method);
/* audio_close(): Silence, give the port back to the OS */
void audio_close(void);
bool audio_is_open(void);
out_t audio_method(void);

/* audio_play(): Start a tone, or change the one that is playing. duty is
   the square wave's high time in %, volume the amplitude in % (not for a
   square wave in OUT_PIN, which is always full) */
void audio_play(double freq, wave_t wave, int duty, int volume);
/* audio_silence(): Stop the tone; the port stays open */
void audio_silence(void);
bool audio_playing(void);

/* audio_sample(): The waveform at phase 0..1, -1 to 1, for drawing */
double audio_sample(wave_t wave, int duty, double phase);

#endif /* TONE_AUDIO */
