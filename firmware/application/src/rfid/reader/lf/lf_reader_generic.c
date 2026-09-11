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
 * Increased from 128 to 512 to reduce overrun risk during USB transfer.
 * The main loop drains it as fast as possible into the output buffer.
 */
#define CIRCULAR_BUFFER_SIZE (512)
static circular_buffer cb;

static void saadc_cb(nrf_saadc_value_t *vals, size_t size) {
    for (int i = 0; i < size; i++) {
        nrf_saadc_value_t val = vals[i];
        if (!cb_push_back(&cb, &val)) {
            return;  /* buffer full — oldest samples dropped */
        }
    }
}

static void init_saadc_hw(void) {
    lf_125khz_radio_saadc_enable(saadc_cb);
}

static void uninit_saadc_hw(void) {
    lf_125khz_radio_saadc_disable();
}

bool raw_read_to_buffer(uint8_t *data, size_t maxlen, uint32_t timeout_ms, size_t *outlen,
                        bool raw16, uint16_t settle_ms) {
    *outlen = 0;

    cb_init(&cb, CIRCULAR_BUFFER_SIZE, sizeof(uint16_t));
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
     * as "settle does nothing" no matter how long it is set. */
    {
        uint16_t discard = 0;
        while (cb_pop_front(&cb, &discard)) {
        }
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
    stop_lf_125khz_radio();
    uninit_saadc_hw();
    cb_free(&cb);

    return true;
}
