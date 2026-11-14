#ifndef FM_DSP_H
#define FM_DSP_H

// Signal measures for field measurement (field_measure.c): levels, dips in the field's envelope, and the strongest line
// in a capture's spectrum. Plain C with no SDK calls, so a host build can test it.

#include <stdbool.h>
#include <stdint.h>

#define FM_FFT_MAX (4096)  // the largest FFT; a work buffer holds this many floats

typedef struct {
    int16_t min, p10, p50, p90, max;
    uint16_t noise;  // the noise's standard deviation, from the median step between consecutive samples
} fm_stats_t;

void fm_stats(const int16_t *x, uint32_t n, fm_stats_t *s);

// The q-th fraction of x[0..n), exact for values 0..16383 (others are clamped to that range).
int16_t fm_percentile(const int16_t *x, uint32_t n, float q);

// One drop in the envelope below the high level (the 98th percentile): where it starts, how long it lasts and how
// deep it goes, in samples and ADC counts. A dip still open at the end of the capture has FM_DIP_OPEN set in its depth.
typedef struct {
    uint16_t start, width, depth;
} fm_dip_t;
#define FM_DIP_OPEN (0x8000)

// Finds dips of at least min_depth counts, 8 noise sigmas and an eighth of the high level, whichever is largest.
// Stores up to max of them and returns the number stored; *total counts every dip found.
uint16_t fm_dips(const int16_t *x, uint32_t n, const fm_stats_t *s, uint16_t min_depth, fm_dip_t *out, uint16_t max,
                 uint16_t *total);

// ---------------------------------------------------------------- Hitag 2 authentication mode

// The reader-side authentication mode of a Hitag 2 install, from the length of the reader's command that follows the
// tag's UID. The exchange is START_AUTH (5 bits), then the tag's 32-bit UID, then a reader command that is 32 bits
// (PWD, password mode) or 64 bits (AUTH, crypto mode). This reads the command's length only: it classifies the
// install and recovers no secret.
enum { FM_AUTH_NONE = 0, FM_AUTH_PASSWORD = 1, FM_AUTH_CRYPTO = 2, FM_AUTH_AMBIGUOUS = 3 };

typedef struct {
    uint8_t mode;         // FM_AUTH_*
    uint8_t start_gaps;   // gaps in the START_AUTH burst that anchored it (0: none seen)
    uint8_t auth_gaps;    // gaps in the reader's command after the UID reply (about its bit length, 0 if none)
    uint16_t auth_span;   // that command's span, first gap to last, in samples (10 us each at 100 kHz)
} fm_hitag_t;

// Classifies a Hitag 2 reader's authentication mode from one envelope capture x[0..n) (the probe's 100 kHz capture,
// already smoothed), using the same dip detection as fm_dips with min_depth as its floor. Command gaps are dips at
// least three tenths of the high level deep and under 2 ms wide; a quiet window of at least a few ms (the tag's UID
// reply) separates the START_AUTH burst from the reader's next command. Fills *out; mode is FM_AUTH_NONE when no
// START_AUTH-anchored command is found. Reads the frame's length, not its content.
void fm_hitag_scan(const int16_t *x, uint32_t n, const fm_stats_t *s, uint16_t min_depth, fm_hitag_t *out);

// Where a line at f Hz lands after sampling at fs: 0 .. fs / 2.
float fm_fold(double f, float fs);

// The strongest line in the spectrum of x[0..n): an FFT of nfft points (a power of two, at most FM_FFT_MAX; n is cut
// or zero-padded to it), then a Goertzel search around the best bin. Lines below min_hz, and within excl_hz of any
// frequency in excl, are skipped. work holds nfft floats. Returns the line's frequency in Hz (0 if none) and sets
// *snr_db against the median bin.
float fm_line(const int16_t *x, uint32_t n, uint32_t nfft, float fs, float min_hz, const float *excl, int n_excl,
              float excl_hz, float *work, float *snr_db);

// The magnitude of the Hann-windowed, mean-removed x[0..n) at f Hz, and the median magnitude of a few frequencies
// spread over the band around it (the noise for an SNR).
float fm_goertzel(const int16_t *x, uint32_t n, float f, float fs);
float fm_noise_near(const int16_t *x, uint32_t n, float f, float fs, float span_hz);

#endif
