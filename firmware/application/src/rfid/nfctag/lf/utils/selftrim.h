#ifndef LF_SELFTRIM_H
#define LF_SELFTRIM_H

// Measure a reader's clock against ours while we emulate, and choose a PSK1 trim.
//
// The reader's 125 kHz carrier ripples on LF_RSSI. Sampled with the SAADC at 20 kHz it aliases to
// about 5 kHz, where an offset of 1 ppm in the reader's clock moves the line by 0.125 Hz. A
// Goertzel scan of +/-320 ppm around the nominal alias finds the line, accepted up to +/-300;
// the trim is its offset in steps of 3.8 ppm (lf_psk1_apply_trim). Our own modulation puts
// lines at the nominal alias and multiples of the frame rate around it, which the scan skips.

#include <stdint.h>

#ifndef SELFTRIM_N_MAX
#define SELFTRIM_N_MAX          (4096)  // a host build may raise it to test longer captures
#endif
#define SELFTRIM_MIN_SNR_DB     (22.0f)
#define SELFTRIM_MIN_PPM        (15.0f)  // just above what 4096 samples can separate from our own line (12 ppm)
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
// (our lines move with it). carrier: nominal reader carrier, Hz.
void selftrim_detect(const int16_t *x, uint16_t n, float fs, float carrier, float self_ppm,
                     selftrim_result_t *r);

#endif
