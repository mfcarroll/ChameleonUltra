#include <string.h>

#include "lf_fsk2a.h"
#include "lf_slicer.h"

/* AWID's preamble: eight bits, `00000001`. ⚠ Eight bits is a thin gate on its own, which is
 * why the format also demands the preamble again 96 bits later AND odd parity over every
 * nibble — all three are the reference's own checks. */
const uint8_t LF_FSK2A_PREAMBLE_AWID[AWID_FSK_PREAMBLE_BITS] = { 0, 0, 0, 0, 0, 0, 0, 1 };

/* ⭐ AWID'S PARITY IS PER NIBBLE AND ODD, over bits 8..95 — `bit_lib_test_parity(data, 8, 88,
 * BitLibParityOdd, 4)`. It is the same structure the payload rides in: three data bits then a
 * parity LSB, so checking it and extracting the payload are the same walk. */
static bool awid_accept(const uint8_t *word_bits, uint16_t frame_bits) {
    if (frame_bits < AWID_FSK_FRAME_BITS) {
        return false;
    }
    for (uint8_t i = 0; i < 22; i++) {
        uint8_t sum = 0;
        for (uint8_t k = 0; k < 4; k++) {
            sum = (uint8_t)(sum + (word_bits[8 + i * 4u + k] & 1u));
        }
        if ((sum & 1u) == 0) {
            return false;              /* odd parity: an even nibble is a reject */
        }
    }
    return true;
}

const lf_fsk2a_format_t LF_FSK2A_FORMAT_AWID = {
    .preamble = LF_FSK2A_PREAMBLE_AWID,
    .preamble_bits = AWID_FSK_PREAMBLE_BITS,
    .frame_bits = AWID_FSK_FRAME_BITS,
    .pulses_short = 6,
    .pulses_long = 5,
    .require_repeat = true,
    .accept = awid_accept,
};

void awid_fsk_payload(const uint8_t *word_bits, uint8_t out9[9]) {
    memset(out9, 0, 9);
    /* 22 nibbles x 3 data bits = 66 bits, left-aligned in 72. ⚠ The last 6 bits of the
     * 9-byte buffer are NOT carried on the wire and stay zero — that is the format, not a
     * truncation here (C194). */
    size_t bit = 0;
    for (uint8_t i = 0; i < 22; i++) {
        for (uint8_t k = 0; k < 3; k++, bit++) {
            if (word_bits[8 + i * 4u + k] & 1u) {
                out9[bit / 8] = (uint8_t)(out9[bit / 8] | (0x80u >> (bit % 8)));
            }
        }
    }
}

/* Paradox: `00001111`. */
const uint8_t LF_FSK2A_PREAMBLE_PARADOX[PARADOX_FSK_PREAMBLE_BITS] = { 0, 0, 0, 0, 1, 1, 1, 1 };

/* ⭐ PARADOX'S REAL GATE IS STRUCTURAL, NOT A CHECKSUM: every bit PAIR from 8 to 95 must
 * DIFFER. That is 44 independent one-bit checks — far stronger than its 8-bit preamble, and
 * it is what makes an 8-bit preamble survivable here. */
static bool paradox_accept(const uint8_t *word_bits, uint16_t frame_bits) {
    if (frame_bits < PARADOX_FSK_FRAME_BITS) {
        return false;
    }
    for (uint16_t i = PARADOX_FSK_PREAMBLE_BITS; i < PARADOX_FSK_FRAME_BITS; i += 2) {
        if ((word_bits[i] & 1u) == (word_bits[i + 1] & 1u)) {
            return false;
        }
    }
    return true;
}

const lf_fsk2a_format_t LF_FSK2A_FORMAT_PARADOX = {
    .preamble = LF_FSK2A_PREAMBLE_PARADOX,
    .preamble_bits = PARADOX_FSK_PREAMBLE_BITS,
    .frame_bits = PARADOX_FSK_FRAME_BITS,
    .pulses_short = 6,
    .pulses_long = 5,
    .require_repeat = true,
    .accept = paradox_accept,
};

/* Pyramid: sixteen bits of `0000000000000001` then eight of `00000001`. */
const uint8_t LF_FSK2A_PREAMBLE_PYRAMID[PYRAMID_FSK_PREAMBLE_BITS] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 1,
    0, 0, 0, 0, 0, 0, 0, 1
};

/* ⭐ PYRAMID'S CRC-8, poly 0x31 with input and output REFLECTED — `bit_lib_crc8(data, 13,
 * 0x31, 0x00, true, true, 0x00)`. Reflecting both ends is equivalent to running the mirrored
 * polynomial 0x8C right-to-left, which is what this does.
 *
 * ⛔ The parameters are transcribed and then CHECKED against a real capture before shipping,
 * because Gallagher's were written from memory and were wrong in a way that reads NOTHING
 * rather than reading badly (C172). */
static uint8_t pyramid_crc8(const uint8_t *d, size_t len) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; i++) {
        crc ^= d[i];
        for (uint8_t b = 0; b < 8; b++) {
            crc = (uint8_t)((crc & 1u) ? (((unsigned)crc >> 1) ^ 0x8Cu) : ((unsigned)crc >> 1));
        }
    }
    return crc;
}

static bool pyramid_accept(const uint8_t *word_bits, uint16_t frame_bits) {
    if (frame_bits < PYRAMID_FSK_FRAME_BITS) {
        return false;
    }
    uint8_t body[13];
    for (uint8_t i = 0; i < 13; i++) {
        uint8_t v = 0;
        for (uint8_t k = 0; k < 8; k++) {
            v = (uint8_t)(((unsigned)v << 1) | (word_bits[16 + i * 8u + k] & 1u));
        }
        body[i] = v;
    }
    uint8_t want = 0;
    for (uint8_t k = 0; k < 8; k++) {
        want = (uint8_t)(((unsigned)want << 1) | (word_bits[120 + k] & 1u));
    }
    return pyramid_crc8(body, 13) == want;
}

const lf_fsk2a_format_t LF_FSK2A_FORMAT_PYRAMID = {
    .preamble = LF_FSK2A_PREAMBLE_PYRAMID,
    .preamble_bits = PYRAMID_FSK_PREAMBLE_BITS,
    .frame_bits = PYRAMID_FSK_FRAME_BITS,
    .pulses_short = 6,
    .pulses_long = 5,
    .require_repeat = true,
    .accept = pyramid_accept,
};

/* FDX-A: 0x55 then 0x1D. */
const uint8_t LF_FSK2A_PREAMBLE_FDXA[FDXA_FSK_PREAMBLE_BITS] = {
    0, 1, 0, 1, 0, 1, 0, 1,   /* 0x55 */
    0, 0, 0, 1, 1, 1, 0, 1    /* 0x1D */
};

/* ⭐ Manchester INSIDE the FSK. Returns false on a matched pair, which is an encoding
 * violation rather than a data value — and is most of this format's gate. */
static bool fdxa_manchester(const uint8_t *word_bits, uint8_t out5[5]) {
    for (uint8_t i = 0; i < 5; i++) {
        out5[i] = 0;
    }
    for (uint8_t k = 0; k < 40; k++) {
        const uint8_t a = word_bits[16 + k * 2u] & 1u;
        const uint8_t b = word_bits[17 + k * 2u] & 1u;
        if (a == b) {
            return false;
        }
        if (a) {                                  /* 10 -> 1, 01 -> 0 */
            out5[k / 8] = (uint8_t)(out5[k / 8] | (0x80u >> (k % 8)));
        }
    }
    return true;
}

void fdxa_fsk_payload(const uint8_t *word_bits, uint8_t out5[5]) {
    (void)fdxa_manchester(word_bits, out5);
}

/* ⛔ THREE CHECKS ON TOP OF THE PREAMBLE: the Manchester pair rule (40 of them), odd parity
 * on each of the five decoded bytes, and the frame repeat handled by `require_repeat`. */
static bool fdxa_accept(const uint8_t *word_bits, uint16_t frame_bits) {
    if (frame_bits < FDXA_FSK_FRAME_BITS) {
        return false;
    }
    uint8_t dec[5];
    if (!fdxa_manchester(word_bits, dec)) {
        return false;
    }
    for (uint8_t i = 0; i < 5; i++) {
        uint8_t sum = 0;
        for (uint8_t b = 0; b < 8; b++) {
            sum = (uint8_t)(sum + ((dec[i] >> b) & 1u));
        }
        if ((sum & 1u) == 0) {
            return false;                          /* odd parity per byte */
        }
    }
    return true;
}

const lf_fsk2a_format_t LF_FSK2A_FORMAT_FDXA = {
    .preamble = LF_FSK2A_PREAMBLE_FDXA,
    .preamble_bits = FDXA_FSK_PREAMBLE_BITS,
    .frame_bits = FDXA_FSK_FRAME_BITS,
    .pulses_short = 6,
    .pulses_long = 5,
    .require_repeat = true,
    .accept = fdxa_accept,
};

bool lf_fsk2a_decode_fmt(int16_t *samples, size_t n,
                         const lf_fsk2a_format_t *fmt, lf_decode_result_t *out) {
    memset(out, 0, sizeof(*out));
    const uint16_t FB = fmt->frame_bits;
    /* A bit is 50 carrier cycles; demand a frame plus slack before trying. */
    if (n < (size_t)(FB + 8) * 50u) {
        return false;
    }

    static uint8_t bits[LF_SAMPLED_MAX_CAPTURE_SAMPLES / 50u + 8u];
    int32_t best_energy = 0;

    for (uint8_t lp = 1; lp <= 3; lp += 2) {
        lf_slicer_build_dc(samples, n);

        /* ⭐ PAIR ADJACENT RUNS INTO FULL SUB-PERIODS. The slicer's duty bias lives in the
         * SPLIT between the two halves and cancels in the sum — which is the whole reason
         * this family decodes where the ASK edge route did not (C145, C171, C193). */
        size_t nb = 0;
        uint32_t run = 0, prev_run = 0;
        bool have_prev = false;
        bool cur = lf_slicer_level(samples, n, 0, lp);
        int last_pulse = -1;
        uint32_t count = 0;

        for (size_t i = 1; i < n && nb + 8 < sizeof(bits); i++) {
            bool lvl = lf_slicer_level(samples, n, i, lp);
            run++;
            if (lvl == cur) {
                continue;
            }
            cur = lvl;
            if (!have_prev) {
                prev_run = run; have_prev = true; run = 0; continue;
            }
            const uint32_t period = prev_run + run;
            have_prev = false; run = 0;

            if (period < LF_FSK2A_MIN_PERIOD || period >= LF_FSK2A_MAX_PERIOD) {
                count = 0;                    /* not a tone — drop the partial run */
                continue;
            }
            const int pulse = (period >= (LF_FSK2A_SHORT_SAMPLES + LF_FSK2A_LONG_SAMPLES) / 2)
                              ? 1 : 0;
            count++;
            if (last_pulse >= 0 && pulse != last_pulse) {
                const uint8_t per_bit = last_pulse ? fmt->pulses_long : fmt->pulses_short;
                uint32_t emit = (count + 1u) / per_bit;
                while (emit-- > 0 && nb + 8 < sizeof(bits)) {
                    bits[nb++] = (uint8_t)last_pulse;
                }
                count = 0;
            }
            last_pulse = pulse;
        }

        if (nb > (size_t)best_energy) {
            best_energy = (int32_t)nb;        /* bits recovered: "something was there" */
        }

        for (size_t i = 0; i + FB <= nb; i++) {
            bool match = true;
            for (uint8_t k = 0; k < fmt->preamble_bits && match; k++) {
                match = (bits[i + k] == fmt->preamble[k]);
            }
            if (!match) {
                continue;
            }
            /* ⛔ The preamble again one frame later — AWID's own second check, and most of
             * the gate for an 8-bit preamble. */
            if (fmt->require_repeat) {
                if (i + FB + fmt->preamble_bits > nb) {
                    continue;
                }
                bool rep = true;
                for (uint8_t k = 0; k < fmt->preamble_bits && rep; k++) {
                    rep = (bits[i + FB + k] == fmt->preamble[k]);
                }
                if (!rep) {
                    continue;
                }
            }
            if (fmt->accept != NULL && !fmt->accept(&bits[i], FB)) {
                continue;
            }
            memset(out->id, 0, sizeof(out->id));
            for (uint16_t k = 0; k < FB; k++) {
                out->id[k / 8] = (uint8_t)(((unsigned)out->id[k / 8] << 1) | bits[i + k]);
            }
            memcpy(out->word_bits, &bits[i], FB);
            out->frame_bits = FB;
            out->bit_pos = (uint8_t)(i & 0xFFu);
            out->energy = best_energy;
            return true;
        }
    }

    out->energy = best_energy;
    return false;
}

bool awid_fsk_decode(int16_t *samples, size_t n, lf_decode_result_t *out) {
    return lf_fsk2a_decode_fmt(samples, n, &LF_FSK2A_FORMAT_AWID, out);
}

bool paradox_fsk_decode(int16_t *samples, size_t n, lf_decode_result_t *out) {
    return lf_fsk2a_decode_fmt(samples, n, &LF_FSK2A_FORMAT_PARADOX, out);
}

bool pyramid_fsk_decode(int16_t *samples, size_t n, lf_decode_result_t *out) {
    return lf_fsk2a_decode_fmt(samples, n, &LF_FSK2A_FORMAT_PYRAMID, out);
}

bool fdxa_fsk_decode(int16_t *samples, size_t n, lf_decode_result_t *out) {
    return lf_fsk2a_decode_fmt(samples, n, &LF_FSK2A_FORMAT_FDXA, out);
}
