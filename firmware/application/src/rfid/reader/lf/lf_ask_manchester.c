#include <string.h>

#include "lf_ask_manchester.h"

/* Gallagher's preamble: 0x7FEA = 0111111111101010 (C171, and Momentum's own
 * GALLAGHER_PREAMBLE check uses the same 16 bits). */
const uint8_t LF_ASK_PREAMBLE_GALLAGHER[GALLAGHER_ASK_PREAMBLE_BITS] = {
    0, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 0, 1, 0, 1, 0
};

/* ⭐ GALLAGHER'S CRC IS THE GATE, and it is a real computed check rather than a constant —
 * the same role NexWatch's parity plays (C164). Momentum refuses a frame whose CRC-8
 * disagrees, and so does this.
 *
 * ⛔ THE PARAMETERS ARE POLY 0x07, INIT 0x2C — VERIFIED AGAINST A REAL FRAME, NOT GUESSED.
 * The first version of this file used 0x1D/0xFF from memory, which returns 0xEE where the
 * tag carries 0x49. That would have rejected every genuine Gallagher and looked exactly like
 * "the protocol does not work on this hardware" — a gate that is wrong in the safe direction
 * is still a gate that reads nothing. Checked on the bench credential before it compiled:
 * payload A3 3C DB C3 C6 B0 A3 61 -> 0x49, matching both the frame and the Proxmark's own
 * "CRC: 49 (ok)".
 *
 * ⛔ THE PAYLOAD BYTES ARE TAKEN AT STRIDE 9, NOT BY FILTERING OUT EVERY 9TH BIT. Byte i is
 * the 8 bits at 16 + 9i, so the interleaved parity bit is stepped over rather than removed.
 * The two descriptions give the same bytes here but not the same code, and the filtering
 * version silently runs off the end of the payload. */
static uint8_t gallagher_crc8(const uint8_t *d, size_t len) {
    uint8_t crc = 0x2C;
    for (size_t i = 0; i < len; i++) {
        crc ^= d[i];
        for (uint8_t b = 0; b < 8; b++) {
            crc = (uint8_t)((crc & 0x80u) ? (((unsigned)crc << 1) ^ 0x07u)
                                          : ((unsigned)crc << 1));
        }
    }
    return crc;
}

static inline uint8_t bits_to_byte(const uint8_t *word_bits, size_t at) {
    uint8_t v = 0;
    for (uint8_t k = 0; k < 8; k++) {
        v = (uint8_t)(((unsigned)v << 1) | (word_bits[at + k] & 1u));
    }
    return v;
}

static bool gallagher_accept(const uint8_t *word_bits, uint16_t frame_bits) {
    if (frame_bits < GALLAGHER_ASK_FRAME_BITS) {
        return false;
    }
    uint8_t payload[8];
    for (uint8_t i = 0; i < 8; i++) {
        payload[i] = bits_to_byte(word_bits, 16u + 9u * i);
    }
    return gallagher_crc8(payload, 8) == bits_to_byte(word_bits, 16u + 9u * 8u);
}

const lf_ask_format_t LF_ASK_FORMAT_GALLAGHER = {
    .preamble = LF_ASK_PREAMBLE_GALLAGHER,
    .preamble_bits = GALLAGHER_ASK_PREAMBLE_BITS,
    .frame_bits = GALLAGHER_ASK_FRAME_BITS,
    .accept = gallagher_accept,
};

/* ⭐ THE LOCAL DC, AS BLOCK MEANS RATHER THAN A PREFIX SUM. A single global threshold is
 * wrong on a capture that drifts, and a per-sample running mean would need a second buffer
 * the size of the capture — 28KB on a part that already holds one. 56 block means over a
 * 14336-sample capture is 224 bytes and follows the drift closely enough: the block is 256
 * samples, or 8 bit periods, while the drift this corrects is far slower. */
#define DC_BLOCK_SHIFT 8
#define DC_BLOCK       (1u << DC_BLOCK_SHIFT)
#define DC_MAX_BLOCKS  ((LF_PSK1_MAX_CAPTURE_SAMPLES / DC_BLOCK) + 1)

static int32_t m_dc[DC_MAX_BLOCKS];

static void build_dc(const int16_t *s, size_t n) {
    size_t nb = (n + DC_BLOCK - 1) / DC_BLOCK;
    for (size_t b = 0; b < nb; b++) {
        size_t from = b * DC_BLOCK;
        size_t to = from + DC_BLOCK;
        if (to > n) {
            to = n;
        }
        int32_t acc = 0;
        for (size_t i = from; i < to; i++) {
            acc += s[i];
        }
        m_dc[b] = acc / (int32_t)(to - from);
    }
}

/* Smoothed sample: lp==1 is the raw value. ⚠ lp==1 is what the clean captures wanted —
 * the level path needs no low-pass at all, which is the opposite of the edge route's
 * requirement (C171). lp==3 is kept because the CLIPPED captures preferred it. */
static inline int32_t smp(const int16_t *s, size_t n, size_t i, uint8_t lp) {
    if (lp <= 1 || i == 0 || i + 1 >= n) {
        return s[i];
    }
    return ((int32_t)s[i - 1] + s[i] + s[i + 1]) / 3;
}

static inline bool level_at(const int16_t *s, size_t n, size_t i, uint8_t lp) {
    return smp(s, n, i, lp) > m_dc[i >> DC_BLOCK_SHIFT];
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

bool lf_ask_manchester_decode_fmt(int16_t *samples, size_t n,
                                  const lf_ask_format_t *fmt, indala_psk_result_t *out) {
    memset(out, 0, sizeof(*out));
    const uint16_t FB = fmt->frame_bits;
    if (n < (size_t)(FB + 4) * LF_ASK_BIT_SAMPLES) {
        return false;
    }

    static uint8_t bits[LF_PSK1_MAX_CAPTURE_SAMPLES / LF_ASK_BIT_SAMPLES];
    int32_t best_energy = 0;

    for (uint8_t lp = 1; lp <= 3; lp += 2) {
        build_dc(samples, n);
        for (uint8_t phase = 0; phase < LF_ASK_BIT_SAMPLES; phase++) {
            size_t nb = 0;
            int32_t viol = 0;
            for (size_t i = phase; i + LF_ASK_BIT_SAMPLES <= n; i += LF_ASK_BIT_SAMPLES) {
                /* ⭐ The two half-bit centres. Manchester's bit is the transition between
                 * them; a MATCHED pair is an encoding violation, marked 2 so the preamble
                 * search can never match it rather than being silently guessed. */
                bool a = level_at(samples, n, i + LF_ASK_BIT_SAMPLES / 4, lp);
                bool b = level_at(samples, n, i + (3 * LF_ASK_BIT_SAMPLES) / 4, lp);
                if (a == b) {
                    bits[nb++] = 2;
                    viol++;
                } else {
                    bits[nb++] = a ? 1u : 0u;
                }
            }
            /* ⚠ The violation count is this decoder's "energy": it distinguishes "nothing is
             * there" from "something is there I could not read". An empty antenna slices to
             * near-random levels and violates about half the time; a real frame at the right
             * phase violates almost never. Reported as its inverse so louder means better,
             * matching what the shared capture engine expects of `energy`. */
            if (nb > 0) {
                int32_t e = (int32_t)((nb - (size_t)viol) * 100u / nb);
                if (e > best_energy) {
                    best_energy = e;
                }
            }

            for (size_t i = 0; i + FB <= nb; i++) {
                for (uint8_t inv = 0; inv < 2; inv++) {
                    if (preamble_err(bits, i, inv != 0, fmt->preamble,
                                     fmt->preamble_bits) != 0) {
                        continue;
                    }
                    uint8_t word[LF_ASK_MAX_FRAME_BITS];
                    bool clean = true;
                    for (uint16_t k = 0; k < FB; k++) {
                        uint8_t v = bits[i + k];
                        if (v > 1) {
                            clean = false;
                            break;
                        }
                        word[k] = (inv != 0) ? (uint8_t)(1u - v) : v;
                    }
                    if (!clean) {
                        continue;
                    }
                    /* ⛔ The format's own rule, inside the search so a failing candidate is
                     * passed over rather than ending it — same reason as the PSK side. */
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

bool gallagher_ask_decode(int16_t *samples, size_t n, indala_psk_result_t *out) {
    return lf_ask_manchester_decode_fmt(samples, n, &LF_ASK_FORMAT_GALLAGHER, out);
}
