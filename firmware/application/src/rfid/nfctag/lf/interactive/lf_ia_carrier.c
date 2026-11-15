#include "lf_ia_carrier.h"

#include <math.h>

#define TWO_PI       (6.28318530718f)
// Two passes keep the cost down (it runs in the engine's interrupt): a coarse scan of the band, then a fine one
// around the coarse peak. Hann's main lobe is 4 fs / n wide (2 kHz at 200 samples), so a 500 Hz coarse step
// cannot miss the line.
#define COARSE_STEP_HZ (500.0f)
#define FINE_STEP_HZ   (50.0f)
#define FINE_HALF      (10u)     // fine points each side of the coarse peak
#define SCAN_MAX       (128u)    // coarse points kept for the median

static float m_win[LF_IA_CARRIER_MAX_SAMPLES];  // the windowed, mean-free samples
static float m_pow[SCAN_MAX];

// Power at f Hz of the n windowed samples (Goertzel).
static float power_at(size_t n, float fs, float f) {
    const float w = TWO_PI * f / fs;
    const float c = 2.0f * cosf(w);
    float s1 = 0.0f, s2 = 0.0f;
    for (size_t i = 0; i < n; i++) {
        const float s0 = m_win[i] + c * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return s1 * s1 + s2 * s2 - c * s1 * s2;
}

static float median(float *v, size_t n) {
    // Insertion sort: n is a few hundred at most, and this runs once per listen.
    for (size_t i = 1; i < n; i++) {
        const float t = v[i];
        size_t j = i;
        while (j > 0 && v[j - 1] > t) {
            v[j] = v[j - 1];
            j--;
        }
        v[j] = t;
    }
    return v[n / 2];
}

void lf_ia_carrier_estimate(const int16_t *x, size_t n, float fs, float lo_hz, float hi_hz, float nominal_hz,
                            lf_ia_carrier_t *out) {
    out->status = LF_IA_CARRIER_TOO_FEW;
    out->carrier_hz = nominal_hz;
    out->snr_db = 0.0f;
    if (n > LF_IA_CARRIER_MAX_SAMPLES) {
        n = LF_IA_CARRIER_MAX_SAMPLES;
    }
    if (n < LF_IA_CARRIER_MIN_SAMPLES) {
        return;
    }

    float mean = 0.0f;
    for (size_t i = 0; i < n; i++) {
        mean += x[i];
    }
    mean /= (float)n;
    // A Hann window keeps the field's slow drift and the DC out of the band.
    for (size_t i = 0; i < n; i++) {
        const float h = 0.5f - 0.5f * cosf(TWO_PI * (float)i / (float)(n - 1));
        m_win[i] = ((float)x[i] - mean) * h;
    }

    // The band, folded: carrier f lands at f - fs. Coarse pass.
    const float a_lo = lo_hz - fs, a_hi = hi_hz - fs;
    size_t k_n = (size_t)((a_hi - a_lo) / COARSE_STEP_HZ) + 1u;
    if (k_n > SCAN_MAX) {
        k_n = SCAN_MAX;
    }
    const float step = (a_hi - a_lo) / (float)(k_n - 1u);
    size_t k_pk = 0;
    float p_pk = -1.0f;
    for (size_t k = 0; k < k_n; k++) {
        m_pow[k] = power_at(n, fs, a_lo + step * (float)k);
        if (m_pow[k] > p_pk) {
            p_pk = m_pow[k];
            k_pk = k;
        }
    }

    // Fine pass around the coarse peak, then a parabola through the fine peak and its neighbours, on log power.
    const float centre = a_lo + step * (float)k_pk;
    float fpow[2u * FINE_HALF + 1u];
    size_t f_pk = 0;
    float fp_pk = -1.0f;
    for (size_t k = 0; k < 2u * FINE_HALF + 1u; k++) {
        fpow[k] = power_at(n, fs, centre + FINE_STEP_HZ * ((float)k - (float)FINE_HALF));
        if (fpow[k] > fp_pk) {
            fp_pk = fpow[k];
            f_pk = k;
        }
    }
    float a_pk = centre + FINE_STEP_HZ * ((float)f_pk - (float)FINE_HALF);
    if (f_pk > 0 && f_pk + 1u < 2u * FINE_HALF + 1u) {
        const float l = logf(fpow[f_pk - 1] + 1e-9f), m = logf(fpow[f_pk] + 1e-9f), r = logf(fpow[f_pk + 1] + 1e-9f);
        const float d = l - 2.0f * m + r;
        if (d < 0.0f) {
            a_pk += FINE_STEP_HZ * 0.5f * (l - r) / d;
        }
    }
    p_pk = fp_pk > p_pk ? fp_pk : p_pk;

    const float med = median(m_pow, k_n);
    out->snr_db = 10.0f * log10f((p_pk + 1e-9f) / (med + 1e-9f));
    if (out->snr_db < LF_IA_CARRIER_MIN_SNR_DB) {
        out->status = LF_IA_CARRIER_WEAK;
        return;
    }
    out->status = LF_IA_CARRIER_OK;
    out->carrier_hz = fs + a_pk;
}
