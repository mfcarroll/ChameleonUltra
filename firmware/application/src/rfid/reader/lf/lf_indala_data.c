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
 * ⭐ THE SAMPLE PHASE IS NOT OPTIONAL, AND THE STOCK ONE IS THE WORST ONE.
 *
 * Indala's subcarrier is fc/2, so it arrives at exactly two samples per cycle. A T5577
 * derives it by dividing the very field this reader generates, so it is phase-LOCKED to
 * the sample trigger: the recovered amplitude is proportional to cos(phi) for a CONSTANT
 * phi, and an unlucky phi nulls the subcarrier to any depth on every read, forever.
 * Stock firmware triggers straight off PWMPERIODEND — phase 0 — and phase 0 decodes
 * 0 of 5. Measured over 32 phases x 5 captures, at 62.5ns per tick:
 *
 *     ticks   0    4    8   12   16   20   24   28   32   36   40   44   48   52  56  60
 *     decodes 0/5  1/5  4/5  5/5  4/5  5/5  4/5  5/5  3/5  5/5  4/5  4/5  2/5  1/5 2/5 2/5
 *     ticks  64..127 — all 0/5
 *
 * ⚠ THAT WINDOW IS ONE TAG, ONE UNIT, ONE COUPLING GEOMETRY. It has never been checked
 * against a second Indala tag or a second Chameleon, and it could move. So this does not
 * hard-code the best phase — it ROTATES, best-measured first, across the whole live
 * window. If the window has shifted this costs a few more captures rather than failing;
 * resampling the measured captures with only ticks 32-60 alive still reaches a read in a
 * median of 7 captures.
 */
static const uint8_t PHASE_ROTATION[] = {
    20, 12, 28, 36, 44, 4, 56, 0
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
 * ⚠ What that does NOT establish is a rate below ~1/50000; it is resampling 160 real
 * captures, so it cannot see a failure mode absent from them. The defensible claim is the
 * one the data supports: no wrong word ever repeated, and agreement removes the 12%.
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
static bool stack_decode(indala_stack_t *s, size_t n) {
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
    if (!indala_psk1_decode(m_scratch, n, &s->res)) {
        return false;
    }
    memcpy(s->word, s->res.id, 8);
    s->have_word = true;
    return true;
}

bool indala_read(uint8_t *data, uint32_t timeout_ms) {
    bool ok = false;
    uint8_t winner_phase = 0;
    const indala_stack_t *winner = NULL;

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
            if (!stack_decode(cur, got)) {
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

    if (!ok || winner == NULL) {
        return false;
    }

    memcpy(&data[0], winner->res.id, 8);
    data[8]  = winner->res.fc;
    data[9]  = (uint8_t)(winner->res.csn >> 8);
    data[10] = (uint8_t)(winner->res.csn & 0xFF);
    data[11] = (uint8_t)((winner->res.wiegand26_ok ? 0x04u : 0x00u) |
                         (winner->res.parity & 0x03u));
    data[12] = winner_phase;
    data[13] = winner->res.offset;
    data[14] = (uint8_t)winner->n;   /* captures stacked to get this */
    data[15] = 0;

    /* ⚠ NRF_LOG takes at most six format arguments (LOG_INTERNAL_0..6); more is a build
     * error deep inside the macro expansion rather than anything that names this line. */
    uint32_t hi = ((uint32_t)winner->res.id[0] << 24) | ((uint32_t)winner->res.id[1] << 16) |
                  ((uint32_t)winner->res.id[2] << 8)  | winner->res.id[3];
    uint32_t lo = ((uint32_t)winner->res.id[4] << 24) | ((uint32_t)winner->res.id[5] << 16) |
                  ((uint32_t)winner->res.id[6] << 8)  | winner->res.id[7];
    NRF_LOG_INFO("indala %08lx%08lx fc %u phase %u stacked %u",
                 (unsigned long)hi, (unsigned long)lo,
                 winner->res.fc, winner_phase, winner->n);
    return true;
}
