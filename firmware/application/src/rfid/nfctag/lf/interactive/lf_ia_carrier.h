#pragma once

// The reader's carrier frequency, from the field line's raw samples. No hardware state: the engine and the host tests
// both use it.
//
// With the load on, the field line still ripples at the reader's carrier. Sampled at fs, a carrier between fs and
// 1.5 fs folds to carrier - fs, so the strongest line in that band gives the carrier. The engine samples at 100 kHz,
// so a 125 kHz carrier sits at 25 kHz, and a reader 1 % off moves it by 1.25 kHz. A few milliseconds of samples are
// enough for a tenth of a percent, which is the scale where a tag's timing starts to matter. (The PSK self-trim works
// at the ppm scale, from a capture hundreds of milliseconds long.)
//
// The carrier's harmonics fold into the band too: the third to 4 fs - 3 carrier (25 kHz - 3 offset at 100 kHz), the
// fifth to 25 kHz + 5 offset. They are weaker, so the strongest line is taken as the carrier.

#include <stddef.h>
#include <stdint.h>

typedef enum {
    LF_IA_CARRIER_OK,
    LF_IA_CARRIER_TOO_FEW,  // fewer samples than the estimate needs
    LF_IA_CARRIER_WEAK,     // the strongest line is not clear of the rest of the band
} lf_ia_carrier_status_t;

typedef struct {
    lf_ia_carrier_status_t status;
    float carrier_hz;  // the estimate (nominal_hz unless status is OK)
    float snr_db;      // the peak against the band's median
} lf_ia_carrier_t;

#define LF_IA_CARRIER_MIN_SAMPLES (96u)
#define LF_IA_CARRIER_MAX_SAMPLES (512u)
#define LF_IA_CARRIER_MIN_SNR_DB  (15.0f)

// Estimates the carrier from n samples x taken at fs Hz, searching carriers from lo_hz to hi_hz (inside fs..1.5 fs).
// nominal_hz is returned if the estimate fails. n is capped at LF_IA_CARRIER_MAX_SAMPLES.
void lf_ia_carrier_estimate(const int16_t *x, size_t n, float fs, float lo_hz, float hi_hz, float nominal_hz,
                            lf_ia_carrier_t *out);
