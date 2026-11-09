#ifndef FP_DSP_H
#define FP_DSP_H

// Signal measures for the field probe (field_probe.c): levels, dips in the field's envelope, and the strongest line
// in a capture's spectrum. Plain C with no SDK calls, so a host build can test it.

#include <stdbool.h>
#include <stdint.h>

#define FP_FFT_MAX (4096)  // the largest FFT; a work buffer holds this many floats

typedef struct {
    int16_t min, p10, p50, p90, max;
    uint16_t noise;  // the noise's standard deviation, from the median step between consecutive samples
} fp_stats_t;

void fp_stats(const int16_t *x, uint32_t n, fp_stats_t *s);

// One drop in the envelope below the high level (the 98th percentile): where it starts, how long it lasts and how
// deep it goes, in samples and ADC counts. A dip still open at the end of the capture has FP_DIP_OPEN set in its depth.
typedef struct {
    uint16_t start, width, depth;
} fp_dip_t;
#define FP_DIP_OPEN (0x8000)

// Finds dips of at least min_depth counts, 8 noise sigmas and an eighth of the high level, whichever is largest.
// Stores up to max of them and returns the number stored; *total counts every dip found.
uint16_t fp_dips(const int16_t *x, uint32_t n, const fp_stats_t *s, uint16_t min_depth, fp_dip_t *out, uint16_t max,
                 uint16_t *total);

// Where a line at f Hz lands after sampling at fs: 0 .. fs / 2.
float fp_fold(double f, float fs);

// The strongest line in the spectrum of x[0..n): an FFT of nfft points (a power of two, at most FP_FFT_MAX; n is cut
// or zero-padded to it), then a Goertzel search around the best bin. Lines below min_hz, and within excl_hz of any
// frequency in excl, are skipped. work holds nfft floats. Returns the line's frequency in Hz (0 if none) and sets
// *snr_db against the median bin.
float fp_line(const int16_t *x, uint32_t n, uint32_t nfft, float fs, float min_hz, const float *excl, int n_excl,
              float excl_hz, float *work, float *snr_db);

// The magnitude of the Hann-windowed, mean-removed x[0..n) at f Hz, and the median magnitude of a few frequencies
// spread over the band around it (the noise for an SNR).
float fp_goertzel(const int16_t *x, uint32_t n, float f, float fs);
float fp_noise_near(const int16_t *x, uint32_t n, float f, float fs, float span_hz);

#endif
