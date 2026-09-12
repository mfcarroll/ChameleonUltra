#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Indala PSK1 demodulator — pure integer, no floating point, no FFT, no nRF dependency.
 *
 * ⭐ PSK1: THE SUBCARRIER PHASE *IS* THE DATA. It is NOT a differential encoding.
 * "A '1' flips the phase" is PSK2, and decoding this tag that way is what convinced this
 * project for weeks that the Chameleon was 31 dB short of reading Indala. The authority is
 * the Proxmark, which reads the same tag: PSKDemod() emits the phase per bit,
 * cmdlfindala.c:1259 matches preamble64 against that stream DIRECTLY, and only if that
 * fails does it call psk1TOpsk2() (cmdlfindala.c:1293) and try again.
 *
 * The chain, in full:
 *
 *   sample at 125kHz, carrier-locked, at a sample phase inside the working window
 *     -> mix by (-1)^n           fc/2 is EXACTLY fs/2, so this is the entire mixer: no
 *                                oscillator, no phase estimate, no clock recovery. The
 *                                original DC and any slow envelope drift move UP to fs/2.
 *     -> [1,2,1] notch at fs/2   removes what the mix just put there. NOT OPTIONAL.
 *     -> 32-sample boxcar/bit    the matched filter for a rectangular bit at RF/32
 *     -> bit = (integrator > 0)  PSK1
 *     -> exact search for the 33-bit preamble, normal and inverted
 *     -> read 64 bits from the preamble position
 *
 * ⚠ NOTHING HERE MAY DISCARD A SETTLE WINDOW. Dropping the first 400 samples as "turn-on
 * transient" takes the decode rate from 51/160 to 0/160: it removes the first frame's
 * preamble and leaves too few bits after the second. The mix moves the transient to fs/2
 * and the notch removes it, which is the whole reason no discard is needed.
 */

/** Samples per bit. Indala is RF/32 and the SAADC takes one sample per carrier cycle. */
#define INDALA_PSK_BIT_SAMPLES  32

/** Bits in an Indala frame. */
#define INDALA_PSK_FRAME_BITS   64

/** Bits in the fixed preamble (cmdlfindala.c:50) — and the first 33 bits of every ID. */
#define INDALA_PSK_PREAMBLE_BITS 33

/** Samples in one capture. 4096 = two whole 64-bit frames at RF/32.
 *
 * ⭐ ONE FRAME IS NOT ENOUGH, ever, at any SNR: the preamble can start anywhere in the
 * capture, so a 2048-sample buffer only contains a complete frame for one starting phase
 * in 2048. Two frames guarantees one whole frame lands inside. */
#define INDALA_PSK_CAPTURE_SAMPLES 4096

/* ⛔ THE STRADDLE GATE. A frame is rejected when it is BOTH loud and ragged — see the long
 * note at the gate itself in lf_indala_psk.c. Both conditions are required: shape alone
 * costs 61% of back-side reads, because a genuine frame 26 dB down is ragged too.
 *
 *   reject when   mean|integ| >= INDALA_PSK_STRADDLE_AMP
 *           and   min|integ| * INDALA_PSK_STRADDLE_DIV < mean|integ|
 *
 * Measured over 320 captures on both placements: rejects 21 of 21 straddles, keeps 110 of
 * 114 front-side true frames (and 40 of 40 at the phases PHASE_ROTATION actually uses),
 * and touches nothing on the back — 51 of 51 kept. */
#define INDALA_PSK_STRADDLE_AMP  2048   /* geometric mean of the two populations: 2.2x the
                                           loudest back-side frame, 2.5x below the quietest
                                           straddle. Coupling-dependent — see the gate. */
#define INDALA_PSK_STRADDLE_DIV  8      /* min/mean < 1/8. Straddles measured 0.001-0.092,
                                           front-side true frames 0.36-0.62. */

/** Upper bound on bits recoverable from one capture, for the stack-allocated workspace. */
#define INDALA_PSK_MAX_BITS (INDALA_PSK_CAPTURE_SAMPLES / INDALA_PSK_BIT_SAMPLES)

/** Shortest capture that can hold a preamble plus a whole word plus slack. */
#define INDALA_PSK_MIN_SAMPLES (INDALA_PSK_BIT_SAMPLES * (INDALA_PSK_FRAME_BITS + 4))

typedef struct {
    uint8_t  id[8];        /**< the 64-bit frame, big-endian: id[0] is the first bit. */
    uint8_t  fc;           /**< format-26 facility code, de-scrambled. */
    uint16_t csn;          /**< format-26 card number, de-scrambled. */
    uint8_t  parity;       /**< the two format-26 parity bits, b2 b1. */
    bool     wiegand26_ok; /**< both parity bits agree with the de-scrambled fc/csn. */
    uint8_t  offset;       /**< winning sample offset within the bit period, 0..31. */
    uint8_t  bit_pos;      /**< bit index of the preamble in that offset's stream. */
    bool     inverted;     /**< the frame was found as the inverted preamble. */
    int32_t  amp;          /**< mean |bit integrator| over the 64 word bits. */
    int32_t  min_amp;      /**< SMALLEST |bit integrator| in the frame. With `amp` this is
                                the straddle test: a frame whose weakest bit has collapsed
                                relative to its average is an integrator sitting across bit
                                boundaries, which decodes to a repeatable WRONG word. See
                                the gate in lf_indala_psk.c. */
    int32_t  energy;       /**< ⭐ SET EVEN WHEN NO FRAME DECODES — this is the one field
                                that distinguishes "nothing is there" from "something is
                                there that I cannot read". The largest mean |bit
                                integrator| over the WHOLE capture, across all 32 sample
                                offsets: same units as `amp`, and computed from integrators
                                the offset loop already builds, so it costs one abs and one
                                add per bit. See INDALA_PSK_ENERGY_PRESENT. */
} indala_psk_result_t;

/* ⭐ "A SUBCARRIER IS THERE BUT I COULD NOT READ IT" — the level above which `energy`
 * means a real source rather than an empty antenna.
 *
 * ⛔ This is a MEASURED LEVEL, not a guess, and like INDALA_PSK_STRADDLE_AMP it is
 * coupling-dependent (C43). It exists to drive a STATUS MESSAGE, never a credential, so its
 * failure mode is a misleading hint rather than a wrong card number — which is why it can be
 * a single absolute number where the straddle gate needed two conditions.
 *
 * ⚠ Name what is MEASURED, not what is inferred. Energy present with no frame is the
 * observation; "it is an emulator" is the likely cause and belongs in the host's message,
 * not in this constant or in the status code.
 *
 * MEASURED, 336 captures, `research/indala-psk-read/ctest` over the committed sets plus a
 * paired pair taken on one device minutes apart:
 *
 *                                   n     min     p50     max
 *     empty, front                160     190     282    1312
 *     empty, back                 160     237     298    1330
 *     empty, rig A same session     4     294     633    1772
 *     real tag, front             160    5075    8767   11720
 *     Flipper emulation, rig A      4    8451    8523    8685
 *     real tag, back              160     286     740    1793   <- ⛔ overlaps empty
 *
 * ⛔ THIS CANNOT DETECT A WEAK UNDECODABLE SOURCE AND MUST NOT CLAIM TO. A back-side tag is
 * quieter than a front-side empty antenna, so no absolute level separates them. The bar is
 * set by the EMPTY distribution — 1.7x above the loudest empty ever seen and 1.7x below the
 * quietest real tag — which makes a false "signal present" on a bare antenna the thing it is
 * engineered against. A weak source that fails to decode still reports plain "not found",
 * exactly as before, and that is the correct conservative failure.
 */
#define INDALA_PSK_ENERGY_PRESENT  3000

/**
 * Demodulate one Indala PSK1 frame from a carrier-locked capture.
 *
 * ⚠ `samples` IS MODIFIED IN PLACE — it is converted to baseband so the decode needs no
 * second 8KB buffer. Pass a copy if the caller still needs the raw capture.
 *
 * @param samples  raw 14-bit SAADC conversions, one per carrier cycle, 0..16383.
 * @param n        sample count; must be >= INDALA_PSK_MIN_SAMPLES.
 * @param out      filled in on success. ⚠ `out->energy` is filled in EITHER WAY, and is
 *                 the only field that may be read after a false return.
 * @return         true if a frame was recovered.
 */
bool indala_psk1_decode(int16_t *samples, size_t n, indala_psk_result_t *out);
