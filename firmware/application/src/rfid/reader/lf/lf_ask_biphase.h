#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lf_indala_psk.h"   /* lf_decode_result_t, and the shared capture engine's types */

/*
 * ASK / BIPHASE demodulator — the fourth decode path on this device, and the first that never
 * asks what level the envelope is sitting at.
 *
 * ⭐⭐ WHY IT IS NOT lf_ask_manchester.c WITH A FLAG. That decoder samples the LEVEL at two
 * half-bit centres and calls the transition between them the bit. At RF/64 there is no level
 * left to sample: the LF front end is AC-coupled with a time constant of about 27 samples —
 * a step to 14112 decays to 4664 in 30 — so a 32-sample half-bit has returned to the baseline
 * before its centre. ⛔ Measured, not argued: half-bit-centre sampling of a real GProxII tag
 * finds NO frame across 3 low-pass widths x 32 phases x 2 pairings x 2 inversions, on four
 * captures. The blank is the finding (C204).
 *
 * ⭐⭐ WHAT BIPHASE GIVES YOU INSTEAD. There is a transition at EVERY bit boundary by
 * construction, and an extra one mid-bit for one of the two symbol values. So:
 *
 *   - the boundaries are a self-calibrating amplitude reference. What a transition "looks
 *     like" is measured from this capture, at this coupling, at this field strength, rather
 *     than being a constant that would need retuning per bench.
 *   - one decision per bit, not two: did the middle step too?
 *
 * ⇒ `bit = 1` iff a mid-bit transition. Exact on the bench credential, 96 of 96 (C204).
 *
 * ⛔⛔ THE 24-BIT GATE BOTH REFERENCES USE IS NOT ENOUGH HERE, AND THE CROSS-PROTOCOL NULL IS
 * WHAT PROVED IT. With only the 6-bit preamble and the 18 spacers — exactly what the Proxmark
 * and Momentum each check — this decoder claimed a GProxII frame on **Securakey 4 of 4,
 * Noralsy 4 of 4, Gallagher 4 of 8, InstaFob 2 of 3**. ⛔ And the Securakey ones were STABLE:
 * `f858000c64680003858000c6` on three separate captures, which means the capture engine's
 * two-agreeing-stacks rule would have confirmed it rather than caught it. A reader that says
 * "GProxII" to a Securakey tag is worse than one that says nothing.
 *
 * ⭐ THE FIX IS THE FORMAT'S OWN LENGTH FIELD, and it is a deviation from both references that
 * is stated rather than smuggled. Strip the spacers, take the XOR key the tag carries in its
 * first byte, un-XOR the second, and its top six bits are the format length. pm3 and Momentum
 * both COMPUTE it and both know exactly two values, 26 and 36 — but both only *report* an
 * unknown length, because a single-protocol demod has nothing to be confused with. We do.
 * ⇒ Nulls with the length gate: Securakey 0/4, Noralsy 0/4, Gallagher 0/8, InstaFob 0/3,
 * Keri 0/8, Indala224 0/4, IDTECK 0/16, AWID 0/6, PAC 0/4, empty 0/5, and 0 of the 320 front
 * captures. ⚠ **One survives: NexWatch 1 of 4.** Not blank, so it is reported as it is — but
 * unlike the Securakey case it is sporadic, which is the shape two-agreeing-stacks does catch.
 * ⚠ THE COST, SAID PLAINLY: a genuine GProxII whose format length is neither 26 nor 36 would
 * be rejected here. Neither reference has ever seen one; that is not the same as none existing.
 *
 * ⚠ THE THRESHOLD REFERENCE IS THE MEAN, NOT THE MEDIAN, and that was measured both ways:
 * against the same capture the mean at 0.3 wins on NINE sample phases where the median wins
 * on one or two. A decoder that only works at one phase is one drift away from working at
 * none. The mean also needs no sort, and so no second static buffer.
 *
 * ⛔ THIS DOES NOT RETIRE C171. Edge TIMING lost to level sampling on Gallagher, and it lost
 * for a reason that still holds: run lengths carry a systematic duty bias (C141). This
 * decoder does not measure when a transition happened — the grid says when — it measures only
 * WHETHER one happened, at a position the bit clock already fixes. Those are different
 * instruments and the scar from one does not transfer to the other.
 */

/* ⛔ Per-protocol, exactly as in the ASK/Manchester family — GProxII is RF/64 and FDX-B is
 * RF/32, so this is never a family constant. */
#define LF_BIPHASE_MAX_BIT_SAMPLES 64
#define LF_BIPHASE_MIN_BIT_SAMPLES 32

/** The window either side of a grid point whose means are differenced to get the step. */
#define LF_BIPHASE_SLOPE_WINDOW 8

/** Longest frame any biphase format here uses. GProxII is 96 bits; FDX-B will be 128. */
#define LF_BIPHASE_MAX_FRAME_BITS 128
_Static_assert(LF_BIPHASE_MAX_FRAME_BITS <= LF_DECODE_MAX_FRAME_BITS,
               "the shared result buffer must hold the longest biphase frame too");
#define LF_BIPHASE_MAX_PREAMBLE_BITS 16

/* GProxII (Guardall G-Prox II / Verex / Chubb): 96 bits at RF/64, T5577 config `00150060`,
 * which `lf t55xx detect` reads back as BIPHASE / RF/64 / not inverted.
 *
 * ⛔⛔ THE HALF-BIT IS 32 SAMPLES AND THAT IS MEASURED, NOT READ OFF THE CONFIG WORD. The
 * Proxmark demodulates this protocol with `ASKbiphaseDemod(0, 64, ...)`, whose pairing step
 * then HALVES the bit count — which cannot also yield a 96-bit frame from 96 stored T5577
 * bits. The two readings of "RF/64" disagree, so the capture was asked instead:
 * autocorrelation puts the frame period at exactly 6144 samples, and 6144 / 96 = 64 samples
 * per BIT. ⇒ bit 64, half-bit 32.
 *
 * ⭐ ITS GATE IS 24 EXACT BITS, not the 6-bit preamble alone. After the preamble the frame is
 * 18 groups of five whose FIFTH bit is a spacer and must be zero — `removeParity(..., 5, 3,
 * 90)` in the Proxmark, where ptype 3 means "should be 0 spacer bit" rather than a parity
 * test. Six preamble bits on their own would match noise constantly; with the spacers it is
 * a stronger gate than Securakey's 19 (C204). */
#define GPROXII_BIPHASE_FRAME_BITS    96
#define GPROXII_BIPHASE_PREAMBLE_BITS 6
#define GPROXII_BIPHASE_BIT_SAMPLES   64
#define GPROXII_BIPHASE_SPACER_GROUPS 18

/* ⛔⛔ MEASURED BY TRUNCATION, AND THE GUESS WAS WRONG A FOURTH TIME. This constant read
 * 12288 — "two whole frames", the same reasoning that has now failed for NexWatch, Gallagher
 * and Noralsy (C161, C165). Four drive-7 captures, truncated:
 *
 *     6400            0 of 4
 *     7168 .. 12544   1 of 4      <- 12288 is what the guess would have shipped
 *     12800 .. 14336  4 of 4
 *
 * ⭐ The threshold is EXACTLY explained, which is rare here. Every capture decodes at bit
 * position 100, and 100 + 96 = 196 bits x 64 samples = 12544 — the last length that fails.
 * 12800 is 200 bit slots: the 196 the frame needs plus the slope windows either side.
 *
 * ⚠ SHIPS AT THE BUFFER MAXIMUM WITH ONLY 1.12x HEADROOM, which is the thinnest margin of
 * any format here (Noralsy has 1.7x, Gallagher 1.4x). Securakey and InstaFob carry the same
 * warning: there is no room left, so a GProxII variant needing a longer capture could not be
 * added without growing LF_SAMPLED_MAX_CAPTURE_SAMPLES. 14336 is 114ms on the wire. */
#define GPROXII_BIPHASE_CAPTURE_SAMPLES 14336
_Static_assert(GPROXII_BIPHASE_CAPTURE_SAMPLES <= LF_SAMPLED_MAX_CAPTURE_SAMPLES,
               "GProxII's capture must fit the shared sample buffer");

extern const uint8_t LF_BIPHASE_PREAMBLE_GPROXII[GPROXII_BIPHASE_PREAMBLE_BITS];

/** A biphase format, mirroring `lf_ask_format_t` so the families read alike. */
typedef struct {
    const uint8_t *preamble;   /**< one byte per bit, MSB of the frame first. */
    uint8_t  preamble_bits;
    uint16_t frame_bits;
    /** ⛔ Carrier cycles per BIT (two half-bits). RF/64 for GProxII. */
    uint8_t  bit_samples;
    /** ⭐ The format's own acceptance rule, checked INSIDE the candidate search. NULL for
     *  none — but see the header note: a 6-bit preamble without one is not a gate. */
    bool (*accept)(const uint8_t *word_bits, uint16_t frame_bits);
} lf_biphase_format_t;

extern const lf_biphase_format_t LF_BIPHASE_FORMAT_GPROXII;

/**
 * Demodulate one ASK/biphase frame from a carrier-locked capture.
 *
 * ⚠ `samples` is NOT modified.
 *
 * @param out  filled on success; `energy` is set either way and is the only field readable
 *             after a false return. Here energy is the mean bit-boundary step in ADC counts,
 *             which is a direct measure of whether anything is modulating at all.
 */
bool lf_ask_biphase_decode_fmt(const int16_t *samples, size_t n,
                               const lf_biphase_format_t *fmt, lf_decode_result_t *out);

/** GProxII's decode, in the shape the shared capture engine wants. */
bool gproxii_biphase_decode(int16_t *samples, size_t n, lf_decode_result_t *out);
