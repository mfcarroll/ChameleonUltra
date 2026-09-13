#include <stdlib.h>
#include <string.h>

#include "noralsy.h"
#include "t55xx.h"
#include "tag_base_type.h"

/* ⭐ THE THIRD PROTOCOL THROUGH THE SAME EMITTER, and Gallagher's number rather than
 * Securakey's: Noralsy is RF/32. At the 125kHz base clock one PWM tick is one carrier cycle,
 * so counter_top = 32 spans one bit and a half duty makes it Manchester. */
#define NORALSY_CARRIER_CYCLES_PER_BIT (32)

static nrf_pwm_values_wave_form_t m_noralsy_vals[NORALSY_BIT_COUNT] = {};

static const nrf_pwm_sequence_t m_noralsy_seq = {
    .values.p_wave_form = m_noralsy_vals,
    .length = NRF_PWM_VALUES_LENGTH(m_noralsy_vals),
    .repeats = 0,
    .end_delay = 0,
};

static noralsy_codec *noralsy_alloc(void) {
    noralsy_codec *d = malloc(sizeof(noralsy_codec));
    memset(d->data, 0, NORALSY_DATA_SIZE);
    return d;
}

static void noralsy_free(noralsy_codec *d) {
    free(d);
}

static uint8_t *noralsy_get_data(noralsy_codec *d) {
    return d->data;
}

/* ⚠ Stubs — the read path is the whole-capture demodulator, not this interface. */
static void noralsy_decoder_start(noralsy_codec *d, uint8_t format) {
    (void)d;
    (void)format;
}

static bool noralsy_decoder_feed(noralsy_codec *d, uint16_t val) {
    (void)d;
    (void)val;
    return false;
}

static const nrf_pwm_sequence_t *noralsy_modulator(noralsy_codec *d, uint8_t *buf) {
    (void)d;
    for (int i = 0; i < NORALSY_BIT_COUNT; i++) {
        bool bit = (buf[i / 8] >> (7 - (i % 8))) & 1u;
        m_noralsy_vals[i].channel_0 = (uint16_t)((bit ? (1u << 15) : 0u) |
                                      (NORALSY_CARRIER_CYCLES_PER_BIT / 2));
        m_noralsy_vals[i].channel_1 = 0;
        m_noralsy_vals[i].channel_2 = 0;
        m_noralsy_vals[i].counter_top = NORALSY_CARRIER_CYCLES_PER_BIT;
    }
    return &m_noralsy_seq;
}

const protocol noralsy = {
    .tag_type = TAG_TYPE_NORALSY,
    .data_size = NORALSY_DATA_SIZE,
    .alloc = (codec_alloc)noralsy_alloc,
    .free = (codec_free)noralsy_free,
    .get_data = (codec_get_data)noralsy_get_data,
    .modulator = (modulator)noralsy_modulator,
    .decoder =
        {
            .start = (decoder_start)noralsy_decoder_start,
            .feed = (decoder_feed)noralsy_decoder_feed,
        },
};

/* T5577 writer: transcribe. ⭐ CHECKED, not inherited: a Proxmark clone of card 112233 holds
 * `BB0214FF / 01124022 / 33670000`, so block 1 is the frame head (C181). */
uint8_t noralsy_t55xx_writer(uint8_t *frame12, uint32_t *blks) {
    blks[0] = T5577_NORALSY_CONFIG;
    for (int w = 0; w < 3; w++) {
        uint32_t v = 0;
        for (int i = 0; i < 4; i++) v = (v << 8) | frame12[w * 4 + i];
        blks[1 + w] = v;
    }
    return 4;
}
