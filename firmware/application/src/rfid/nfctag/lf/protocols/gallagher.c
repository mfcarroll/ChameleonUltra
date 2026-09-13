#include <stdlib.h>
#include <string.h>

#include "gallagher.h"
#include "t55xx.h"
#include "tag_base_type.h"

/* ⭐⭐ ASK/MANCHESTER EMULATION COSTS ONE PWM ENTRY PER BIT AND NOTHING ELSE NEW, because
 * em410x has been doing exactly this since before any of the PSK work. The mechanism:
 * `pwm_init()` runs the peripheral at 125kHz for every non-PSK1 tag type, so ONE TICK IS ONE
 * CARRIER CYCLE. An entry with `counter_top = 32` therefore spans 32 carrier cycles — one
 * RF/32 bit — and a duty of half that is a half-bit low followed by a half-bit high, which
 * IS Manchester. The data bit selects the polarity through the top bit of channel_0.
 *
 * ⇒ em410x is the same code with counter_top 64 (RF/64) and a 64-bit frame. The difference
 * between the two protocols at this layer is two numbers.
 *
 * ⛔ AND THIS IS WHY THE PSK1 MODULATOR COULD NOT BE BENT INTO CARRYING IT. That one emits a
 * phase-modulated fc/2 SUBCARRIER at a 1MHz base clock and keeps the field amplitude
 * constant; ASK keys the amplitude itself. They are different physical layers that happen to
 * share a peripheral, and `IS_PSK1_TYPE` is what selects between the two base clocks — so
 * Gallagher must stay OUT of that macro or it would be clocked 8x too fast.
 *
 * ⚠ NO SEQUENCE TERMINATOR IS EMITTED, though a real Gallagher T5577 sets the ST bit in its
 * config (C171). Both reference decoders accept the frame without it — they gate on the
 * 16-bit preamble appearing again 96 bits later, which a plain looping frame satisfies — so
 * this is a deliberate simplification rather than an oversight. ⇒ If a reader is ever found
 * that needs the terminator, that is the first thing to suspect. */
static nrf_pwm_values_wave_form_t m_gallagher_vals[GALLAGHER_BIT_COUNT] = {};

static const nrf_pwm_sequence_t m_gallagher_seq = {
    .values.p_wave_form = m_gallagher_vals,
    .length = NRF_PWM_VALUES_LENGTH(m_gallagher_vals),
    .repeats = 0,
    .end_delay = 0,
};

#define GALLAGHER_CARRIER_CYCLES_PER_BIT (32)

static gallagher_codec *gallagher_alloc(void) {
    gallagher_codec *d = malloc(sizeof(gallagher_codec));
    memset(d->data, 0, GALLAGHER_DATA_SIZE);
    return d;
}

static void gallagher_free(gallagher_codec *d) {
    free(d);
}

static uint8_t *gallagher_get_data(gallagher_codec *d) {
    return d->data;
}

/* ⚠ Stubs, for the same reason Indala's and Keri's are: the tag-emulation ADC path is not
 * the read path. `lf gallagher read` demodulates a whole capture in
 * rfid/reader/lf/lf_ask_manchester.c. */
static void gallagher_decoder_start(gallagher_codec *d, uint8_t format) {
    (void)d;
    (void)format;
}

static bool gallagher_decoder_feed(gallagher_codec *d, uint16_t val) {
    (void)d;
    (void)val;
    return false;
}

// buf is the 12-byte frame, MSB first on air, preamble included.
static const nrf_pwm_sequence_t *gallagher_modulator(gallagher_codec *d, uint8_t *buf) {
    (void)d;
    for (int i = 0; i < GALLAGHER_BIT_COUNT; i++) {
        bool bit = (buf[i / 8] >> (7 - (i % 8))) & 1u;
        m_gallagher_vals[i].channel_0 = (uint16_t)((bit ? (1u << 15) : 0u) |
                                        (GALLAGHER_CARRIER_CYCLES_PER_BIT / 2));
        m_gallagher_vals[i].channel_1 = 0;
        m_gallagher_vals[i].channel_2 = 0;
        m_gallagher_vals[i].counter_top = GALLAGHER_CARRIER_CYCLES_PER_BIT;
    }
    return &m_gallagher_seq;
}

const protocol gallagher = {
    .tag_type = TAG_TYPE_GALLAGHER,
    .data_size = GALLAGHER_DATA_SIZE,
    .alloc = (codec_alloc)gallagher_alloc,
    .free = (codec_free)gallagher_free,
    .get_data = (codec_get_data)gallagher_get_data,
    .modulator = (modulator)gallagher_modulator,
    .decoder =
        {
            .start = (decoder_start)gallagher_decoder_start,
            .feed = (decoder_feed)gallagher_decoder_feed,
        },
};

// T5577 writer: block 0 the ASK RF/32 configuration, blocks 1-3 the 96 bits big-endian.
//
// ⭐ A STRAIGHT TRANSCRIPTION, and as with NexWatch that is the MEASURED answer rather than
// the assumed one. A Proxmark clone of region 1 / facility 4321 / card 6789 / issue 2 holds
// `7FEAA31E / 76D86C6D / 868CC249`, whose first block begins with the frame's own `0x7FEA`
// preamble — so the block form and the air frame coincide. ⛔ Keri's do NOT: it holds
// `(id << 3) | 7`, three bits out of phase with its air frame (C158), and emitting the wrong
// one of the two gave a stable WRONG credential 6 of 6 (C160). Which case a protocol is in is
// read off a real clone's block dump, never assumed from a neighbour.
//
// ⚠ THERE IS NO EMULATOR HERE YET, and that is deliberate rather than an omission. The shared
// PSK1 modulator cannot carry this: it emits a phase-modulated fc/2 subcarrier, where ASK
// needs the field amplitude keyed. An ASK/Manchester emitter is its own piece of work.
uint8_t gallagher_t55xx_writer(uint8_t *frame12, uint32_t *blks) {
    blks[0] = T5577_GALLAGHER_CONFIG;
    for (int w = 0; w < 3; w++) {
        uint32_t v = 0;
        for (int i = 0; i < 4; i++) v = (v << 8) | frame12[w * 4 + i];
        blks[1 + w] = v;
    }
    return 4;
}
