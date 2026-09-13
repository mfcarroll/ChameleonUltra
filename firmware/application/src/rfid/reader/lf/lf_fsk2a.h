#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lf_indala_psk.h"   /* lf_decode_result_t, and the shared capture engine's types */

/*
 * FSK2a demodulator — the AWID / Paradox / Pyramid family.
 *
 * ⭐⭐ EASIER ON THIS FRONT END THAN ASK WAS, WHICH IS NOT THE ORDER §10 EXPECTED. The data
 * lives in the SUB-PERIOD, and the sampler takes one sample per carrier cycle, so the two
 * tones are 8 and 10 samples — far above Nyquist and trivially separable. Measured on a real
 * emission: 1650 of 1655 sub-periods land exactly on 8 or 10 (C193).
 *
 * ⛔ THE SLICER'S DUTY BIAS CANCELS HERE, and that is why this works where the ASK edge
 * decoder did not. An 8-sample period splits 3+5 and a 10-sample splits 4+6 — the same bias
 * C145 measured and C171 had to route around — but only the SUM is used, and the bias is in
 * the split.
 *
 * ⛔⛔ AND IT DOES NOT TOUCH THE HID/ioProx SAADC MACHINERY. §10 ranked this family LAST
 * because it "reuses" that path, where HID's unexplained 15-20% intermittency lives (C45).
 * It does not: this is a decoder handed to the SHARED capture engine, which carries the
 * BLE-advertising guard that C47 identified as missing from the per-protocol readers — the
 * leading explanation for that very intermittency. ⇒ This path may FIX C45 rather than
 * inherit it, and that is worth testing once HID can be re-measured.
 */

/** Carrier cycles in the two tones. RF/8 and RF/10 for this whole family. */
#define LF_FSK2A_SHORT_SAMPLES 8
#define LF_FSK2A_LONG_SAMPLES  10
/** A period outside this range is not a tone at all and resets the run. */
#define LF_FSK2A_MIN_PERIOD    6
#define LF_FSK2A_MAX_PERIOD    13

/** Longest frame this family uses. AWID, Paradox and Pyramid are all 96 or 128 bits. */
#define LF_FSK2A_MAX_FRAME_BITS 128
#define LF_FSK2A_MAX_PREAMBLE_BITS 16

/** AWID: 96-bit frame, 8-bit preamble `00000001`, and 66 carried payload bits. */
#define AWID_FSK_FRAME_BITS    96
#define AWID_FSK_PREAMBLE_BITS 8
/** ⛔ MEASURED, like every capture length here — see FINDINGS.md. */
#define AWID_FSK_CAPTURE_SAMPLES 14336

extern const uint8_t LF_FSK2A_PREAMBLE_AWID[AWID_FSK_PREAMBLE_BITS];

typedef struct {
    const uint8_t *preamble;
    uint8_t  preamble_bits;
    uint16_t frame_bits;
    /** ⭐ Sub-periods per bit. Six RF/8 make a '0' and five RF/10 make a '1' — both exactly
     *  50 carrier cycles, which is the bit period. Transcribed from Momentum's
     *  `fsk_demod_alloc(MIN_TIME, 6, MAX_TIME, 5)`. */
    uint8_t  pulses_short;
    uint8_t  pulses_long;
    /** ⭐ Require the preamble again one frame later. AWID's own `can_be_decoded` does, and
     *  for a format whose preamble is only 8 bits it is most of the gate. */
    bool     require_repeat;
    /** The format's own acceptance rule, checked inside the candidate search. NULL for none. */
    bool (*accept)(const uint8_t *word_bits, uint16_t frame_bits);
} lf_fsk2a_format_t;

extern const lf_fsk2a_format_t LF_FSK2A_FORMAT_AWID;

/** Demodulate one FSK2a frame. ⚠ `samples` is NOT modified. */
bool lf_fsk2a_decode_fmt(int16_t *samples, size_t n,
                         const lf_fsk2a_format_t *fmt, lf_decode_result_t *out);

/** AWID's decode, in the shape the shared capture engine wants. */
bool awid_fsk_decode(int16_t *samples, size_t n, lf_decode_result_t *out);

/** The 66 payload bits an AWID frame carries, left-aligned into 9 bytes.
 *  ⭐ Each nibble at `8 + 4i` is THREE data bits plus an odd-parity LSB (C194). */
void awid_fsk_payload(const uint8_t *word_bits, uint8_t out9[9]);
