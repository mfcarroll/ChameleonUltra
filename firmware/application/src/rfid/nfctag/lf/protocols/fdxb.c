#include <stdlib.h>
#include <string.h>

#include "fdxb.h"
#include "t55xx.h"
#include "tag_base_type.h"

/* ⭐⭐ THE SECOND BIPHASE EMITTER, AND IT IS gproxii.c's MODULATOR AT A DIFFERENT SCALE.
 *
 * The encoding rule is the decoder's, not the protocol's prose: `lf_ask_biphase.c` asks exactly
 * ONE question per bit — did the MIDDLE of this bit step as well as its boundary? — so this
 * emitter answers it by construction. A transition falls at every bit BOUNDARY, and a 1 adds a
 * mid-bit transition. A 0 holds its level for the whole period.
 *
 * ⭐ WHAT DIFFERS FROM GProxII, AND IT IS ONLY THE TWO NUMBERS:
 *
 *   GProxII   96 bits at RF/64 — counter_top 64, bit 512us, half-bit 256us
 *   FDX-B    128 bits at RF/32 — counter_top 32, bit 256us, half-bit 128us
 *
 * Both are far inside the bandwidth F12 closed off: C422/C423 found emission recovers fully by
 * RF/16 and the failure is confined to RF/8 and RF/10, so neither of these rates is at risk.
 *
 * ⭐⭐ THE HELD LEVEL IS THE PART THAT LOOKS WRONG AND IS NOT, AND IT IS NOW MEASURED RATHER
 * THAN ARGUED. A 0 bit emits no transition at all for a whole bit period, which C242 claimed
 * this PWM could not do — *a biphase 0 is a HELD level and this PWM emits nothing for one*.
 * C429 refuted that on the air with the frame built to maximise it: a 92-held-level GProxII
 * frame emitted 89.4% of its periods in the predicted band, and the Flipper decoded the whole
 * emission byte-exact. The idiom below is why.
 *
 * ⛔ THE HIGH CASE MUST BE `counter_top + 1`, NOT `counter_top`. `jablotron.c` and `pac.c` both
 * say so with the nRF52840 PS in hand — "compare >= counter_top -> pin held HIGH" — and using
 * exactly counter_top leaves a 1-tick glitch because the counter DOES reach it. C242's probe
 * used the glitchy form, which is why what it actually demonstrated was that the GLITCHY form
 * is silent, a different claim from the one it recorded.
 *
 * ⚠ THE FRAME IS NOT BUILT HERE. `buf` is the 128-bit frame exactly as it goes on air, MSB
 * first, header and control bits included — the same contract `write_fdxb_to_t55xx` already
 * uses, which takes its 16 bytes ready-made from the CLI. Nothing in this file validates it;
 * `lf fdxb econfig` warns and `lf_ask_biphase.c`'s accept() gate is what actually checks the
 * 11-bit header and the thirteen group-control bits. */
static nrf_pwm_values_wave_form_t m_fdxb_vals[FDXB_BIT_COUNT] = {};

static const nrf_pwm_sequence_t m_fdxb_seq = {
    .values.p_wave_form = m_fdxb_vals,
    .length = NRF_PWM_VALUES_LENGTH(m_fdxb_vals),
    .repeats = 0,
    .end_delay = 0,
};

#define FDXB_CARRIER_CYCLES_PER_BIT (32)

static fdxb_codec *fdxb_alloc(void) {
    fdxb_codec *d = malloc(sizeof(fdxb_codec));
    memset(d->data, 0, FDXB_DATA_SIZE);
    return d;
}

static void fdxb_free(fdxb_codec *d) {
    free(d);
}

static uint8_t *fdxb_get_data(fdxb_codec *d) {
    return d->data;
}

/* ⚠ Stubs, as for every other LF protocol here — the tag-emulation ADC path is not the read
 * path. `lf fdxb read` demodulates a whole capture in rfid/reader/lf/lf_ask_biphase.c. */
static void fdxb_decoder_start(fdxb_codec *d, uint8_t format) {
    (void)d;
    (void)format;
}

static bool fdxb_decoder_feed(fdxb_codec *d, uint16_t val) {
    (void)d;
    (void)val;
    return false;
}

// buf is the 16-byte frame, MSB first on air, preamble included.
static const nrf_pwm_sequence_t *fdxb_modulator(fdxb_codec *d, uint8_t *buf) {
    (void)d;
    bool level = false;
    for (int i = 0; i < FDXB_BIT_COUNT; i++) {
        /* ⛔⛔ INVERTED RELATIVE TO GProxII, AND THIS IS MEASURED, NOT STYLISTIC. FDX-B's
         * mid-bit transition means ZERO where GProxII's means one — `lf_ask_biphase.h` already
         * recorded the asymmetry from the T5577 side (*BIPHASEa (CDP) / RF/32 / Inverted Yes*,
         * where GProxII is not inverted) and C430 confirmed it on the air: emitting this frame
         * uninverted produced correct biphase structure that NO reader would decode, and
         * emitting its bitwise complement decoded as country 999 / national 1337 on the
         * Flipper. ⚠ Without this `!`, `lf fdxb econfig` and `lf fdxb write` would disagree
         * about what a frame MEANS — the emulator would need the complement of the bytes the
         * writer and the reader both use. */
        const bool bit = !((buf[i / 8] >> (7 - (i % 8))) & 1u);
        level = !level;                     /* the boundary transition */
        uint16_t ch0;
        if (bit) {
            /* Half at `level`, half at its opposite. The top bit of channel_0 inverts the
             * output, so it carries which half is high — the same trick gallagher.c uses. */
            ch0 = (uint16_t)((level ? 0u : (1u << 15)) |
                             (FDXB_CARRIER_CYCLES_PER_BIT / 2));
        } else {
            /* Held for the whole bit — see the header note on `counter_top + 1`. */
            ch0 = (uint16_t)(level ? (FDXB_CARRIER_CYCLES_PER_BIT + 1u) : 0u);
        }
        m_fdxb_vals[i].channel_0 = ch0;
        m_fdxb_vals[i].channel_1 = 0;
        m_fdxb_vals[i].channel_2 = 0;
        m_fdxb_vals[i].counter_top = FDXB_CARRIER_CYCLES_PER_BIT;
        if (bit) {
            level = !level;                 /* the mid-bit transition leaves the level flipped */
        }
    }
    return &m_fdxb_seq;
}

const protocol fdxb = {
    .tag_type = TAG_TYPE_FDXB,
    .data_size = FDXB_DATA_SIZE,
    .alloc = (codec_alloc)fdxb_alloc,
    .free = (codec_free)fdxb_free,
    .get_data = (codec_get_data)fdxb_get_data,
    .modulator = (modulator)fdxb_modulator,
    .decoder =
        {
            .start = (decoder_start)fdxb_decoder_start,
            .feed = (decoder_feed)fdxb_decoder_feed,
        },
};
