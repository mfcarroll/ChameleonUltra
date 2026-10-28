#ifndef LF_SELFTRIM_H
#define LF_SELFTRIM_H

// Measure a reader's clock against ours while we emulate, and choose a PSK1 trim.
//
// research exp/fsk-selftrim: ported from feat/lf-psk-selftrim @ 4b333efb (accept 300 ppm, scan 320; fast
// 4-Goertzel detector) for FSK (HID Prox, ioProx). Changes: the frame rate, which places our own sidebands, is a
// parameter (PSK1 64-bit: 125000 / 2048 = 61.04 Hz; HID 26.57 Hz; ioProx ~30.5 Hz); the window is 8192 samples
// (410 ms at 20 kHz); the windowed samples go into a caller's work buffer, which may be the capture itself (no
// separate 32 KB float array). The FSK path ignores r->trim (3.8 ppm steps) and converts r->ppm itself
// (lf_selftrim_fsk.c).
//
// The reader's 125 kHz carrier ripples on LF_RSSI. Sampled with the SAADC at 20 kHz it aliases to
// about 5 kHz, where an offset of 1 ppm in the reader's clock moves the line by 0.125 Hz. A
// Goertzel scan of +/-320 ppm around the nominal alias finds the line, accepted up to +/-300;
// the trim is its offset in steps of 3.8 ppm (lf_psk1_apply_trim). Our own modulation puts
// lines at the nominal alias and multiples of the frame rate around it, which the scan skips.

#include <stdint.h>

#ifndef SELFTRIM_N_MAX
#define SELFTRIM_N_MAX          (8192)  // 410 ms at 20 kHz
#endif
#define SELFTRIM_MIN_SNR_DB     (22.0f)
#define SELFTRIM_MIN_PPM        (15.0f)  // just above our own-line exclusion (1.5 Hz = 12 ppm)
#define SELFTRIM_MAX_PPM        (300.0f)
#define SELFTRIM_PPM_PER_STEP   (3.8f)

typedef enum {
    SELFTRIM_TRIM = 0,          // valid: reader is >= MIN_PPM away, trim = round(ppm / 3.8)
    SELFTRIM_NEAR_MATCHED = 1,  // valid: reader is within MIN_PPM, trim = 0
    SELFTRIM_WEAK = 2,          // no line above MIN_SNR_DB
    SELFTRIM_SELF_LINE = 3,     // strongest line is our own
    SELFTRIM_TOO_FAR = 4,
    SELFTRIM_NO_PEAK = 5,
    SELFTRIM_MATCHED = 6,       // a trim is applied and the strongest line is on our own (trimmed) line:
                                // the reader is matched; ppm/snr describe that line. No change.
} selftrim_why_t;

typedef struct {
    float ppm;          // reader vs our clock (meaningful unless NO_PEAK)
    float snr_db;
    float fpk;          // alias frequency of the peak, Hz
    int16_t trim;       // valid only
    uint8_t valid;
    uint8_t why;        // selftrim_why_t
    uint16_t evals;     // Goertzel evaluations used
} selftrim_result_t;

// x: n raw samples (n <= SELFTRIM_N_MAX), sampled at fs Hz. self_ppm: our own applied trim in ppm
// (our lines move with it). carrier: nominal reader carrier, Hz. frame_hz: our frame rate (our sidebands
// sit at multiples of it around our own line); 0 = exclude only our own line. work: n floats; it may start at x
// (in place: x must then have room for n floats, and is overwritten), otherwise it must not overlap x.
extern float g_selftrim_min_snr_db;  // default SELFTRIM_MIN_SNR_DB
extern float g_selftrim_self_hz;     // default 1.5 Hz (12 ppm)
void selftrim_detect(const int16_t *x, uint16_t n, float fs, float carrier, float self_ppm, float frame_hz,
                     float *work, selftrim_result_t *r);

// research exp/fsk-selftrim-wide: the same search over +/-max_ppm by one FFT (any width the alias band allows).
// own_ppm: our own line's ppm in this capture; the idle tone (fc/8 at that rate) and its harmonics are skipped. other: |X[k]| of an earlier capture of the same reader with our tone at another rate
// (SELFTRIM_FFT_N / 2 floats, from selftrim_fft_mags), or NULL; lines are then taken only where both have them.
// n <= SELFTRIM_FFT_N samples (zero-padded to it); work: SELFTRIM_FFT_N floats, not overlapping x (x is not modified).
#define SELFTRIM_FFT_N          (4096)
void selftrim_detect_fft(const int16_t *x, uint16_t n, float fs, float carrier, float own_ppm, float max_ppm,
                         const float *other, float *work, selftrim_result_t *r);
// |X[k]| of the windowed, zero-padded capture at work[k], k < SELFTRIM_FFT_N / 2 (work: SELFTRIM_FFT_N floats).
void selftrim_fft_mags(const int16_t *x, uint16_t n, float *work);

#endif
