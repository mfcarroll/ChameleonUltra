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
    20, 12, 28, 36, 16, 24, 40, 8, 44, 32, 56, 4, 48, 60, 52
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
#define INDALA_MAX_CANDIDATES 4

/** Per-capture ceiling. 4096 samples at 125kHz is 32.8ms of sampling; 200ms is headroom
 *  for the settle and the ring drain, not a budget anything is expected to use. */
#define INDALA_CAPTURE_TIMEOUT_MS 200

/* 8KB. The decoder works in place on this, so there is no second copy. */
static int16_t m_samples[INDALA_PSK_CAPTURE_SAMPLES];

bool indala_read(uint8_t *data, uint32_t timeout_ms) {
    uint8_t  cand[INDALA_MAX_CANDIDATES][8];
    uint8_t  seen[INDALA_MAX_CANDIDATES] = { 0 };
    uint8_t  ncand = 0;
    bool     ok = false;
    indala_psk_result_t r;
    indala_psk_result_t winner;
    uint8_t  winner_phase = 0;

    memset(&winner, 0, sizeof(winner));

    autotimer *p_at = bsp_obtain_timer(0);
    for (uint32_t attempt = 0; !ok && NO_TIMEOUT_1MS(p_at, timeout_ms); attempt++) {
        uint8_t phase = PHASE_ROTATION[attempt % PHASE_ROTATION_COUNT];
        lf_125khz_radio_saadc_phase_set(phase);

        size_t got = 0;
        if (!raw_read_samples(m_samples, INDALA_PSK_CAPTURE_SAMPLES,
                              INDALA_CAPTURE_TIMEOUT_MS, &got, 0)) {
            continue;
        }
        if (!indala_psk1_decode(m_samples, got, &r)) {
            continue;
        }

        /* Count this word against every candidate seen so far, not just the previous one:
         * a wrong frame between two good ones must not break the run. */
        uint8_t slot = INDALA_MAX_CANDIDATES;
        for (uint8_t i = 0; i < ncand; i++) {
            if (memcmp(cand[i], r.id, 8) == 0) {
                slot = i;
                break;
            }
        }
        if (slot == INDALA_MAX_CANDIDATES) {
            if (ncand < INDALA_MAX_CANDIDATES) {
                slot = ncand++;
                memcpy(cand[slot], r.id, 8);
            } else {
                /* Four distinct words and none repeating means this is not a readable
                 * coupling; start over rather than letting noise accumulate a majority. */
                ncand = 0;
                memset(seen, 0, sizeof(seen));
                continue;
            }
        }
        if (++seen[slot] >= INDALA_AGREE_COUNT) {
            winner = r;
            winner_phase = phase;
            ok = true;
        }
    }
    bsp_return_timer(p_at);

    /* ⚠ Never leave a sample phase set: every other LF reader on this device shares the
     * trigger and expects the stock PWMPERIODEND one. */
    lf_125khz_radio_saadc_phase_set(0);

    if (!ok) {
        return false;
    }

    memcpy(&data[0], winner.id, 8);
    data[8]  = winner.fc;
    data[9]  = (uint8_t)(winner.csn >> 8);
    data[10] = (uint8_t)(winner.csn & 0xFF);
    data[11] = (uint8_t)((winner.wiegand26_ok ? 0x04u : 0x00u) | (winner.parity & 0x03u));
    data[12] = winner_phase;
    data[13] = winner.offset;
    data[14] = 0;
    data[15] = 0;

    /* ⚠ NRF_LOG takes at most six format arguments (LOG_INTERNAL_0..6); more is a build
     * error deep inside the macro expansion rather than anything that names this line. */
    uint32_t hi = ((uint32_t)winner.id[0] << 24) | ((uint32_t)winner.id[1] << 16) |
                  ((uint32_t)winner.id[2] << 8)  | winner.id[3];
    uint32_t lo = ((uint32_t)winner.id[4] << 24) | ((uint32_t)winner.id[5] << 16) |
                  ((uint32_t)winner.id[6] << 8)  | winner.id[7];
    NRF_LOG_INFO("indala %08lx%08lx fc %u csn %u phase %u",
                 (unsigned long)hi, (unsigned long)lo,
                 winner.fc, winner.csn, winner_phase);
    return true;
}
