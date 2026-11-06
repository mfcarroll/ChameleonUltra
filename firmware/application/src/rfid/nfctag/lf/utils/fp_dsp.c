#include "fp_dsp.h"

#include <math.h>
#include <string.h>

#define PI_F (3.14159265358979f)

// ---------------------------------------------------------------- levels

// The q-th fraction of x, exact: a coarse histogram of 64-count bins finds the bin, a second pass counts inside it.
static uint16_t m_hist[256];
static uint16_t m_step_hist[1024];

static int16_t clamp14(int16_t v) {
    return v < 0 ? 0 : (v > 16383 ? 16383 : v);
}

static int16_t percentile(const int16_t *x, uint32_t n, float q) {
    const uint32_t want = (uint32_t)(q * (float)(n - 1));
    memset(m_hist, 0, sizeof(m_hist));
    for (uint32_t i = 0; i < n; i++) {
        m_hist[clamp14(x[i]) >> 6]++;
    }
    uint32_t below = 0;
    int b = 0;
    for (; b < 256; b++) {
        if (below + m_hist[b] > want) {
            break;
        }
        below += m_hist[b];
    }
    memset(m_hist, 0, 64 * sizeof(m_hist[0]));
    for (uint32_t i = 0; i < n; i++) {
        const int16_t v = clamp14(x[i]);
        if ((v >> 6) == b) {
            m_hist[v & 63]++;
        }
    }
    for (int k = 0; k < 64; k++) {
        if (below + m_hist[k] > want) {
            return (int16_t)((b << 6) | k);
        }
        below += m_hist[k];
    }
    return (int16_t)((b << 6) | 63);
}

void fp_stats(const int16_t *x, uint32_t n, fp_stats_t *s) {
    memset(s, 0, sizeof(*s));
    if (n < 2) {
        return;
    }
    s->min = x[0];
    s->max = x[0];
    for (uint32_t i = 1; i < n; i++) {
        if (x[i] < s->min) s->min = x[i];
        if (x[i] > s->max) s->max = x[i];
    }
    s->p10 = percentile(x, n, 0.10f);
    s->p50 = percentile(x, n, 0.50f);
    s->p90 = percentile(x, n, 0.90f);

    // For independent noise of deviation sigma, the median absolute step between samples is 0.954 sigma.
    memset(m_step_hist, 0, sizeof(m_step_hist));
    for (uint32_t i = 1; i < n; i++) {
        int d = x[i] - x[i - 1];
        d = d < 0 ? -d : d;
        m_step_hist[d > 1023 ? 1023 : d]++;
    }
    uint32_t c = 0;
    int k = 0;
    for (; k < 1023; k++) {
        c += m_step_hist[k];
        if (c > (n - 1) / 2) {
            break;
        }
    }
    s->noise = (uint16_t)((k * 1000 + 477) / 954);
}

// ---------------------------------------------------------------- dips

uint16_t fp_dips(const int16_t *x, uint32_t n, const fp_stats_t *s, uint16_t min_depth, fp_dip_t *out, uint16_t max,
                 uint16_t *total) {
    // The high level is the 98th percentile: a reader's field can be off for most of a capture. A gap with the
    // load on drops most of the way to zero, where a steady field wanders by about 1 %.
    const int32_t high = percentile(x, n, 0.98f);
    int32_t enter = 8 * (int32_t)s->noise;
    if (enter < min_depth) {
        enter = min_depth;
    }
    if (enter < high / 8) {
        enter = high / 8;
    }
    // A dip starts below high - enter and ends back above high - enter / 2, so noise at the threshold can't split it.
    const int32_t lo = high - enter, hi = high - enter / 2;
    uint16_t stored = 0, found = 0;
    bool in = false;
    uint32_t start = 0;
    int32_t deepest = 0;
    for (uint32_t i = 0; i <= n; i++) {
        const bool end = (i == n);
        const int32_t v = end ? high : x[i];
        if (!in && !end && v < lo) {
            in = true;
            start = i;
            deepest = v;
        } else if (in) {
            if (v < deepest) deepest = v;
            if (v > hi || end) {
                in = false;
                found++;
                if (stored < max) {
                    uint32_t depth = (uint32_t)(high - deepest);
                    depth = depth > 0x7FFF ? 0x7FFF : depth;
                    out[stored].start = (uint16_t)(start > 0xFFFF ? 0xFFFF : start);
                    out[stored].width = (uint16_t)((i - start) > 0xFFFF ? 0xFFFF : (i - start));
                    out[stored].depth = (uint16_t)(depth | (end ? FP_DIP_OPEN : 0));
                    stored++;
                }
            }
        }
    }
    *total = found;
    return stored;
}

// ---------------------------------------------------------------- spectrum

float fp_fold(double f, float fs) {
    double r = fmod(f, (double)fs);
    if (r < 0) r += fs;
    return (float)(r > fs / 2.0 ? fs - r : r);
}

static float mean16(const int16_t *x, uint32_t n) {
    float m = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        m += x[i];
    }
    return n ? m / (float)n : 0.0f;
}

// In place, radix 2: the DFT of m complex values (interleaved re, im); m a power of 2.
static void fft_complex(float *z, int m) {
    for (int i = 1, j = 0; i < m; i++) {
        int bit = m >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            const float tr = z[2 * i], ti = z[2 * i + 1];
            z[2 * i] = z[2 * j];
            z[2 * i + 1] = z[2 * j + 1];
            z[2 * j] = tr;
            z[2 * j + 1] = ti;
        }
    }
    for (int len = 2; len <= m; len <<= 1) {
        const int h = len >> 1;
        for (int k = 0; k < h; k++) {
            const float a = -2.0f * PI_F * k / len;
            const float wr = cosf(a), wi = sinf(a);
            for (int i = k; i < m; i += len) {
                float *p = z + 2 * i, *q = z + 2 * (i + h);
                const float tr = q[0] * wr - q[1] * wi, ti = q[0] * wi + q[1] * wr;
                q[0] = p[0] - tr;
                q[1] = p[1] - ti;
                p[0] += tr;
                p[1] += ti;
            }
        }
    }
}

// |X[k]| of the N-point real DFT, from Z, the N / 2-point complex DFT of the samples packed as (even, odd).
static float real_bin(float ar, float ai, float br, float bi, int k, int nfft) {
    const float er = 0.5f * (ar + br), ei = 0.5f * (ai - bi);
    const float or_ = 0.5f * (ai + bi), oi = -0.5f * (ar - br);
    const float a = 2.0f * PI_F * k / nfft, c = cosf(a), s = sinf(a);
    const float xr = er + c * or_ + s * oi, xi = ei + c * oi - s * or_;
    return sqrtf(xr * xr + xi * xi);
}

// Leaves |X[k]| at work[k], k < nfft / 2.
static void mags(const int16_t *x, uint32_t n, uint32_t nfft, float *work) {
    if (n > nfft) n = nfft;
    const float m = mean16(x, n);
    for (uint32_t i = 0; i < n; i++) {
        work[i] = (x[i] - m) * (0.5f - 0.5f * cosf(2.0f * PI_F * i / (n - 1)));
    }
    for (uint32_t i = n; i < nfft; i++) {
        work[i] = 0.0f;
    }
    const int half = (int)nfft / 2;
    fft_complex(work, half);
    for (int k = 1; k <= half / 2; k++) {
        const int j = half - k;
        const float ar = work[2 * k], ai = work[2 * k + 1], br = work[2 * j], bi = work[2 * j + 1];
        const float mk = real_bin(ar, ai, br, bi, k, (int)nfft), mj = real_bin(br, bi, ar, ai, j, (int)nfft);
        work[2 * k] = mk;
        work[2 * j] = mj;
    }
    for (int k = 1; k < half; k++) {
        work[k] = work[2 * k];
    }
    work[0] = 0.0f;
}

float fp_goertzel(const int16_t *x, uint32_t n, float f, float fs) {
    const float m = mean16(x, n);
    const float w = 2.0f * PI_F * f / fs, c = 2.0f * cosf(w);
    float s1 = 0.0f, s2 = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        const float y = (x[i] - m) * (0.5f - 0.5f * cosf(2.0f * PI_F * i / (n - 1)));
        const float s0 = y + c * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    const float re = s1 - s2 * cosf(w), im = s2 * sinf(w);
    return sqrtf(re * re + im * im);
}

static float median_f(float *v, int n) {
    for (int i = 1; i < n; i++) {
        const float t = v[i];
        int j = i - 1;
        while (j >= 0 && v[j] > t) {
            v[j + 1] = v[j];
            j--;
        }
        v[j + 1] = t;
    }
    return (n & 1) ? v[n / 2] : 0.5f * (v[n / 2 - 1] + v[n / 2]);
}

float fp_noise_near(const int16_t *x, uint32_t n, float f, float fs, float span_hz) {
    float v[16];
    int k = 0;
    for (int i = -8; i <= 8; i++) {
        if (i == 0) continue;
        float g = f + span_hz * i / 8.0f;
        if (g <= 0.0f || g >= fs / 2) continue;
        v[k++] = fp_goertzel(x, n, g, fs);
    }
    return k ? median_f(v, k) : 0.0f;
}

static bool excluded(float f, const float *excl, int n_excl, float excl_hz) {
    for (int i = 0; i < n_excl; i++) {
        if (fabsf(f - excl[i]) < excl_hz) {
            return true;
        }
    }
    return false;
}

float fp_line(const int16_t *x, uint32_t n, uint32_t nfft, float fs, float min_hz, const float *excl, int n_excl,
              float excl_hz, float *work, float *snr_db) {
    *snr_db = 0.0f;
    if (n < 64 || nfft > FP_FFT_MAX) {
        return 0.0f;
    }
    const uint32_t used = n < nfft ? n : nfft;
    mags(x, used, nfft, work);
    const int half = (int)nfft / 2;
    const float binw = fs / nfft;
    int best = -1;
    for (int k = 2; k < half - 1; k++) {
        const float f = k * binw;
        if (f < min_hz || excluded(f, excl, n_excl, excl_hz)) continue;
        if (work[k] >= work[k - 1] && work[k] >= work[k + 1] && (best < 0 || work[k] > work[best])) {
            best = k;
        }
    }
    if (best < 0) {
        return 0.0f;
    }
    // Refine with the Goertzel on the samples: 17 points across +-1 bin, then a parabola through the best three.
    const float f0 = best * binw, step = binw / 8.0f;
    float m[17];
    int bi = 0;
    for (int i = 0; i < 17; i++) {
        m[i] = fp_goertzel(x, used, f0 + (i - 8) * step, fs);
        if (m[i] > m[bi]) bi = i;
    }
    float fp = f0 + (bi - 8) * step;
    float pk = m[bi];
    if (bi > 0 && bi < 16) {
        const float a = m[bi - 1], b = m[bi], c = m[bi + 1], den = a - 2.0f * b + c;
        if (den < 0.0f) {
            fp += 0.5f * (a - c) / den * step;
            pk = fp_goertzel(x, used, fp, fs);
        }
    }
    // The noise is the median bin of the band the search covered.
    int cnt = 0;
    for (int k = 2; k < half - 1; k++) {
        if (k * binw >= min_hz) work[cnt++] = work[k];
    }
    // The zero-padded FFT samples the same windowed spectrum as the Goertzel over the used samples, so the scales match.
    const float noise = cnt ? median_f(work, cnt) : 0.0f;
    *snr_db = noise > 0.0f ? 20.0f * log10f(pk / noise) : 99.0f;
    return fp;
}
