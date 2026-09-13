#include <stdlib.h>
#include <string.h>

#include "awid.h"
#include "fsk2a_t55xx.h"
#include "t55xx.h"
#include "tag_base_type.h"

/* ⭐⭐ FSK2a EMULATION IS ONE PWM ENTRY PER TONE PERIOD, NOT PER BIT — and that one sentence
 * is the whole difference from the ASK emitters next door.
 *
 * The peripheral runs at 125kHz for every non-PSK1 tag type, so one tick is one carrier
 * cycle. Gallagher spends one entry on a whole RF/32 bit; FSK2a has no single bit period to
 * spend an entry on, because the data IS the tone. So:
 *
 *   a 0 bit -> SIX entries of counter_top 8   (RF/8, 48 carrier cycles)
 *   a 1 bit -> FIVE entries of counter_top 10 (RF/10, 50 carrier cycles)
 *
 * ⛔ THOSE TWO ARE NOT THE SAME LENGTH, 48 against 50, and that is the protocol rather than a
 * rounding error here. Anything that assumes a constant bit period on this family — a frame
 * length in samples, a phase estimate, a terminator position — is wrong by up to 4%.
 *
 * ⭐ The counts come from the shipping DECODER, not from a datasheet: `lf_fsk2a_format_t`
 * carries `pulses_short = 6` and `pulses_long = 5` for all four formats in the family, and
 * that decoder reads real AWID, Paradox, Pyramid and FDX-A tags. Emitting what our own
 * decoder counts is the round trip this file is tested by (ctest/roundtrip.c).
 *
 * ⚠ WHICH TONE IS WHICH IS NOT A GUESS EITHER. The decoder calls the LONG period pulse 1 and
 * the SHORT one pulse 0, so a 1 bit is RF/10. Getting this backwards produces a frame whose
 * every bit is inverted, which a preamble search would simply fail rather than flagging —
 * the silent failure mode C160 paid for on Keri.
 *
 * ⚠ The sequence length is set PER CALL because it depends on the data: a frame of all zeros
 * is 576 entries and a frame of all ones is 480. The ASK emitters can use a const sequence
 * because theirs is always one entry per bit. */
static nrf_pwm_values_wave_form_t m_awid_vals[AWID_MAX_PWM_ENTRIES] = {};

static nrf_pwm_sequence_t m_awid_seq = {
    .values.p_wave_form = m_awid_vals,
    .length = NRF_PWM_VALUES_LENGTH(m_awid_vals),
    .repeats = 0,
    .end_delay = 0,
};

#define AWID_TONE_SHORT_CYCLES  8   /* RF/8  — a 0 bit, six of them */
#define AWID_TONE_LONG_CYCLES   10  /* RF/10 — a 1 bit, five of them */
#define AWID_PULSES_SHORT       6
#define AWID_PULSES_LONG        5

static awid_codec *awid_alloc(void) {
    awid_codec *d = malloc(sizeof(awid_codec));
    memset(d->data, 0, AWID_DATA_SIZE);
    return d;
}

static void awid_free(awid_codec *d) {
    free(d);
}

static uint8_t *awid_get_data(awid_codec *d) {
    return d->data;
}

/* ⚠ Stubs, as for every other LF protocol here: the tag-emulation ADC path is not the read
 * path. `lf awid read` demodulates a whole capture in rfid/reader/lf/lf_fsk2a.c. */
static void awid_decoder_start(awid_codec *d, uint8_t format) {
    (void)d;
    (void)format;
}

static bool awid_decoder_feed(awid_codec *d, uint16_t val) {
    (void)d;
    (void)val;
    return false;
}

// buf is the 12-byte frame, MSB first on air, preamble included.
static const nrf_pwm_sequence_t *awid_modulator(awid_codec *d, uint8_t *buf) {
    (void)d;
    uint16_t n = 0;
    for (int i = 0; i < AWID_BIT_COUNT; i++) {
        const bool bit = (buf[i / 8] >> (7 - (i % 8))) & 1u;
        const uint16_t top = bit ? AWID_TONE_LONG_CYCLES : AWID_TONE_SHORT_CYCLES;
        const uint8_t pulses = bit ? AWID_PULSES_LONG : AWID_PULSES_SHORT;
        for (uint8_t p = 0; p < pulses; p++) {
            /* ⭐⭐ A FIXED 4-CYCLE MARK, AND A GAP THAT CARRIES THE FREQUENCY — NOT 50% DUTY.
             *
             * This is copied from a REAL emission rather than assumed. A Flipper emulating
             * AWID, captured by Chameleon #1 on rig A and decoded byte-exact by our own
             * reader, puts its HIGH run at 4 samples on essentially every tone (1207 of 1667)
             * and varies only the LOW run: 4 for the RF/8 tone and 6 for the RF/10 one. The
             * period histogram is 8 and 10 as expected, but the DUTY is not half.
             *
             * ⚠ Our first version emitted 5 high / 5 low for the long tone, which sums to the
             * same period and which our own decoder reads perfectly — it only ever looks at
             * the SUM. The Flipper reads it 0 of 6. A real tag shorts its coil for a fixed
             * time and lets the gap carry the data, so a 50% duty at the longer period leaves
             * the field loaded 25% longer than any real AWID tag would.
             *
             * ⛔ Whether that is what the Flipper objects to is NOT established — its
             * demodulator sums high and low into one period and should not care. But matching
             * a measured reference costs one constant, and guessing differently from the only
             * working emission on this bench needs a reason we do not have. */
            m_awid_vals[n].channel_0 = (uint16_t)(AWID_TONE_SHORT_CYCLES / 2u);
            m_awid_vals[n].channel_1 = 0;
            m_awid_vals[n].channel_2 = 0;
            m_awid_vals[n].counter_top = top;
            n++;
        }
    }
    m_awid_seq.length = (uint16_t)(n * 4u);   /* NRF_PWM_VALUES_LENGTH counts uint16s */
    return &m_awid_seq;
}

const protocol awid = {
    .tag_type = TAG_TYPE_AWID,
    .data_size = AWID_DATA_SIZE,
    .alloc = (codec_alloc)awid_alloc,
    .free = (codec_free)awid_free,
    .get_data = (codec_get_data)awid_get_data,
    .modulator = (modulator)awid_modulator,
    .decoder =
        {
            .start = (decoder_start)awid_decoder_start,
            .feed = (decoder_feed)awid_decoder_feed,
        },
};

uint8_t awid_t55xx_writer(uint8_t *frame12, uint32_t *blks) {
    return fsk2a_t55xx_blocks(frame12, 3, T5577_AWID_CONFIG, blks);
}
