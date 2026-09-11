#pragma once

#include <stdbool.h>
#include <stdint.h>

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
bool indala_read(uint8_t *data, uint32_t timeout_ms);
