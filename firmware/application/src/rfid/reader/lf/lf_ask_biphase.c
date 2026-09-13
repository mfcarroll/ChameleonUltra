#include <string.h>

#include "lf_ask_biphase.h"

/* GProxII's preamble: 111110 — six bits, and see the header for why they are not the gate. */
const uint8_t LF_BIPHASE_PREAMBLE_GPROXII[GPROXII_BIPHASE_PREAMBLE_BITS] = {
    1, 1, 1, 1, 1, 0
};

/* ⭐ THE REAL GATE: 18 spacer bits that must all be zero.
 *
 * After the 6-bit preamble the frame is 18 groups of five, and the fifth bit of each group is
 * a spacer. The Proxmark expresses this as `removeParity(bits, 0, 5, 3, 90)` where ptype 3
 * means "should be 0 spacer bit" — NOT a parity test over the group, which is what every
 * other ptype in that function does and what a reader skimming the call would assume.
 *
 * ⛔ Verified against the bench credential before it shipped: all 18 spacers of
 * `fac2a38c2b081af0210b12c2` are zero, and `ctest/roundtrip.c` pins that. Without this the
 * gate is six bits, which noise clears roughly once every 64 positions. */
/* Eight bits LEAST significant first. ⚠ Not a style choice — GProxII's descrambler is
 * defined that way (`bytebits_to_byteLSBF` in the Proxmark), and reading these bytes MSB
 * first gives 5 where the bench tag's format length is 26. */
static uint8_t bits_to_byte_lsbf(const uint8_t *bits) {
    uint8_t v = 0;
    for (uint8_t k = 0; k < 8; k++) {
        v = (uint8_t)(v | ((bits[k] & 1u) << k));
    }
    return v;
}

static bool gproxii_accept(const uint8_t *word_bits, uint16_t frame_bits) {
    if (frame_bits < GPROXII_BIPHASE_FRAME_BITS) {
        return false;
    }
    /* The 18 spacers, and the payload with them stripped out. */
    uint8_t body[GPROXII_BIPHASE_SPACER_GROUPS * 4u];
    for (uint8_t g = 0; g < GPROXII_BIPHASE_SPACER_GROUPS; g++) {
        const uint8_t *grp = &word_bits[GPROXII_BIPHASE_PREAMBLE_BITS + 5u * g];
        if (grp[4] != 0u) {
            return false;
        }
        for (uint8_t k = 0; k < 4; k++) {
            body[4u * g + k] = grp[k];
        }
    }
    /* ⭐ The format length, descrambled. The first byte is the XOR key the tag carries; the
     * second, once un-XORed, holds the length in its top six bits. Verified on the bench
     * credential before it shipped: key 141 and length 26, which are exactly the `--xor 141
     * --fmt 26` the Proxmark cloned — so the descramble is confirmed by a value we chose
     * ourselves, not merely by self-consistency. */
    const uint8_t xor_key = bits_to_byte_lsbf(body);
    const uint8_t fmt_len = (uint8_t)((bits_to_byte_lsbf(body + 8) ^ xor_key) >> 2);
    return fmt_len == 26u || fmt_len == 36u;
}

const lf_biphase_format_t LF_BIPHASE_FORMAT_GPROXII = {
    .preamble = LF_BIPHASE_PREAMBLE_GPROXII,
    .preamble_bits = GPROXII_BIPHASE_PREAMBLE_BITS,
    .frame_bits = GPROXII_BIPHASE_FRAME_BITS,
    .bit_samples = GPROXII_BIPHASE_BIT_SAMPLES,
    .accept = gproxii_accept,
};

/* The step across a grid point: the mean of the W samples after it minus the mean of the W
 * before. ⚠ Integer throughout — the sums are kept rather than divided, so this returns W
 * times the step and every comparison below is against a threshold scaled the same way. */
static int32_t step_at(const int16_t *s, size_t t) {
    int32_t before = 0, after = 0;
    for (uint8_t k = 0; k < LF_BIPHASE_SLOPE_WINDOW; k++) {
        before += s[t - 1u - k];
        after += s[t + k];
    }
    return after - before;
}

static inline int32_t iabs32(int32_t v) {
    return v < 0 ? -v : v;
}

static int preamble_err(const uint8_t *bits, size_t at, bool inv,
                        const uint8_t *pre, uint8_t pre_bits) {
    int err = 0;
    for (uint8_t k = 0; k < pre_bits; k++) {
        uint8_t b = bits[at + k];
        if (inv) {
            b = (uint8_t)(1u - b);
        }
        if (b != pre[k]) {
            err++;
        }
    }
    return err;
}

bool lf_ask_biphase_decode_fmt(const int16_t *samples, size_t n,
                               const lf_biphase_format_t *fmt, lf_decode_result_t *out) {
    memset(out, 0, sizeof(*out));
    const uint16_t FB = fmt->frame_bits;
    const uint8_t SPB = fmt->bit_samples;
    const uint8_t W = LF_BIPHASE_SLOPE_WINDOW;
    if (SPB < LF_BIPHASE_MIN_BIT_SAMPLES || SPB > LF_BIPHASE_MAX_BIT_SAMPLES) {
        return false;
    }
    if (n < (size_t)(FB + 4) * SPB) {
        return false;
    }

    static uint8_t bits[LF_SAMPLED_MAX_CAPTURE_SAMPLES / LF_BIPHASE_MIN_BIT_SAMPLES];
    int32_t best_energy = 0;

    for (uint8_t phase = 0; phase < SPB; phase++) {
        /* The first grid point whose "before" window is inside the buffer. */
        size_t first = phase;
        while (first < (size_t)W + 1u) {
            first += SPB;
        }
        const size_t last = (n > (size_t)SPB / 2u + W) ? (n - (size_t)SPB / 2u - W) : 0u;

        /* Pass 1: what a bit-boundary transition looks like IN THIS CAPTURE. Boundaries carry
         * one by construction, so their mean magnitude is the reference — no constant, no
         * per-bench tuning. */
        int32_t sum = 0;
        size_t count = 0;
        for (size_t t = first; t < last; t += SPB) {
            sum += iabs32(step_at(samples, t));
            count++;
        }
        if (count == 0) {
            continue;
        }
        const int32_t mean_edge = sum / (int32_t)count;
        /* ⚠ Reported in ADC counts per sample so it is comparable to the other readers'
         * amplitudes: an empty antenna gives near zero here, a coupled tag hundreds. */
        if (mean_edge / W > best_energy) {
            best_energy = mean_edge / W;
        }

        /* ⚠ The fraction is swept rather than fixed at the 0.3 that won on the bench capture.
         * 0.3 was measured on ONE tag at one coupling; a threshold that is optimal there is
         * not obviously optimal on a weaker or stronger signal, and this branch has had to
         * retract two generalisations already made from a single specimen (C171, C189). */
        for (uint8_t num = 3; num <= 5; num++) {
            const int32_t th = (mean_edge * num) / 10;
            size_t nb = 0;
            for (size_t t = first; t < last; t += SPB) {
                /* ⭐ The whole decoder: did the MIDDLE of this bit step as well? */
                bits[nb++] = (iabs32(step_at(samples, t + SPB / 2u)) > th) ? 1u : 0u;
            }

            for (size_t i = 0; i + FB <= nb; i++) {
                for (uint8_t inv = 0; inv < 2; inv++) {
                    if (preamble_err(bits, i, inv != 0, fmt->preamble,
                                     fmt->preamble_bits) != 0) {
                        continue;
                    }
                    uint8_t word[LF_BIPHASE_MAX_FRAME_BITS];
                    for (uint16_t k = 0; k < FB; k++) {
                        word[k] = (inv != 0) ? (uint8_t)(1u - bits[i + k]) : bits[i + k];
                    }
                    if (fmt->accept != NULL && !fmt->accept(word, FB)) {
                        continue;
                    }
                    memset(out->id, 0, sizeof(out->id));
                    for (uint16_t k = 0; k < FB; k++) {
                        out->id[k / 8] = (uint8_t)(((unsigned)out->id[k / 8] << 1) | word[k]);
                    }
                    memcpy(out->word_bits, word, FB);
                    out->frame_bits = FB;
                    out->offset = phase;
                    out->bit_pos = (uint8_t)i;
                    out->inverted = (inv != 0);
                    out->energy = best_energy;
                    return true;
                }
            }
        }
    }

    out->energy = best_energy;
    return false;
}

bool gproxii_biphase_decode(int16_t *samples, size_t n, lf_decode_result_t *out) {
    return lf_ask_biphase_decode_fmt(samples, n, &LF_BIPHASE_FORMAT_GPROXII, out);
}
