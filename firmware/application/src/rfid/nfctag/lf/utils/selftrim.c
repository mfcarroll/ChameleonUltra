#include "selftrim.h"

#include <math.h>
#include <stdbool.h>

// research exp/fsk-phasecont: the SNR floor, settable (FSK mode 9's wide search on a 200 ms idle-tone capture uses a
// lower one; the narrow measurement and its guard then confirm or reject the result).
float g_selftrim_min_snr_db = SELFTRIM_MIN_SNR_DB;

#define SPAN_PPM    (320.0f)   // past SELFTRIM_MAX_PPM, so a line at the limit still has scan points either side
#define NOISE_HZ    (300.0f)
#define SELF_HZ     (1.5f)
float g_selftrim_self_hz = SELF_HZ;  // the scan's exclusion around our own line(s); settable as the floor
#define PI_F        (3.14159265358979f)
#ifndef MAX_COARSE
#define MAX_COARSE  (160)   // 131 needed at 8192 samples / 20 kHz (61 at 4096); a host build may raise these
#endif
#ifndef MAX_NOISE
#define MAX_NOISE   (256)   // ~104 needed at 8192 / 20 kHz (106 at 4096)
#endif

static const float *m_y;   // mean-removed, Hann-windowed samples (the caller's work buffer)

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

// From a coarse peak fp: refine it, measure its SNR against noise, and classify (valid / trim / why). m_y holds the
// windowed samples. Shared by the Goertzel scan and the FFT search.
static void finish(uint16_t n, float fs, float f0, float hz_per_ppm, float fself, int nsb, float frame_hz, float noise,
                   float fp, float max_ppm, float self_ppm, selftrim_result_t *r) {
    const float binw = fs / n;
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
    for (int j = -nsb; j <= nsb; j++) {
        const float d = fabsf(fp - (fself + j * frame_hz));
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
    const bool self_ok = ssnr >= g_selftrim_min_snr_db;
    // research exp/fsk-selftrim: "our own position is the strongest => matched" holds only once a trim is applied.
    // Untrimmed, FSK's measure drive (U1 + LF_MOD) puts our own line at 26-28 dB, which can beat a reader 140 ppm
    // away; then the best line outside our own decides, and our own line wins only when nothing else passes.
    const bool trimmed = fabsf(self_ppm) >= SELFTRIM_MIN_PPM;
    if (!trimmed && r->snr_db < g_selftrim_min_snr_db && self_ok) {
        // nothing outside our own line, and a line on it: a reader within MIN_PPM of our clock (or one we can't see
        // apart from ourselves). Valid, trim 0.
        r->why = SELFTRIM_NEAR_MATCHED;
        r->valid = 1;
        r->fpk = sf;
        r->ppm = (sf - f0) / hz_per_ppm;
        r->snr_db = ssnr;
    } else if (r->snr_db < g_selftrim_min_snr_db || near_self <= g_selftrim_self_hz || (trimmed && self_ok && sm > pk)) {
        // weak, our own line, or (trimmed) the line on our trimmed position is the strongest: that is the reader
        // we matched, and a weaker line elsewhere is a sideband (a reader restarting its field shows several)
        r->why = r->snr_db < g_selftrim_min_snr_db ? SELFTRIM_WEAK : SELFTRIM_SELF_LINE;
        if (self_ok && trimmed) {
            r->why = SELFTRIM_MATCHED;
            r->fpk = sf;
            r->ppm = (sf - f0) / hz_per_ppm;
            r->snr_db = ssnr;
        }
    } else if (fabsf(ppm) > max_ppm) {
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

void selftrim_detect(const int16_t *x, uint16_t n, float fs, float carrier, float self_ppm, float frame_hz,
                     float *work, selftrim_result_t *r) {
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
    // Hann window; cos(2 pi j / (n - 1)) by rotation rather than a cosf per sample. Filled from the end: work may
    // start at x (in place), and writing work[i] (bytes 4i..4i+3) only overwrites x[2i] and x[2i+1], which are
    // already read when i >= 1 (x[0] is read before work[0] is written). The window is symmetric, so w(n-1-i) = w(i).
    const float dw = 2.0f * PI_F / (n - 1);
    const float rc = cosf(dw), rs = sinf(dw);
    float wc = 1.0f, ws = 0.0f;
    for (uint16_t j = 0; j < n; j++) {
        const uint16_t i = (uint16_t)(n - 1u - j);
        const float xi = x[i];
        __asm__ __volatile__("" ::: "memory");   // x and work may alias (int16 vs float): read before writing
        work[i] = (xi - mean) * (0.5f - 0.5f * wc);
        const float t = wc * rc - ws * rs;
        ws = ws * rc + wc * rs;
        wc = t;
    }
    m_y = work;

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
    const int nsb = frame_hz > 0.0f ? 8 : 0;   // our frame sidebands, +/-8 x the frame rate around our own line
    int best = -1;
    for (int i = 1; i < 2 * k; i++) {
        if (mc[i] < mc[i - 1] || mc[i] < mc[i + 1]) {
            continue;
        }
        const float f = f0 + (i - k) * df;
        bool self = false;
        for (int j = -nsb; j <= nsb && !self; j++) {
            self = fabsf(f - (fself + j * frame_hz)) <= g_selftrim_self_hz;
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

    finish(n, fs, f0, hz_per_ppm, fself, nsb, frame_hz, noise,
           parab(f0 + (best - k) * df, mc[best - 1], mc[best], mc[best + 1], df), SELFTRIM_MAX_PPM, self_ppm, r);
}

// research exp/fsk-selftrim-wide: the wide search as one FFT. The Goertzel scan above costs a pass over the samples
// per frequency, fine for +/-320 ppm (~130 points) but not for a reader thousands of ppm off (Pavilion P1 elevator:
// -4187 ppm, READERS-pavilion-2026-10-28). One real FFT of the windowed capture gives every bin; the strongest line
// is then refined and classified by finish(), as the scan's is.
//
// What else is in the band (PM3 bench, NOTES-wide-2026-10-28): the measure burst's idle tone (fc/8) and all its
// harmonics, folded, are the strongest lines; the reader's carrier mixed with them (fr +- k fc/8) can beat the carrier
// itself, and from one capture any member of that comb is as good a reader as the carrier. So: our tone's harmonics are
// excluded, and with a second capture whose tone ran at another rate (other), only lines in both at the same frequency
// count (per bin, the smaller magnitude): the reader's carrier stays put, our harmonics and every product move. The
// reader's own harmonics alias in too (harmonic h of f lands at fold(h f)); a line that is the image of another line's
// harmonic is taken back to that fundamental.

#define FFT_N       SELFTRIM_FFT_N
#define FFT_HARM    (7)     // harmonics checked as images: 2 .. FFT_HARM
#define FFT_CAND    (16)    // strongest local maxima considered
#define FFT_OWN_HARM (24)   // our tone's harmonics excluded: 1 .. FFT_OWN_HARM (h8 is our own line)

static float m_fft_noise[MAX_NOISE];

// Mean-removed, Hann-windowed x[0..n) into w[0..n) (the same window as selftrim_detect). w must not overlap x.
static void hann_into(float *w, const int16_t *x, uint16_t n) {
    float mean = 0.0f;
    for (uint16_t i = 0; i < n; i++) {
        mean += x[i];
    }
    mean /= n;
    for (uint16_t i = 0; i < n; i++) {
        w[i] = (x[i] - mean) * (0.5f - 0.5f * cosf(2.0f * PI_F * i / (n - 1)));
    }
}

// In place, radix 2: the DFT of m complex values (interleaved re, im); m a power of 2.
static void fft_complex(float *z, int m) {
    for (int i = 1, j = 0; i < m; i++) {  // bit-reversed order
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

// |X[k]| of the FFT_N-point real DFT from Z, the FFT_N / 2-point complex DFT of the samples packed as (even, odd):
// X[k] = E + W^k O, E = (Z[k] + conj Z[m - k]) / 2, O = (Z[k] - conj Z[m - k]) / 2i, W = exp(-2 pi i / FFT_N).
static float real_bin(float ar, float ai, float br, float bi, int k) {
    const float er = 0.5f * (ar + br), ei = 0.5f * (ai - bi);
    const float or_ = 0.5f * (ai + bi), oi = -0.5f * (ar - br);
    const float a = 2.0f * PI_F * k / FFT_N, c = cosf(a), s = sinf(a);
    const float xr = er + c * or_ + s * oi, xi = ei + c * oi - s * or_;
    return sqrtf(xr * xr + xi * xi);
}

// Overwrites z (the complex FFT) with |X[k]| at z[2k], k = 1 .. FFT_N / 2 - 1 (each pair k, m - k read first).
static void real_mags(float *z) {
    const int m = FFT_N / 2;
    for (int k = 1; k <= m / 2; k++) {
        const int j = m - k;
        const float ar = z[2 * k], ai = z[2 * k + 1], br = z[2 * j], bi = z[2 * j + 1];
        const float mk = real_bin(ar, ai, br, bi, k), mj = real_bin(br, bi, ar, ai, j);
        z[2 * k] = mk;
        z[2 * j] = mj;
    }
}

// Where a line at f (Hz, before sampling) lands in 0 .. fs / 2.
static float fold(double f, float fs) {
    double r = fmod(f, (double)fs);
    return (float)(r > fs / 2.0 ? fs - r : r);
}

void selftrim_fft_mags(const int16_t *x, uint16_t n, float *work) {
    if (n > FFT_N) {
        n = FFT_N;
    }
    hann_into(work, x, n);
    for (int i = n; i < FFT_N; i++) {
        work[i] = 0.0f;
    }
    fft_complex(work, FFT_N / 2);
    real_mags(work);
    for (int k = 1; k < FFT_N / 2; k++) {
        work[k] = work[2 * k];  // compact: |X[k]| at work[k] (k < 2k: nothing unread is overwritten)
    }
    work[0] = 0.0f;
}

void selftrim_detect_fft(const int16_t *x, uint16_t n, float fs, float carrier, float own_ppm, float other_own_ppm,
                         float max_ppm, const float *other, float *work, selftrim_result_t *r) {
    if (n > FFT_N) {
        n = FFT_N;
    }
    const float m = roundf(carrier / fs);
    const float f0 = fabsf(carrier - m * fs);
    const bool mirrored = carrier < m * fs;
    const float hz_per_ppm = carrier * 1e-6f * (mirrored ? -1.0f : 1.0f);
    const float binw = fs / FFT_N;
    // the idle tone's fc/8 on our clock, from our own line's ppm (its 8th harmonic is our own line)
    const double fsc = (double)carrier / 8.0 * (1.0 + (double)own_ppm * 1e-6);
    const double fsc_o = (double)carrier / 8.0 * (1.0 + (double)other_own_ppm * 1e-6);

    selftrim_fft_mags(x, n, work);
    const float *mag = work;  // |X[k]|, k < FFT_N / 2; FFT_N / 2 .. FFT_N - 1 free for the combined spectrum
    float *cmb = work + FFT_N / 2;

    const float span = max_ppm * fabsf(hz_per_ppm);
    int klo = (int)floorf((f0 - span) / binw) - 1, khi = (int)ceilf((f0 + span) / binw) + 1;
    klo = klo < 2 ? 2 : klo;
    khi = khi > FFT_N / 2 - 2 ? FFT_N / 2 - 2 : khi;

    // noise: the median bin across this capture's window (lines are few), on at most MAX_NOISE bins
    const int stride = (khi - klo) / MAX_NOISE + 1;
    int nn = 0;
    for (int k = klo; k <= khi && nn < MAX_NOISE; k += stride) {
        m_fft_noise[nn++] = mag[k];
    }
    const float noise = nn ? median(m_fft_noise, nn) : 0.0f;
    const float thr = noise * powf(10.0f, g_selftrim_min_snr_db / 20.0f);

    // the lines both captures have where they are: the reader's carrier; our tone's harmonics and the reader mixed
    // with them move with our rate and drop out
    // (the earlier capture scaled to this one's noise floor first: field strength and gain differ between captures)
    float scale = 1.0f;
    if (other) {
        int no = 0;
        for (int k = klo; k <= khi && no < MAX_NOISE; k += stride) {
            m_fft_noise[no++] = other[k];
        }
        const float onoise = no ? median(m_fft_noise, no) : 0.0f;
        scale = onoise > 0.0f ? noise / onoise : 1.0f;
    }
    for (int k = klo - 1; k <= khi + 1; k++) {
        cmb[k] = other ? fminf(mag[k], other[k] * scale) : mag[k];
    }
#define C(k) cmb[k]

    // this capture's tone harmonics are not the reader, wherever they fold: +-2.5 bins, the Hann main lobe and a little
    // (a 50 dB line's skirt is a local maximum of its own a bin or two out). The earlier capture's too, except its h8
    // (our own line, the one near a reader on our clock: excluding it masked the PM3 at -140 against a -240 tone; the
    // minimum drops it, this capture's h8 being hundreds of Hz away). Its h1 moves least between the two (4000 ppm =
    // 62 Hz) and is the strongest line, so its skirt can sit on this capture's: a pair locked on it once (-3738 ppm).
    const float excl = 2.5f * binw;
    int cand[FFT_CAND];
    int nc = 0;
    for (int k = klo + 1; k < khi; k++) {
        const float v = C(k);
        if (v < C(k - 1) || v < C(k + 1)) {
            continue;
        }
        const float f = k * binw;
        bool own = false;
        for (int h = 1; h <= FFT_OWN_HARM && !own; h++) {
            own = fabsf(f - fold(h * fsc, fs)) <= excl || (other && h != 8 && fabsf(f - fold(h * fsc_o, fs)) <= excl);
        }
        if (own) {
            continue;
        }
        int p = nc < FFT_CAND ? nc++ : FFT_CAND;  // insert, strongest first
        while (p > 0 && C(cand[p - 1]) < v) {
            if (p < FFT_CAND) {
                cand[p] = cand[p - 1];
            }
            p--;
        }
        if (p < FFT_CAND) {
            cand[p] = k;
        }
    }
    r->valid = 0;
    r->trim = 0;
    r->evals = FFT_N;
    if (nc == 0) {
        r->why = SELFTRIM_NO_PEAK;
        r->ppm = r->snr_db = r->fpk = 0.0f;
        return;
    }

    // the strongest line, unless it is the image of another (above the floor) line's harmonic
    int best = cand[0];
    for (int pass = 0; pass < 4; pass++) {
        bool moved = false;
        for (int c = 0; c < nc && !moved; c++) {
            const int k = cand[c];
            if (k == best || C(k) < thr) {
                continue;
            }
            const double fr = (double)carrier * (1.0 + (double)((k * binw - f0) / hz_per_ppm) * 1e-6);
            for (int h = 2; h <= FFT_HARM && !moved; h++) {
                if (fabsf(fold(h * fr, fs) - best * binw) <= 1.5f * binw) {
                    best = k;
                    moved = true;
                }
            }
        }
        if (!moved) {
            break;
        }
    }
    const float fp = parab(best * binw, C(best - 1), C(best), C(best + 1), binw);
    const bool weak = C(best) < thr;  // in the combined spectrum: present in both captures, above the floor
#undef C

    // The frequency is the combined spectrum's (parabolic on the log, Hann: ~0.05 bin): refining on this capture alone
    // can be pulled by a product next to the reader that the other capture does not have (PM3 divisor 96: a lock 23 ppm
    // off). The SNR is this capture's line at it. Not finish(): its "our own line is the strongest => matched" is for a
    // trim that plays on the reader, and mode 9's measure tone is detuned from it on purpose.
    hann_into(work, x, n);  // the spectrum overwrote it
    m_y = work;
    const float f = fp;
    const float pk = goertzel(n, f, fs);
    r->evals += 1;
    r->fpk = f;
    r->ppm = (f - f0) / hz_per_ppm;
    r->snr_db = noise > 0.0f ? 20.0f * log10f(pk / noise) : 99.0f;
    if (weak || r->snr_db < g_selftrim_min_snr_db) {
        r->why = SELFTRIM_WEAK;
    } else if (fabsf(r->ppm) > max_ppm) {
        r->why = SELFTRIM_TOO_FAR;
    } else {
        r->why = SELFTRIM_TRIM;
        r->valid = 1;
    }
}
