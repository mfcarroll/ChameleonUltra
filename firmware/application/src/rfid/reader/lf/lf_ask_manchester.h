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

/* ⛔⛔ THE BIT RATE IS PER-PROTOCOL AND IS NOT RF/32 FOR THE WHOLE FAMILY. Gallagher is
 * RF/32, Securakey is RF/40, and em410x on the other reader is RF/64. This was a hard-coded
 * 32 for exactly one protocol's lifetime; it is a format field now, because a decoder that
 * silently samples a 40-sample bit at 32 finds nothing and gives no hint why. */
#define LF_ASK_MAX_BIT_SAMPLES   64
#define LF_ASK_MIN_BIT_SAMPLES   32

/** Longest frame any ASK format here uses. Gallagher, Securakey and Noralsy are 96 bits;
 *  ⛔ InstaFob is 225, because the tag transmits its whole T5577 page 0. */
#define LF_ASK_MAX_FRAME_BITS  240
_Static_assert(LF_ASK_MAX_FRAME_BITS <= LF_PSK1_MAX_FRAME_BITS,
               "the shared result buffer must hold the longest ASK frame too");
/** ⛔ 19 is Securakey's, and all 19 must be the preamble: the first 10 are a constant run
 *  and the next 9 are the format selector, so truncating to 16 would keep the run and throw
 *  away the discriminating half. */
#define LF_ASK_MAX_PREAMBLE_BITS 24

/** Gallagher: 96-bit frame, 16-bit preamble 0x7FEA, RF/32 (C171). */
#define GALLAGHER_ASK_FRAME_BITS    96
#define GALLAGHER_ASK_PREAMBLE_BITS 16
#define GALLAGHER_ASK_BIT_SAMPLES   32

/* Securakey: 96-bit frame at RF/40, config `000C8060`, and a 19-bit preamble.
 *
 * ⚠ THE PREAMBLE IS 10 CONSTANT BITS PLUS A 9-BIT FORMAT SELECTOR, and there are THREE known
 * selectors — `000000000`, `001011010` and `001100000` — of which only the middle one is on
 * this bench. The other two are NOT implemented, because a format nobody here can test is
 * worth less than nothing: it looks supported. That is `idteck.c`'s lesson.
 *
 * ⛔⛔ AND UNLIKE GALLAGHER THERE IS NO COMPUTED CHECK TO GATE ON. The Proxmark's own reader
 * says so in as many words — "How the checksum is calculated is unknown" — and Momentum's
 * `can_be_decoded` tests nothing but those 19 bits. So this format's entire gate is 19 bits,
 * 10 of which are a run, where Gallagher has 16 bits plus a CRC-8. ⇒ Its cross-protocol null
 * is not a formality here, it is the only evidence that the gate holds. */
/* Noralsy: 96-bit frame at RF/32, config `00088068`.
 *
 * ⚠ ONE BIT OF CONFIG FROM GALLAGHER'S `00088060`, AND `lf t55xx detect` CANNOT IDENTIFY IT
 * — the Proxmark reports "Could not detect modulation automatically" on a tag its own clone
 * command just wrote. ⇒ Do NOT use `detect` as the oracle for this protocol; the clone's
 * block dump is the authority, and it is what T5577_NORALSY_CONFIG is copied from.
 *
 * ⭐ ITS GATE IS THE STRONGEST IN THE ASK FAMILY: a 12-bit preamble plus TWO computed 4-bit
 * checksums, where Gallagher has 16 bits + CRC-8 and Securakey has 19 bits and nothing.
 * ⚠ Momentum checks only 12 preamble bits and says why in a comment: the frame's next 20 look
 * constant on every specimen but are not confirmed to be. Copied verbatim rather than
 * "improved" to 32 — widening a gate on an unconfirmed constant is how a format starts
 * rejecting legitimate tags nobody has seen yet. */
#define NORALSY_ASK_FRAME_BITS      96
#define NORALSY_ASK_PREAMBLE_BITS   12
#define NORALSY_ASK_BIT_SAMPLES     32

/* InstaFob: 225 bits at RF/32, and its structure is unlike anything else in this family.
 *
 * ⭐⭐ ITS "PREAMBLE" IS THE T5577 CONFIGURATION WORD ITSELF, TRANSMITTED AS DATA. The frame
 * is 1 + 7x32 bits — the tag clocks out its ENTIRE page 0, block 0 included — and Momentum
 * identifies it by finding `0x00107060` at bit 7 of that stream. So the 32 "fixed" bits this
 * format gates on are not a vendor preamble at all; they are the chip's own configuration.
 *
 * ⭐ That makes the gate strong — 32 exact bits, against Securakey's 19 — but it also means a
 * DIFFERENTLY CONFIGURED InstaFob tag would not match, and there is no way to know from here
 * whether other configurations exist in the field.
 *
 * ⚠ THE FRAME THIS DECODER RETURNS IS A ROTATION of Momentum's buffer: we search for the
 * config word and return 225 bits starting there, where Momentum's numbering puts it at bit
 * 7. The two are the same cycle 7 bits apart — exactly the Keri situation (C158) — and
 * anything comparing our raw against a reference must account for it. */
#define INSTAFOB_ASK_FRAME_BITS     225
#define INSTAFOB_ASK_PREAMBLE_BITS  32
#define INSTAFOB_ASK_BIT_SAMPLES    32

#define SECURAKEY_ASK_FRAME_BITS    96
#define SECURAKEY_ASK_PREAMBLE_BITS 19
#define SECURAKEY_ASK_BIT_SAMPLES   40

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

/* ⛔ MEASURED, like every other capture length here, and the prediction held. Securakey's bit
 * is RF/40, so its 96-bit frame is 3840 samples against Gallagher's 3072 — less slack in the
 * same buffer — and the threshold came out HIGHER as expected: 7680 -> 0 of 4, 9216 -> 4 of
 * 4, flat to 14336, against Gallagher's 10240 measured in frames rather than samples (2.4
 * frames here, 3.3 there). ⚠ 14336 is the shipped value and also the buffer maximum, so this
 * format has NO headroom left: a variant needing a longer capture could not be added without
 * growing LF_PSK1_MAX_CAPTURE_SAMPLES. Worth knowing before the next RF/40 protocol. */
#define SECURAKEY_ASK_CAPTURE_SAMPLES 14336

/* ⛔ MEASURED by truncation like the rest, and this one is the CHEAPEST in the family:
 *
 *     3456 samples  0 of 4
 *     3584 samples  4 of 4      <- 112 bits: the 96-bit frame plus 16 of slack
 *     3712 .. 14336 4 of 4
 *
 * ⭐ Against Gallagher's 10240 at the SAME bit rate and the SAME frame length. The difference
 * is where the frame sits: Noralsy's decodes at bit 12 of the stream where Gallagher's sits
 * at 104, so far less of the buffer is spent reaching the first whole frame. ⚠ Inferred from
 * the reported bit positions, not demonstrated — the same caveat C165 carries.
 *
 * 6144 is two whole frames and 1.7x the measured threshold, at 49ms on the wire against the
 * 114ms Gallagher and Securakey need. */
#define NORALSY_ASK_CAPTURE_SAMPLES 6144

/* ⚠ THE BUFFER MAXIMUM, AND IT IS ONLY 1.99 FRAMES. A 225-bit frame at RF/32 is 7200 samples,
 * so a whole frame lands inside for 7136 of 7200 start offsets — 99.1%, not the 100% every
 * other format here enjoys. A cost rather than a wall, and the reason this format has no
 * margin left to give (C185). */
#define INSTAFOB_ASK_CAPTURE_SAMPLES 14336
_Static_assert(INSTAFOB_ASK_CAPTURE_SAMPLES <= LF_PSK1_MAX_CAPTURE_SAMPLES,
               "InstaFob's capture must fit the shared sample buffer");

extern const uint8_t LF_ASK_PREAMBLE_GALLAGHER[GALLAGHER_ASK_PREAMBLE_BITS];
extern const uint8_t LF_ASK_PREAMBLE_SECURAKEY[SECURAKEY_ASK_PREAMBLE_BITS];
extern const uint8_t LF_ASK_PREAMBLE_NORALSY[NORALSY_ASK_PREAMBLE_BITS];
extern const uint8_t LF_ASK_PREAMBLE_INSTAFOB[INSTAFOB_ASK_PREAMBLE_BITS];

/** An ASK format, mirroring `lf_psk1_format_t` so the two families read alike. */
typedef struct {
    const uint8_t *preamble;   /**< one byte per bit, MSB of the frame first. */
    uint8_t  preamble_bits;
    uint16_t frame_bits;
    /** ⛔ Carrier cycles per bit — RF/32 for Gallagher, RF/40 for Securakey. Not a family
     *  constant; see the note at LF_ASK_MAX_BIT_SAMPLES. */
    uint8_t  bit_samples;
    /** ⭐ The format's own acceptance rule, checked INSIDE the candidate search — the same
     *  shape and the same reason as `lf_psk1_format_t.accept`. NULL for none. */
    bool (*accept)(const uint8_t *word_bits, uint16_t frame_bits);
} lf_ask_format_t;

extern const lf_ask_format_t LF_ASK_FORMAT_GALLAGHER;
extern const lf_ask_format_t LF_ASK_FORMAT_SECURAKEY;
extern const lf_ask_format_t LF_ASK_FORMAT_NORALSY;
extern const lf_ask_format_t LF_ASK_FORMAT_INSTAFOB;

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

/** Securakey's decode. ⚠ Its gate is the 19-bit preamble alone — see the note above. */
bool securakey_ask_decode(int16_t *samples, size_t n, indala_psk_result_t *out);

/** Noralsy's decode — 12-bit preamble plus TWO computed nibble checksums. */
bool noralsy_ask_decode(int16_t *samples, size_t n, indala_psk_result_t *out);

/** InstaFob's decode. ⚠ Returns a 225-bit frame ROTATED 7 bits from Momentum's numbering. */
bool instafob_ask_decode(int16_t *samples, size_t n, indala_psk_result_t *out);
