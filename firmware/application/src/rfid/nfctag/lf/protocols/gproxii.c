#include <stdlib.h>
#include <string.h>

#include "gproxii.h"
#include "t55xx.h"
#include "tag_base_type.h"

/* ⭐⭐ BIPHASE EMULATION IS A HELD LEVEL PER HALF-BIT, which makes it the simplest emitter here
 * and NOT the shape either neighbour uses.
 *
 *   Gallagher (ASK/Manchester): one entry per BIT, duty fixed at half, and the DATA picks the
 *     polarity — the entry itself is the transition.
 *   AWID (FSK2a): six or five entries per bit, duty half, and the data picks the counter_top.
 *   GProxII (biphase): two entries per bit, counter_top fixed at a half-bit, and the data
 *     picks whether the LEVEL changes between them. Each entry holds ONE level for its whole
 *     period — duty 0 or duty == counter_top — and nothing toggles inside an entry.
 *
 * ⭐ The rule, straight from the decoder this is tested against: a transition at every bit
 * BOUNDARY by construction, and an extra one mid-bit for a 1. `lf_ask_biphase.c` asks exactly
 * one question per bit — did the middle step too — so this emitter answers it by construction.
 *
 * ⚠ WHY 32 AND NOT 64. A GProxII bit is RF/64, but the emitter's unit is the HALF-bit, so
 * `counter_top` is 32 — the same value Gallagher uses for a whole RF/32 bit. That matters
 * beyond tidiness: AWID's emitter uses counter_top 8 and 10, it is the one emitter here the
 * Flipper will not read, and Momentum's own demodulator accepts its ideal output (C220). If
 * this one reads on the same rig, the difference between them is the counter_top magnitude and
 * that localises the AWID defect. If it does NOT read, the fault is broader than FSK. */
static nrf_pwm_values_wave_form_t m_gproxii_vals[GPROXII_PWM_ENTRIES] = {};

static const nrf_pwm_sequence_t m_gproxii_seq = {
    .values.p_wave_form = m_gproxii_vals,
    .length = NRF_PWM_VALUES_LENGTH(m_gproxii_vals),
    .repeats = 0,
    .end_delay = 0,
};

#define GPROXII_CARRIER_CYCLES_PER_HALF_BIT (32)

static gproxii_codec *gproxii_alloc(void) {
    gproxii_codec *d = malloc(sizeof(gproxii_codec));
    memset(d->data, 0, GPROXII_DATA_SIZE);
    return d;
}

static void gproxii_free(gproxii_codec *d) {
    free(d);
}

static uint8_t *gproxii_get_data(gproxii_codec *d) {
    return d->data;
}

/* ⚠ Stubs, as for every other LF protocol here — the tag-emulation ADC path is not the read
 * path. `lf gproxii read` demodulates a whole capture in rfid/reader/lf/lf_ask_biphase.c. */
static void gproxii_decoder_start(gproxii_codec *d, uint8_t format) {
    (void)d;
    (void)format;
}

static bool gproxii_decoder_feed(gproxii_codec *d, uint16_t val) {
    (void)d;
    (void)val;
    return false;
}

// buf is the 12-byte frame, MSB first on air, preamble included.
static const nrf_pwm_sequence_t *gproxii_modulator(gproxii_codec *d, uint8_t *buf) {
    (void)d;
    bool level = false;
    for (int i = 0; i < GPROXII_BIT_COUNT; i++) {
        const bool bit = (buf[i / 8] >> (7 - (i % 8))) & 1u;
        /* Every bit boundary is a transition. */
        level = !level;
        for (uint8_t half = 0; half < 2; half++) {
            if (half == 1 && bit) {
                level = !level;      /* the extra mid-bit transition IS the 1 */
            }
            /* A HELD level: duty 0 or the whole period, never half. */
            m_gproxii_vals[i * 2 + half].channel_0 =
                (uint16_t)(level ? GPROXII_CARRIER_CYCLES_PER_HALF_BIT : 0u);
            m_gproxii_vals[i * 2 + half].channel_1 = 0;
            m_gproxii_vals[i * 2 + half].channel_2 = 0;
            m_gproxii_vals[i * 2 + half].counter_top = GPROXII_CARRIER_CYCLES_PER_HALF_BIT;
        }
    }
    return &m_gproxii_seq;
}

const protocol gproxii = {
    .tag_type = TAG_TYPE_GPROXII,
    .data_size = GPROXII_DATA_SIZE,
    .alloc = (codec_alloc)gproxii_alloc,
    .free = (codec_free)gproxii_free,
    .get_data = (codec_get_data)gproxii_get_data,
    .modulator = (modulator)gproxii_modulator,
    .decoder =
        {
            .start = (decoder_start)gproxii_decoder_start,
            .feed = (decoder_feed)gproxii_decoder_feed,
        },
};
