#include <string.h>

#include "bsp_time.h"
#include "lf_125khz_radio.h"
#include "bsp_delay.h"
#include "lf_indala_data.h"
#include "lf_indala_psk.h"
#include "lf_ask_manchester.h"
#include "lf_fsk2a.h"
#include "lf_ask_biphase.h"
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
 * ⭐⭐ RE-MEASURED 2026-09-15, AND THE PARAGRAPH ABOVE NOW DESCRIBES A DECODER THAT NO LONGER
 * EXISTS. It predates the straddle gate (C48) and Indala's zero-bit gate (C257). On the SAME
 * 160 front captures with today's gates: **110 frames and 0 wrong** — the 21 are gone, so
 * there is nothing left to repeat within a phase. On the phasebits corpus, which does still
 * produce wrong frames, there are **13 and every one is DISTINCT**, so agreement at 2 removes
 * all of them and a higher count would remove nothing (C292).
 *
 * ⇒ KEEP THE COUNT AT 2 — and keep the paragraph above, which is history rather than a live
 * hazard and is labelled so rather than deleted. The 60-92 dead band is still out of
 * PHASE_ROTATION, and if that rotation is ever widened the warning becomes live again.
 *
 * ⚠ The Flipper family uses 6 for every PSK1 protocol and 3 for everything else (C291). On
 * this data 6 would buy nothing over 2. No tree explains its choice, so that is a difference
 * in evidence rather than a disagreement about the protocol.
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

/* ⛔ A PER-CAPTURE TIMEOUT MUST SCALE WITH THE CAPTURE, and a flat one silently broke the
 * long format.
 *
 * Sampling is real time on the wire: at 125kHz, `count` samples take count/125 ms. The flat
 * 200ms was ~6x headroom for 4096 samples (32.8ms) and only 1.7x for 14336 (114.7ms) — and
 * the margin is not decoration, because a ring overflow loses samples that then have to be
 * re-collected, costing wall time. The 224-bit read therefore never completed a single
 * capture: `raw_read_samples` returned false every time, the decoder never ran, and the read
 * reported "LF tag not found" on a tag the 64-bit read could hear perfectly.
 *
 * ⚠ THE SYMPTOM NAMED THE WRONG THING. "Not found" is what an empty antenna reports, so the
 * failure looked like a signal problem rather than a budget one. What isolated it was running
 * the 64-bit read on the SAME TAG in the same minute: it reported "subcarrier is present",
 * proving the antenna, the tag and the energy measure were all fine and only the longer
 * capture was failing. */
#define INDALA_CAPTURE_TIMEOUT_MS(count) (200u + ((uint32_t)(count) / 125u) * 2u)

/** Captures attempted per sample phase before moving on. Was the stacking depth; it is now
 *  simply how many independent tries each phase gets, and two of them must agree. */
#define INDALA_TRIES_PER_PHASE  8

/* ⭐ 8KB, and that is the whole reader now — down from 48KB. The decoder works IN PLACE on
 * this buffer, which is exactly why the scratch copy is gone: nothing has to survive the
 * decode any more. */
/* ⚠ SIZED FOR THE LONGEST FRAME, FILLED TO WHAT THE FORMAT ASKS FOR. 28KB rather than 8,
 * and still a third of what the stacked 64-bit reader used to cost. The capture LENGTH is a
 * parameter because it is real time on the wire: 4096 samples is 33ms and 14336 is 114ms, so
 * a 64-bit read that captured the whole buffer would be 3.5x slower for nothing. */
static int16_t m_samples[LF_SAMPLED_MAX_CAPTURE_SAMPLES];

/* ⭐⭐ GPROXII'S OWN PHASE ORDER, AND IT IS MEASURED ON THE DEVICE RATHER THAN INHERITED.
 *
 * The shared rotation above was tuned on an Indala tag and its own comment warns that the map
 * "could move". It does: with the default order this protocol reads at phase 112, which sits
 * SEVENTH of eight, so a 3-second budget spends itself on six phases that never decode and
 * then times out — or worse, accepts a marginal frame from one of them (C206, C208).
 *
 * ⚠ WHY 112'S NEIGHBOURS COME NEXT RATHER THAN THE OLD ENTRIES. On the sniff path this tag
 * decodes at ALL 16 phases tried across 0..120, so the sniff evidence cannot rank them at all;
 * the only ranking that exists is the device's, and it has exactly one winner. Ordering the
 * fallbacks by DISTANCE from that winner is a guess about smoothness, and it is labelled as
 * one. The old rotation's entries are kept at the end so nothing that used to work is lost.
 *
 * ⛔ The mechanism is NOT established. The reader path reports roughly twice the edge
 * amplitude the sniff path does on the same tag, which would point at clipping that only an
 * extreme sampling instant escapes — but that is a hypothesis, and the phase order here is
 * justified by the device measurement alone. */
/* ⭐ THE UPPER BAND, SWEPT DENSELY, AND FEWER TRIES EACH — both halves are forced by the same
 * arithmetic. A GProxII capture is 14336 samples = 114ms, so a 3-second budget buys about 20
 * captures TOTAL. At the shared 8 tries per phase that is two and a half phases: a rotation of
 * eight is a fiction, and the read spends itself before reaching anything that works.
 *
 * ⚠ THE BAND IS CHOSEN FROM THE DEVICE, the only evidence that ranks phases at all. It read
 * at 112 once and at 104 the next run; the sniff path decodes at ALL 16 phases tried across
 * 0..120, so it cannot rank them. Two device wins, both in the upper band, is thin — said
 * plainly — but it is what there is, and the neighbours are included because of it.
 *
 * ⚠ MECHANISM, HYPOTHESIS ONLY: the phase sets where in the 8us carrier period the ADC
 * samples, and every failing device read reports a mean boundary step of ~5050 ADC counts
 * against the sniff path's 2300-2900. A step that large on every boundary is not tag envelope;
 * it looks like residual CARRIER being sampled, which an envelope decoder cannot see past and
 * a PSK one would not care about. The shared rotation was tuned on PSK. Not established. */
static const uint8_t GPROXII_PHASE_ROTATION[] = {
    112, 120, 104, 96, 20, 0
};
#define GPROXII_PHASE_ROTATION_COUNT \
    (sizeof(GPROXII_PHASE_ROTATION) / sizeof(GPROXII_PHASE_ROTATION[0]))
/* ⛔⛔ THREE CONFIGURATIONS TRIED, AND THE BEST OF THEM IS 3 OF 10. Recorded as a table
 * because each was a real measurement and the next person should not repeat them:
 *
 *   phases {112,120,104,96,20,0}  8 tries  3s   3 of 10 exact, 0 wrong   <- shipped
 *   phases {104,112,96,120,88}    4 tries  3s   0 of 12
 *   phases {104,112,120,96,88,20} 8 tries  6s   0 of 12
 *
 * ⚠ Fewer tries per phase buys phase coverage and costs the two-agreeing-captures rule its
 * chances; on this protocol that trade is the wrong way round. And a longer budget did not
 * help either, which is what rules TIME out as the remaining cause.
 * ⛔ The bench was checked immediately after the 0-of-12 run and is FINE: the Proxmark reads
 * the tag, and a `lf sniff` capture taken by this same device decodes exactly on the host with
 * an edge of 2819. The reader path measures ~5070 on the same tag minutes apart. That 1.8x is
 * the whole remaining mystery and no configuration here can tune past it. */
#define GPROXII_TRIES_PER_PHASE 8

bool lf_sampled_read_phases(lf_sampled_decode_fn decode, size_t capture_samples,
                            lf_sampled_read_t *out, uint32_t timeout_ms, int32_t *energy_out,
                            const uint8_t *phases, uint8_t phase_count, uint8_t tries,
                            uint8_t drive, uint16_t gap_ms);

bool lf_sampled_read(lf_sampled_decode_fn decode, size_t capture_samples,
                     lf_sampled_read_t *out, uint32_t timeout_ms, int32_t *energy_out) {
    return lf_sampled_read_phases(decode, capture_samples, out, timeout_ms, energy_out,
                                  PHASE_ROTATION, (uint8_t)PHASE_ROTATION_COUNT,
                                  INDALA_TRIES_PER_PHASE, 0, 0);
}

/* ⭐⭐ THE INSTRUMENT C209 ASKED FOR: run the READER's own capture and hand back the samples
 * it actually got, instead of decoding them.
 *
 * ⛔ WHY IT HAD TO EXIST. On the same tag in the same minute, the reader path measures a mean
 * bit-boundary step of ~5070 ADC counts and `lf sniff` measures 2819 — and the sniff capture
 * decodes exactly on the host while the reader finds nothing. Every setting the two paths
 * differ in has been swept without reproducing it, and both go through the same
 * `capture_begin` and the same SAADC init, so INSPECTION has run out. The only thing left is
 * to look at the reader's own samples, and nothing could.
 *
 * ⭐ It costs no RAM: it captures into `m_samples`, the buffer the reader already fills, and
 * the caller chunks straight out of it. ⚠ Which also means a subsequent read overwrites it,
 * so a host must fetch all the chunks before scanning again — the same contract `lf sniff`
 * already has with its own static buffer.
 *
 * ⚠ Instrumentation. Remove with the rest before upstreaming (§9b). */
bool lf_reader_capture_probe(size_t capture_samples, uint8_t drive, uint8_t phase,
                             uint8_t repeats, uint16_t settle_ms, uint16_t gap_ms,
                             const int16_t **out, size_t *got) {
    if (capture_samples > LF_SAMPLED_MAX_CAPTURE_SAMPLES) {
        capture_samples = LF_SAMPLED_MAX_CAPTURE_SAMPLES;
    }
    if (repeats < 1) {
        repeats = 1;
    }
    lf_125khz_radio_saadc_phase_set(phase);
    *got = 0;
    bool ok = false;
    /* ⭐ `repeats` exists to test ONE hypothesis: a real read takes up to forty captures
     * back to back and the probe took one, and only the probe worked. Returning the LAST of
     * N asks whether a capture late in such a run differs from the first. */
    for (uint8_t i = 0; i < repeats; i++) {
        /* ⚠ A gap with the FIELD OFF, which is not the same as `settle_ms` — settle is
         * field-ON time before the window opens. If what degrades capture 2 is something that
         * holds charge across `stop_lf_125khz_radio()`, only this can let it drain. */
        if (i > 0 && gap_ms > 0) {
            bsp_delay_ms(gap_ms);
        }
        lf_125khz_radio_drive_set(drive);
        *got = 0;
        ok = raw_read_samples(m_samples, capture_samples,
                              INDALA_CAPTURE_TIMEOUT_MS(capture_samples), got, settle_ms);
    }
    /* ⚠ Restore both, for the reason the sniff command's own note gives: a reader that leaves
     * the field or the sample phase altered breaks whatever runs next, invisibly. */
    lf_125khz_radio_drive_set(4);
    lf_125khz_radio_saadc_phase_set(0);
    *out = m_samples;
    return ok;
}

bool lf_sampled_read_phases(lf_sampled_decode_fn decode, size_t capture_samples,
                  lf_sampled_read_t *out, uint32_t timeout_ms, int32_t *energy_out,
                  const uint8_t *phases, uint8_t phase_count, uint8_t tries,
                  uint8_t drive, uint16_t gap_ms) {
    if (capture_samples > LF_SAMPLED_MAX_CAPTURE_SAMPLES) {
        capture_samples = LF_SAMPLED_MAX_CAPTURE_SAMPLES;
    }
    bool ok = false;
    uint8_t winner_phase = 0;
    lf_decode_result_t winner_res;
    uint8_t winner_tries = 0;
    /* ⚠ The LOUDEST capture, not the last one. A read spends up to eight captures per sample
     * phase across several phases; a source that is present for only part of that budget
     * still means "something was there", and taking the final capture's value would report
     * whatever the antenna happened to be hearing when the timeout expired. */
    int32_t loudest = 0;

    autotimer *p_at = bsp_obtain_timer(0);

    for (size_t pi = 0; pi < phase_count && !ok; pi++) {
        const uint8_t phase = phases[pi];
        lf_125khz_radio_saadc_phase_set(phase);

        /* ⚠ RESET PER PHASE. A word decoded at one sample phase does not corroborate one at
         * another: that would be a different measurement agreeing, and the rule's evidence is
         * that two reads of the SAME configuration landed on the same word. */
        bool have_prev = false;
        uint8_t prev_word[8] = { 0 };
        lf_decode_result_t res;

        for (uint8_t k = 0; k < tries && !ok; k++) {
            if (!NO_TIMEOUT_1MS(p_at, timeout_ms)) {
                break;
            }
            /* ⛔⛔ RE-ASSERT THE DRIVE BEFORE EVERY CAPTURE, NOT ONCE PER READ, AND THAT IS
             * A BUG FIX RATHER THAN BELT AND BRACES. Measured: the reader's own captures
             * decode exactly at drive 7 and not at all at drive 4 (C210), and a read that set
             * the drive once before forty captures behaved like drive 4 — mean boundary step
             * ~5070 against drive 7's ~2500, and 3 reads in 10. Each capture runs its own
             * `capture_begin`/`capture_end`, and the duty written into the PWM sequence does
             * not survive that cycle reliably.
             * ⚠ `drive` 0 means "leave it alone", which is what every existing reader passes —
             * they sweep drive themselves in `lf_drive_swept_read` or take the stock field. */
            if (drive != 0) {
                lf_125khz_radio_drive_set(drive);
            }
            /* ⛔⛔ A FIELD-OFF GAP BEFORE EVERY CAPTURE BUT THE FIRST, AND IT IS THE FIX FOR
             * C211 — the one thing that worked after settle, threshold shape, drive, phase
             * order and budget had all been tried and refuted.
             *
             * Measured with the reader-capture probe, four captures back to back, returning the
             * last: gap 0 and 5ms -> wrong frame; 20ms -> wrong; 25, 30, 40 -> exact but 35
             * came back one bit out; 50, 150 and 250ms -> exact every time. So the threshold is
             * around 25ms and it is soft near there. 50 is 2x it, chosen the way every other
             * margin here is.
             *
             * ⚠ WHAT IS CHARGING IS NOT ESTABLISHED. `capture_end` already stops the field, so
             * it is something that holds charge across that — the tag's own storage, or the LF
             * amplifier's AC coupling. What IS established is the symptom: a capture taken too
             * soon after another CLIPS, min 0 and max 16380 against a clean capture's 472 and
             * 14176 (C212), and this decoder thresholds on step magnitude, which is exactly
             * what clipping destroys.
             *
             * ⚠ IT COSTS LATENCY AND ONLY THIS PROTOCOL PAYS IT. 114ms capture + 50ms gap means
             * a 3s budget buys about 14 captures rather than 20. Every other reader passes 0
             * here and is untouched. */
            if (gap_ms > 0 && (pi > 0 || k > 0)) {
                bsp_delay_ms(gap_ms);
            }
            size_t got = 0;
            if (!raw_read_samples(m_samples, capture_samples,
                                  INDALA_CAPTURE_TIMEOUT_MS(capture_samples), &got, 0)) {
                continue;
            }
            /* ⛔⛔ DISCARD A SPLICED CAPTURE. `raw_read_samples` returns true for one: it
             * checks only that it filled the buffer, and a capture that dropped samples from
             * the ring fills it just as completely — out of two pieces of waveform with an
             * unknown gap between them. A decoder does not fail on that; it resynchronises
             * after the gap and returns a frame whose tail is wrong.
             *
             * ⚠ This costs nothing when drops do not happen and is the safe direction when
             * they do: a discarded capture is retried, a spliced one is believed. */
            if (lf_capture_dropped() != 0) {
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
    lf_sampled_read_t r;
    if (!lf_sampled_read(indala_psk1_decode, INDALA_PSK_CAPTURE_SAMPLES,
                      &r, timeout_ms, energy_out)) {
        return false;
    }
    const lf_decode_result_t *res = &r.res;
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
    lf_sampled_read_t r;
    if (!lf_sampled_read(idteck_psk1_decode, INDALA_PSK_CAPTURE_SAMPLES,
                      &r, timeout_ms, energy_out)) {
        return false;
    }
    const lf_decode_result_t *res = &r.res;

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

/* ⭐ KERI DE-SCRAMBLE. The 32-bit internal id carries the facility code and card number
 * interleaved through a fixed bit permutation; these two tables are Momentum's, transcribed
 * from protocol_keri.c, and the Proxmark's cmdlfkeri.c agrees with them.
 *
 * ⚠ 255 means "this source bit is used by neither field" — most of them are. A 32-bit id
 * yields a 5-bit facility code and a 21-bit card number, so eleven bits go nowhere, and
 * that is the format rather than an omission here.
 *
 * ⚠ ADVISORY, like Indala's format-26 fields. A Keri tag written with `-t i` carries a raw
 * internal id with no facility/card structure at all, so the de-scramble of one is
 * meaningless — which is why `keri_read` returns the internal id as well and the host
 * prints both. */
static const uint8_t KERI_CARD_TO_ID[32] = {
    255, 255, 255, 255, 13, 12, 20, 5,   16,  6,  21,
    17,  8,   255, 0,   7,  10, 15, 255, 11,  4,  1,
    255, 18,  255, 19,  2,  14, 3,  9,   255, 255
};
static const uint8_t KERI_CARD_TO_FC[32] = {
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 0,   255, 255, 255, 255, 2,   255, 255, 255,
    3,   255, 4,   255, 255, 255, 255, 255, 1,   255
};

static void keri_descramble(uint32_t internal_id, uint32_t *fc, uint32_t *cn) {
    *fc = 0;
    *cn = 0;
    for (uint8_t i = 0; i < 32; i++) {
        uint32_t bit = (internal_id >> i) & 1u;
        if (KERI_CARD_TO_ID[i] < 32) *cn |= bit << KERI_CARD_TO_ID[i];
        if (KERI_CARD_TO_FC[i] < 32) *fc |= bit << KERI_CARD_TO_FC[i];
    }
}

/* ⭐ KERI — the same capture, the same demodulator, the same 4096 samples as Indala26 and
 * IDTECK, because it is the same air layer (C157). Only the preamble and the payload
 * interpretation differ, which is the whole argument for `lf_psk1_format_t`. */
bool keri_read(uint8_t *data, uint32_t timeout_ms, int32_t *energy_out) {
    lf_sampled_read_t r;
    /* ⛔ NOT INDALA_PSK_CAPTURE_SAMPLES — see KERI_PSK_CAPTURE_SAMPLES. At 4096 this reader
     * returned 0x43 on a real Keri tag six times running while the host decoded the same
     * captures 3 of 4. */
    if (!lf_sampled_read(keri_psk1_decode, KERI_PSK_CAPTURE_SAMPLES,
                      &r, timeout_ms, energy_out)) {
        return false;
    }
    const lf_decode_result_t *res = &r.res;

    /* Bits 32..63 of the frame are the internal id; its top bit is also the last bit of
     * the preamble and is therefore always 1. */
    uint32_t id = ((uint32_t)res->id[4] << 24) | ((uint32_t)res->id[5] << 16) |
                  ((uint32_t)res->id[6] << 8)  |  (uint32_t)res->id[7];
    uint32_t fc = 0, cn = 0;
    keri_descramble(id, &fc, &cn);

    memcpy(&data[0], res->id, 8);
    data[8]  = (uint8_t)(fc & 0xFF);
    data[9]  = (uint8_t)(cn >> 16);
    data[10] = (uint8_t)(cn >> 8);
    data[11] = (uint8_t)(cn & 0xFF);
    data[12] = r.phase;
    data[13] = res->offset;
    data[14] = r.tries;
    data[15] = 0;

    NRF_LOG_INFO("keri id %08lx fc %lu cn %lu phase %u tries %u",
                 (unsigned long)id, (unsigned long)fc, (unsigned long)cn,
                 r.phase, r.tries);
    return true;
}

/* ⭐ NEXWATCH'S DESCRAMBLE — a pure bit permutation, and both references carry the same
 * table (`hex_2_id` in cmdlfnexwatch.c and protocol_nexwatch.c). Entry i says which bit of
 * the SCRAMBLED word supplies bit (31 - i) of the card number. */
static const uint8_t NEXWATCH_HEX_2_ID[32] = {
    31, 27, 23, 19, 15, 11, 7, 3,
    30, 26, 22, 18, 14, 10, 6, 2,
    29, 25, 21, 17, 13, 9,  5, 1,
    28, 24, 20, 16, 12, 8,  4, 0
};

static uint32_t nexwatch_descramble(uint32_t scrambled) {
    uint32_t id = 0;
    for (uint8_t idx = 0; idx < 32; idx++) {
        uint32_t bit = (scrambled >> NEXWATCH_HEX_2_ID[idx]) & 1u;
        id |= bit << (31u - idx);
    }
    return id;
}

static uint8_t nexwatch_reflect8(uint8_t v) {
    uint8_t r = 0;
    for (uint8_t i = 0; i < 8; i++) {
        r = (uint8_t)(((unsigned)r << 1) | ((v >> i) & 1u));
    }
    return r;
}

/* NexWatch's checksum: a running SUBTRACT over the descrambled card number's four bytes, the
 * magic and the reflected parity, then reflected. Verbatim from `nexwatch_checksum`. */
static uint8_t nexwatch_checksum(uint8_t magic, uint32_t id, uint8_t parity) {
    uint8_t a = (uint8_t)((id >> 24) & 0xFFu);
    a = (uint8_t)(a - ((id >> 16) & 0xFFu));
    a = (uint8_t)(a - ((id >> 8) & 0xFFu));
    a = (uint8_t)(a - (id & 0xFFu));
    a = (uint8_t)(a - magic);
    a = (uint8_t)(a - (nexwatch_reflect8(parity) >> 4));
    return nexwatch_reflect8(a);
}

/* ⭐ NEXWATCH — the same capture and the same demodulator once more, which is the fourth
 * protocol through `lf_psk1_format_t` and the reason it exists. The acceptance (40 fixed
 * bits plus the computed parity) lives in the format; what is left here is interpretation.
 *
 * ⚠ THE MAGIC BYTE IS INFERRED, NOT READ. It is not carried in the frame at all: the
 * checksum is computed over the card number, the parity and a vendor constant, so the only
 * way to name the vendor is to try the three known constants and see which reproduces the
 * frame's checksum. ⛔ A tag whose checksum matches none of them is still a valid read — it
 * passed a 44-bit gate — and reports magic 0x00 rather than failing. Refusing it would
 * refuse a vendor we have not met. */
bool nexwatch_read(uint8_t *data, uint32_t timeout_ms, int32_t *energy_out) {
    lf_sampled_read_t r;
    if (!lf_sampled_read(nexwatch_psk1_decode, NEXWATCH_PSK_CAPTURE_SAMPLES,
                      &r, timeout_ms, energy_out)) {
        return false;
    }
    const lf_decode_result_t *res = &r.res;

    /* bits 40..71 are the scrambled card number; 72..75 the mode; 76..79 the parity;
     * 80..87 the checksum. The frame is byte-aligned throughout, so these are whole bytes. */
    uint32_t scrambled = ((uint32_t)res->id[5] << 24) | ((uint32_t)res->id[6] << 16) |
                         ((uint32_t)res->id[7] << 8)  |  (uint32_t)res->id[8];
    uint32_t cn = nexwatch_descramble(scrambled);
    uint8_t mode   = (uint8_t)(res->id[9] >> 4);
    uint8_t parity = (uint8_t)(res->id[9] & 0x0Fu);
    uint8_t chk    = res->id[10];

    static const uint8_t MAGICS[3] = {
        NEXWATCH_MAGIC_QUADRAKEY, NEXWATCH_MAGIC_NEXKEY, NEXWATCH_MAGIC_HONEYWELL
    };
    uint8_t magic = 0;
    for (uint8_t i = 0; i < 3; i++) {
        if (nexwatch_checksum(MAGICS[i], cn, parity) == chk) {
            magic = MAGICS[i];
            break;
        }
    }

    memcpy(&data[0], res->id, 12);
    data[12] = (uint8_t)(cn >> 24);
    data[13] = (uint8_t)(cn >> 16);
    data[14] = (uint8_t)(cn >> 8);
    data[15] = (uint8_t)(cn & 0xFFu);
    data[16] = magic;
    data[17] = mode;
    data[18] = r.phase;
    data[19] = res->offset;

    NRF_LOG_INFO("nexwatch cn %lu mode %u magic %02x phase %u tries %u",
                 (unsigned long)cn, mode, magic, r.phase, r.tries);
    return true;
}

/* Forward declaration: the field-strength sweep is defined with the Noralsy reader, where the
 * measurement that forced it is written up.
 *
 * ⚠ IT IS NOT AN ASK THING, WHICH IS WHY IT IS NO LONGER CALLED ONE. It was named `lf_ask_read`
 * when three ASK protocols were its only callers; today the four FSK2a readers go through it
 * too, and a reviewer reading `lf_fsk2a.c` call `lf_ask_read` would reasonably take it for a
 * mistake. Same class of rename as C192, and for the same reason: a shared thing named after
 * the first family to use it is a trap for whoever comes second. ⚠ GProxII does NOT use it —
 * it needs a fixed drive and its own phase order, not a sweep. */
static bool lf_drive_swept_read(lf_sampled_decode_fn decode, size_t capture_samples,
                        lf_sampled_read_t *out, uint32_t timeout_ms, int32_t *energy_out);

/* ⭐ GALLAGHER — the first protocol of the ASK/biphase family, and it goes through the SAME
 * capture engine as every PSK protocol here. `lf_sampled_read` is modulation-agnostic despite
 * its name: it rotates the sample phase, suspends BLE advertising (C47) and requires two
 * independent captures to agree before returning, and none of that is PSK-specific. Only the
 * decoder handed to it changes.
 *
 * ⚠ The capture is 14336 samples — 114ms, against 33ms for the 64-bit PSK formats — because
 * the threshold was MEASURED at 10240 and the guess of 6144 decoded 0 of 4 (C172). */
bool gallagher_read(uint8_t *data, uint32_t timeout_ms, int32_t *energy_out) {
    lf_sampled_read_t r;
    if (!lf_drive_swept_read(gallagher_ask_decode, GALLAGHER_ASK_CAPTURE_SAMPLES,
                     &r, timeout_ms, energy_out)) {
        return false;
    }
    memcpy(&data[0], r.res.id, 12);
    data[12] = r.phase;
    data[13] = r.res.offset;
    data[14] = r.tries;
    data[15] = 0;
    NRF_LOG_INFO("gallagher phase %u offset %u tries %u", r.phase, r.res.offset, r.tries);
    return true;
}

/* ⭐ SECURAKEY — the same capture engine and the same decoder as Gallagher, with a different
 * `lf_ask_format_t`. The only protocol-specific thing here is the capture length. */
bool securakey_read(uint8_t *data, uint32_t timeout_ms, int32_t *energy_out) {
    lf_sampled_read_t r;
    if (!lf_drive_swept_read(securakey_ask_decode, SECURAKEY_ASK_CAPTURE_SAMPLES,
                     &r, timeout_ms, energy_out)) {
        return false;
    }
    memcpy(&data[0], r.res.id, 12);
    data[12] = r.phase;
    data[13] = r.res.offset;
    data[14] = r.tries;
    data[15] = 0;
    NRF_LOG_INFO("securakey phase %u offset %u tries %u", r.phase, r.res.offset, r.tries);
    return true;
}

/* ⛔⛔ THE ASK READS SWEEP DRIVE, AND THIS REINSTATES A RULE I WITHDREW ONE PROTOCOL TOO EARLY.
 *
 * C169 found our reader saturating on a Gallagher tag at every drive but 7, and inferred that
 * every ASK reader must sweep drive as `pac_read()` does. C171 then measured Gallagher
 * decoding 4 of 4 from the CLIPPED captures as well as the clean ones, so the rule was
 * withdrawn: Manchester is a transition code, clipping keeps the sign and destroys only the
 * magnitude.
 *
 * ⛔ NORALSY REFUTES THE WITHDRAWAL. Measured on the bench tag, the shipping decoder against
 * one capture per setting: **stock, 1, 2, 4 and 6 all decode 0; drive 7 decodes.** The device
 * read returned `LF tag not found` 6 times while the host read the same tag 4 of 4 from
 * captures that had been taken at `--drive 7` — the reader was simply never given the field
 * strength the evidence was collected at.
 *
 * ⇒ The honest rule is neither of the two I wrote. Clipping does not stop Manchester in
 * PRINCIPLE (C171 stands), and it demonstrably stops it for SOME protocols in practice. Both
 * of my earlier claims generalised from a single protocol — PAC to the family, then Gallagher
 * to the family — and the family disagrees with both. Sweep, and let the tag decide.
 *
 * ⛔⛔ "STEP ORDER MATTERS FOR LATENCY, NOT CORRECTNESS" IS WHAT THIS COMMENT USED TO SAY,
 * AND IT IS FALSE. Two protocols in this sweep read at ONE field strength and no other:
 * Noralsy at drive 7 (C182) and FDX-B at drive 4 (C273, measured 16/16 at 4 against 0/16
 * at 7, 0/16 at 6 and nothing at 2 or 1). For those two the sweep is not an optimisation,
 * it is the only reason they read at all.
 *
 * ⚠ AND THE TABLE TRUNCATES. `steps = timeout_ms / LF_ASK_DRIVE_MIN_STEP_MS` takes a
 * PREFIX of this array, so under a short budget the tail is never tried: 250ms reaches
 * only drive 4, 500ms only {4,7}. Noralsy needs the second entry and would go unreadable
 * with no other symptom.
 *
 * ⭐ Nothing is broken today and the margin is why: every caller passes
 * INDALA_READ_TIMEOUT_MS = 3000, which asks for 12 steps and clamps to 4, so all four are
 * always tried. ⇒ **That margin is load-bearing. Shortening the timeout to make reads
 * snappier is the change that breaks Noralsy**, and this note exists so that is discovered
 * here rather than in the field.
 *
 * ⚠ 4 goes first because it is the stock value and what Gallagher, Securakey and FDX-B
 * read at, so the common case pays nothing. That part of the old note stands. */
static const uint8_t LF_ASK_DRIVE_STEPS[] = { 4, 7, 6, 2 };
#define LF_ASK_DRIVE_STEP_COUNT (sizeof(LF_ASK_DRIVE_STEPS) / sizeof(LF_ASK_DRIVE_STEPS[0]))
/* A 96-bit frame at RF/32 is 24.6ms and the capture engine wants several tries per step. */
#define LF_ASK_DRIVE_MIN_STEP_MS (250)

/* ⭐ One capture engine, one acceptance rule, one extra loop. `lf_sampled_read` already rotates
 * the sample phase and requires two independent captures to agree; this wraps it in the field
 * strength sweep the ASK family needs, and divides the caller's budget between the steps
 * rather than multiplying it — the same discipline `pac_read` uses, and for the same reason. */
static bool lf_drive_swept_read(lf_sampled_decode_fn decode, size_t capture_samples,
                        lf_sampled_read_t *out, uint32_t timeout_ms, int32_t *energy_out) {
    uint32_t steps = timeout_ms / LF_ASK_DRIVE_MIN_STEP_MS;
    if (steps < 1) {
        steps = 1;
    } else if (steps > LF_ASK_DRIVE_STEP_COUNT) {
        steps = LF_ASK_DRIVE_STEP_COUNT;
    }
    uint32_t step_ms = timeout_ms / steps;

    bool ok = false;
    int32_t loudest = 0;
    for (uint32_t i = 0; i < steps && !ok; i++) {
        lf_125khz_radio_drive_set(LF_ASK_DRIVE_STEPS[i]);
        int32_t e = 0;
        ok = lf_sampled_read(decode, capture_samples, out, step_ms, &e);
        if (e > loudest) {
            loudest = e;
        }
    }
    /* ⚠ RESTORE THE STOCK DRIVE. A reader that leaves the field weakened breaks whatever runs
     * next, which is invisible in the thing being measured — exactly the bug found in
     * `cmd_processor_lf_sniff` (C148/L122). */
    lf_125khz_radio_drive_set(4);
    if (energy_out != NULL) {
        *energy_out = loudest;
    }
    return ok;
}

/* ⭐ NORALSY — the third ASK protocol, and the cheapest read in the family: 6144 samples
 * against Gallagher's and Securakey's 14336, because its frame sits near the start of the
 * stream rather than a hundred bits in (C181). */
bool noralsy_read(uint8_t *data, uint32_t timeout_ms, int32_t *energy_out) {
    lf_sampled_read_t r;
    if (!lf_drive_swept_read(noralsy_ask_decode, NORALSY_ASK_CAPTURE_SAMPLES,
                     &r, timeout_ms, energy_out)) {
        return false;
    }
    memcpy(&data[0], r.res.id, 12);
    data[12] = r.phase;
    data[13] = r.res.offset;
    data[14] = r.tries;
    data[15] = 0;
    NRF_LOG_INFO("noralsy phase %u offset %u tries %u", r.phase, r.res.offset, r.tries);
    return true;
}

/* ⭐ INSTAFOB — the fourth ASK protocol and the only one whose frame is not 96 bits. 225 bits
 * at RF/32 is 7200 samples, so the 14336 maximum holds 1.99 frames and a whole frame lands
 * inside for 99.1% of start offsets rather than the 100% every other format here enjoys
 * (C185). ⚠ That is why this one uses the buffer maximum and has no margin to give. */
bool instafob_read(uint8_t *data, uint32_t timeout_ms, int32_t *energy_out) {
    lf_sampled_read_t r;
    if (!lf_drive_swept_read(instafob_ask_decode, INSTAFOB_ASK_CAPTURE_SAMPLES,
                     &r, timeout_ms, energy_out)) {
        return false;
    }
    memcpy(&data[0], r.res.id, INSTAFOB_READ_FRAME_BYTES);
    data[29] = r.phase;
    data[30] = r.res.offset;
    data[31] = r.tries;
    NRF_LOG_INFO("instafob phase %u offset %u tries %u", r.phase, r.res.offset, r.tries);
    return true;
}

/* ⭐ AWID — the FSK2a family's first protocol, and it goes through the SAME capture engine as
 * every PSK and ASK protocol here. ⛔ It does NOT touch `lf_hidprox_data.c`'s per-protocol
 * SAADC reader, which is the family C47 found missing the BLE-advertising guard and where
 * HID's unexplained intermittency lives (C45). ⇒ §10 ranked this family last on the
 * assumption that it must reuse that path; it does not (C193). */
bool awid_read(uint8_t *data, uint32_t timeout_ms, int32_t *energy_out) {
    lf_sampled_read_t r;
    if (!lf_drive_swept_read(awid_fsk_decode, AWID_FSK_CAPTURE_SAMPLES, &r, timeout_ms, energy_out)) {
        return false;
    }
    memcpy(&data[0], r.res.id, 12);
    awid_fsk_payload(r.res.word_bits, &data[12]);
    data[21] = r.phase;
    data[22] = r.tries;
    data[23] = 0;
    NRF_LOG_INFO("awid phase %u tries %u", r.phase, r.tries);
    return true;
}

/* ⭐ PARADOX AND PYRAMID — the same decoder as AWID with a different `lf_fsk2a_format_t`,
 * which is the whole claim the FSK design made and the only family here where a second and
 * third protocol genuinely cost one descriptor each (C197). */
bool paradox_read(uint8_t *data, uint32_t timeout_ms, int32_t *energy_out) {
    lf_sampled_read_t r;
    if (!lf_drive_swept_read(paradox_fsk_decode, PARADOX_FSK_CAPTURE_SAMPLES,
                     &r, timeout_ms, energy_out)) {
        return false;
    }
    memcpy(&data[0], r.res.id, 12);
    data[12] = r.phase;
    data[13] = r.tries;
    data[14] = 0;
    data[15] = 0;
    return true;
}

bool pyramid_read(uint8_t *data, uint32_t timeout_ms, int32_t *energy_out) {
    lf_sampled_read_t r;
    if (!lf_drive_swept_read(pyramid_fsk_decode, PYRAMID_FSK_CAPTURE_SAMPLES,
                     &r, timeout_ms, energy_out)) {
        return false;
    }
    memcpy(&data[0], r.res.id, 16);
    data[16] = r.phase;
    data[17] = r.tries;
    data[18] = 0;
    data[19] = 0;
    return true;
}

/* ⭐ FDX-A — the fourth and last FSK protocol, and the only one with two encoding layers:
 * FSK2a on the wire carrying Manchester inside it (C199). */
bool fdxa_read(uint8_t *data, uint32_t timeout_ms, int32_t *energy_out) {
    lf_sampled_read_t r;
    if (!lf_drive_swept_read(fdxa_fsk_decode, FDXA_FSK_CAPTURE_SAMPLES, &r, timeout_ms, energy_out)) {
        return false;
    }
    memcpy(&data[0], r.res.id, 12);
    fdxa_fsk_payload(r.res.word_bits, &data[12]);
    data[17] = r.phase;
    data[18] = r.tries;
    data[19] = 0;
    return true;
}

/* ⭐ GPROXII — the same capture engine again, with the BIPHASE decoder rather than the
 * ASK/Manchester or FSK2a one. Four decode paths now share `lf_sampled_read`: it rotates the
 * sample phase, suspends BLE advertising (C47) and requires two independent captures to agree,
 * and none of that has ever been specific to a modulation.
 *
 * ⛔ THE TWO-AGREEING-STACKS RULE IS LOAD-BEARING HERE IN A WAY IT IS NOT FOR THE OTHERS.
 * GProxII's remaining false positive is NexWatch at 1 capture in 4 (C205) — sporadic, so two
 * captures agreeing does reject it. It was NOT load-bearing against the Securakey false
 * positive, which was byte-identical on three captures; that one had to be killed in the gate.
 *
 * ⚠ 114ms per capture — the buffer maximum — because the threshold measured 12800 and there
 * is only 1.12x headroom above it. */
bool gproxii_read(uint8_t *data, uint32_t timeout_ms, int32_t *energy_out) {
    lf_sampled_read_t r;
    /* ⛔⛔ FIXED AT DRIVE 7 — NOT `lf_drive_swept_read`'s SWEEP, AND THE SWEEP IS WHAT THIS FIXES.
     *
     * The first device reads of this protocol went through that sweep, which starts at the
     * stock drive 4 and stops at the FIRST success. Drive 4 succeeded — with bit errors that
     * cleared the preamble, all 18 spacers AND the format-length gate:
     *
     *     fac2a38c2b081af0212b12c2   Length 26  FC 123  Card 1081   <- wrong card, confident
     *     fac2a38c2b081af0250b12c6   Length 26  FC 123  Card 1321   <- wrong card, confident
     *
     * ⛔ TWO CAPTURES AGREED ON EACH OF THOSE. The capture engine's two-agreeing-stacks rule
     * did not catch it and could not: at a fixed drive the distortion is the same in both
     * captures, so they agree on the same wrong answer. 2 reads of 6 returned a WRONG card
     * number — the stable-wrong-credential failure this project exists downstream of (C160).
     *
     * ⭐ The host had already measured the answer and the sweep discarded it: at drive 7 this
     * decoder is 96/96 bits exact, at drive 6 it is 95/96 and at drives 4 and 2 it is in the
     * low 80s (C204). Only drive 7 is clean, so only drive 7 is used.
     *
     * ⚠ A SWEEP STARTING AT 7 WOULD STILL BE WRONG. It would fall through to 6, 4 and 2 on a
     * weakly coupled tag and hand back exactly these frames. A fixed drive fails to read
     * instead, which is the safe direction: "not found" is recoverable, a wrong card number
     * read confidently is not. ⇒ If a GProxII turns up that needs another field strength, the
     * answer is a stronger gate, not a wider sweep.
     *
     * ⛔⛔ AND PINNING THE DRIVE WAS NOT SUFFICIENT — THIS ARM IS NOT VERIFIED. At drive 7 the
     * device returns `fac2a38c2b081af0210b12c6` where the tag holds `...12c2`: a PERSISTENT
     * single-bit error at frame bit 93, on 8 reads of 8. It happens to sit outside format 26's
     * facility and card fields, so 7 of those 8 still print the right credential — and the
     * eighth printed **Card 1321** instead of 1337. ⇒ Do NOT read this as "works with a cosmetic
     * raw difference". A decoder that is one bit wrong every time is one bit away from being
     * wrong where it matters, and once in eight it already is.
     *
     * ⚠ The host decodes `lf sniff --drive 7` captures from this same device 4 of 4 EXACT, so
     * the algorithm is right and something between the sniff path and the reader path is not.
     * That is the open question; see NEXT.md. Until it is answered this command ships as
     * research, and the grid must say NOT VERIFIED rather than a read count. */
    bool ok = lf_sampled_read_phases(gproxii_biphase_decode, GPROXII_BIPHASE_CAPTURE_SAMPLES,
                                     &r, timeout_ms, energy_out,
                                     GPROXII_PHASE_ROTATION,
                                     (uint8_t)GPROXII_PHASE_ROTATION_COUNT,
                                     GPROXII_TRIES_PER_PHASE, 7,
                                     GPROXII_CAPTURE_GAP_MS);
    /* ⚠ Restore the stock drive — a reader that leaves the field weakened breaks whatever runs
     * next, invisibly (C148/L122). */
    lf_125khz_radio_drive_set(4);
    if (!ok) {
        return false;
    }
    memcpy(&data[0], r.res.id, 12);
    data[12] = r.phase;
    data[13] = r.res.bit_pos;
    data[14] = r.tries;
    data[15] = r.res.inverted ? 1u : 0u;
    NRF_LOG_INFO("gproxii phase %u pos %u tries %u", r.phase, r.res.bit_pos, r.tries);
    return true;
}

/* ⭐ FDX-B — the second biphase protocol, and it takes NONE of GProxII's special handling.
 *
 * ⚠ Measured rather than inherited, which is the whole point of doing the second protocol of
 * a family: it reads at the STOCK drive where GProxII needs 7 and decodes at nothing else, and
 * four captures back to back with no gap decode exactly where GProxII's second capture is
 * already wrong (C211, C214). So it uses the plain shared reader — shared rotation, no drive
 * override, no gap — and GProxII's overrides stay GProxII's.
 *
 * ⚠ Its capture is 10240 samples, deliberately NOT the buffer maximum: at 14336 one capture
 * in four comes back with a corrupted tail that the CRC cannot see. */
bool fdxb_read(uint8_t *data, uint32_t timeout_ms, int32_t *energy_out) {
    lf_sampled_read_t r;
    if (!lf_sampled_read(fdxb_biphase_decode, FDXB_BIPHASE_CAPTURE_SAMPLES,
                         &r, timeout_ms, energy_out)) {
        return false;
    }
    memcpy(&data[0], r.res.id, 16);
    data[16] = r.phase;
    data[17] = r.res.bit_pos;
    data[18] = r.tries;
    data[19] = r.res.inverted ? 1u : 0u;
    NRF_LOG_INFO("fdxb phase %u pos %u tries %u", r.phase, r.res.bit_pos, r.tries);
    return true;
}

/* ⭐ INDALA224, and the only thing that differs from the others is the capture length and
 * the payload. 28 bytes of frame is more than the 16-byte scan convention carries, so this
 * returns the frame in full and leaves interpretation to the host — there is no agreed
 * facility-code layout for 224-bit Indala the way there is for format 26, and inventing one
 * in firmware would be the kind of guess this project keeps having to retract.
 *
 * ⚠ A capture is 114ms here against 33ms for the 64-bit formats, so the same 3s budget buys
 * ~26 attempts rather than ~90. */
/* ⛔⛔ THE 224-BIT READER IS DISABLED AND MUST STAY DISABLED UNTIL ITS ACCEPTANCE RULE IS
 * SOLVED. Set to 1 only with the measurement that justifies it.
 *
 * The demodulation WORKS: against a tag whose memory is known, the exact 224 bits come out of
 * the differential view of all four committed captures (caps/indala224/). What does not work
 * is deciding WHICH candidate is the frame:
 *
 *   - Indala224's preamble is a 1 and 29 zeros, so a one-bit-shifted alignment matches it
 *     almost as well as the true one.
 *   - The direct (PSK1) view of this PSK2 tag contains a preamble-matching candidate that
 *     repeats at the frame period just as convincingly as the real one — ~98% either way —
 *     so the repeat test cannot arbitrate between the two views.
 *   - That impostor is IDENTICAL across captures, so the two-capture agreement rule would
 *     confirm it rather than catch it. This is C90's failure mode in a weaker format.
 *
 * ✅ RESOLVED 2026-09-12 by decoding this format as PSK2 ONLY — see `differential_only` in
 * lf_indala_psk.h. A reader cannot discover the modulation from the signal, because the
 * direct view of a PSK2 tag is the running XOR of its data and is therefore just as
 * self-consistent as the data itself; it has to be told. Told, the result is 2 of 4 captures
 * decoding and **both correct, zero wrong** — correct-or-nothing, which the two-capture
 * agreement rule turns into a reliable read. */
#define INDALA224_READER_TRUSTED 1

bool indala224_read(uint8_t *data, uint32_t timeout_ms, int32_t *energy_out) {
#if !INDALA224_READER_TRUSTED
    /* Still report energy, so `lf indala read --224` says "a subcarrier is present but no
     * frame could be decoded" rather than pretending the antenna is empty. */
    lf_sampled_read_t probe;
    (void)lf_sampled_read(indala224_psk1_decode, INDALA224_PSK_CAPTURE_SAMPLES,
                       &probe, timeout_ms, energy_out);
    (void)data;
    return false;
#else
    lf_sampled_read_t r;
    if (!lf_sampled_read(indala224_psk1_decode, INDALA224_PSK_CAPTURE_SAMPLES,
                      &r, timeout_ms, energy_out)) {
        return false;
    }
    memcpy(&data[0], r.res.id, INDALA224_READ_FRAME_BYTES);
    data[INDALA224_READ_FRAME_BYTES + 0] = r.phase;
    data[INDALA224_READ_FRAME_BYTES + 1] = r.res.offset;
    data[INDALA224_READ_FRAME_BYTES + 2] = r.tries;
    data[INDALA224_READ_FRAME_BYTES + 3] = 0;

    NRF_LOG_INFO("indala224 %02x%02x%02x.. phase %u tries %u",
                 r.res.id[0], r.res.id[1], r.res.id[2], r.phase, r.tries);
    return true;
#endif
}
