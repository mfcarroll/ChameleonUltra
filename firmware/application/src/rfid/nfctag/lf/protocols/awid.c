#include <stdlib.h>
#include <string.h>

#include "awid.h"
#include "fsk2a_mod.h"
#include "fsk2a_t55xx.h"
#include "t55xx.h"
#include "tag_base_type.h"

/* ⭐⭐ FSK2a EMULATION SPENDS SEVERAL PWM ENTRIES PER TONE, AT A CONSTANT `counter_top`.
 *
 * ⛔ It used to spend ONE entry per tone and vary `counter_top` between 8 and 10 at a 125kHz
 * base clock. That is the obvious encoding, it round-trips through our own decoder exactly, and
 * it does not reach the air: captured off the coil it emitted a CONSTANT TONE (C382, C383). The
 * waveform now comes from lf/utils/fsk2a_mod.c, which keeps `counter_top` constant and carries
 * the frequency in the duty pattern; that header holds the full account.
 *
 * ⭐ THE BIT-TO-TONE MAPPING BELOW IS UNCHANGED AND IS NOT A GUESS. `lf_fsk2a_format_t` in the
 * shipping DECODER carries `pulses_short = 6` and `pulses_long = 5` for all four formats in this
 * family, and it reads real AWID, Paradox, Pyramid and FDX-A tags. The decoder calls the LONG
 * period pulse 1, so a 1 bit is RF/10 — inverting that yields a frame with every bit flipped,
 * which a preamble search fails silently rather than flagging (C160's mode on Keri).
 *
 * ⛔ A 0 BIT AND A 1 BIT ARE NOT THE SAME LENGTH — 48 carrier cycles against 50 — and that is
 * the protocol, not a rounding error here. Anything assuming a constant bit period on this
 * family (a frame length in samples, a phase estimate, a terminator position) is wrong by ~4%.
 *
 * ⚠ The sequence length is therefore set PER CALL: an all-zeros frame is shorter than an
 * all-ones one. The ASK emitters can use a const sequence because theirs is one entry per bit. */

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
//
// ⭐⭐ THE WAVEFORM IS BUILT BY lf/utils/fsk2a_mod.c NOW, NOT HERE, AND THE ENCODING CHANGED.
// This used to spend one PWM entry per tone and vary `counter_top` between 8 and 10 to carry
// the frequency. Captured off the air, that emitted a CONSTANT TONE — 3620 periods in the RF/8
// band and TWO in the RF/10 band — so there was no frequency modulation for a reader to decode
// (C382), confirmed by forcing both tones equal and watching the whole emission move (C383).
// The shared builder keeps `counter_top` constant at 16 ticks and switches the DUTY instead.
// ⚠ It also needs the 1MHz base clock, which is why TAG_TYPE_AWID is in IS_FSK2A_1MHZ_TYPE.
static const nrf_pwm_sequence_t *awid_modulator(awid_codec *d, uint8_t *buf) {
    (void)d;
    /* ⭐ The pulse counts come from the shipping DECODER (`lf_fsk2a_format_t` carries
     * pulses_short 6 and pulses_long 5 for all four formats in this family), not from a
     * datasheet — emitting what our own decoder counts is the round trip this file is tested
     * by. The tone periods are 8 and 10 carrier cycles; at 1MHz that is 64 and 80 ticks, whose
     * gcd is 16. */
    static const lf_fsk2a_params_t params = {
        .counter_top  = 16,
        .short_cycles = AWID_TONE_SHORT_CYCLES,
        .long_cycles  = AWID_TONE_LONG_CYCLES,
        .short_pulses = AWID_PULSES_SHORT,
        .long_pulses  = AWID_PULSES_LONG,
        .mark_cycles  = LF_FSK2A_MARK_CYCLES,
    };
    return lf_fsk2a_build(&params, buf, AWID_BIT_COUNT);
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
