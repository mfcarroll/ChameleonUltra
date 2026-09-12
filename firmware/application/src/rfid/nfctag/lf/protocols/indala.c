#include <stdlib.h>
#include <string.h>

#include "indala.h"
#include "t55xx.h"
#include "tag_base_type.h"
#include "utils/psk1.h"

// The shared modulator is fixed at a 64-bit frame. If this protocol's frame
// length ever changes, it needs its own path rather than silently transmitting
// the wrong number of bits.
_Static_assert(INDALA_BIT_COUNT == LF_PSK1_RF32_FRAME_BITS,
               "Indala frame is not 64 bits — lf_psk1_rf32_modulator cannot carry it");

// Indala: 64-bit PSK1 frame at RF/32 with the subcarrier at carrier/2. Identical to
// IDTECK at the physical layer — same T5577 config word, same bit rate, same frame
// length — so both share utils/psk1.c's modulator and its single 8KB PWM buffer.
// What differs is the frame: Indala's 33-bit preamble (1010, 28 zeros, 1) against
// IDTECK's 32-bit "IDTK", and the payload interpretation above it.

static indala_codec *indala_alloc(void) {
    indala_codec *d = malloc(sizeof(indala_codec));
    memset(d->data, 0, INDALA_DATA_SIZE);
    return d;
}

static void indala_free(indala_codec *d) {
    free(d);
}

static uint8_t *indala_get_data(indala_codec *d) {
    return d->data;
}

// ⚠ NOT A STUB BY OVERSIGHT. The tag-emulation ADC path is envelope-filtered at 125kHz,
// which discards exactly the thing PSK1 carries the data in — subcarrier PHASE. Reading
// Indala needs the whole-capture demodulator in rfid/reader/lf/lf_indala_psk.c, which
// works on raw conversions rather than being fed sample by sample through this
// interface. `lf indala read` is that path; these keep the protocol struct complete.
static void indala_decoder_start(indala_codec *d, uint8_t format) {
    (void)d;
    (void)format;
}

static bool indala_decoder_feed(indala_codec *d, uint16_t val) {
    (void)d;
    (void)val;
    return false;
}

// buf is the 8-byte frame to transmit, MSB first on air, preamble included. The CLI
// layer composes it, exactly as it does for IDTECK.
static const nrf_pwm_sequence_t *indala_modulator(indala_codec *d, uint8_t *buf) {
    (void)d;
    return lf_psk1_rf32_modulator(buf);
}

// ── Indala 224-bit ─────────────────────────────────────────────────────────────
// The same air layer, a longer frame, and PSK2 rather than PSK1 — see the note on
// LF_PSK1_PHASE_DIFFERENTIAL in utils/psk1.h for why the modulation is named rather than
// discovered. Everything else is shared: one PWM buffer, one sequence builder.
_Static_assert(INDALA224_BIT_COUNT <= LF_PSK1_EMU_MAX_FRAME_BITS,
               "Indala224 frame does not fit the shared PSK1 PWM buffer");

static indala224_codec *indala224_alloc(void) {
    indala224_codec *d = malloc(sizeof(indala224_codec));
    memset(d->data, 0, INDALA224_DATA_SIZE);
    return d;
}

static void indala224_free(indala224_codec *d) {
    free(d);
}

static uint8_t *indala224_get_data(indala224_codec *d) {
    return d->data;
}

// ⚠ Stubs for the same reason Indala26's are — see the note there. `lf indala read --224`
// is the read path; it demodulates a whole 14336-sample capture rather than being fed
// through this interface.
static void indala224_decoder_start(indala224_codec *d, uint8_t format) {
    (void)d;
    (void)format;
}

static bool indala224_decoder_feed(indala224_codec *d, uint16_t val) {
    (void)d;
    (void)val;
    return false;
}

// buf is the 28-byte frame to transmit, MSB first on air, preamble included.
static const nrf_pwm_sequence_t *indala224_modulator(indala224_codec *d, uint8_t *buf) {
    (void)d;
    return lf_psk1_modulator(buf, INDALA224_BIT_COUNT, LF_PSK1_PHASE_DIFFERENTIAL);
}

const protocol indala224 = {
    .tag_type = TAG_TYPE_INDALA224,
    .data_size = INDALA224_DATA_SIZE,
    .alloc = (codec_alloc)indala224_alloc,
    .free = (codec_free)indala224_free,
    .get_data = (codec_get_data)indala224_get_data,
    .modulator = (modulator)indala224_modulator,
    .decoder =
        {
            .start = (decoder_start)indala224_decoder_start,
            .feed = (decoder_feed)indala224_decoder_feed,
        },
};

const protocol indala = {
    .tag_type = TAG_TYPE_INDALA,
    .data_size = INDALA_DATA_SIZE,
    .alloc = (codec_alloc)indala_alloc,
    .free = (codec_free)indala_free,
    .get_data = (codec_get_data)indala_get_data,
    .modulator = (modulator)indala_modulator,
    .decoder =
        {
            .start = (decoder_start)indala_decoder_start,
            .feed = (decoder_feed)indala_decoder_feed,
        },
};

// T5577 writer: block 0 holds the PSK1 RF/32 configuration, blocks 1-2 the 64-bit frame
// big-endian. ⭐ Confirmed against two independent sources: it is what Proxmark's
// `lf indala clone` writes (cmdlfindala.c) and what block 0 of a working bench tag reads
// back as. Verified end to end — `lf indala write` reads the credential back off the tag.
uint8_t indala_t55xx_writer(uint8_t *uid, uint32_t *blks) {
    uint32_t hi = 0, lo = 0;
    for (int i = 0; i < 4; i++) hi = (hi << 8) | uid[i];
    for (int i = 4; i < 8; i++) lo = (lo << 8) | uid[i];
    blks[0] = T5577_INDALA_CONFIG;
    blks[1] = hi;
    blks[2] = lo;
    return 3;
}

// T5577 writer for the 224-bit frame: block 0 the PSK2 RF/32 configuration, blocks 1-7 the
// 224 bits big-endian. ⚠ That is every block page 0 has, so this protocol cannot carry a
// password — see T5577_INDALA224_CONFIG for why the PWD bit is absent.
//
// ⛔ THE CONFIG WORD IS PSK2 WHILE THE FRAME IS BUILT AS PSK1 BITS, and that is correct
// rather than a mismatch: the 224 bits handed in are the DATA, and PSK2 is how the T5577 is
// told to put them on the air — phase change per 1 bit. The reader undoes it by XOR-ing
// consecutive recovered phases, which is what `LF_PSK1_FORMAT_INDALA224.differential_only`
// does (C107). Writing this config with PSK1 instead would put the running XOR of the data
// on the air and no reader here would recover the frame.
uint8_t indala224_t55xx_writer(uint8_t *raw28, uint32_t *blks) {
    blks[0] = T5577_INDALA224_CONFIG;
    for (int w = 0; w < 7; w++) {
        uint32_t v = 0;
        for (int i = 0; i < 4; i++) v = (v << 8) | raw28[w * 4 + i];
        blks[1 + w] = v;
    }
    return 8;
}
