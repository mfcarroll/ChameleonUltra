#include <stdlib.h>
#include <string.h>

#include "nexwatch.h"
#include "t55xx.h"
#include "tag_base_type.h"
#include "utils/psk1.h"

_Static_assert(NEXWATCH_BIT_COUNT <= LF_PSK1_EMU_MAX_FRAME_BITS,
               "NexWatch frame does not fit the shared PSK1 PWM buffer");

static nexwatch_codec *nexwatch_alloc(void) {
    nexwatch_codec *d = malloc(sizeof(nexwatch_codec));
    memset(d->data, 0, NEXWATCH_DATA_SIZE);
    return d;
}

static void nexwatch_free(nexwatch_codec *d) {
    free(d);
}

static uint8_t *nexwatch_get_data(nexwatch_codec *d) {
    return d->data;
}

// ⚠ Stubs for the same reason Indala's and Keri's are: the tag-emulation ADC path is
// envelope-filtered at 125kHz, which discards the subcarrier PHASE that PSK1 carries the
// data in. `lf nexwatch read` is the read path — it demodulates a whole capture in
// rfid/reader/lf/lf_indala_psk.c rather than being fed sample by sample through here.
static void nexwatch_decoder_start(nexwatch_codec *d, uint8_t format) {
    (void)d;
    (void)format;
}

static bool nexwatch_decoder_feed(nexwatch_codec *d, uint16_t val) {
    (void)d;
    (void)val;
    return false;
}

// buf is the 12-byte frame to transmit, MSB first on air, preamble included.
//
// ⭐ PSK1 (DIRECT), not PSK2 — the config word the Proxmark's own clone writes is
// T5577_MODULATION_PSK1 and `lf t55xx detect` agrees (C164). ⚠ Unlike Indala224, this
// protocol therefore needs no doubled buffer: the DIRECT mode's telescoping form is
// periodic at the frame period whatever the frame's parity. See the note in psk1.h.
static const nrf_pwm_sequence_t *nexwatch_modulator(nexwatch_codec *d, uint8_t *buf) {
    (void)d;
    return lf_psk1_modulator(buf, NEXWATCH_BIT_COUNT, LF_PSK1_PHASE_DIRECT);
}

const protocol nexwatch = {
    .tag_type = TAG_TYPE_NEXWATCH,
    .data_size = NEXWATCH_DATA_SIZE,
    .alloc = (codec_alloc)nexwatch_alloc,
    .free = (codec_free)nexwatch_free,
    .get_data = (codec_get_data)nexwatch_get_data,
    .modulator = (modulator)nexwatch_modulator,
    .decoder =
        {
            .start = (decoder_start)nexwatch_decoder_start,
            .feed = (decoder_feed)nexwatch_decoder_feed,
        },
};

// T5577 writer: block 0 the PSK1 RF/32 configuration, blocks 1-3 the 96 bits big-endian.
//
// ⭐ A STRAIGHT TRANSCRIPTION, AND THAT IS THE MEASURED ANSWER RATHER THAN THE OBVIOUS ONE.
// Keri's writer has to ROTATE, because its T5577 holds `(id << 3) | 7` and its air frame
// starts three bits before a block boundary (C158); emitting the un-rotated form there gave
// a confident wrong credential 6 of 6 (C160). NexWatch is the other case: the Proxmark's own
// clone of card 12345678 holds `56000000 / 00436455 / 121E6000`, whose first block IS the
// frame's `0x56` preamble. ⇒ Verified against a real clone's block dump before it was
// written, exactly as C160 says to.
uint8_t nexwatch_t55xx_writer(uint8_t *frame12, uint32_t *blks) {
    blks[0] = T5577_NEXWATCH_CONFIG;
    for (int w = 0; w < 3; w++) {
        uint32_t v = 0;
        for (int i = 0; i < 4; i++) v = (v << 8) | frame12[w * 4 + i];
        blks[1 + w] = v;
    }
    return 4;
}
