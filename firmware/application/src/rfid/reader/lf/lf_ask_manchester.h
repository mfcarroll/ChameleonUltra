#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lf_indala_psk.h"   /* indala_psk_result_t, and the shared capture engine's types */

/*
 * ASK / Manchester demodulator at RF/32 — the biphase family's decoder.
 *
 * ⭐⭐ IT SAMPLES LEVELS AT BIT CENTRES AND NEVER MEASURES AN EDGE, and that is a measured
 * choice rather than a stylistic one. On a real Gallagher tag through this front end the
 * bit-centre path decoded 4 captures of 4 exactly, while an edge/run-length decoder over the
 * SAME captures found NOTHING at any of 8 low-pass widths x 2 pairing phases x 2 polarities
 * (C171). That is C145 repeating itself in a second modulation family: there, edge timings
 * scored 26 errors of 128 against the level path's 6 on PAC.
 *
 * ⛔ WHY EDGES LOSE HERE. Rising and falling edges cross the slicer at different points, so
 * every run length carries a systematic duty bias — 1-bit runs measuring 30.0 samples where
 * the true period is 32.000 (C141). That bias corrupts each run length while leaving the
 * LEVEL in the middle of each half-bit untouched. ⇒ Sample where the bias is not.
 *
 * ⛔ THIS IS ALSO A DEVIATION FROM NEXT.md §10, WHICH PLANNED THE GPIO/COMPARATOR PATH FOR
 * THIS FAMILY, and the deviation is evidence-led: the comparator is an edge instrument.
 * ⚠ Stated honestly: C171's edge arm is a HOST SIMULATION of slicing over ADC samples, not
 * the comparator silicon itself, so this is strong evidence and not proof about that
 * peripheral. What it is proof of is that the SAADC + bit-centre route works, which is the
 * one that can be verified end to end today.
 *
 * ⭐ Manchester survives the amplifier saturation that ruins NRZ: the bit is WHICH WAY the
 * level moves at mid-bit, so clipping destroys magnitude and preserves sign. Measured — the
 * same decoder reads the clipped captures 4 of 4 as well as the clean ones (C169 corrected).
 */

/** Samples per bit. The biphase family here is RF/32, one sample per carrier cycle. */
#define LF_ASK_BIT_SAMPLES   32

/** Longest frame any ASK format here uses. Gallagher is 96 bits. */
#define LF_ASK_MAX_FRAME_BITS  128
#define LF_ASK_MAX_PREAMBLE_BITS 16

/** Gallagher: 96-bit frame, 16-bit preamble 0x7FEA (C171). */
#define GALLAGHER_ASK_FRAME_BITS    96
#define GALLAGHER_ASK_PREAMBLE_BITS 16

/* ⛔⛔ MEASURED BY TRUNCATION, AND THE GUESS WAS WRONG BY MORE THAN A FACTOR OF TWO — which
 * is the third time this rule has paid for itself (C161, C165). This constant read 6144
 * ("two frames, as NexWatch uses") until it was measured. Same four captures, truncated:
 *
 *     3072 .. 6144   0 of 4      <- 6144 is what the guess would have shipped
 *     7168 .. 9216   3 of 4
 *     10240 .. 14336 4 of 4
 *
 * ⭐ A 96-bit frame is 3072 samples, so the threshold is ~3.3 frames, not 2. The reason is
 * visible in the decode: the winning frame starts at bit 104 or 200 in most captures, never
 * at 0 — the bit-centre search has to find BOTH a sample phase that slices cleanly and a
 * whole frame after it, and those two conditions are not satisfied by the first frame in the
 * buffer. ⚠ Inferred from the reported bit positions, not demonstrated.
 *
 * ⚠ 14336 is 114ms on the wire against Indala26's 33ms, so a Gallagher read is ~3.5x slower.
 * That is the honest cost of this protocol, not a tuning failure. */
#define GALLAGHER_ASK_CAPTURE_SAMPLES 14336

extern const uint8_t LF_ASK_PREAMBLE_GALLAGHER[GALLAGHER_ASK_PREAMBLE_BITS];

/** An ASK format, mirroring `lf_psk1_format_t` so the two families read alike. */
typedef struct {
    const uint8_t *preamble;   /**< one byte per bit, MSB of the frame first. */
    uint8_t  preamble_bits;
    uint16_t frame_bits;
    /** ⭐ The format's own acceptance rule, checked INSIDE the candidate search — the same
     *  shape and the same reason as `lf_psk1_format_t.accept`. NULL for none. */
    bool (*accept)(const uint8_t *word_bits, uint16_t frame_bits);
} lf_ask_format_t;

extern const lf_ask_format_t LF_ASK_FORMAT_GALLAGHER;

/**
 * Demodulate one ASK/Manchester frame from a carrier-locked capture.
 *
 * ⚠ `samples` is NOT modified — unlike the PSK decoder, which converts in place. The level
 * path needs the original amplitudes, so the caller's buffer is left intact.
 *
 * @param out  filled on success; `energy` is set either way and is the only field readable
 *             after a false return.
 */
bool lf_ask_manchester_decode_fmt(int16_t *samples, size_t n,
                                  const lf_ask_format_t *fmt, indala_psk_result_t *out);

/** Gallagher's decode, in the shape the shared capture engine wants. */
bool gallagher_ask_decode(int16_t *samples, size_t n, indala_psk_result_t *out);
