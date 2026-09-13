#include <stdlib.h>
#include <string.h>

#include "securakey.h"
#include "t55xx.h"
#include "tag_base_type.h"

/* ⭐ THE SAME EMITTER AS GALLAGHER WITH ONE NUMBER CHANGED, which is the whole claim §10 made
 * for grouping by modulation. At the 125kHz base clock one PWM tick is one carrier cycle, so
 * an entry with counter_top = 40 spans one RF/40 bit and a half duty makes it Manchester.
 *
 * ⛔ 40, NOT 32. Gallagher is RF/32 and em410x RF/64; emitting Securakey at Gallagher's rate
 * would produce a clean Manchester signal at the wrong speed, which a reader rejects with no
 * indication that the rate is the problem (C175). */
#define SECURAKEY_CARRIER_CYCLES_PER_BIT (40)

static nrf_pwm_values_wave_form_t m_securakey_vals[SECURAKEY_BIT_COUNT] = {};

static const nrf_pwm_sequence_t m_securakey_seq = {
    .values.p_wave_form = m_securakey_vals,
    .length = NRF_PWM_VALUES_LENGTH(m_securakey_vals),
    .repeats = 0,
    .end_delay = 0,
};

static securakey_codec *securakey_alloc(void) {
    securakey_codec *d = malloc(sizeof(securakey_codec));
    memset(d->data, 0, SECURAKEY_DATA_SIZE);
    return d;
}

static void securakey_free(securakey_codec *d) {
    free(d);
}

static uint8_t *securakey_get_data(securakey_codec *d) {
    return d->data;
}

/* ⚠ Stubs — the read path is the whole-capture demodulator in
 * rfid/reader/lf/lf_ask_manchester.c, not this interface. */
static void securakey_decoder_start(securakey_codec *d, uint8_t format) {
    (void)d;
    (void)format;
}

static bool securakey_decoder_feed(securakey_codec *d, uint16_t val) {
    (void)d;
    (void)val;
    return false;
}

static const nrf_pwm_sequence_t *securakey_modulator(securakey_codec *d, uint8_t *buf) {
    (void)d;
    for (int i = 0; i < SECURAKEY_BIT_COUNT; i++) {
        bool bit = (buf[i / 8] >> (7 - (i % 8))) & 1u;
        m_securakey_vals[i].channel_0 = (uint16_t)((bit ? (1u << 15) : 0u) |
                                        (SECURAKEY_CARRIER_CYCLES_PER_BIT / 2));
        m_securakey_vals[i].channel_1 = 0;
        m_securakey_vals[i].channel_2 = 0;
        m_securakey_vals[i].counter_top = SECURAKEY_CARRIER_CYCLES_PER_BIT;
    }
    return &m_securakey_seq;
}

const protocol securakey = {
    .tag_type = TAG_TYPE_SECURAKEY,
    .data_size = SECURAKEY_DATA_SIZE,
    .alloc = (codec_alloc)securakey_alloc,
    .free = (codec_free)securakey_free,
    .get_data = (codec_get_data)securakey_get_data,
    .modulator = (modulator)securakey_modulator,
    .decoder =
        {
            .start = (decoder_start)securakey_decoder_start,
            .feed = (decoder_feed)securakey_decoder_feed,
        },
};

/* T5577 writer: block 0 the ASK RF/40 config, blocks 1-3 the 96 bits big-endian.
 *
 * ⭐ TRANSCRIBE, and it was CHECKED rather than assumed from Gallagher. A Proxmark clone of
 * `7FCB400001ADEA5344300000` holds `7FCB4000 / 01ADEA53 / 44300000` — block 1 is the frame
 * head, so the block form and the air frame coincide. ⛔ Keri's do not (C158), and copying a
 * neighbour's alignment without looking is how that cost a wrong credential 6 of 6 (C160). */
uint8_t securakey_t55xx_writer(uint8_t *frame12, uint32_t *blks) {
    blks[0] = T5577_SECURAKEY_CONFIG;
    for (int w = 0; w < 3; w++) {
        uint32_t v = 0;
        for (int i = 0; i < 4; i++) v = (v << 8) | frame12[w * 4 + i];
        blks[1 + w] = v;
    }
    return 4;
}
