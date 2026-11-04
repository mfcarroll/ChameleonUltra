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
 * It must hold a whole SAADC batch: the SAADC hands saadc_cb() ADC_BUF_SIZE
 * (2048, ble_main.c) samples at once, every 16.4ms, and anything that doesn't
 * fit is dropped. 2560 holds one batch with slack; the main loop drains it
 * long before the next. It is malloc'd, so __HEAP_SIZE in application/Makefile
 * must cover it.
 */
#define CIRCULAR_BUFFER_SIZE (2560)
static circular_buffer cb;

/* Samples lost because the ring was full. Zero unless something is wrong. */
static volatile uint32_t m_cb_dropped = 0;

static void saadc_cb(nrf_saadc_value_t *vals, size_t size) {
    for (int i = 0; i < size; i++) {
        nrf_saadc_value_t val = vals[i];
        if (!cb_push_back(&cb, &val)) {
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

uint32_t raw_read_dropped(void) {
    return m_cb_dropped;
}

bool raw_read_to_buffer(uint8_t *data, size_t maxlen, uint32_t timeout_ms, size_t *outlen,
                        bool raw16, uint16_t settle_ms, uint8_t rate_khz) {
    *outlen = 0;

    m_cb_dropped = 0;
    if (!cb_init(&cb, CIRCULAR_BUFFER_SIZE, sizeof(uint16_t))) {
        return false;
    }
    init_saadc_hw();
    start_lf_125khz_radio();

    /* The antenna rings for about 400us after the field starts and settles over the next
     * 800us, so the capture starts at least 2ms in. A tag can need longer to power up.
     * The SAADC delivers samples in batches, so the settling samples are skipped by count. */
    if (settle_ms < 2) {
        settle_ms = 2;
    }
    uint32_t skip = (uint32_t)settle_ms * (rate_khz != 0 ? rate_khz : 125u);

    autotimer *p_at = bsp_obtain_timer(0);
    size_t step = raw16 ? 2 : 1;
    while (NO_TIMEOUT_1MS(p_at, timeout_ms + settle_ms) && *outlen + step <= maxlen) {
        uint16_t val = 0;
        while (*outlen + step <= maxlen && cb_pop_front(&cb, &val)) {
            if (skip != 0) {
                skip--;
                continue;
            }
            if ((int16_t)val < 0) {
                val = 0;  /* the SAADC reads slightly below ground as a negative value */
            }
            if (raw16) {
                data[(*outlen)++] = (uint8_t)(val >> 8);  /* 14 bits, big-endian */
                data[(*outlen)++] = (uint8_t)val;
            } else {
                val >>= 5;  /* 14 bits to 9, clamped to 8 */
                data[(*outlen)++] = val > 0xff ? 0xff : (uint8_t)val;
            }
        }
        bsp_wdt_feed();
    }

    bsp_return_timer(p_at);
    stop_lf_125khz_radio();
    uninit_saadc_hw();
    cb_free(&cb);

    return true;
}
