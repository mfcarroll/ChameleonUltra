#include <string.h>

#include "bsp_delay.h"
#include "bsp_time.h"
#include "circular_buffer.h"
#include "lf_125khz_radio.h"
#include "lf_reader_data.h"
#include "nrfx_saadc.h"
#include "protocols/pac.h"
#include "protocols/protocols.h"

#define NRF_LOG_MODULE_NAME pac_reader
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
NRF_LOG_MODULE_REGISTER();

#define PAC_BUFFER_SIZE (6144)

static circular_buffer cb;

// SAADC callback — push raw ADC samples to circular buffer.
// NRZ/Direct modulation requires ADC sampling (not GPIOTE edge timing)
// because the comparator may not produce clean digital edges for NRZ signals.
static void pac_saadc_cb(nrf_saadc_value_t *vals, size_t size) {
    for (size_t i = 0; i < size; i++) {
        nrf_saadc_value_t val = vals[i];
        if (!cb_push_back(&cb, &val)) {
            return;
        }
    }
}

static void init_pac_hw(void) {
    lf_125khz_radio_saadc_enable(pac_saadc_cb);
}

static void uninit_pac_hw(void) {
    lf_125khz_radio_saadc_disable();
}

/* ⭐⭐ THE READER'S OWN FIELD IS TOO STRONG FOR A WELL-COUPLED TAG, AND THAT IS THE BUG.
 *
 * PAC read 0 of 10 on a T5577 the Proxmark reads perfectly. The cause is not the decoder: on a
 * strongly coupled tag the LF amplifier SATURATES, and **37% of every capture sits pinned at a
 * rail** with the clipping upstream of the ADC, where no gain setting can reach it — 1/6 is
 * already the lowest, and the other tap, AIN0, is pinned the other way (C140).
 *
 * ⭐ Measured against a known frame, scored as bit errors out of 128 rather than pass/fail:
 *
 *     drive 2   34.0% rail   6 errors        drive 6   26.0% rail   0 errors  ← decodes
 *     drive 4   36.4% rail   6 errors        drive 7    0.0% rail   1 error   ← too weak
 *
 * So there is a genuine optimum: enough field to modulate strongly, little enough not to clip.
 * ⛔ Which step wins is a property of the COUPLING, not of the protocol — 6 is right for a tag
 * lying on the antenna and would be wrong for one held at a distance, which is exactly the
 * spread C46 saw when three byte-identical tags read 0/6, 3/6 and 7/9.
 *
 * ⇒ Sweep rather than pick. Stock first, so a tag that already reads is unaffected and costs
 * nothing; then weaker, for the loud tags that are the whole complaint; then stronger, for the
 * marginal ones. ⚠ Each step needs its own `decoder.start` because the demodulator calibrates
 * its thresholds during a prescan, and a calibration from the previous field strength is worse
 * than none. */
static const uint8_t PAC_DRIVE_STEPS[] = { 4, 6, 2, 7 };
#define PAC_DRIVE_STEP_COUNT   (sizeof(PAC_DRIVE_STEPS) / sizeof(PAC_DRIVE_STEPS[0]))
/* A frame is 128 bits at RF/32 = 32.8ms, so a step needs several frames to be a fair try. */
#define PAC_DRIVE_MIN_STEP_MS  (120)

bool pac_read(uint8_t *data, uint32_t timeout_ms) {
    void *codec = pac.alloc();

    /* C47: suspend advertising for the capture — see lf_reader_data.h. Held across every
     * step: re-advertising between them would put the bursts back inside the capture. */
    lf_adv_guard_t adv;
    lf_adv_suspend(&adv);

    /* ⛔ DROP STEPS RATHER THAN OVERRUN THE CALLER'S TIMEOUT. Flooring the step length and
     * keeping all four would turn a 200ms request into 480ms — the caller asked for a budget,
     * not a suggestion, and a read that silently takes 2.4x as long is the kind of thing that
     * surfaces much later as "the CLI hangs". Fit as many whole steps as the budget allows,
     * always at least one, and divide the budget evenly between them. */
    uint32_t steps = timeout_ms / PAC_DRIVE_MIN_STEP_MS;
    if (steps < 1) {
        steps = 1;
    } else if (steps > PAC_DRIVE_STEP_COUNT) {
        steps = PAC_DRIVE_STEP_COUNT;
    }
    uint32_t step_ms = timeout_ms / steps;

    bool ok = false;
    for (uint8_t i = 0; i < steps && !ok; i++) {
        pac.decoder.start(codec, 0);
        lf_125khz_radio_drive_set(PAC_DRIVE_STEPS[i]);

        /* Start the carrier first, then wait for T55XX POR (~5ms) before enabling the SAADC,
         * so the prescan calibration sees real NRZ levels rather than power-up noise. */
        start_lf_125khz_radio();
        bsp_delay_ms(10);

        cb_init(&cb, PAC_BUFFER_SIZE, sizeof(uint16_t));
        init_pac_hw();

        autotimer *p_at = bsp_obtain_timer(0);
        while (!ok && NO_TIMEOUT_1MS(p_at, step_ms)) {
            uint16_t val = 0;
            while (!ok && NO_TIMEOUT_1MS(p_at, step_ms) && cb_pop_front(&cb, &val)) {
                if (pac.decoder.feed(codec, val)) {
                    memcpy(data, pac.get_data(codec), pac.data_size);
                    ok = true;
                    break;
                }
            }
        }
        bsp_return_timer(p_at);

        stop_lf_125khz_radio();
        uninit_pac_hw();
        cb_free(&cb);
        if (!ok) {
            NRF_LOG_INFO("pac: no frame at drive %d", PAC_DRIVE_STEPS[i]);
        }
    }

    /* ⛔ Never leave a weakened field behind: the setting is global to the LF radio and the
     * next reader to run would inherit it. */
    lf_125khz_radio_drive_set(4);
    lf_adv_resume(&adv);

    pac.free(codec);
    return ok;
}
