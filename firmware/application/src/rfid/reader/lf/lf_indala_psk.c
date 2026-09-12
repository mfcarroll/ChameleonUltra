#include <limits.h>
#include <string.h>
#include "lf_indala_psk.h"

/*
 * ⭐ THE PREAMBLE IS THE SAME FOR EVERY INDALA TAG (cmdlfindala.c:50), which is what makes
 * searching for it legitimate rather than searching for the answer. It also happens to be
 * the first 33 bits of the bench tag a0000000e6bd0e92 — 1010, then the 28-bit zero run,
 * then a 1.
 */
const uint8_t LF_PSK1_PREAMBLE_INDALA[INDALA_PSK_PREAMBLE_BITS] = {
    1, 0, 1, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1
};

/* IDTECK: 0x4944544B, "IDTK" in ASCII, MSB first on air. Unlike Indala's this one has no
 * long constant run, so it is a far more selective pattern — which is why an Indala reader
 * cannot mistake an IDTECK tag for a credential, only for noise (C85). */
const uint8_t LF_PSK1_PREAMBLE_IDTECK[IDTECK_PSK_PREAMBLE_BITS] = {
    0, 1, 0, 0, 1, 0, 0, 1,   /* 0x49 'I' */
    0, 1, 0, 0, 0, 1, 0, 0,   /* 0x44 'D' */
    0, 1, 0, 1, 0, 1, 0, 0,   /* 0x54 'T' */
    0, 1, 0, 0, 1, 0, 1, 1    /* 0x4B 'K' */
};

/* Indala224: a 1 then 29 zeros. ⛔ Almost pure constant run — see the header. */
const uint8_t LF_PSK1_PREAMBLE_INDALA224[INDALA224_PSK_PREAMBLE_BITS] = {
    1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

const lf_psk1_format_t LF_PSK1_FORMAT_INDALA64 = {
    .preamble = LF_PSK1_PREAMBLE_INDALA,
    .preamble_bits = INDALA_PSK_PREAMBLE_BITS,
    .frame_bits = INDALA_PSK_FRAME_BITS,
    /* ⛔ The IDTECK veto, one-directional on purpose — C90/C91. */
    .reject_preamble = LF_PSK1_PREAMBLE_IDTECK,
    .reject_preamble_bits = IDTECK_PSK_PREAMBLE_BITS,
    .require_repeat = false,
};

const lf_psk1_format_t LF_PSK1_FORMAT_IDTECK = {
    .preamble = LF_PSK1_PREAMBLE_IDTECK,
    .preamble_bits = IDTECK_PSK_PREAMBLE_BITS,
    .frame_bits = INDALA_PSK_FRAME_BITS,
    .reject_preamble = NULL,
    .reject_preamble_bits = 0,
    .require_repeat = false,
};

const lf_psk1_format_t LF_PSK1_FORMAT_INDALA224 = {
    .preamble = LF_PSK1_PREAMBLE_INDALA224,
    .preamble_bits = INDALA224_PSK_PREAMBLE_BITS,
    .frame_bits = INDALA224_PSK_FRAME_BITS,
    .reject_preamble = NULL,
    .reject_preamble_bits = 0,
    /* ⛔ NOT OPTIONAL for this format. 29 of its 30 preamble bits are a constant run. */
    .require_repeat = true,
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
static uint8_t preamble_err(const uint8_t *bits, size_t i, bool inverted,
                            const uint8_t *preamble, uint8_t preamble_bits) {
    uint8_t err = 0;
    for (size_t j = 0; j < preamble_bits; j++) {
        uint8_t want = inverted ? (uint8_t)(1u - preamble[j]) : preamble[j];
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

bool lf_psk1_decode_fmt(int16_t *samples, size_t n,
                        const lf_psk1_format_t *fmt, indala_psk_result_t *out) {
    if (samples == NULL || out == NULL || fmt == NULL || fmt->preamble == NULL ||
            fmt->preamble_bits == 0 || fmt->preamble_bits > LF_PSK1_MAX_PREAMBLE_BITS ||
            fmt->frame_bits == 0 || fmt->frame_bits > LF_PSK1_MAX_FRAME_BITS ||
            n < INDALA_PSK_MIN_SAMPLES(fmt->frame_bits)) {
        return false;
    }
    const uint16_t FB = fmt->frame_bits;
    bool rejected = false;
    if (n > INDALA_PSK_CAPTURE_SAMPLES) {
        n = INDALA_PSK_CAPTURE_SAMPLES;
    }
    out->energy = 0;
    baseband_in_place(samples, n);

    /* ⚠ THE BIT PHASE WITHIN THE 32-SAMPLE PERIOD IS UNKNOWN, so all 32 are tried, and
     * ranking them by preamble errors ALONE is not enough: several offsets match the
     * preamble exactly while straddling the true bit boundaries, and those returned
     * a0000000e69d0e82 where the aligned one returns a0000000e6bd0e92. Rank instead by
     * integrator MAGNITUDE among the preamble-clean offsets — the best-aligned boxcar is
     * the one whose integrators are largest, because a straddling one averages part of
     * each neighbour and partially cancels. */
    int32_t  best_amp = -1;
    int32_t  best_min = 0;
    uint8_t  best_err = 0xFF;
    uint8_t  best_word[LF_PSK1_MAX_FRAME_BITS];
    uint8_t  best_off = 0, best_pos = 0;
    bool     best_inv = false;
    bool     found = false;

    int32_t integ[INDALA_PSK_MAX_BITS];
    uint8_t bits[INDALA_PSK_MAX_BITS];

    /* ⭐ WHOLE-CAPTURE ENERGY, so a failed read can say WHICH failure it was. The preamble
     * search below only ever reports frames it recognises, so on its own it cannot tell an
     * empty antenna from a source it is unable to demodulate — and those two have printed
     * the identical "LF tag not found" at the user, which is the trap this project fell
     * into itself with far better instruments than a user will have.
     *
     * The measure is the offset loop's own integrators: no extra pass, no extra buffer.
     * A carrier-locked subcarrier drives them hard at the aligned offset; an empty antenna
     * leaves noise that averages toward zero whatever the offset. A source that is fc/2 but
     * NOT carrier-locked drifts in phase across the capture and partially cancels, so it
     * reads well below a real tag — and still far above nothing, which is exactly the
     * discrimination wanted.
     *
     * ⚠ Bound: |integ| <= 4*32*16383 = 2.1e6 and nb <= 128, so the sum stays inside int32. */
    int32_t best_energy = 0;

    for (size_t off = 0; off < INDALA_PSK_BIT_SAMPLES; off++) {
        size_t nb = (n - off) / INDALA_PSK_BIT_SAMPLES;
        if (nb < FB) {
            continue;
        }
        if (nb > INDALA_PSK_MAX_BITS) {
            nb = INDALA_PSK_MAX_BITS;
        }
        int32_t sum_abs = 0;
        for (size_t k = 0; k < nb; k++) {
            integ[k] = bit_integrator(samples, n, off + k * INDALA_PSK_BIT_SAMPLES);
            /* PSK1: the phase IS the data, so the bit is simply the integrator's sign. */
            bits[k] = (integ[k] > 0) ? 1u : 0u;
            sum_abs += (integ[k] < 0) ? -integ[k] : integ[k];
        }
        int32_t mean_abs = sum_abs / (int32_t)nb;
        if (mean_abs > best_energy) {
            best_energy = mean_abs;
        }

        for (size_t i = 0; i + FB <= nb; i++) {
            /* ⛔⛔ THE REJECT PREAMBLE — see the note above lf_psk1_decode_ex in the header.
             * Costs almost nothing: preamble_err bails on the first wrong bit. */
            if (fmt->reject_preamble != NULL && !rejected) {
                for (uint8_t inv = 0; inv < 2; inv++) {
                    if (preamble_err(bits, i, inv != 0, fmt->reject_preamble,
                                     fmt->reject_preamble_bits) <= PREAMBLE_MAX_ERR) {
                        rejected = true;
                        break;
                    }
                }
            }
            for (uint8_t inv = 0; inv < 2; inv++) {
                uint8_t err = preamble_err(bits, i, inv != 0, fmt->preamble,
                                           fmt->preamble_bits);
                if (err > PREAMBLE_MAX_ERR) {
                    continue;
                }
                /* Sum rather than mean: every candidate spans the same 64 bits, so the
                 * divide would cancel. Bounded by 64 * 4 * 32 * 16383 = 1.4e8. */
                int32_t amp = 0;
                int32_t mn = INT32_MAX;
                for (size_t k = 0; k < FB; k++) {
                    int32_t v = integ[i + k];
                    v = (v < 0) ? -v : v;
                    amp += v;
                    if (v < mn) {
                        mn = v;
                    }
                }
                if (err < best_err || (err == best_err && amp > best_amp)) {
                    best_err = err;
                    best_amp = amp;
                    best_min = mn;
                    best_off = (uint8_t)off;
                    best_pos = (uint8_t)i;
                    best_inv = (inv != 0);
                    for (size_t k = 0; k < FB; k++) {
                        uint8_t b = bits[i + k];
                        best_word[k] = (inv != 0) ? (uint8_t)(1u - b) : b;
                    }
                    found = true;
                }
            }
        }
    }

    out->energy = best_energy;

    /* ⛔ A capture carrying the reject format yields NO credential, even if a frame in the
     * requested format was also found. `energy` is still reported, so the caller can still
     * say "something is there" rather than "nothing is there". */
    if (rejected || !found) {
        return false;
    }

    /* ⛔⛔ THE STRADDLE GATE — the only thing between this decoder and a WRONG CREDENTIAL
     * that two independent captures will agree on.
     *
     * At sample phases 60-92 the decoder locks onto a candidate that is NOT aligned to the
     * data, and returns a coherent, repeatable, WRONG word — phase 64 gives
     * a0000000b5af0b92 on 5 captures out of 5. The acceptance rule in lf_indala_data.c
     * cannot catch that, because both captures agree on it.
     *
     * ⛔ DO NOT GATE ON THE SAMPLE OFFSET, however tempting the numbers look. Those wrong
     * frames all won at offset 16 or 22 while the true ones won at 9, 10 or 12, and an
     * earlier version of this comment called 16 "exactly half the 32-sample bit period" and
     * treated it as the signature. It is not one. Half a bit from the true alignment of 9
     * would be 25, not 16 — the arithmetic never worked. And a SECOND Indala tag, read on
     * hardware, decodes correctly at offset 22, which is one of the "straddle" offsets:
     * the winning offset is a property of the TAG's frame timing, not of the sample phase
     * or of correctness. Only the shape test below is measured to separate them.
     *
     * ⭐ The signature is the SHAPE of the frame, not its level and not its offset. A
     * candidate that is not aligned to the data has at least one bit whose integrator
     * spans a transition almost symmetrically and therefore cancels to near ZERO, so the
     * weakest bit of the frame collapses relative to the average. That is measured; the
     * finer mechanism (exactly how far off alignment has to be) is not established:
     *
     *                          min|integ| / mean|integ|      mean |integ|
     *     front, true frames          0.36 - 0.62            7240 - 12700
     *     front, straddles           0.001 - 0.092           5120 -  6832
     *     back,  true frames         0.015 - 0.059            468 -   936
     *
     * ⚠ min/mean ALONE IS NOT ENOUGH and that is why there are two conditions. At 26 dB
     * down a genuine frame is also ragged — back-side true frames sit at 0.015-0.059,
     * indistinguishable in shape from a straddle. Gating on shape alone would cost 61% of
     * back-side reads. But the two cases are an order of magnitude apart in LEVEL, so
     * requiring BOTH "loud" and "ragged" rejects 21 of 21 straddles while leaving every
     * back-side frame untouched (51 of 51 kept). Measured over 320 captures, both
     * placements.
     *
     * ⚠ INDALA_PSK_STRADDLE_AMP IS AN ABSOLUTE LEVEL and therefore coupling-dependent —
     * the one property C43 warns about. It sits at the geometric mean of the two
     * populations, 2.2x above the loudest back-side frame and 2.5x below the quietest
     * straddle, which is the widest margin the data supports. A tag coupled well enough to
     * produce a straddle but too weakly to clear this bar would slip through. That is why
     * this gate does NOT replace keeping 60-92 out of PHASE_ROTATION; it is the second
     * layer, for phases and placements nobody has characterised.
     *
     * ⇒ It only ever REJECTS. A rejected capture costs one retry at the next phase, and
     * nothing that is accepted today becomes accepted that was not before. */
    if (best_amp / FB >= INDALA_PSK_STRADDLE_AMP &&
            best_min * INDALA_PSK_STRADDLE_DIV < best_amp / FB) {
        return false;
    }

    /* ⭐⭐ THE REPEAT TEST — 224 bits of evidence where the preamble offers 30.
     *
     * A tag transmits its frame continuously, so the bits one frame period away from any
     * position are the same bits. For a format whose preamble is almost entirely a constant
     * run, that self-agreement is the only acceptance test worth having: C90 showed a loud
     * wrong-protocol source forging Indala26's 33-bit preamble and returning a confident
     * wrong credential, and Indala224's preamble is weaker still.
     *
     * ⚠ THE REPEAT MAY BE INVERTED. Momentum accepts the second frame's preamble normal OR
     * inverted because PSK2-style cards alternate phase frame to frame, so this counts
     * agreement and disagreement and requires one of them to be total. Comparing only
     * "equal" would reject those cards entirely.
     *
     * ⚠ Only bits that actually exist in the capture are compared; whichever side of the
     * frame the neighbouring copy falls on, the capture holds one whole period of it. A
     * position with no neighbour contributes nothing rather than counting as agreement. */
    if (fmt->require_repeat) {
        /* ⚠ Rebuild the bit stream at the WINNING offset. `bits` was overwritten by every
         * offset tried after it, and `samples` is untouched since baseband_in_place, so one
         * extra pass is both correct and cheap — and only this format pays for it. */
        size_t nb_all = (n - best_off) / INDALA_PSK_BIT_SAMPLES;
        if (nb_all > INDALA_PSK_MAX_BITS) {
            nb_all = INDALA_PSK_MAX_BITS;
        }
        for (size_t k = 0; k < nb_all; k++) {
            int32_t v = bit_integrator(samples, n, best_off + k * INDALA_PSK_BIT_SAMPLES);
            bits[k] = (v > 0) ? 1u : 0u;
        }

        size_t compared = 0, same = 0;
        const size_t pos = best_pos;
        for (size_t k = 0; k < FB; k++) {
            /* ⚠ `best_word` is already de-inverted; `bits` is not. Compare against the raw
             * stream in the same sense the frame was read in, or an inverted-preamble frame
             * would score 0 agreement against its own neighbours. */
            const uint8_t want = best_inv ? (uint8_t)(1u - best_word[k]) : best_word[k];
            if (pos + k >= FB) {
                compared++;
                if (bits[pos + k - FB] == want) {
                    same++;
                }
            }
            if (pos + k + FB < nb_all) {
                compared++;
                if (bits[pos + k + FB] == want) {
                    same++;
                }
            }
        }
        /* ⛔ Require a whole frame's worth of corroboration, and require it to be unanimous
         * one way or the other. A partial match is a coincidence, not a period. */
        if (compared < FB || (same != compared && same != 0)) {
            return false;
        }
    }

    const size_t frame_bytes = (size_t)FB / 8;
    for (size_t k = 0; k < frame_bytes; k++) {
        uint8_t byte = 0;
        for (size_t b = 0; b < 8; b++) {
            byte = (uint8_t)((byte << 1) | best_word[k * 8 + b]);
        }
        out->id[k] = byte;
    }
    out->offset   = best_off;
    out->bit_pos  = best_pos;
    out->inverted = best_inv;
    out->amp      = best_amp / FB;
    out->min_amp  = best_min;
    /* ⚠ ADVISORY, NOT A GATE. The parity is reported so a caller can prefer a clean read,
     * but it does NOT reject a frame here: it is two bits, it only covers format 26, and
     * the preamble search already produced 0 false positives in 160 empty captures. Making
     * it a gate would discard real reads of other formats for no measured gain. */
    /* Format-26 is an INDALA view of the frame and is meaningless for any other preamble,
     * so it is zeroed here and filled in only by the Indala wrapper below. */
    out->fc = 0;
    out->csn = 0;
    out->parity = 0;
    out->wiegand26_ok = false;
    out->frame_bits = FB;
    memcpy(out->word_bits, best_word, FB);
    return true;
}

/* ⛔ NO REJECT PREAMBLE HERE, and that asymmetry is deliberate — see lf_psk1_decode_ex.
 * Indala's preamble matches a loud IDTECK tag, so Indala must veto on IDTECK; the reverse
 * never happened in 480 captures, so vetoing here would only throw away genuine reads. */
bool idteck_psk1_decode(int16_t *samples, size_t n, indala_psk_result_t *out) {
    return lf_psk1_decode_fmt(samples, n, &LF_PSK1_FORMAT_IDTECK, out);
}

bool indala224_psk1_decode(int16_t *samples, size_t n, indala_psk_result_t *out) {
    return lf_psk1_decode_fmt(samples, n, &LF_PSK1_FORMAT_INDALA224, out);
}

bool indala_psk1_decode(int16_t *samples, size_t n, indala_psk_result_t *out) {
    if (!lf_psk1_decode_fmt(samples, n, &LF_PSK1_FORMAT_INDALA64, out)) {
        return false;
    }
    descramble26(out->word_bits, out);
    return true;
}
