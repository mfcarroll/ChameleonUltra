#include "lf_reader_generic.h"
#include "lf_reader_data.h"

#include "bsp_delay.h"
#include "bsp_wdt.h"
#include "bsp_time.h"
#include "circular_buffer.h"
#include "lf_125khz_radio.h"
#include "lf_reader_data.h"
#include "protocols/protocols.h"

#define NRF_LOG_MODULE_NAME lfgeneric
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
NRF_LOG_MODULE_REGISTER();

/*
 * Circular buffer for SAADC samples.
 *
 * ⛔⛔ IT MUST HOLD A WHOLE EasyDMA BATCH, AND FOR A LONG TIME IT DID NOT.
 * The SAADC fills adc_buf[] — ADC_BUF_SIZE = 2048 samples, ble_main.c:71 — and only
 * then fires NRFX_SAADC_EVT_DONE, so saadc_cb() is handed 2048 samples AT ONCE every
 * 16.4ms. Against a 512-entry ring that silently dropped 1536 of every 2048 (75%):
 * cb_push_back() fails and the callback returns, discarding the rest of the batch.
 *
 * ⇒ THE CONSEQUENCE WAS NOT "a few lost samples". A capture became ISLANDS of 512
 * contiguous samples (4.1ms) separated by 12.3ms of thrown-away time, stitched together
 * as if they were adjacent. A "2000 sample, 16ms" capture actually spanned 64ms, and
 * every island boundary was a discontinuity — which is what produced the full-scale
 * jumps this project spent days deglitching around, and five false readings.
 *
 * 2560 holds one batch with slack. Batches are 16.4ms apart and the main loop drains
 * one in microseconds, so a second cannot arrive before the first is consumed.
 * ⚠ This is malloc'd, so __HEAP_SIZE in application/Makefile must cover it.
 */
#define CIRCULAR_BUFFER_SIZE (2560)
static circular_buffer cb;

/* Dropped-sample counter. A drop now means something is genuinely wrong rather than
 * being the normal case, so it is worth being able to see. */
static volatile uint32_t m_cb_dropped = 0;

static void saadc_cb(nrf_saadc_value_t *vals, size_t size) {
    for (int i = 0; i < size; i++) {
        nrf_saadc_value_t val = vals[i];
        if (!cb_push_back(&cb, &val)) {
            /* Ring full: the rest of this batch is lost and the capture will have a
             * time discontinuity here. With a batch-sized ring this should never
             * happen; count it so it cannot go unnoticed again. */
            m_cb_dropped += (uint32_t)(size - i);
            return;
        }
    }
}

static void init_saadc_hw(void) {
    lf_125khz_radio_saadc_enable(saadc_cb);
}

static void uninit_saadc_hw(void) {
    lf_125khz_radio_saadc_disable();
}

/* ⭐ ONE COPY OF THE CAPTURE PROLOGUE, because there are now two entry points.
 *
 * Everything load-bearing about starting an LF capture lives here: suspending BLE,
 * allocating a batch-sized ring, starting the field, waiting out the settle, and
 * discarding what was sampled while waiting. lf_reader_data.h already carries a scar from
 * a hand-maintained duplicate of raw_read_to_buffer's prototype going stale twice; a
 * duplicated *body* would be the same mistake with worse symptoms, since it would drift
 * silently rather than failing the build. */
typedef struct {
    bool adv_paused;
} lf_capture_ctx_t;

static bool capture_begin(lf_capture_ctx_t *ctx, uint16_t settle_ms) {
    /* ⭐ SUSPEND BLE ADVERTISING FOR THE DURATION.
     *
     * The bursts that this code's comments have long blamed on "USB transfer overruns"
     * are not lost samples at all — they are the 125kHz FIELD COLLAPSING. In a glitchy
     * capture the envelope drops to near zero for ~1.6ms and returns; the sniff output's
     * own "Gaps: N samples below 0x52 (real field drops)" line has been reporting exactly
     * this all along. Measured samples across one such event:
     *     3140 1472 156 28 24 16380 12 0 0 4 20
     *
     * Each BLE advertising event is a radio transmit burst, and BLE_ADV_MODE_FAST places
     * them tens of ms apart. Against a 16ms capture that predicts a minority of captures
     * being hit — and 4 of 10 were.
     *
     * ⚠ Only when not connected: dropping advertising is harmless, dropping a live link
     * is not. */
    ctx->adv_paused = false;
    if (!g_is_ble_connected) {
        advertising_stop();
        ctx->adv_paused = true;
    }

    m_cb_dropped = 0;
    if (!cb_init(&cb, CIRCULAR_BUFFER_SIZE, sizeof(uint16_t))) {
        /* malloc failed — reporting success here would hand back an empty buffer that
         * looks like a legitimately quiet capture. */
        NRF_LOG_ERROR("lf capture: could not allocate %d-sample ring", CIRCULAR_BUFFER_SIZE);
        if (ctx->adv_paused) {
            advertising_start(false);
        }
        return false;
    }
    init_saadc_hw();
    start_lf_125khz_radio();

    /* Wait for the antenna -- and the TAG -- to settle before capturing.
     * The LC circuit rings for ~400µs on field startup, then takes another ~800µs to
     * reach steady state, which is where the historical 2ms came from. A tag needs
     * longer: it charges off the field before it transmits at full amplitude. */
    if (settle_ms == 0) {
        settle_ms = 2;
    }
    for (uint16_t i = 0; i < settle_ms; i++) {
        bsp_delay_ms(1);
        bsp_wdt_feed();     /* a long settle must not look like a hung main loop */
    }

    /* ⭐ DISCARD WHAT WAS SAMPLED WHILE WAITING. The SAADC is already running and
     * filling the ring during the delay above, so without this the head of the buffer
     * holds the startup transient and the capture returns it FIRST -- making a longer
     * settle return the same early samples rather than later ones, which would show up
     * as "settle does nothing" no matter how long it is set.
     *
     * ⚠ THIS IS NOT THE SAME AS DISCARDING A SETTLE WINDOW FROM THE CAPTURE ITSELF. It
     * drops what was sampled BEFORE the window opens. Dropping the first samples of the
     * window instead takes the Indala decode from 51/160 to 0/160 — see the note in
     * lf_indala_psk.h. */
    {
        uint16_t discard = 0;
        while (cb_pop_front(&cb, &discard)) {
        }
    }
    return true;
}

static void capture_end(lf_capture_ctx_t *ctx) {
    stop_lf_125khz_radio();
    uninit_saadc_hw();
    cb_free(&cb);

    if (ctx->adv_paused) {
        advertising_start(false);
    }

    if (m_cb_dropped) {
        NRF_LOG_WARNING("lf capture: dropped %lu samples — capture is discontinuous",
                        (unsigned long)m_cb_dropped);
    }
}

bool raw_read_to_buffer(uint8_t *data, size_t maxlen, uint32_t timeout_ms, size_t *outlen,
                        bool raw16, uint16_t settle_ms) {
    *outlen = 0;

    lf_capture_ctx_t ctx;
    if (!capture_begin(&ctx, settle_ms)) {
        return false;
    }

    /* raw16 costs two bytes per sample, so stop one short of the end rather than
     * writing half a sample the host would then parse as a whole one. */
    const size_t step = raw16 ? 2 : 1;
    autotimer *p_at = bsp_obtain_timer(0);
    while (NO_TIMEOUT_1MS(p_at, timeout_ms) && *outlen + step <= maxlen) {
        uint16_t val = 0;
        while (cb_pop_front(&cb, &val) && *outlen + step <= maxlen) {
            if (raw16) {
                /* Full 14-bit conversion, big-endian. The high byte can never exceed
                 * 0x3F, which is what lets the host tell the two formats apart. */
                val &= 0x3FFF;
                data[*outlen] = (uint8_t)(val >> 8);
                data[*outlen + 1] = (uint8_t)(val & 0xFF);
            } else {
                uint16_t v8 = val >> 5;  /* 14-bit ADC → 9-bit, then >>5 gives 8-bit */
                data[*outlen] = v8 > 0xff ? 0xff : (uint8_t)v8;
            }
            *outlen += step;
        }
        bsp_wdt_feed();  /* prevent watchdog reset during long captures */
    }

    bsp_return_timer(p_at);
    capture_end(&ctx);
    return true;
}

/* Fill a caller's array with raw 14-bit conversions, for decoders that run over a whole
 * buffer rather than sample by sample.
 *
 * ⚠ UNLIKE raw_read_to_buffer THIS INSISTS ON A FULL BUFFER. A short capture is not a
 * degraded Indala read, it is a failed one: the demodulator needs two whole 64-bit frames
 * (4096 samples) to guarantee that one of them lands entirely inside the window, so
 * returning 3000 samples would merely produce a confident wrong answer. */
bool raw_read_samples(int16_t *samples, size_t count, uint32_t timeout_ms, size_t *outlen,
                      uint16_t settle_ms) {
    *outlen = 0;

    lf_capture_ctx_t ctx;
    if (!capture_begin(&ctx, settle_ms)) {
        return false;
    }

    autotimer *p_at = bsp_obtain_timer(0);
    while (NO_TIMEOUT_1MS(p_at, timeout_ms) && *outlen < count) {
        uint16_t val = 0;
        while (*outlen < count && cb_pop_front(&cb, &val)) {
            /* 14-bit, so the cast to int16_t is always in range and always positive. */
            samples[(*outlen)++] = (int16_t)(val & 0x3FFF);
        }
        bsp_wdt_feed();
    }

    bsp_return_timer(p_at);
    capture_end(&ctx);
    return *outlen == count;
}
