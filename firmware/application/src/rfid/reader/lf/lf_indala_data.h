#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lf_indala_psk.h"

/** Total budget for one read, in ms. Generous on purpose: a tag that reads easily still
 *  returns in ~100ms, and this only bounds how long a marginal one is given. */
#define INDALA_READ_TIMEOUT_MS 3000

/** Bytes written by indala_read(), matching the 16-byte convention of the other LF scans. */
#define INDALA_READ_DATA_SIZE 16

/**
 * Read an Indala credential (PSK1, RF/32, fc/2 subcarrier).
 *
 * Captures 4096 samples at a rotating sample phase and demodulates each capture with
 * lf_indala_psk.c, returning only once TWO captures produce the same 64-bit frame. See
 * the notes in lf_indala_data.c for why both the rotation and the agreement are required
 * and what each is worth.
 *
 * @param data        INDALA_READ_DATA_SIZE bytes:
 *                      [0..7]   the 64-bit frame, big-endian
 *                      [8]      format-26 facility code
 *                      [9..10]  format-26 card number, big-endian
 *                      [11]     bit2 = Wiegand-26 parity checks out, bits1..0 = parity b2 b1
 *                      [12]     sample phase that read it, in 62.5ns ticks
 *                      [13]     sample offset within the bit period, 0..31
 *                      [14..15] reserved, zero
 * @param timeout_ms  TOTAL budget across every capture attempt, not per capture.
 *                    A capture costs ~35ms, and the measured median is 2.
 * @return            true if two captures agreed.
 */
/**
 * @param data        INDALA_READ_DATA_SIZE bytes, written only on success.
 * @param timeout_ms  total budget across all sample phases.
 * @param energy_out  ⭐ optional; the LOUDEST whole-capture fc/2 energy seen across every
 *                    capture this call took, set whether or not a frame decoded. This is
 *                    what lets a failed read say WHICH failure it was — see
 *                    INDALA_PSK_ENERGY_PRESENT. Pass NULL if the caller does not care.
 * @return            true if two independent captures agreed on a frame.
 */
bool indala_read(uint8_t *data, uint32_t timeout_ms, int32_t *energy_out);

/** Bytes written by idteck_read(): the 8-byte frame, then checksum, then the 24-bit card
 *  number most significant first, then phase/offset/tries — the same shape as Indala's. */
#define IDTECK_READ_DATA_SIZE 16

/** What one successful PSK1 read produced, before any protocol puts it in a payload. */
typedef struct {
    lf_decode_result_t res;
    uint8_t phase;    /**< the sample phase that won, in 62.5ns ticks */
    uint8_t tries;    /**< captures taken at the winning phase to reach it */
} lf_sampled_read_t;

/**
 * ⭐ The shared PSK1 capture engine: sample-phase rotation, two independent accumulators and
 * the two-capture agreement rule, with the protocol supplied as a decode function.
 *
 * ⛔ `decode` must carry its own veto if it needs one. Indala does (C90/C91); IDTECK must
 * not.
 */
/* ⭐⭐ THE NAMING RULE, AND IT IS THE ONE §9 NAMED AS BLOCKER #1 FOR UPSTREAMING.
 *
 * Anything SHARED by the PSK and ASK families carries a modulation-neutral name:
 * `lf_sampled_read`, `lf_sampled_read_t`, `lf_sampled_decode_fn`, `lf_decode_result_t`,
 * `LF_SAMPLED_MAX_CAPTURE_SAMPLES`, `LF_DECODE_MAX_FRAME_BITS`. Anything genuinely specific
 * to phase-shift keying keeps `psk1`: `lf_psk1_format_t`, `lf_psk1_decode_fmt`,
 * `LF_PSK1_FORMAT_*`, `lf_psk1_modulator`.
 *
 * ⛔ This engine was called `lf_psk1_read` while `lf_ask_manchester.c` called it, which reads
 * as a bug to anyone who has not been told otherwise. It is NOT PSK-specific: it rotates the
 * sample phase, suspends BLE advertising (C47) and requires two independent captures to
 * agree, none of which depends on how the bits are carried. Only the decoder handed to it
 * does. */
bool lf_sampled_read(lf_sampled_decode_fn decode, size_t capture_samples,
                  lf_sampled_read_t *out, uint32_t timeout_ms, int32_t *energy_out);

/** IDTECK, same engine, same timeout, same energy reporting. */
bool idteck_read(uint8_t *data, uint32_t timeout_ms, int32_t *energy_out);

/** Bytes written by keri_read(): the 8-byte frame, then the 32-bit internal id, then the
 *  de-scrambled facility code and card number, then phase / offset / tries. */
#define KERI_READ_DATA_SIZE 16
bool keri_read(uint8_t *data, uint32_t timeout_ms, int32_t *energy_out);

/** Bytes written by nexwatch_read(): the 12-byte frame, then the descrambled 32-bit card
 *  number, then the magic byte, the mode, phase / offset / tries.
 *
 * ⚠ THE FRAME IS RETURNED IN FULL and the host prints it. 96 bits is more than a card
 * number, and the checksum's magic byte is an INFERENCE — see `nexwatch_psk1_decode` — so
 * anything downstream that wants to re-derive the fingerprint can. */
#define NEXWATCH_READ_DATA_SIZE 20
/** ⭐ NOT IN THE FRAME. The checksum is computed over the card number, the parity and one of
 *  these; which one is discovered by trying all three, exactly as both references do. A
 *  fourth vendor's tag reads fine and reports magic 0x00 / "unknown". */
#define NEXWATCH_MAGIC_QUADRAKEY 0xBE
#define NEXWATCH_MAGIC_NEXKEY    0x88
#define NEXWATCH_MAGIC_HONEYWELL 0x86
bool nexwatch_read(uint8_t *data, uint32_t timeout_ms, int32_t *energy_out);

/** Bytes written by gallagher_read(): the 12-byte frame, then phase, offset, and a pad.
 *
 * ⭐ THE FRAME AND NOTHING ELSE, deliberately. Gallagher's region / facility / card / issue
 * come out of a 256-byte descramble LUT plus four bit-field extractions, and that
 * interpretation belongs on the host for the same reason Indala224's does: it is pure data
 * manipulation the CLI can do, it is testable there against a known credential, and putting
 * it in firmware would spend flash to produce numbers the host must re-derive anyway. */
#define GALLAGHER_READ_DATA_SIZE 16
bool gallagher_read(uint8_t *data, uint32_t timeout_ms, int32_t *energy_out);

/** Bytes written by securakey_read(): the 12-byte frame, then phase, offset, tries, pad.
 *  ⚠ The frame and nothing else — same reasoning as Gallagher's, and more so here, since
 *  Securakey's field layout has three variants and no known checksum (C175). */
#define SECURAKEY_READ_DATA_SIZE 16
bool securakey_read(uint8_t *data, uint32_t timeout_ms, int32_t *energy_out);

/** Bytes written by noralsy_read(): the 12-byte frame, then phase, offset, tries, pad. */
#define NORALSY_READ_DATA_SIZE 16
bool noralsy_read(uint8_t *data, uint32_t timeout_ms, int32_t *energy_out);

/** Bytes of frame in an InstaFob read: 225 bits rounds to 29 bytes. */
#define INSTAFOB_READ_FRAME_BYTES 29
/** Bytes written by instafob_read(): the frame, then phase, offset, tries. */
#define INSTAFOB_READ_DATA_SIZE   32

/* ⛔ THERE IS NO `write_instafob_to_t55xx`, AND THAT IS DELIBERATE. The config word is known
 * (`0x00107060`) and the writer would be four lines, but NOTHING ON THIS BENCH COULD VERIFY
 * IT: the Proxmark has no InstaFob support at all and the Flipper sits on the other rig, so
 * the only reader available for our own written tag would be our own reader. A write command
 * that ships on self-certification is the `idteck.c` failure exactly — it looks supported.
 * ⇒ Read only, until the Flipper can face the T5577 (C185, and it is in Needs hands). */
bool instafob_read(uint8_t *data, uint32_t timeout_ms, int32_t *energy_out);

/** Bytes written by gproxii_read(): the 12-byte frame, then phase, bit position, tries and
 *  the inversion flag.
 *
 *  ⚠ The frame and nothing else — same reasoning as Gallagher's. GProxII's facility code and
 *  card number come out of an XOR descramble plus bit-field extractions whose layout depends
 *  on the format length, and that interpretation is testable on the host against a credential
 *  we chose. */
#define GPROXII_READ_DATA_SIZE 16
bool gproxii_read(uint8_t *data, uint32_t timeout_ms, int32_t *energy_out);

/** Bytes written by awid_read(): the 12-byte frame, the 9-byte payload, then phase/tries.
 *
 * ✅ `write_awid_to_t55xx` SHIPS and is verified: our write, the Proxmark's read, 5 of 5, with
 * the tag wiped first and the block dump identical to the Proxmark's own clone (C203). */
#define AWID_READ_DATA_SIZE 24
bool awid_read(uint8_t *data, uint32_t timeout_ms, int32_t *energy_out);

/** Bytes written by paradox_read(): the 12-byte frame, then phase, tries, pad. */
#define PARADOX_READ_DATA_SIZE 16
bool paradox_read(uint8_t *data, uint32_t timeout_ms, int32_t *energy_out);

/** Bytes written by pyramid_read(): the 16-byte frame, then phase, tries, pad. */
#define PYRAMID_READ_DATA_SIZE 20
bool pyramid_read(uint8_t *data, uint32_t timeout_ms, int32_t *energy_out);

/** Bytes written by fdxa_read(): the 12-byte frame, the 5 decoded bytes, phase, tries, pad. */
#define FDXA_READ_DATA_SIZE 20
bool fdxa_read(uint8_t *data, uint32_t timeout_ms, int32_t *energy_out);

/** Bytes of frame in an Indala224 read: 224 bits. */
#define INDALA224_READ_FRAME_BYTES 28
/** Bytes written by indala224_read(): the frame, then phase, offset, tries, and a pad. */
#define INDALA224_READ_DATA_SIZE   32

/** Indala224, same engine, a longer capture, and the repeat gate doing the accepting. */
bool indala224_read(uint8_t *data, uint32_t timeout_ms, int32_t *energy_out);
