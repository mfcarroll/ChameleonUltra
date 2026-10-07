#include "selftrim.h"

#include <math.h>
#include <stdbool.h>

#define SPAN_PPM    (400.0f)   // = SELFTRIM_MAX_PPM: the scan covers everything a result may report
#define NOISE_HZ    (300.0f)
#define SELF_HZ     (1.5f)
#define FRAME_HZ    (125000.0f / 2048.0f)   // Indala frame rate, 61.04 Hz
#define PI_F        (3.14159265358979f)
#ifndef MAX_COARSE
#define MAX_COARSE  (128)   // 61 needed at 4096 samples / 20 kHz; a host build may raise these
#endif
#ifndef MAX_NOISE
#define MAX_NOISE   (256)   // 106 needed at 4096 / 20 kHz
#endif

static float m_y[SELFTRIM_N_MAX];   // mean-removed, Hann-windowed samples

#ifndef NOISE_STRIDE
#define NOISE_STRIDE (2)    // noise median over every 2nd bin: the same decisions as every bin, half the work
#endif
#define BANK        (4)     // frequencies per pass over the samples: the Goertzel recursion is a serial chain, so
                            // running several side by side keeps the FPU busy

// Magnitudes of the DFT of m_y[0..n) at f[0..cnt) (Goertzel), BANK frequencies per pass.
static void goertzel_bank(uint16_t n, const float *f, int cnt, float fs, float *out) {
    for (int b = 0; b < cnt; b += BANK) {
        float c[BANK], s1[BANK] = {0}, s2[BANK] = {0};
        const int m = cnt - b < BANK ? cnt - b : BANK;
        for (int j = 0; j < BANK; j++) {
            c[j] = 2.0f * cosf(2.0f * PI_F * f[b + (j < m ? j : 0)] / fs);
        }
        // two samples per step, s1/s2 swapping roles: s0 = y + c s1 - s2 without the moves
        uint32_t i = 0;
        for (; i + 1 < n; i += 2) {
            const float y0 = m_y[i], y1 = m_y[i + 1];
            for (int j = 0; j < BANK; j++) {
                s2[j] = (c[j] * s1[j] - s2[j]) + y0;   // vfnms + vadd: no register copies
                s1[j] = (c[j] * s2[j] - s1[j]) + y1;
            }
        }
        if (i < n) {
            for (int j = 0; j < BANK; j++) {
                const float s0 = m_y[i] + c[j] * s1[j] - s2[j];
                s2[j] = s1[j];
                s1[j] = s0;
            }
        }
        for (int j = 0; j < m; j++) {
            const float w = 2.0f * PI_F * f[b + j] / fs;
            const float re = s1[j] - s2[j] * cosf(w);
            const float im = s2[j] * sinf(w);
            out[b + j] = sqrtf(re * re + im * im);
        }
    }
}

static float goertzel(uint16_t n, float f, float fs) {
    float m;
    goertzel_bank(n, &f, 1, fs, &m);
    return m;
}

static float median(float *v, int n) {
    for (int i = 1; i < n; i++) {   // insertion sort; n is ~100
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

// Peak position from three magnitudes at step apart, parabolic on the log.
static float parab(float fb, float ma, float mb, float mc, float step) {
    const float la = logf(ma + 1e-9f), lb = logf(mb + 1e-9f), lc = logf(mc + 1e-9f);
    const float d = la - 2.0f * lb + lc;
    return d < 0.0f ? fb + 0.5f * (la - lc) / d * step : fb;
}

// Trimmed case: the strongest line within SELF_HZ of our own trimmed line (magnitude, frequency, SNR).
static float self_peak(uint16_t n, float fs, float fself, float noise, float *bf_out, float *snr_out) {
    float fv[13], mv[13];
    for (int j = -6; j <= 6; j++) {   // +/-1.5 Hz at 0.25 Hz steps
        fv[j + 6] = fself + j * (SELF_HZ / 6.0f);
    }
    goertzel_bank(n, fv, 13, fs, mv);
    float bm = 0.0f, bf = fself;
    for (int j = 0; j < 13; j++) {
        if (mv[j] > bm) {
            bm = mv[j];
            bf = fv[j];
        }
    }
    *bf_out = bf;
    *snr_out = noise > 0.0f ? 20.0f * log10f(bm / noise) : 99.0f;
    return bm;
}

void selftrim_detect(const int16_t *x, uint16_t n, float fs, float carrier, float self_ppm,
                     selftrim_result_t *r) {
    if (n > SELFTRIM_N_MAX) {
        n = SELFTRIM_N_MAX;
    }
    const float m = roundf(carrier / fs);
    const float f0 = fabsf(carrier - m * fs);
    const bool mirrored = carrier < m * fs;
    const float hz_per_ppm = carrier * 1e-6f * (mirrored ? -1.0f : 1.0f);
    const float binw = fs / n;

    float mean = 0.0f;
    for (uint16_t i = 0; i < n; i++) {
        mean += x[i];
    }
    mean /= n;
    // Hann window; cos(2 pi i / (n - 1)) by rotation rather than a cosf per sample
    const float dw = 2.0f * PI_F / (n - 1);
    const float rc = cosf(dw), rs = sinf(dw);
    float wc = 1.0f, ws = 0.0f;
    for (uint16_t i = 0; i < n; i++) {
        m_y[i] = (x[i] - mean) * (0.5f - 0.5f * wc);
        const float t = wc * rc - ws * rs;
        ws = ws * rc + wc * rs;
        wc = t;
    }

    const float span = SPAN_PPM * fabsf(hz_per_ppm);
    const float df = binw / 4.0f;
    int k = (int)(span / df);
    if (2 * k + 1 > MAX_COARSE) {
        k = (MAX_COARSE - 1) / 2;
    }
    static float mc[MAX_COARSE];
    static float fv[MAX_NOISE > MAX_COARSE ? MAX_NOISE : MAX_COARSE];
    for (int i = -k; i <= k; i++) {
        fv[i + k] = f0 + i * df;
    }
    goertzel_bank(n, fv, 2 * k + 1, fs, mc);
    uint16_t evals = 2 * k + 1;

    static float nv[MAX_NOISE];
    int nn = 0;
    const int nb = (int)(NOISE_HZ / binw);
    for (int i = -nb; i <= nb && nn < MAX_NOISE; i++) {
        if (fabsf(i * binw) > span + 2.0f * binw && (i % NOISE_STRIDE) == 0) {
            fv[nn++] = f0 + i * binw;
        }
    }
    goertzel_bank(n, fv, nn, fs, nv);
    evals += nn;
    const float noise = nn ? median(nv, nn) : 0.0f;

    const float fself = f0 + self_ppm * hz_per_ppm;
    int best = -1;
    for (int i = 1; i < 2 * k; i++) {
        if (mc[i] < mc[i - 1] || mc[i] < mc[i + 1]) {
            continue;
        }
        const float f = f0 + (i - k) * df;
        bool self = false;
        for (int j = -8; j <= 8 && !self; j++) {
            self = fabsf(f - (fself + j * FRAME_HZ)) <= SELF_HZ;
        }
        if (self) {
            continue;
        }
        if (best < 0 || mc[i] > mc[best]) {
            best = i;
        }
    }
    r->valid = 0;
    r->trim = 0;
    r->evals = evals;
    if (best < 0) {
        r->why = SELFTRIM_NO_PEAK;
        r->ppm = r->snr_db = r->fpk = 0.0f;
        return;
    }

    float fp = parab(f0 + (best - k) * df, mc[best - 1], mc[best], mc[best + 1], df);
    const float d2 = binw / 16.0f;
    const float f3[3] = {fp - d2, fp, fp + d2};
    float m3[3];
    goertzel_bank(n, f3, 3, fs, m3);
    if (m3[1] >= m3[0] && m3[1] >= m3[2]) {
        fp = parab(fp, m3[0], m3[1], m3[2], d2);
    }
    const float pk = goertzel(n, fp, fs);
    r->evals += 4;
    const float ppm = (fp - f0) / hz_per_ppm;
    r->fpk = fp;
    r->ppm = ppm;
    r->snr_db = noise > 0.0f ? 20.0f * log10f(pk / noise) : 99.0f;

    float near_self = 1e9f;
    for (int j = -8; j <= 8; j++) {
        const float d = fabsf(fp - (fself + j * FRAME_HZ));
        if (d < near_self) {
            near_self = d;
        }
    }
    // A reader within SELF_HZ of our own (trimmed) line sits on it, and the scan above skips it. Look there too: if
    // a line there clears the threshold and is the stronger, the reader is matched to us (a near-matched reader that
    // gates its field shows sidebands either side, which must not trim).
    float sf = 0.0f, ssnr = 0.0f;
    const float sm = self_peak(n, fs, fself, noise, &sf, &ssnr);
    r->evals += 13;
    const bool self_ok = ssnr >= SELFTRIM_MIN_SNR_DB;
    if (r->snr_db < SELFTRIM_MIN_SNR_DB || near_self <= SELF_HZ || (self_ok && sm > pk)) {
        // weak, our own line, or (trimmed) the line on our trimmed position is the strongest: that is the reader
        // we matched, and a weaker line elsewhere is a sideband (a reader restarting its field shows several)
        r->why = r->snr_db < SELFTRIM_MIN_SNR_DB ? SELFTRIM_WEAK : SELFTRIM_SELF_LINE;
        if (self_ok) {
            r->why = SELFTRIM_MATCHED;
            r->fpk = sf;
            r->ppm = (sf - f0) / hz_per_ppm;
            r->snr_db = ssnr;
        }
    } else if (fabsf(ppm) > SELFTRIM_MAX_PPM) {
        r->why = SELFTRIM_TOO_FAR;
    } else if (fabsf(ppm) < SELFTRIM_MIN_PPM) {
        r->why = SELFTRIM_NEAR_MATCHED;
        r->valid = 1;
    } else {
        r->why = SELFTRIM_TRIM;
        r->valid = 1;
        r->trim = (int16_t)lroundf(ppm / SELFTRIM_PPM_PER_STEP);
    }
}
