#include "lf_indala_psk.h"

/*
 * ⭐ THE PREAMBLE IS THE SAME FOR EVERY INDALA TAG (cmdlfindala.c:50), which is what makes
 * searching for it legitimate rather than searching for the answer. It also happens to be
 * the first 33 bits of the bench tag a0000000e6bd0e92 — 1010, then the 28-bit zero run,
 * then a 1.
 */
static const uint8_t PREAMBLE[INDALA_PSK_PREAMBLE_BITS] = {
    1, 0, 1, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1
};

/*
 * ⭐ EXACT MATCH, NOT A CORRELATION, AND NOT A TOLERANCE.
 *
 * A correlator cannot be used here: as a template the preamble is dominated by a 28-bit
 * CONSTANT run, so it slides against itself almost as well eight bits off as on. The peak
 * is inherently broad, and it landed a nibble out — returning the right bits in the wrong
 * 64-bit window (bea0000000e6bd0e for a0000000e6bd0e92). The Proxmark does not correlate
 * either; preambleSearch() is an exact match on the demodulated stream.
 *
 * Zero tolerance is not caution, it is measured: across the 320 committed captures,
 * tolerances of 0, 1, 2 and 3 bit errors ALL give 51/160 on the tag — and 0 of 160 empty
 * captures produce a preamble match at ANY of them. The tolerance buys nothing and can
 * only cost, so it is 0. Raising it is a one-line change if a weaker coupling ever needs
 * it; re-measure the empty null if you do.
 */
#define PREAMBLE_MAX_ERR 0

/* Format-26 de-scramble tables, transcribed from cmdlfindala.c:263-289. Index into the
 * 64-bit frame; listed most-significant field bit first. */
static const uint8_t FC_BITS[8]   = { 57, 49, 44, 47, 48, 53, 39, 58 };
static const uint8_t CSN_BITS[16] = { 42, 45, 43, 40, 52, 36, 35, 51,
                                      46, 33, 37, 54, 56, 59, 50, 41 };

static inline uint32_t popcount12(uint32_t v) {
    v = v - ((v >> 1) & 0x555);
    v = (v & 0x333) + ((v >> 2) & 0x333);
    return (((v + (v >> 4)) & 0xF0F) * 0x101) >> 8;
}

/*
 * Mix the fc/2 subcarrier down to DC, in place.
 *
 * At EXACTLY fs/2 the mixer is a multiplication by (-1)^n — no oscillator and no phase
 * estimate, because the tag derives its subcarrier by dividing the very field this reader
 * generates. The original DC and any slow envelope drift move UP to fs/2, where the notch
 * below removes them.
 *
 * Range: a 14-bit conversion minus its own mean is within ±16383, so this stays in int16
 * and the capture buffer can be reused rather than doubled.
 */
static void baseband_in_place(int16_t *y, size_t n) {
    int32_t sum = 0;
    for (size_t i = 0; i < n; i++) {
        sum += y[i];
    }
    const int32_t mean = sum / (int32_t)n;
    for (size_t i = 0; i < n; i++) {
        int32_t v = (int32_t)y[i] - mean;
        y[i] = (int16_t)((i & 1u) ? -v : v);
    }
}

/*
 * ⭐ THE [1,2,1] NOTCH, FOLDED INTO THE BIT INTEGRATOR FOR FREE.
 *
 * Without a baseband filter NOTHING decodes — 0 of 160 captures, against 51 with one. The
 * reason is narrow and specific: the mix above moves the carrier ripple to fs/2, and the
 * 32-sample boxcar rejects only ~2.4-2.8% there. Measured on this bench that is ~316
 * counts of ripple against a ~10-count subcarrier, so ~9 counts leak into every single bit
 * decision.
 *
 * ⇒ The filter's job is therefore A NULL AT fs/2, not a low cutoff. [1,2,1] puts a
 * double zero exactly there while costing 0.03dB at the 3.9kbit/s data. Measured against
 * the 12kHz brick-wall FFT the research decoder used, on the same 160 captures, paired:
 *
 *     none        0/160     [1,1]       45/160     [1,3,3,1]   45/160
 *     FFT 12kHz  43/160     [1,2,1]  ⭐ 51/160     [1,4,6,4,1] 45/160
 *                           [1,1,1]     31/160
 *
 * [1,2,1] STRICTLY DOMINATES the FFT: every capture the FFT decodes, it decodes, plus 8
 * more (McNemar b=0 c=8, p=0.008). And [1,1,1] — a 3-tap boxcar, which smooths just as
 * hard but nulls at fs/3 instead of fs/2 — is the WORST of the set. That is the mechanism
 * showing itself: it is the null placement that matters, not the smoothing.
 *
 * Folding it into the boxcar costs four adds per BIT rather than three per SAMPLE.
 * Summing the [1,2,1]-filtered signal over a 32-sample window is identically a weighted
 * sum of the unfiltered signal over 34 samples with weights 1,3,4,4,...,4,3,1, so
 *
 *     integrator = 4*sum(y[a..a+31]) + y[a-1] - y[a] - y[a+31] + y[a+32]
 *
 * with y read as 0 outside the capture, matching numpy's convolve(mode='same').
 * The factor of 4 is never divided out: every use downstream is a sign test or a
 * comparison between integrators, and a common scale changes neither.
 */
static int32_t bit_integrator(const int16_t *y, size_t n, size_t a) {
    int32_t box = 0;
    for (size_t j = 0; j < INDALA_PSK_BIT_SAMPLES; j++) {
        box += y[a + j];
    }
    int32_t v = 4 * box - y[a] - y[a + INDALA_PSK_BIT_SAMPLES - 1];
    if (a > 0) {
        v += y[a - 1];
    }
    if (a + INDALA_PSK_BIT_SAMPLES < n) {
        v += y[a + INDALA_PSK_BIT_SAMPLES];
    }
    return v;
}

/** Match the preamble at `i`, normal or inverted. Returns the error count, or 0xFF once
 *  it exceeds the tolerance (so a hopeless position costs a few comparisons, not 33). */
static uint8_t preamble_err(const uint8_t *bits, size_t i, bool inverted) {
    uint8_t err = 0;
    for (size_t j = 0; j < INDALA_PSK_PREAMBLE_BITS; j++) {
        uint8_t want = inverted ? (uint8_t)(1u - PREAMBLE[j]) : PREAMBLE[j];
        if (bits[i + j] != want) {
            if (++err > PREAMBLE_MAX_ERR) {
                return 0xFF;
            }
        }
    }
    return err;
}

/* De-scramble format 26 and check its Wiegand parity. Advisory only — see the note in
 * indala_psk1_decode(). */
static void descramble26(const uint8_t *w, indala_psk_result_t *out) {
    uint32_t fc = 0, csn = 0;
    for (uint8_t k = 0; k < 8; k++) {
        fc = (fc << 1) | w[FC_BITS[k]];
    }
    for (uint8_t k = 0; k < 16; k++) {
        csn = (csn << 1) | w[CSN_BITS[k]];
    }
    out->fc = (uint8_t)fc;
    out->csn = (uint16_t)csn;
    out->parity = (uint8_t)((w[34] << 1) | w[38]);

    /* Standard Wiegand-26 over the 24-bit payload: leading bit gives the top 12 bits even
     * parity, trailing bit gives the bottom 12 odd parity. Confirmed against the bench
     * tag, whose Proxmark-reported "Parity: 11" is exactly what these rules produce. */
    uint32_t payload = (fc << 16) | csn;
    uint8_t want_even = (uint8_t)(popcount12(payload >> 12) & 1u);
    uint8_t want_odd  = (uint8_t)(1u - (popcount12(payload & 0xFFFu) & 1u));
    out->wiegand26_ok = (((out->parity >> 1) & 1u) == want_even) &&
                        ((out->parity & 1u) == want_odd);
}

bool indala_psk1_decode(int16_t *samples, size_t n, indala_psk_result_t *out) {
    if (samples == NULL || out == NULL || n < INDALA_PSK_MIN_SAMPLES) {
        return false;
    }
    if (n > INDALA_PSK_CAPTURE_SAMPLES) {
        n = INDALA_PSK_CAPTURE_SAMPLES;
    }
    baseband_in_place(samples, n);

    /* ⚠ THE BIT PHASE WITHIN THE 32-SAMPLE PERIOD IS UNKNOWN, so all 32 are tried, and
     * ranking them by preamble errors ALONE is not enough: several offsets match the
     * preamble exactly while straddling the true bit boundaries, and those returned
     * a0000000e69d0e82 where the aligned one returns a0000000e6bd0e92. Rank instead by
     * integrator MAGNITUDE among the preamble-clean offsets — the best-aligned boxcar is
     * the one whose integrators are largest, because a straddling one averages part of
     * each neighbour and partially cancels. */
    int32_t  best_amp = -1;
    uint8_t  best_err = 0xFF;
    uint8_t  best_word[INDALA_PSK_FRAME_BITS];
    uint8_t  best_off = 0, best_pos = 0;
    bool     best_inv = false;
    bool     found = false;

    int32_t integ[INDALA_PSK_MAX_BITS];
    uint8_t bits[INDALA_PSK_MAX_BITS];

    for (size_t off = 0; off < INDALA_PSK_BIT_SAMPLES; off++) {
        size_t nb = (n - off) / INDALA_PSK_BIT_SAMPLES;
        if (nb < INDALA_PSK_FRAME_BITS) {
            continue;
        }
        if (nb > INDALA_PSK_MAX_BITS) {
            nb = INDALA_PSK_MAX_BITS;
        }
        for (size_t k = 0; k < nb; k++) {
            integ[k] = bit_integrator(samples, n, off + k * INDALA_PSK_BIT_SAMPLES);
            /* PSK1: the phase IS the data, so the bit is simply the integrator's sign. */
            bits[k] = (integ[k] > 0) ? 1u : 0u;
        }

        for (size_t i = 0; i + INDALA_PSK_FRAME_BITS <= nb; i++) {
            for (uint8_t inv = 0; inv < 2; inv++) {
                uint8_t err = preamble_err(bits, i, inv != 0);
                if (err > PREAMBLE_MAX_ERR) {
                    continue;
                }
                /* Sum rather than mean: every candidate spans the same 64 bits, so the
                 * divide would cancel. Bounded by 64 * 4 * 32 * 16383 = 1.4e8. */
                int32_t amp = 0;
                for (size_t k = 0; k < INDALA_PSK_FRAME_BITS; k++) {
                    int32_t v = integ[i + k];
                    amp += (v < 0) ? -v : v;
                }
                if (err < best_err || (err == best_err && amp > best_amp)) {
                    best_err = err;
                    best_amp = amp;
                    best_off = (uint8_t)off;
                    best_pos = (uint8_t)i;
                    best_inv = (inv != 0);
                    for (size_t k = 0; k < INDALA_PSK_FRAME_BITS; k++) {
                        uint8_t b = bits[i + k];
                        best_word[k] = (inv != 0) ? (uint8_t)(1u - b) : b;
                    }
                    found = true;
                }
            }
        }
    }

    if (!found) {
        return false;
    }

    for (size_t k = 0; k < 8; k++) {
        uint8_t byte = 0;
        for (size_t b = 0; b < 8; b++) {
            byte = (uint8_t)((byte << 1) | best_word[k * 8 + b]);
        }
        out->id[k] = byte;
    }
    out->offset   = best_off;
    out->bit_pos  = best_pos;
    out->inverted = best_inv;
    out->amp      = best_amp / INDALA_PSK_FRAME_BITS;
    /* ⚠ ADVISORY, NOT A GATE. The parity is reported so a caller can prefer a clean read,
     * but it does NOT reject a frame here: it is two bits, it only covers format 26, and
     * the preamble search already produced 0 false positives in 160 empty captures. Making
     * it a gate would discard real reads of other formats for no measured gain. */
    descramble26(best_word, out);
    return true;
}
