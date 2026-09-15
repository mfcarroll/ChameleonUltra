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
 *   GProxII (biphase): ONE entry per bit, counter_top the whole bit, and the data picks
 *     whether a mid-bit transition is added — carried by channel_0's top (inversion) bit,
 *     the same trick gallagher.c uses. A 0 bit holds one level for the whole period.
 *
 * ⭐ The rule, straight from the decoder this is tested against: a transition at every bit
 * BOUNDARY by construction, and an extra one mid-bit for a 1. `lf_ask_biphase.c` asks exactly
 * one question per bit — did the middle step too — so this emitter answers it by construction.
 *
 * ⚠ `counter_top` IS 64 — THE WHOLE BIT — AND THIS COMMENT USED TO SAY 32 (C428). An earlier
 * design spent two entries per bit at a half-bit counter_top; the code has long since spent
 * ONE entry per bit at `GPROXII_CARRIER_CYCLES_PER_BIT`, putting the mid-bit transition in
 * channel_0's inversion bit instead. The buffer is sized to match, one entry per bit, and was
 * re-checked against F13's class of defect — it does not overflow.
 *
 * ⛔ The paragraph here also used to propose this emitter as a CONTROL that would localise the
 * AWID defect by comparing counter_top magnitudes. That question is answered and the answer is
 * not magnitude as such: C422 held the ratio, duty and buffer constant and moved only the tone
 * RATE, and the emission recovered — F12 is a bandwidth limit (C424/C425, consistent but
 * unconfirmed), and C427 found no firmware fix for it. Do not re-run that comparison. */
static nrf_pwm_values_wave_form_t m_gproxii_vals[GPROXII_BIT_COUNT] = {};

static const nrf_pwm_sequence_t m_gproxii_seq = {
    .values.p_wave_form = m_gproxii_vals,
    .length = NRF_PWM_VALUES_LENGTH(m_gproxii_vals),
    .repeats = 0,
    .end_delay = 0,
};

#define GPROXII_CARRIER_CYCLES_PER_BIT (64)

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
//
// ⭐⭐ ONE ENTRY PER BIT, AND THAT IS AN EXPERIMENT RATHER THAN A TIDY-UP. The first version of
// this emitter spent TWO entries per bit, one per half-bit, each holding a level — and the
// Flipper read it 0 of 6 with a Gallagher control at 6 of 6, exactly as it reads AWID's
// five-or-six-entries-per-bit emitter at 0 of 6 (C222).
//
// ⭐ The only structural property separating the seven emitters that work from the two that do
// not is that every working one stores ONE entry per bit. This version makes GProxII one of
// them WITHOUT changing anything else, which turns that correlation into a two-way test:
//
//   reads now  -> entries-per-bit was the cause, and held levels are fine
//   still 0/6  -> held levels are the cause, since Gallagher works with one entry per bit
//
// ⚠ Either answer is worth having and neither needs an instrument this bench lacks.
//
// The encoding: a bit period is 64 carrier cycles and a transition falls at every boundary by
// construction, so the level at the START of each bit alternates. A 1 adds a mid-bit
// transition, which is a 50% square — Gallagher's exact shape. A 0 holds its level for the
// whole bit, which is duty 0 or duty == counter_top.
static const nrf_pwm_sequence_t *gproxii_modulator(gproxii_codec *d, uint8_t *buf) {
    (void)d;
    bool level = false;
    for (int i = 0; i < GPROXII_BIT_COUNT; i++) {
        const bool bit = (buf[i / 8] >> (7 - (i % 8))) & 1u;
        level = !level;                     /* the boundary transition */
        uint16_t ch0;
        if (bit) {
            /* Half at `level`, half at its opposite. The top bit of channel_0 inverts the
             * output, so it carries which half is high — the same trick gallagher.c uses. */
            ch0 = (uint16_t)((level ? 0u : (1u << 15)) |
                             (GPROXII_CARRIER_CYCLES_PER_BIT / 2));
        } else {
            /* ⛔⛔ HELD FOR THE WHOLE BIT — AND THE HIGH CASE MUST BE `counter_top + 1`, NOT
             * `counter_top`. This codebase already knew: `jablotron.c` and `pac.c` both hold a
             * level and both use compare = counter_top + 1, each with a comment citing the
             * nRF52840 PS — "compare >= counter_top -> pin held HIGH", and using exactly
             * counter_top leaves a 1-tick glitch because the counter DOES reach it.
             *
             * ⚠ The first two versions of this emitter used `counter_top`, so every held-high
             * bit carried that glitch. Worse, the em410x probe that "proved" held levels are
             * silent (C242) used the same wrong idiom — so what it actually demonstrated was
             * that the GLITCHY form is silent, which is a different claim. */
            ch0 = (uint16_t)(level ? (GPROXII_CARRIER_CYCLES_PER_BIT + 1u) : 0u);
        }
        m_gproxii_vals[i].channel_0 = ch0;
        m_gproxii_vals[i].channel_1 = 0;
        m_gproxii_vals[i].channel_2 = 0;
        m_gproxii_vals[i].counter_top = GPROXII_CARRIER_CYCLES_PER_BIT;
        if (bit) {
            level = !level;                 /* the mid-bit transition leaves the level flipped */
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
