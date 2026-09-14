/* ⭐⭐⭐ HOW STRONG IS EACH ACCEPTANCE GATE? — in bits, measured, not asserted.
 *
 * Four gates were added or corrected on this branch (C253 Securakey's ten zero spacers, C255
 * GProxII's Wiegand parity, C257 Indala26's bits 60 and 61, C258 Keri's frame repeat), and
 * each was verified the same way: break ONE named bit on a real tag and watch the read refuse.
 *
 * ⛔ THAT IS n=1 PER GATE, AND IT PROVES ONLY THAT THE GATE IS NOT A NO-OP. It cannot say how
 * much protection the gate actually provides, and a gate that rejects one deliberately broken
 * bit while accepting 99% of everything else would pass that test looking healthy.
 *
 * So measure it. Feed each format's own `accept` hook, reached through the SHIPPING descriptor
 * table rather than a transcription, uniformly random frames of that format's own length, and
 * count what survives. A gate worth k bits admits ~2^-k of them.
 *
 * ⭐ THE PREDICTIONS ARE WRITTEN DOWN BEFORE THE RUN, so a disagreement is a finding rather
 * than something to rationalise afterwards:
 *     SECURAKEY   ten zero spacers at bits 10,19,...,91      => 10 bits   ✓ measured 10.03
 *     INDALA64    bits 60 and 61 must be zero                =>  2 bits   ✓ measured  1.99
 *     GPROXII     even parity over 33..44, odd over 45..56   =>  2 bits   ⛔ WRONG, see below
 *     everything else                                        =>  0 (no hook)
 *
 * ⛔ THE GPROXII PREDICTION WAS WRONG AND THE CODE WAS RIGHT — recorded rather than quietly
 * re-pinned. It counted only the Wiegand parity C255 ADDED and ignored the two layers already
 * in `gproxii_accept`: eighteen spacer bits that must each be zero, and a format-length field
 * that must descramble to 26 or 36. Those are ~18 and ~5 bits, so the gate is ~25 bits, not 2,
 * and 0 hits in 200,000 (2^17.6) is exactly what a 25-bit gate should produce. FDX-B is the
 * same shape: eleven control bits that must each be ONE, plus a 16-bit CRC, so ~27 bits.
 *
 * ⛔⛔ AND THAT IS THE PROBLEM WITH A ZERO: a 25-bit gate and a gate that returns false
 * unconditionally are INDISTINGUISHABLE from the outside, and the second one would break every
 * read while looking like excellent protection here. So the strong gates are DECOMPOSED —
 * satisfy the structural bits, leave the rest random, and measure what is left. A gate that is
 * always-false stays at zero; a real one climbs to its residual. That residual is predicted
 * too: GProxII keeps the length gate and parity (~7 bits), FDX-B keeps its CRC16 (16 bits).
 *
 * ⭐ Total protection against a random frame is the PREAMBLE plus the gate: every preamble bit
 * must match, so it contributes `preamble_bits` outright. That total is the number worth
 * comparing across protocols, and it is what this prints. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include "lf_ask_manchester.h"
#include "lf_ask_biphase.h"
#include "lf_indala_psk.h"

#define N 200000

static uint64_t rng_state;
static uint64_t rnd(void) {
    rng_state ^= rng_state << 13; rng_state ^= rng_state >> 7; rng_state ^= rng_state << 17;
    return rng_state;
}

/* One row per format, pulled from the shipping descriptors so nothing is retyped. */
typedef struct {
    const char *name;
    const uint8_t *preamble_bits;
    uint16_t frame_bits;
    bool (*accept)(const uint8_t *, uint16_t);
    uint8_t pre_bits;
    bool require_repeat;
} row_t;

static double measure(const row_t *r) {
    if (r->accept == NULL) { return 1.0; }
    uint8_t bits[512];
    long acc = 0;
    rng_state = 0x243F6A8885A308D3ull ^ (uint64_t)r->frame_bits;
    for (long t = 0; t < N; t++) {
        for (uint16_t b = 0; b < r->frame_bits; b++) {
            if ((b & 63u) == 0u) { rnd(); }
            bits[b] = (uint8_t)((rng_state >> (b & 63u)) & 1u);
        }
        if (r->accept(bits, r->frame_bits)) { acc++; }
    }
    return (double)acc / (double)N;
}

/* ⭐ THE NULL FOR A GATE THAT ADMITS NOTHING. Fill the bits the format REQUIRES structurally,
 * leave everything else random, and re-measure. The required positions come from the shipping
 * headers' own constants, so this is not a transcription of the rule — it is the same numbers
 * the decoder compiles against. An always-false hook cannot climb off zero here. */
static double measure_structured(const row_t *r) {
    if (r->accept == NULL) { return 1.0; }
    uint8_t bits[512];
    long acc = 0;
    rng_state = 0x13198A2E03707344ull ^ (uint64_t)r->frame_bits;
    for (long t = 0; t < N; t++) {
        for (uint16_t b = 0; b < r->frame_bits; b++) {
            if ((b & 63u) == 0u) { rnd(); }
            bits[b] = (uint8_t)((rng_state >> (b & 63u)) & 1u);
        }
        if (strcmp(r->name, "GPROXII") == 0) {
            for (uint8_t g = 0; g < GPROXII_BIPHASE_SPACER_GROUPS; g++) {
                bits[GPROXII_BIPHASE_PREAMBLE_BITS + 5u * g + 4u] = 0u;
            }
        } else if (strcmp(r->name, "FDXB") == 0) {
            for (uint8_t g = 0; g < FDXB_BIPHASE_CONTROL_GROUPS; g++) {
                bits[FDXB_BIPHASE_PREAMBLE_BITS + 9u * g + 8u] = 1u;
            }
        } else {
            return -1.0;
        }
        if (r->accept(bits, r->frame_bits)) { acc++; }
    }
    return (double)acc / (double)N;
}

int main(void) {
    row_t rows[] = {
        {"GALLAGHER", NULL, LF_ASK_FORMAT_GALLAGHER.frame_bits, LF_ASK_FORMAT_GALLAGHER.accept,
         LF_ASK_FORMAT_GALLAGHER.preamble_bits, false},
        {"INSTAFOB",  NULL, LF_ASK_FORMAT_INSTAFOB.frame_bits,  LF_ASK_FORMAT_INSTAFOB.accept,
         LF_ASK_FORMAT_INSTAFOB.preamble_bits, false},
        {"NORALSY",   NULL, LF_ASK_FORMAT_NORALSY.frame_bits,   LF_ASK_FORMAT_NORALSY.accept,
         LF_ASK_FORMAT_NORALSY.preamble_bits, false},
        {"SECURAKEY", NULL, LF_ASK_FORMAT_SECURAKEY.frame_bits, LF_ASK_FORMAT_SECURAKEY.accept,
         LF_ASK_FORMAT_SECURAKEY.preamble_bits, false},
        {"GPROXII",   NULL, LF_BIPHASE_FORMAT_GPROXII.frame_bits, LF_BIPHASE_FORMAT_GPROXII.accept,
         LF_BIPHASE_FORMAT_GPROXII.preamble_bits, false},
        {"FDXB",      NULL, LF_BIPHASE_FORMAT_FDXB.frame_bits,    LF_BIPHASE_FORMAT_FDXB.accept,
         LF_BIPHASE_FORMAT_FDXB.preamble_bits, false},
        {"NEXWATCH",  NULL, LF_PSK1_FORMAT_NEXWATCH.frame_bits,  LF_PSK1_FORMAT_NEXWATCH.accept,
         LF_PSK1_FORMAT_NEXWATCH.preamble_bits, LF_PSK1_FORMAT_NEXWATCH.require_repeat},
        {"INDALA64",  NULL, LF_PSK1_FORMAT_INDALA64.frame_bits,  LF_PSK1_FORMAT_INDALA64.accept,
         LF_PSK1_FORMAT_INDALA64.preamble_bits, LF_PSK1_FORMAT_INDALA64.require_repeat},
        {"IDTECK",    NULL, LF_PSK1_FORMAT_IDTECK.frame_bits,    LF_PSK1_FORMAT_IDTECK.accept,
         LF_PSK1_FORMAT_IDTECK.preamble_bits, LF_PSK1_FORMAT_IDTECK.require_repeat},
        {"INDALA224", NULL, LF_PSK1_FORMAT_INDALA224.frame_bits, LF_PSK1_FORMAT_INDALA224.accept,
         LF_PSK1_FORMAT_INDALA224.preamble_bits, LF_PSK1_FORMAT_INDALA224.require_repeat},
        {"KERI",      NULL, LF_PSK1_FORMAT_KERI.frame_bits,      LF_PSK1_FORMAT_KERI.accept,
         LF_PSK1_FORMAT_KERI.preamble_bits, LF_PSK1_FORMAT_KERI.require_repeat},
    };
    const size_t ROWS = sizeof(rows) / sizeof(rows[0]);

    printf("gate strength — %d random frames per format, through the shipping accept hook\n\n", N);
    printf("  %-10s %6s %6s %9s %9s %8s  %s\n",
           "format", "frame", "pream", "gate", "gate", "total", "");
    printf("  %-10s %6s %6s %9s %9s %8s  %s\n",
           "", "bits", "bits", "admits", "bits", "bits", "notes");

    int no_gate = 0, with_gate = 0, bad = 0;
    for (size_t i = 0; i < ROWS; i++) {
        double rate = measure(&rows[i]);
        double gbits = (rate <= 0.0) ? INFINITY : -log2(rate);
        /* ⛔ A zero-rate row has NO measurable total — 200,000 samples bottom out at 17.6
         * bits — and printing `preamble + 0` there would report the two STRONGEST gates as
         * the weakest rows in the table. Print the sampling floor and a `>=` instead. */
        bool floored = (rate <= 0.0);
        double total = (double)rows[i].pre_bits + (floored ? log2((double)N) : gbits);
        if (rows[i].accept == NULL) { no_gate++; } else { with_gate++; }
        printf("  %-10s %6u %6u %8.4f%% %8s%-0.2f %6s%-0.2f  %s%s\n",
               rows[i].name, rows[i].frame_bits, rows[i].pre_bits, rate * 100.0,
               floored ? ">=" : "", floored ? log2((double)N) : gbits,
               floored ? ">=" : "", total,
               rows[i].accept ? "" : "no gate",
               rows[i].require_repeat ? " require_repeat" : "");
        /* ⛔ A hook that admits everything is a no-op and the n=1 bit-break test would not
         * have caught it. That is the whole reason this arm exists. */
        if (rows[i].accept != NULL && rate > 0.999) {
            printf("     ⛔ %s HAS A GATE THAT ADMITS EVERYTHING — it is a no-op\n", rows[i].name);
            bad = 1;
        }
    }

    /* ⛔ THE TWO GATES THAT ADMIT NOTHING, DECOMPOSED — without this they are indistinguishable
     * from a hook that returns false unconditionally. */
    printf("\n  decomposing the gates that admitted 0 of %d:\n", N);
    for (size_t i = 0; i < ROWS; i++) {
        if (strcmp(rows[i].name, "GPROXII") != 0 && strcmp(rows[i].name, "FDXB") != 0) { continue; }
        double sr = measure_structured(&rows[i]);
        double sb = (sr <= 0.0) ? INFINITY : -log2(sr);
        printf("    %-9s structural bits satisfied -> admits %.4f%% (%.2f bits remain)\n",
               rows[i].name, sr * 100.0, sb);
        if (sr <= 0.0) {
            printf("     ⛔ %s STILL ADMITS NOTHING — the hook may be unconditionally false\n",
                   rows[i].name);
            bad = 1;
        }
    }
    /* Predicted residuals: GProxII keeps its length gate and parity (~7 bits); FDX-B keeps a
     * 16-bit CRC. Both are loose bands — the point is that they are FINITE, not that they are
     * exact, because a finite residual is what proves the hook is reachable. */

    /* ⛔ PINNED against the predictions written at the top of this file. */
    struct { const char *name; double lo, hi; } expect[] = {
        {"SECURAKEY", 9.5, 10.5}, {"INDALA64", 1.5, 2.5},
        {"GALLAGHER", 7.5, 8.5},  {"NORALSY", 7.5, 8.5}, {"NEXWATCH", 3.5, 4.5},
    };
    for (size_t e = 0; e < sizeof(expect) / sizeof(expect[0]); e++) {
        for (size_t i = 0; i < ROWS; i++) {
            if (strcmp(rows[i].name, expect[e].name) != 0) { continue; }
            double rate = measure(&rows[i]);
            double gbits = (rate <= 0.0) ? 99.0 : -log2(rate);
            if (gbits < expect[e].lo || gbits > expect[e].hi) {
                printf("  ⛔ MOVED: %s gate measured %.2f bits, predicted %.1f..%.1f\n",
                       expect[e].name, gbits, expect[e].lo, expect[e].hi);
                bad = 1;
            }
        }
    }
    printf("\n  => %d formats carry a gate, %d do not\n", with_gate, no_gate);
    printf("\n%s\n", bad ? "⛔ FAILURES" : "✓ gate strengths as predicted");
    return bad;
}
