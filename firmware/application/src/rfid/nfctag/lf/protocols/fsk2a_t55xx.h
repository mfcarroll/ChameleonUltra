#pragma once

#include <stdint.h>

/**
 * @brief T5577 blocks for an FSK2a frame: the config word, then the frame big-endian.
 *
 * ⭐⭐ ONE FUNCTION FOR THREE PROTOCOLS, AND THAT IS A MEASUREMENT RATHER THAN A FAMILY
 * ASSUMPTION. AWID, Paradox and Pyramid were each cloned by a Proxmark and dumped, and each
 * one's blocks 1..n came back byte-identical to the air frame our own decoder reads off the
 * same tag (C202):
 *
 *   AWID     011D8171 1DD11811 11111111            vs  011d81711dd1181111111111
 *   Paradox  0F555556 95596A6A 9999A59A            vs  0f55555695596a6a9999a59a
 *   Pyramid  00010101 01010101 0101016E B35E5DA4   vs  00010101010101010101016eb35e5da4
 *
 * ⛔ The same question has the OPPOSITE answer for Keri, whose block form is `(id << 3) | 7`
 * — three bits out of phase with its air frame — and emitting the wrong one of the two gave a
 * stable WRONG credential 6 times out of 6 (C160). So it is asked per protocol and answered
 * from a real clone's dump, never inherited from a neighbour. These three happen to agree,
 * which is what licenses one shared transcription; a fourth FSK2a protocol does NOT get to
 * use this function until its own dump has been read.
 *
 * ⚠ There is nothing here to reject. The frame is a fixed-size byte array and every bit of it
 * is data, so unlike keri_t55xx_writer() this cannot fail and its callers have no
 * `blk_count == 0` branch to take.
 *
 * ⭐ Lives here rather than beside its callers in lf_reader_main.c so that it host-compiles:
 * `ctest/roundtrip.c` pins all three vectors above, which makes this the first T5577 writer
 * in the tree with coverage that does not need a tag.
 *
 * @param frame   the air frame, `words * 4` bytes
 * @param words   data words to write (3 for AWID and Paradox, 4 for Pyramid)
 * @param config  block 0 — see T5577_AWID_CONFIG and friends for where each was measured
 * @param blks    out: `words + 1` words, config first
 * @return        blocks to write, always `words + 1`
 */
uint8_t fsk2a_t55xx_blocks(const uint8_t *frame, uint8_t words, uint32_t config, uint32_t *blks);
