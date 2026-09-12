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

/** Bits in a 64-bit-class frame — Indala26 and IDTECK both. */
#define INDALA_PSK_FRAME_BITS   64

/** Bits in an Indala224 frame. */
#define INDALA224_PSK_FRAME_BITS 224

/** The longest frame any format here uses, for fixed-size storage. */
#define LF_PSK1_MAX_FRAME_BITS  INDALA224_PSK_FRAME_BITS
#define LF_PSK1_MAX_FRAME_BYTES (LF_PSK1_MAX_FRAME_BITS / 8)

/** Bits in the fixed preamble (cmdlfindala.c:50) — and the first 33 bits of every ID. */
#define INDALA_PSK_PREAMBLE_BITS 33

/** IDTECK's preamble is 0x4944544B, "IDTK", the first 32 bits of every IDTECK frame. */
#define IDTECK_PSK_PREAMBLE_BITS 32

/* ⭐ KERI IS THE SAME AIR LAYER AS INDALA26 AND IDTECK — PSK1, RF/32, fc/2 subcarrier,
 * 64-bit frame — and this is MEASURED, not assumed from a datasheet: four captures of a
 * Momentum-emulated Keri credential decode to the exact predicted frame through this
 * demodulator unchanged, at sample offset 0 (C157). Its T5577 config word says the same
 * thing from the other side: `603E1040` is PSK1 / PSKCF_RF_2, and `lf t55xx detect` on a
 * Proxmark-cloned Keri reports PSK1, RF/32.
 *
 * Its preamble is 33 bits — `111`, 29 zeros, then a 1 — where Indala's is `1010`, 28
 * zeros, then a 1. They disagree at bit 1, so neither can match the other's window. */
#define KERI_PSK_PREAMBLE_BITS 33

/* ⛔⛔ INDALA224'S PREAMBLE IS A 1 FOLLOWED BY 29 ZEROS, AND THAT IS NOT ENOUGH ON ITS OWN.
 * 29 of its 30 bits are a constant run — weaker than Indala26's 33-bit preamble, which a
 * loud IDTECK tag already forged at sample phase 28 to produce a confident wrong credential
 * (C90). ⇒ A format this weak must carry `require_repeat`, which tests all 224 bits against
 * the frame's own neighbouring copies instead of trusting 30. */
#define INDALA224_PSK_PREAMBLE_BITS 30

/** Longest preamble any format here uses, for fixed-size storage. */
#define LF_PSK1_MAX_PREAMBLE_BITS 33

/* ⭐ THE PREAMBLES ARE THE ONLY THING THAT DIFFERS BETWEEN THESE TWO PROTOCOLS at this
 * layer. Indala and IDTECK are both 64-bit PSK1 at RF/32 on an fc/2 subcarrier, written by
 * the same T5577 config word but for `T5577_PWD` — so the mixer, the notch, the bit
 * integrator, the offset ranking and the straddle gate are shared verbatim, and only the
 * bit pattern being searched for changes. Duplicating the decoder to add IDTECK would have
 * duplicated the 320-capture validation with it. */
extern const uint8_t LF_PSK1_PREAMBLE_INDALA[INDALA_PSK_PREAMBLE_BITS];
extern const uint8_t LF_PSK1_PREAMBLE_IDTECK[IDTECK_PSK_PREAMBLE_BITS];
extern const uint8_t LF_PSK1_PREAMBLE_INDALA224[INDALA224_PSK_PREAMBLE_BITS];
extern const uint8_t LF_PSK1_PREAMBLE_KERI[KERI_PSK_PREAMBLE_BITS];

/** Samples in one capture. 4096 = two whole 64-bit frames at RF/32.
 *
 * ⭐ ONE FRAME IS NOT ENOUGH, ever, at any SNR: the preamble can start anywhere in the
 * capture, so a 2048-sample buffer only contains a complete frame for one starting phase
 * in 2048. Two frames guarantees one whole frame lands inside. */
#define INDALA_PSK_CAPTURE_SAMPLES 4096

/* ⭐ ONE BUFFER, SIZED FOR THE LONGEST FRAME, FILLED TO WHATEVER THE FORMAT NEEDS. Two whole
 * 224-bit frames is 14336 samples = 28KB. The 64-bit formats still capture only 4096 — a
 * capture is real time on the wire (33ms against 114ms), so making them read a buffer they do
 * not use would slow every Indala and IDTECK read by 3.5x for nothing. */
#define INDALA224_PSK_CAPTURE_SAMPLES 14336
#define LF_PSK1_MAX_CAPTURE_SAMPLES   INDALA224_PSK_CAPTURE_SAMPLES

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
#define INDALA_PSK_MAX_BITS (LF_PSK1_MAX_CAPTURE_SAMPLES / INDALA_PSK_BIT_SAMPLES)

/** Shortest capture that can hold a preamble plus a whole word plus slack. */
#define INDALA_PSK_MIN_SAMPLES(frame_bits) (INDALA_PSK_BIT_SAMPLES * ((frame_bits) + 4))

/* ⭐ A FORMAT IS THE ONLY THING THAT DIFFERS BETWEEN THESE PROTOCOLS at this layer. Indala26,
 * IDTECK and Indala224 are all PSK1 at RF/32 on an fc/2 subcarrier; the mixer, the notch, the
 * bit integrator, the offset ranking and the straddle gate are shared verbatim. Passing a
 * descriptor rather than six loose parameters keeps it that way as formats are added. */
typedef struct {
    const uint8_t *preamble;       /**< one byte per bit, MSB of the frame first. */
    uint8_t  preamble_bits;
    uint16_t frame_bits;
    /** ⛔ A format whose presence VETOES this one — see the note on lf_psk1_decode_ex.
     *  NULL for none, and it must stay NULL wherever the confusion is one-directional. */
    const uint8_t *reject_preamble;
    uint8_t  reject_preamble_bits;
    /** ⭐⭐ DECODE THE DIFFERENTIAL STREAM *INSTEAD OF* THE DIRECT ONE — PSK2, not a fallback.
     *
     * A T5577 written as PSK2 encodes the data in phase CHANGES, so the absolute phase this
     * demodulator recovers is the differential of what the reader wants. XOR-ing consecutive
     * bits inverts that. ⛔ Indala224 tags are written PSK2 by the Proxmark's own clone
     * command — config `000820E0`, not `00081040` (C99) — so without this the format cannot
     * be read at all.
     *
     * ⛔⛔ SEARCHING BOTH VIEWS CANNOT WORK, AND THAT IS A PROPERTY OF THE PROBLEM, NOT A BUG
     * TO TUNE AROUND.
     *
     * The direct view of a PSK2 tag is the RUNNING XOR of its data — a deterministic
     * transform, not noise. So it repeats at the frame period exactly as well as the data
     * does (~98% either way, measured), it is identical across captures, and when the data
     * begins with a run of zeros its integral begins with a run of ones, which matches the
     * inverted preamble. Every statistical test inside one capture sees two equally
     * self-consistent frames. Three rules were tried — amplitude, repeat score, and the
     * Proxmark's direct-first ordering — and returned wrong credentials on 1, 2 and 3 of 4
     * captures respectively (C104).
     *
     * ⚠ Momentum's own structural test does not save it either: it demands an exact 30-bit
     * preamble at BOTH frame positions, and at our ~2% bit error rate an exact match over 30
     * specific bits fails about 45% of the time. It rejected the TRUE frame in all four
     * captures (C106).
     *
     * ⇒ A reader cannot discover the modulation from the signal, so it must be told. Every
     * Indala224 specimen available — the Proxmark's own clone command, and Momentum's decoder,
     * which treats these as phase-alternating — is PSK2. This format therefore decodes the
     * DIFFERENTIAL view and only that one.
     *
     * ⚠ OFF FOR THE 64-BIT FORMATS, which are PSK1 and read 110/160 on the direct view.
     *
     * ⭐ The differential needs no polarity search: XOR of consecutive bits is invariant under
     * global inversion, which is the whole point of differential encoding. */
    bool     differential_only;
    /** ⭐ Require the frame to REPEAT at its own period before accepting it. For a format
     *  whose preamble is mostly a constant run this is the real acceptance test: 224 bits of
     *  self-agreement instead of 30 bits of pattern. */
    bool     require_repeat;
} lf_psk1_format_t;

extern const lf_psk1_format_t LF_PSK1_FORMAT_INDALA64;
extern const lf_psk1_format_t LF_PSK1_FORMAT_IDTECK;
extern const lf_psk1_format_t LF_PSK1_FORMAT_INDALA224;
extern const lf_psk1_format_t LF_PSK1_FORMAT_KERI;

typedef struct {
    uint8_t  id[LF_PSK1_MAX_FRAME_BYTES]; /**< the frame, big-endian: id[0] is the first bit.
                                  Only the first frame_bits/8 bytes are meaningful. */
    uint16_t frame_bits;   /**< bits actually recovered, so a caller knows how much of id. */
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
    uint8_t  word_bits[LF_PSK1_MAX_FRAME_BITS]; /**< the frame as one byte per bit, which is
                                what a format de-scramble wants. `id` is the same 64 bits
                                packed. */
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
 * @param n        sample count; must be >= INDALA_PSK_MIN_SAMPLES(frame_bits).
 * @param out      filled in on success. ⚠ `out->energy` is filled in EITHER WAY, and is
 *                 the only field that may be read after a false return.
 * @return         true if a frame was recovered.
 */
bool indala_psk1_decode(int16_t *samples, size_t n, indala_psk_result_t *out);

/**
 * The same demodulation, searching for an arbitrary preamble.
 *
 * `indala_psk1_decode` is this plus the format-26 de-scramble; IDTECK calls it directly and
 * reads its checksum and card number out of `out->id`. The format-26 fields (`fc`, `csn`,
 * `parity`, `wiegand26_ok`) are ZEROED here and are meaningless for any other protocol.
 *
 * @param preamble       one byte per bit, 0 or 1, MSB of the frame first.
 * @param preamble_bits  length, at most LF_PSK1_MAX_PREAMBLE_BITS.
 */
bool lf_psk1_decode_fmt(int16_t *samples, size_t n,
                        const lf_psk1_format_t *fmt, indala_psk_result_t *out);

/** IDTECK's decode: the same demodulation against the "IDTK" preamble, and no veto. */
bool idteck_psk1_decode(int16_t *samples, size_t n, indala_psk_result_t *out);

/** Indala224: the same demodulation against a 30-bit preamble, gated on the repeat. */
bool indala224_psk1_decode(int16_t *samples, size_t n, indala_psk_result_t *out);

/** Keri: the same demodulation against Keri's 33-bit preamble. The credential is the
 *  32-bit internal id, `out->id[4..7]`; its top bit is the preamble's last bit and is
 *  therefore always set. */
bool keri_psk1_decode(int16_t *samples, size_t n, indala_psk_result_t *out);

/** What a reader hands the capture engine: one protocol's whole decode, preamble and any
 *  veto included, so the engine stays protocol-agnostic. */
typedef bool (*lf_psk1_decode_fn)(int16_t *samples, size_t n, indala_psk_result_t *out);
