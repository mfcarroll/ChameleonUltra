#include <stdlib.h>
#include <string.h>

#include "keri.h"
#include "t55xx.h"
#include "tag_base_type.h"
#include "utils/psk1.h"

_Static_assert(KERI_BIT_COUNT == LF_PSK1_RF32_FRAME_BITS,
               "Keri frame is not 64 bits — lf_psk1_rf32_modulator cannot carry it");

// Keri: 64-bit PSK1 frame at RF/32 with the subcarrier at carrier/2 — the SAME physical
// layer as Indala26 and IDTECK, which is measured rather than assumed: four captures of a
// Momentum-emulated Keri credential decode through our Indala demodulator unchanged
// (C157). So this shares utils/psk1.c's modulator and its single PWM buffer, and what
// differs is only the preamble and how the payload is read.

static keri_codec *keri_alloc(void) {
    keri_codec *d = malloc(sizeof(keri_codec));
    memset(d->data, 0, KERI_DATA_SIZE);
    return d;
}

static void keri_free(keri_codec *d) {
    free(d);
}

static uint8_t *keri_get_data(keri_codec *d) {
    return d->data;
}

// ⚠ NOT A STUB BY OVERSIGHT — same reason as Indala's. The tag-emulation ADC path is
// envelope-filtered at 125kHz, which discards subcarrier PHASE, and PSK1 carries the data
// there. `lf keri read` is the path; these keep the protocol struct complete.
static void keri_decoder_start(keri_codec *d, uint8_t format) {
    (void)d;
    (void)format;
}

static bool keri_decoder_feed(keri_codec *d, uint16_t val) {
    (void)d;
    (void)val;
    return false;
}

// buf is the 8-byte frame to transmit, MSB first on air, preamble included.
static const nrf_pwm_sequence_t *keri_modulator(keri_codec *d, uint8_t *buf) {
    (void)d;
    return lf_psk1_rf32_modulator(buf);
}

const protocol keri = {
    .tag_type = TAG_TYPE_KERI,
    .data_size = KERI_DATA_SIZE,
    .alloc = (codec_alloc)keri_alloc,
    .free = (codec_free)keri_free,
    .get_data = (codec_get_data)keri_get_data,
    .modulator = (modulator)keri_modulator,
    .decoder =
        {
            .start = (decoder_start)keri_decoder_start,
            .feed = (decoder_feed)keri_decoder_feed,
        },
};

/* ⛔⛔ THE T5577 BLOCKS ARE NOT THE AIR FRAME — THEY ARE A ROTATION OF IT, AND WRITING THE
 * FRAME STRAIGHT IN WOULD PRODUCE A TAG NOTHING READS.
 *
 * The Proxmark writes `(internal_id << 3) | 7` across blocks 1 and 2 (cmdlfkeri.c), which
 * for internal id 80003039 is 00000004 / 000181CF — nothing like the air frame
 * E0000000 80003039 our decoder recovers. They are the same 64-bit cycle read from
 * different starting points: a T5577 clocks blocks 1-2 round continuously, so
 *
 *     ... 111 [29 zeros] [id] 111 [29 zeros] [id] ...
 *
 * is what leaves the coil either way. The block form puts the three leading 1s at the END
 * (that is the `| 7`) and the preamble's 29 zeros at the start; the reader's window begins
 * three bits earlier. ⇒ Rotate, do not transcribe. Verified against a real clone's own
 * block dump rather than derived (C157).
 *
 * ⚠ The config word is `603E1040`, which is X_MODE with a dynamic bit rate — NOT the plain
 * `T5577_BITRATE_RF_32` the other PSK1 protocols here use. That is the Proxmark's choice
 * and `lf t55xx detect` reports PSK1 / RF/32 for it, so it is kept verbatim rather than
 * "simplified" to the form that looks like its neighbours. */
uint8_t keri_t55xx_writer(uint8_t *frame8, uint32_t *blks) {
    uint32_t id = ((uint32_t)frame8[4] << 24) | ((uint32_t)frame8[5] << 16) |
                  ((uint32_t)frame8[6] << 8)  |  (uint32_t)frame8[7];
    uint64_t data = ((uint64_t)id << 3) | 7u;
    blks[0] = T5577_KERI_CONFIG;
    blks[1] = (uint32_t)(data >> 32);
    blks[2] = (uint32_t)(data & 0xFFFFFFFFu);
    return 3;
}
