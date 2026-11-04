#include "lf_reader_generic.h"
#include "lf_reader_data.h"

#include "bsp_delay.h"
#include "bsp_wdt.h"
#include "bsp_time.h"
#include "circular_buffer.h"
#include "lf_125khz_radio.h"
#include "lf_reader_data.h"
#include "protocols/protocols.h"
#include "protocols/t55xx.h"
#include "timeslot.h"

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

/* The EM4x05 read of word 0 that the Proxmark's `lf search` sends: two start bits, the read
 * command 1001 and six address bits with their parity, all zero. A 0 bit is a field gap and
 * a 1 bit is 32 carrier cycles of field. The first start bit is the longer start gap. */
static void em4x05_read_word0_timeslot_cb(void) {
    static const uint8_t bits[] = {0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0};
    stop_lf_125khz_radio();
    bsp_delay_us(55 * 8);
    start_lf_125khz_radio();
    bsp_delay_us(18 * 8);
    for (size_t i = 1; i < sizeof(bits); i++) {
        if (bits[i]) {
            bsp_delay_us(32 * 8);
        } else {
            stop_lf_125khz_radio();
            bsp_delay_us(23 * 8);
            start_lf_125khz_radio();
            bsp_delay_us(18 * 8);
        }
    }
}

static void send_probe(uint8_t probe) {
    switch (probe) {
        case LF_PROBE_T55XX_TRACE:
            /* A regular read of page 1 block 1 (the trace block), without a password. */
            t55xx_send_cmd(T5577_OPCODE_PAGE1, NULL, 0, NULL, 1);
            break;
        case LF_PROBE_EM4X05_WORD0:
            request_timeslot(10 * 1000, em4x05_read_word0_timeslot_cb);
            break;
        default:
            break;
    }
}

bool raw_read_to_buffer(uint8_t *data, size_t maxlen, uint32_t timeout_ms, size_t *outlen,
                        bool raw16, uint16_t settle_ms, uint8_t rate_khz) {
    return raw_read_probe_to_buffer(data, maxlen, timeout_ms, outlen, raw16, settle_ms, rate_khz,
                                    LF_PROBE_NONE);
}

bool raw_read_probe_to_buffer(uint8_t *data, size_t maxlen, uint32_t timeout_ms, size_t *outlen,
                              bool raw16, uint16_t settle_ms, uint8_t rate_khz, uint8_t probe) {
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

    /* A probe goes out once the tag has powered up, and the capture starts with it, so the
     * capture holds the command's field gaps and then the tag's answer. */
    if (probe != LF_PROBE_NONE) {
        bsp_delay_ms(settle_ms);
        uint16_t stale = 0;
        while (cb_pop_front(&cb, &stale)) {}
        m_cb_dropped = 0;
        skip = 0;
        send_probe(probe);
    }

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
