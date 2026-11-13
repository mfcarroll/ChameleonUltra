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

int16_t fp_percentile(const int16_t *x, uint32_t n, float q) {
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
    s->p10 = fp_percentile(x, n, 0.10f);
    s->p50 = fp_percentile(x, n, 0.50f);
    s->p90 = fp_percentile(x, n, 0.90f);

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

// The thresholds a dip is found by, shared with fp_hitag_scan so both read the envelope the same way. The high level
// is the 98th percentile: a reader's field can be off for most of a capture. A gap with the load on drops most of the
// way to zero, where a steady field wanders by about 1 %. A dip starts below high - enter and ends back above
// high - enter / 2, so noise at the threshold can't split it. Returns high and sets *lo, *hi.
static int32_t dip_thresholds(const int16_t *x, uint32_t n, const fp_stats_t *s, uint16_t min_depth, int32_t *lo,
                              int32_t *hi) {
    const int32_t high = fp_percentile(x, n, 0.98f);
    int32_t enter = 8 * (int32_t)s->noise;
    if (enter < (int32_t)min_depth) {
        enter = min_depth;
    }
    if (enter < high / 8) {
        enter = high / 8;
    }
    *lo = high - enter;
    *hi = high - enter / 2;
    return high;
}

uint16_t fp_dips(const int16_t *x, uint32_t n, const fp_stats_t *s, uint16_t min_depth, fp_dip_t *out, uint16_t max,
                 uint16_t *total) {
    int32_t lo, hi;
    const int32_t high = dip_thresholds(x, n, s, min_depth, &lo, &hi);
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

// ---------------------------------------------------------------- Hitag 2 authentication mode

// The reader's command gaps, segmented into bursts, as fp_hitag_scan walks them. The START_AUTH burst is the first
// one of a few gaps; the reader's PWD/AUTH command is the first long burst after it (the quiet window between the two
// is the tag's UID reply). Only the first exchange is kept.
typedef struct {
    uint32_t start_lo, start_hi;  // a START_AUTH burst has this many gaps (about 5 or 6)
    uint32_t auth_min;            // the PWD/AUTH command has at least this many (32 or 64, far more than START_AUTH)
    bool anchor_found;
    uint32_t anchor_count, anchor_last;
    bool auth_found;
    uint32_t auth_count, auth_first, auth_last;
} hitag_seg_t;

static void seg_close(hitag_seg_t *g, uint32_t count, uint32_t first, uint32_t last) {
    if (count == 0 || g->auth_found) {
        return;
    }
    if (g->anchor_found && count >= g->auth_min) {
        // The reader's command after the tag's UID reply: the first long burst once a START_AUTH has been seen.
        g->auth_found = true;
        g->auth_count = count;
        g->auth_first = first;
        g->auth_last = last;
    } else if (count >= g->start_lo && count <= g->start_hi) {
        // A START_AUTH-sized burst: keep the latest as the anchor until the command arrives (the reader may repeat
        // START_AUTH until a tag answers).
        g->anchor_found = true;
        g->anchor_count = count;
        g->anchor_last = last;
    }
}

static uint8_t hitag_mode(bool found, uint32_t gaps) {
    if (!found) {
        return FP_AUTH_NONE;
    }
    if (gaps <= 45) {
        return FP_AUTH_PASSWORD;  // about 32 bits: a static password
    }
    if (gaps >= 51 && gaps <= 85) {
        return FP_AUTH_CRYPTO;  // about 64 bits: the 48-bit-key cipher
    }
    return FP_AUTH_AMBIGUOUS;  // between the two lengths, or improbably long
}

void fp_hitag_scan(const int16_t *x, uint32_t n, const fp_stats_t *s, uint16_t min_depth, fp_hitag_t *out) {
    memset(out, 0, sizeof(*out));
    int32_t lo, hi;
    const int32_t high = dip_thresholds(x, n, s, min_depth, &lo, &hi);
    const int32_t cmd_depth = high * 3 / 10;  // a command gap is at least 30 % deep, deeper than ripple (HS's door)
    const uint32_t max_width = 200;           // 2 ms at 100 kHz: a wider dip is the field off, not a command gap
    const uint32_t split = 300;               // 3 ms between gap starts: a quiet window this long is the tag's reply

    hitag_seg_t g;
    memset(&g, 0, sizeof(g));
    g.start_lo = 3;
    g.start_hi = 10;
    g.auth_min = 20;

    bool in = false;
    uint32_t start = 0;
    int32_t deepest = 0;
    bool burst = false;
    uint32_t b_count = 0, b_first = 0, b_last = 0;

    // The same dip detection as fp_dips, but each command gap feeds the burst segmentation instead of a store.
    for (uint32_t i = 0; i <= n; i++) {
        const bool end = (i == n);
        const int32_t v = end ? high : x[i];
        if (!in && !end && v < lo) {
            in = true;
            start = i;
            deepest = v;
            continue;
        }
        if (!in) {
            continue;
        }
        if (v < deepest) {
            deepest = v;
        }
        if (v <= hi && !end) {
            continue;
        }
        in = false;
        const uint32_t width = i - start;
        const int32_t depth = high - deepest;
        if (width >= max_width || depth < cmd_depth) {
            continue;  // the field switching off, or ripple: not a reader command gap
        }
        if (burst && start - b_last < split) {
            b_count++;
            b_last = start;
        } else {
            if (burst) {
                seg_close(&g, b_count, b_first, b_last);
            }
            burst = true;
            b_count = 1;
            b_first = start;
            b_last = start;
        }
    }
    if (burst) {
        seg_close(&g, b_count, b_first, b_last);
    }

    out->start_gaps = (uint8_t)(g.anchor_count > 255 ? 255 : g.anchor_count);
    if (g.auth_found) {
        out->auth_gaps = (uint8_t)(g.auth_count > 255 ? 255 : g.auth_count);
        const uint32_t span = g.auth_last - g.auth_first;
        out->auth_span = (uint16_t)(span > 0xFFFF ? 0xFFFF : span);
    }
    out->mode = hitag_mode(g.auth_found, g.auth_count);
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
    // The Hann window's cosine by rotation: (hc, hs) turns by the window's step each sample.
    const float step = 2.0f * PI_F / (float)(n - 1), rc = cosf(step), rs = sinf(step);
    float hc = 1.0f, hs = 0.0f;
    float s1 = 0.0f, s2 = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        const float y = (x[i] - m) * (0.5f - 0.5f * hc);
        const float t = hc * rc - hs * rs;
        hs = hs * rc + hc * rs;
        hc = t;
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
