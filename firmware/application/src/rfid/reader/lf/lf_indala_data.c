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
 * ⛔⛔ STACKING WAS REMOVED 2026-09-12, DELIBERATELY AND WITH THE COST ACCEPTED. Do not
 * put it back without re-reading this and NEXT.md §8.
 *
 * It worked, and the table is preserved because it is real: adding frame-locked captures
 * dropped the noise as sqrt(N) and took the BACK-side decode rate from 32% to 72%.
 *
 *     N        correct     wrong frames    EMPTY-FIELD frames
 *     1   51/160  31.9%         17            0/160
 *     2  168/320  52.5%         37            0/320
 *     3  194/320  60.6%         34            0/320
 *     4  107/160  66.9%         11            0/160
 *     5   23/32   71.9%          1            0/32
 *
 * ⛔ But every one of those numbers is BACK-side, and the front is the documented placement.
 * On the front the rate is 68.75% at EVERY depth including 1 — stacking buys exactly nothing
 * where users are told to put the tag (C58). Against that it cost two 16KB accumulators and
 * an 8KB scratch buffer, and those 40KB are the only reason Indala224 was impossible: a
 * 224-bit frame needs 14336 samples, so the stacked shape wanted 168KB against 49.6KB of
 * free RAM, while decoding in place needs 28KB (C94).
 *
 * ⇒ The trade was made explicitly: the back-side rate returns to 32% and Indala224 becomes
 * buildable. The reader that failed a back-side read now says "a subcarrier is present but no
 * frame could be decoded" (0x43) rather than "not found", which tells the user to move the
 * tag — the actual fix for a bad placement, and cheaper than 40KB of hiding it.
 *
 * ⚠ What is GONE with it: the polarity-resolving correlator (captures arrive with either
 * subcarrier phase, which mattered only when summing them) and the two alternating
 * accumulators. ⛔ What is NOT gone is the agreement rule below — it is not stacking, it is
 * the only thing holding wrong words at 0, and consecutive captures are trivially independent
 * now, which is what that rule always needed.
 */

/** Per-capture ceiling. 4096 samples at 125kHz is 32.8ms of sampling; 200ms is headroom
 *  for the settle and the ring drain, not a budget anything is expected to use. */
#define INDALA_CAPTURE_TIMEOUT_MS 200

/** Captures attempted per sample phase before moving on. Was the stacking depth; it is now
 *  simply how many independent tries each phase gets, and two of them must agree. */
#define INDALA_TRIES_PER_PHASE  8

/* ⭐ 8KB, and that is the whole reader now — down from 48KB. The decoder works IN PLACE on
 * this buffer, which is exactly why the scratch copy is gone: nothing has to survive the
 * decode any more. */
static int16_t m_samples[INDALA_PSK_CAPTURE_SAMPLES];

bool lf_psk1_read(lf_psk1_decode_fn decode, lf_psk1_read_t *out,
                  uint32_t timeout_ms, int32_t *energy_out) {
    bool ok = false;
    uint8_t winner_phase = 0;
    indala_psk_result_t winner_res;
    uint8_t winner_tries = 0;
    /* ⚠ The LOUDEST capture, not the last one. A read spends up to eight captures per sample
     * phase across several phases; a source that is present for only part of that budget
     * still means "something was there", and taking the final capture's value would report
     * whatever the antenna happened to be hearing when the timeout expired. */
    int32_t loudest = 0;

    autotimer *p_at = bsp_obtain_timer(0);

    for (size_t pi = 0; pi < PHASE_ROTATION_COUNT && !ok; pi++) {
        const uint8_t phase = PHASE_ROTATION[pi];
        lf_125khz_radio_saadc_phase_set(phase);

        /* ⚠ RESET PER PHASE. A word decoded at one sample phase does not corroborate one at
         * another: that would be a different measurement agreeing, and the rule's evidence is
         * that two reads of the SAME configuration landed on the same word. */
        bool have_prev = false;
        uint8_t prev_word[8] = { 0 };
        indala_psk_result_t res;

        for (uint8_t k = 0; k < INDALA_TRIES_PER_PHASE && !ok; k++) {
            if (!NO_TIMEOUT_1MS(p_at, timeout_ms)) {
                break;
            }
            size_t got = 0;
            if (!raw_read_samples(m_samples, INDALA_PSK_CAPTURE_SAMPLES,
                                  INDALA_CAPTURE_TIMEOUT_MS, &got, 0)) {
                continue;
            }

            /* ⚠ IN PLACE: this consumes m_samples. Nothing needs the raw capture again. */
            bool decoded = decode(m_samples, got, &res);
            /* Set by the decoder either way — see lf_indala_psk.h. */
            if (res.energy > loudest) {
                loudest = res.energy;
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
             * ⚠ INDEPENDENT is the load-bearing word. With stacking gone this is free —
             * consecutive captures share nothing at all, where the two accumulators had to be
             * kept apart by hand so that a stack of 3 was never compared against a stack of 4
             * containing those same 3.
             *
             * ⛔ It is NOT sufficient on its own and never was. A deterministic error agrees
             * with itself: the dead-band straddle returns the same wrong word every time (the
             * gate in lf_indala_psk.c catches that), and a loud IDTECK tag produced a stable
             * false Indala credential at sample phase 28 (C90, caught by the reject preamble).
             * This rule only rejects errors that SCATTER. */
            if (have_prev && memcmp(prev_word, res.id, 8) == 0) {
                winner_res = res;
                winner_tries = (uint8_t)(k + 1);
                winner_phase = phase;
                ok = true;
            } else {
                memcpy(prev_word, res.id, 8);
                have_prev = true;
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

    if (!ok) {
        return false;
    }

    out->res = winner_res;
    out->phase = winner_phase;
    out->tries = winner_tries;
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
    data[14] = r.tries;   /* captures taken at the winning phase */
    data[15] = 0;

    /* ⚠ NRF_LOG takes at most six format arguments (LOG_INTERNAL_0..6); more is a build
     * error deep inside the macro expansion rather than anything that names this line. */
    uint32_t hi = ((uint32_t)res->id[0] << 24) | ((uint32_t)res->id[1] << 16) |
                  ((uint32_t)res->id[2] << 8)  | res->id[3];
    uint32_t lo = ((uint32_t)res->id[4] << 24) | ((uint32_t)res->id[5] << 16) |
                  ((uint32_t)res->id[6] << 8)  | res->id[7];
    NRF_LOG_INFO("indala %08lx%08lx fc %u phase %u tries %u",
                 (unsigned long)hi, (unsigned long)lo,
                 res->fc, winner_phase, r.tries);
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
    data[14] = r.tries;
    data[15] = 0;

    uint32_t card = ((uint32_t)res->id[7] << 16) | ((uint32_t)res->id[6] << 8) | res->id[5];
    NRF_LOG_INFO("idteck card %lu chksum %02x phase %u tries %u",
                 (unsigned long)card, res->id[4], r.phase, r.tries);
    return true;
}
