#include <string.h>

#include "bsp_time.h"
#include "lf_125khz_radio.h"
#include "lf_indala_data.h"
#include "lf_indala_psk.h"
#include "lf_reader_generic.h"

#define NRF_LOG_MODULE_NAME lf_indala
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
NRF_LOG_MODULE_REGISTER();

/*
 * ⭐ THE SAMPLE PHASE IS NOT OPTIONAL — AND THE PHASE MAP DEPENDS ON WHICH SIDE THE TAG IS ON.
 *
 * Indala's subcarrier is fc/2, so it arrives at exactly two samples per cycle. A T5577
 * derives it by dividing the very field this reader generates, so it is phase-LOCKED to
 * the sample trigger: the recovered amplitude is proportional to cos(phi) for a CONSTANT
 * phi, and an unlucky phi nulls the subcarrier to any depth on every read, forever.
 *
 * Measured 32 phases x 5 captures, at 62.5ns per tick, with the tag on the FRONT (the
 * reading side — see the placement banner in research/indala-psk-read/FINDINGS.md) and
 * again on the back, decoded by this same C decoder:
 *
 *     ticks      0   4   8  12  16  20  24  28  32  36  40  44  48  52  56
 *     FRONT    5/5 5/5 5/5 5/5 5/5 5/5 5/5 5/5 5/5 5/5 5/5 5/5 5/5 5/5 4/5
 *     back     0/5 1/5 4/5 5/5 4/5 5/5 4/5 5/5 3/5 5/5 4/5 4/5 2/5 1/5 2/5
 *
 *     ticks     60  64  68  72  76  80  84  88  92 | 96 100 104 108 112 116 120 124
 *     FRONT    0/5 0/5 0/5 0/5 0/5 0/5 0/5 0/5 0/5 |5/5 5/5 5/5 5/5 5/5 5/5 5/5 5/5
 *     back     2/5 0/5 0/5 0/5 0/5 0/5 0/5 0/5 0/5 |0/5 0/5 0/5 0/5 0/5 0/5 0/5 0/5
 *
 * So phase is TWO WORKING BANDS split by a dead band at 60-92, not a window — and the
 * stock trigger (phase 0) is fine on the front and useless on the back.
 *
 * ⛔⛔ NEVER PUT A PHASE FROM 60-92 IN THIS LIST. It is not merely dead: in that band the
 * decoder returns a WRONG CARD NUMBER, the SAME one every time. Phase 64 returns
 * a0000000b5af0b92 on 5 of 5 captures; phase 88 returns a0000000c6b90c92 on 5 of 5; phase
 * 92 returns a0000000c6b90e92 on 4 of 5. Those frames are not aligned to the data, so
 * their weakest bit integrator cancels to near zero — which is what lf_indala_psk.c's
 * straddle gate now tests for and rejects.
 * ⛔ It does NOT test the sample offset, and neither should anything else: a second Indala
 * tag decodes CORRECTLY on hardware at offset 22, which is one of the offsets these wrong
 * frames won at. The offset belongs to the tag's frame timing, not to correctness.
 *
 * ⇒ TWO INDEPENDENT CAPTURES AGREE ON THAT WRONG WORD, so the acceptance rule below does
 * NOT catch it. Nor would requiring two different phases to agree: a0000000c6b90c92 is
 * produced at BOTH phase 88 and phase 92. The only thing standing between this reader and
 * a confidently wrong credential is that no phase in this list lies in 60-92. Keep it so.
 *
 * ⚠ THIS MAP IS ONE TAG, ONE UNIT, TWO COUPLING GEOMETRIES. It has never been checked
 * against a second Indala tag or a second Chameleon, and it could move. So this does not
 * hard-code the best phase — it ROTATES across phases that decode with the tag on EITHER
 * side, best-measured first. The first five are the strongest on both placements; the last
 * three are insurance chosen for SPREAD rather than rank, including one from the upper
 * band, so a shifted window is unlikely to take out every entry at once.
 */
static const uint8_t PHASE_ROTATION[] = {
    20, 12, 28, 36, 44, 16, 112, 0
};
#define PHASE_ROTATION_COUNT (sizeof(PHASE_ROTATION) / sizeof(PHASE_ROTATION[0]))

/*
 * ⭐ TWO CAPTURES MUST AGREE BEFORE A CARD NUMBER IS RETURNED.
 *
 * A single decode is NOT trustworthy and the margin is not small: of the 68 frames the
 * demodulator recovered from 160 tag captures, 51 were right and 17 were WRONG — mostly
 * one or two flipped bits inside the 28-bit zero run. Returning the first frame therefore
 * returns a wrong credential about 12% of the time (bootstrap over the measured captures,
 * 50000 trials).
 *
 * But every wrong word appeared EXACTLY ONCE across all 160 captures, while the right one
 * appeared 51 times — bit errors land in different places each time, so requiring two
 * captures to produce the same 64 bits removes them. The same bootstrap gives 0 wrong
 * reads in 50000 trials at a cost of a median of 2 captures and 5 at the 95th percentile.
 *
 * ⛔⛔ AND THAT INDEPENDENCE IS A PROPERTY OF A WEAK SIGNAL, NOT OF THIS DECODER. Every
 * capture above was taken with the tag on the BACK, ~26 dB down. Repeated on the FRONT,
 * where the signal actually is: all 17 back-side wrong frames were distinct, but 3 of the
 * 21 front-side ones REPEAT WITHIN A PHASE, two of them on 5 captures out of 5. At 20x the
 * signal the demodulator stops guessing and locks deterministically onto a half-bit-offset
 * alignment, so two independent captures produce the same wrong word and this rule reports
 * full confidence in it. ⇒ Improving the signal moved the failure from random to
 * systematic. See the ⛔⛔ block above PHASE_ROTATION: every one of those frames comes from
 * the 60-92 dead band, and keeping that band out of the rotation is what makes this rule
 * safe — the rule does not make itself safe.
 *
 * ⚠ What that does NOT establish is a rate below ~1/50000; it is resampling 160 real
 * captures, so it cannot see a failure mode absent from them. The defensible claim is the
 * one the data supports: within the phases this reader actually uses, no wrong word ever
 * repeated — 114 decodes at 22 front-side phases produced 0 wrong frames — and agreement
 * removes the 12%.
 */
#define INDALA_AGREE_COUNT   2

/*
 * ⭐ STACKING: WHY THE READ IS NOW TWO ACCUMULATORS RATHER THAN A LIST OF WORDS.
 *
 * The margin on this hardware is far smaller than "43 of 160 captures decode" makes it
 * sound. Measured against the empty-field floor, the bench tag's fc/2 sideband sits at
 * 1.25-1.73x — and a SECOND tag, written and verified by a Proxmark which then read it
 * back, sat at 0.89-1.11x across twelve sample phases. Inaudible. That is the whole
 * operating range: lose ~1.3x to tag position or coil geometry and a tag stops existing.
 *
 * ⇒ The read needs MORE SIGNAL, not a better decoder. And it is available for free,
 * because a T5577 restarts from a fixed point when the field comes up: separate captures
 * are frame-locked, so they can simply be added, and the noise falls as sqrt(N). Measured
 * over every combination of the committed captures:
 *
 *     N        correct     wrong frames    EMPTY-FIELD frames
 *     1   51/160  31.9%         17            0/160
 *     2  168/320  52.5%         37            0/320
 *     3  194/320  60.6%         34            0/320
 *     4  107/160  66.9%         11            0/160
 *     5   23/32   71.9%          1            0/32
 *
 * Stacking five revives sample phases that decode 0/5 individually — INCLUDING PHASE 0,
 * the stock trigger, and phase 64, which is outside the single-capture window entirely.
 * The null is clean at every level: stacked noise produced no frame at all, ever.
 *
 * ⛔ DO NOT READ THE PHASE 64 RESULT AS A REASON TO ADD PHASE 64. That whole table is
 * back-side data. On the front, phase 64 decodes a0000000b5af0b92 on 5 captures out of 5 —
 * a wrong credential, reproducibly. "Stacking revives it" there means stacking revives a
 * confident error. Whether stacking is worth anything at all now that single captures
 * decode 71% of the time on the front is itself unmeasured; see NEXT.md §2.
 *
 * ⚠ CAPTURES ARRIVE WITH EITHER POLARITY. They are frame-locked but the subcarrier phase
 * is not, so a capture can be the inverse of the accumulator — adding it would CANCEL the
 * data rather than reinforce it. stack_add() resolves the sign by correlation first. The
 * mixer is a multiplication by (-1)^n, which squares to 1, so correlating the two BASEBAND
 * signals is identical to correlating the raw mean-removed ones: no mixing needed.
 *
 * ⚠ TWO ACCUMULATORS, NOT ONE, and they never share a capture. The agreement rule below is
 * only evidence if the two words come from independent data; decoding a stack of 3 and
 * then a stack of 4 that contains those same 3 is one measurement reported twice.
 */
#define INDALA_STACK_MAX     8   /* captures per phase, split between the two accumulators */

typedef struct {
    int32_t acc[INDALA_PSK_CAPTURE_SAMPLES];
    uint16_t n;
    bool     have_word;
    uint8_t  word[8];
    indala_psk_result_t res;
} indala_stack_t;

/** Per-capture ceiling. 4096 samples at 125kHz is 32.8ms of sampling; 200ms is headroom
 *  for the settle and the ring drain, not a budget anything is expected to use. */
#define INDALA_CAPTURE_TIMEOUT_MS 200

/* 8KB capture buffer, 8KB scratch, 2x16KB accumulators. The decoder works IN PLACE on
 * whatever it is given, which is why the scratch exists: the accumulator has to survive
 * the decode so the next capture can be added to it. */
static int16_t m_samples[INDALA_PSK_CAPTURE_SAMPLES];
static int16_t m_scratch[INDALA_PSK_CAPTURE_SAMPLES];
static indala_stack_t m_stack_a, m_stack_b;

static void stack_reset(indala_stack_t *s) {
    s->n = 0;
    s->have_word = false;
}

/** Add one capture, resolving its polarity against what is already accumulated. */
static void stack_add(indala_stack_t *s, const int16_t *x, size_t n) {
    if (s->n == 0) {
        for (size_t i = 0; i < n; i++) {
            s->acc[i] = x[i];
        }
        s->n = 1;
        return;
    }

    int32_t sx = 0;
    int64_t sa = 0;
    for (size_t i = 0; i < n; i++) {
        sx += x[i];
        sa += s->acc[i];
    }
    const int32_t mx = sx / (int32_t)n;
    const int32_t ma = (int32_t)(sa / (int64_t)n);

    int64_t dot = 0;
    for (size_t i = 0; i < n; i++) {
        dot += (int64_t)(s->acc[i] - ma) * (int32_t)(x[i] - mx);
    }

    if (dot >= 0) {
        for (size_t i = 0; i < n; i++) {
            s->acc[i] += x[i];
        }
    } else {
        /* Mirror the capture about its own mean — the same signal, opposite polarity. */
        for (size_t i = 0; i < n; i++) {
            s->acc[i] += 2 * mx - x[i];
        }
    }
    s->n++;
}

/** Scale the accumulator back to a 14-bit range and decode it. */
static bool stack_decode(indala_stack_t *s, size_t n, lf_psk1_decode_fn decode) {
    if (s->n == 0) {
        return false;
    }
    const int32_t half = (int32_t)s->n / 2;
    for (size_t i = 0; i < n; i++) {
        int32_t v = (s->acc[i] + half) / (int32_t)s->n;
        if (v < 0) {
            v = 0;
        } else if (v > 16383) {
            v = 16383;
        }
        m_scratch[i] = (int16_t)v;
    }
    if (!decode(m_scratch, n, &s->res)) {
        return false;
    }
    memcpy(s->word, s->res.id, 8);
    s->have_word = true;
    return true;
}

/* ⭐ THE CAPTURE ENGINE IS SHARED, AND THAT IS THE POINT OF §1c. Everything here — the
 * sample-phase rotation, the two independent accumulators, the agreement rule, the timeout
 * discipline — is a property of PSK1-at-RF/32 on this hardware, not of Indala. IDTECK is the
 * same physical layer, so it gets all of it, including the 32KB of accumulators, for the cost
 * of one function pointer. Copying this file for a second protocol would have copied the
 * 320-capture validation with it, and then the two copies would have drifted. */
bool lf_psk1_read(lf_psk1_decode_fn decode, lf_psk1_read_t *out,
                  uint32_t timeout_ms, int32_t *energy_out) {
    bool ok = false;
    uint8_t winner_phase = 0;
    const indala_stack_t *winner = NULL;
    /* ⚠ The LOUDEST capture, not the last one. A read spends up to eight captures per sample
     * phase across several phases; a source that is present for only part of that budget
     * still means "something was there", and taking the final capture's value would report
     * whatever the antenna happened to be hearing when the timeout expired. */
    int32_t loudest = 0;

    autotimer *p_at = bsp_obtain_timer(0);

    for (size_t pi = 0; pi < PHASE_ROTATION_COUNT && !ok; pi++) {
        const uint8_t phase = PHASE_ROTATION[pi];
        lf_125khz_radio_saadc_phase_set(phase);
        stack_reset(&m_stack_a);
        stack_reset(&m_stack_b);

        for (uint8_t k = 0; k < INDALA_STACK_MAX && !ok; k++) {
            if (!NO_TIMEOUT_1MS(p_at, timeout_ms)) {
                break;
            }
            size_t got = 0;
            if (!raw_read_samples(m_samples, INDALA_PSK_CAPTURE_SAMPLES,
                                  INDALA_CAPTURE_TIMEOUT_MS, &got, 0)) {
                continue;
            }

            /* Alternate, so the two accumulators never share a capture. */
            indala_stack_t *cur   = (k & 1u) ? &m_stack_b : &m_stack_a;
            indala_stack_t *other = (k & 1u) ? &m_stack_a : &m_stack_b;

            stack_add(cur, m_samples, got);
            bool decoded = stack_decode(cur, got, decode);
            /* Set by indala_psk1_decode() either way — see lf_indala_psk.h. */
            if (cur->res.energy > loudest) {
                loudest = cur->res.energy;
            }
            if (!decoded) {
                continue;
            }

            /* ⭐ THE ACCEPTANCE RULE, AND IT IS NOT OPTIONAL. One recovered frame in five
             * is WRONG — 24 bad frames across 200 single captures, mostly one or two bits
             * flipped inside the 28-bit zero run. A reader that returns the first frame it
             * decodes returns a wrong credential about 20% of the time. But every one of
             * those 24 was DISTINCT while the truth recurred 77 times, because bit errors
             * land somewhere different each time. So two independent stacks have to agree.
             *
             * ⚠ INDEPENDENT is the load-bearing word, which is why `other` shares no
             * capture with `cur`. Comparing a stack of 3 against a stack of 4 containing
             * the same 3 would be one measurement agreeing with itself. */
            if (other->have_word && memcmp(other->word, cur->word, 8) == 0) {
                winner = cur;
                winner_phase = phase;
                ok = true;
            }
        }
    }
    bsp_return_timer(p_at);

    /* ⚠ Never leave a sample phase set: every other LF reader on this device shares the
     * trigger and expects the stock PWMPERIODEND one. */
    lf_125khz_radio_saadc_phase_set(0);

    if (energy_out != NULL) {
        *energy_out = loudest;
    }

    if (!ok || winner == NULL) {
        return false;
    }

    out->res = winner->res;
    out->phase = winner_phase;
    out->stacked = (uint8_t)winner->n;
    return true;
}

bool indala_read(uint8_t *data, uint32_t timeout_ms, int32_t *energy_out) {
    lf_psk1_read_t r;
    if (!lf_psk1_read(indala_psk1_decode, &r, timeout_ms, energy_out)) {
        return false;
    }
    const indala_psk_result_t *res = &r.res;
    uint8_t winner_phase = r.phase;

    memcpy(&data[0], res->id, 8);
    data[8]  = res->fc;
    data[9]  = (uint8_t)(res->csn >> 8);
    data[10] = (uint8_t)(res->csn & 0xFF);
    data[11] = (uint8_t)((res->wiegand26_ok ? 0x04u : 0x00u) |
                         (res->parity & 0x03u));
    data[12] = winner_phase;
    data[13] = res->offset;
    data[14] = r.stacked;   /* captures stacked to get this */
    data[15] = 0;

    /* ⚠ NRF_LOG takes at most six format arguments (LOG_INTERNAL_0..6); more is a build
     * error deep inside the macro expansion rather than anything that names this line. */
    uint32_t hi = ((uint32_t)res->id[0] << 24) | ((uint32_t)res->id[1] << 16) |
                  ((uint32_t)res->id[2] << 8)  | res->id[3];
    uint32_t lo = ((uint32_t)res->id[4] << 24) | ((uint32_t)res->id[5] << 16) |
                  ((uint32_t)res->id[6] << 8)  | res->id[7];
    NRF_LOG_INFO("indala %08lx%08lx fc %u phase %u stacked %u",
                 (unsigned long)hi, (unsigned long)lo,
                 res->fc, winner_phase, r.stacked);
    return true;
}

/* ⭐ THE WHOLE IDTECK READER. Everything above is shared; this is the payload layout and
 * nothing else, which is what §1c predicted when it said the physical layers are identical.
 *
 * IDTECK packs its 32-bit payload after the "IDTK" preamble as a checksum byte and then a
 * BYTE-REVERSED 24-bit card number — `4944544B55667788` is checksum 0x55 and card 0x887766,
 * which is what the Proxmark prints for the bench tag and what `idteck.c` describes on the
 * emulation side. */
bool idteck_read(uint8_t *data, uint32_t timeout_ms, int32_t *energy_out) {
    lf_psk1_read_t r;
    if (!lf_psk1_read(idteck_psk1_decode, &r, timeout_ms, energy_out)) {
        return false;
    }
    const indala_psk_result_t *res = &r.res;

    memcpy(&data[0], res->id, 8);
    data[8]  = res->id[4];                      /* checksum byte */
    data[9]  = res->id[7];                      /* card number, most significant first */
    data[10] = res->id[6];
    data[11] = res->id[5];
    data[12] = r.phase;
    data[13] = res->offset;
    data[14] = r.stacked;
    data[15] = 0;

    uint32_t card = ((uint32_t)res->id[7] << 16) | ((uint32_t)res->id[6] << 8) | res->id[5];
    NRF_LOG_INFO("idteck card %lu chksum %02x phase %u stacked %u",
                 (unsigned long)card, res->id[4], r.phase, r.stacked);
    return true;
}
