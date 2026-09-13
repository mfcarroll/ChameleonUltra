#include <string.h>

#include "lf_ask_manchester.h"
#include "lf_slicer.h"

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
    .bit_samples = GALLAGHER_ASK_BIT_SAMPLES,
    .accept = gallagher_accept,
};

/* Securakey: 0111111111 then the format selector 001011010 — the one variant on this bench.
 * ⚠ The other two selectors both reference implementations know are deliberately absent; see
 * the note in the header for why an untestable format is worse than a missing one. */
const uint8_t LF_ASK_PREAMBLE_SECURAKEY[SECURAKEY_ASK_PREAMBLE_BITS] = {
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 1, 0, 1, 1, 0, 1, 0
};

/* ⛔ NO `accept` — AND THAT IS A GAP, NOT A DESIGN. Gallagher gates on a CRC-8 over its
 * payload; Securakey has no checksum either reference knows how to compute, so the gate is
 * the 19 preamble bits and nothing else. ⇒ Whether that is sufficient is a MEASUREMENT, and
 * the cross-protocol null is the one that makes it. Do not quote this format's reliability
 * from Gallagher's. */
/* Noralsy: the first 12 bits of `0xBB0`. */
const uint8_t LF_ASK_PREAMBLE_NORALSY[NORALSY_ASK_PREAMBLE_BITS] = {
    1, 0, 1, 1, 1, 0, 1, 1, 0, 0, 0, 0
};

/* ⭐ NORALSY'S TWO CHECKSUMS, and they are the reason a 12-bit preamble is enough here where
 * 19 bits are thin for Securakey. Each is the XOR of 4-bit nibbles over a span; both are
 * verified against the bench credential `BB0214FF0112402233670000` before this shipped —
 * calc1 = 6 against chk1 = 6, calc2 = 7 against chk2 = 7.
 *
 * ⚠ THE SPANS OVERLAP THE CHECKS THEMSELVES, which looks wrong and is not: calc2 covers bits
 * 0..75, and chk1 lives at 72..75 inside that range. Transcribed from the reference exactly
 * rather than "corrected" to a tidier span — the verification above is what says the
 * transcription is right, and a tidier span would fail it. */
static uint8_t noralsy_nibble_xor(const uint8_t *word_bits, uint16_t start, uint16_t len) {
    uint8_t sum = 0;
    for (uint16_t i = 0; i < len; i += 4) {
        uint8_t v = 0;
        for (uint8_t k = 0; k < 4; k++) {
            v = (uint8_t)(((unsigned)v << 1) | (word_bits[start + i + k] & 1u));
        }
        sum ^= v;
    }
    return (uint8_t)(sum & 0x0Fu);
}

static bool noralsy_accept(const uint8_t *word_bits, uint16_t frame_bits) {
    if (frame_bits < NORALSY_ASK_FRAME_BITS) {
        return false;
    }
    uint8_t chk1 = noralsy_nibble_xor(word_bits, 72, 4);
    uint8_t chk2 = noralsy_nibble_xor(word_bits, 76, 4);
    return noralsy_nibble_xor(word_bits, 32, 40) == chk1 &&
           noralsy_nibble_xor(word_bits, 0, 76) == chk2;
}

/* InstaFob: the 32 bits of `0x00107060`, which is the tag's own T5577 config word. */
const uint8_t LF_ASK_PREAMBLE_INSTAFOB[INSTAFOB_ASK_PREAMBLE_BITS] = {
    0, 0, 0, 0, 0, 0, 0, 0,   /* 0x00 */
    0, 0, 0, 1, 0, 0, 0, 0,   /* 0x10 */
    0, 1, 1, 1, 0, 0, 0, 0,   /* 0x70 */
    0, 1, 1, 0, 0, 0, 0, 0    /* 0x60 */
};

/* ⛔ NO `accept`, and unlike Securakey's that is not a gap: the 32 gated bits here are an
 * exact configuration word, not a vendor tag with a constant run in it. ⚠ What it does NOT
 * give is any check on the PAYLOAD — nothing in the frame validates the credential — so a
 * bit error inside the card data is undetectable by this format. Securakey shares that
 * weakness and Gallagher and Noralsy do not. */
const lf_ask_format_t LF_ASK_FORMAT_INSTAFOB = {
    .preamble = LF_ASK_PREAMBLE_INSTAFOB,
    .preamble_bits = INSTAFOB_ASK_PREAMBLE_BITS,
    .frame_bits = INSTAFOB_ASK_FRAME_BITS,
    .bit_samples = INSTAFOB_ASK_BIT_SAMPLES,
    .accept = NULL,
};

const lf_ask_format_t LF_ASK_FORMAT_NORALSY = {
    .preamble = LF_ASK_PREAMBLE_NORALSY,
    .preamble_bits = NORALSY_ASK_PREAMBLE_BITS,
    .frame_bits = NORALSY_ASK_FRAME_BITS,
    .bit_samples = NORALSY_ASK_BIT_SAMPLES,
    .accept = noralsy_accept,
};

const lf_ask_format_t LF_ASK_FORMAT_SECURAKEY = {
    .preamble = LF_ASK_PREAMBLE_SECURAKEY,
    .preamble_bits = SECURAKEY_ASK_PREAMBLE_BITS,
    .frame_bits = SECURAKEY_ASK_FRAME_BITS,
    .bit_samples = SECURAKEY_ASK_BIT_SAMPLES,
    .accept = NULL,
};

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
                                  const lf_ask_format_t *fmt, lf_decode_result_t *out) {
    memset(out, 0, sizeof(*out));
    const uint16_t FB = fmt->frame_bits;
    const uint8_t SPB = fmt->bit_samples;
    if (SPB < LF_ASK_MIN_BIT_SAMPLES || SPB > LF_ASK_MAX_BIT_SAMPLES) {
        return false;
    }
    if (n < (size_t)(FB + 4) * SPB) {
        return false;
    }

    static uint8_t bits[LF_SAMPLED_MAX_CAPTURE_SAMPLES / LF_ASK_MIN_BIT_SAMPLES];
    int32_t best_energy = 0;

    for (uint8_t lp = 1; lp <= 3; lp += 2) {
        lf_slicer_build_dc(samples, n);
        for (uint8_t phase = 0; phase < SPB; phase++) {
            size_t nb = 0;
            int32_t viol = 0;
            for (size_t i = phase; i + SPB <= n; i += SPB) {
                /* ⭐ The two half-bit centres. Manchester's bit is the transition between
                 * them; a MATCHED pair is an encoding violation, marked 2 so the preamble
                 * search can never match it rather than being silently guessed. */
                bool a = lf_slicer_level(samples, n, i + SPB / 4u, lp);
                bool b = lf_slicer_level(samples, n, i + (3u * SPB) / 4u, lp);
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

bool gallagher_ask_decode(int16_t *samples, size_t n, lf_decode_result_t *out) {
    return lf_ask_manchester_decode_fmt(samples, n, &LF_ASK_FORMAT_GALLAGHER, out);
}

bool securakey_ask_decode(int16_t *samples, size_t n, lf_decode_result_t *out) {
    return lf_ask_manchester_decode_fmt(samples, n, &LF_ASK_FORMAT_SECURAKEY, out);
}

bool noralsy_ask_decode(int16_t *samples, size_t n, lf_decode_result_t *out) {
    return lf_ask_manchester_decode_fmt(samples, n, &LF_ASK_FORMAT_NORALSY, out);
}

bool instafob_ask_decode(int16_t *samples, size_t n, lf_decode_result_t *out) {
    return lf_ask_manchester_decode_fmt(samples, n, &LF_ASK_FORMAT_INSTAFOB, out);
}
