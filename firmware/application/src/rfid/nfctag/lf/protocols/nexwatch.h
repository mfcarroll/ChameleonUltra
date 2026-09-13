#pragma once

#include "protocols.h"

// NexWatch: a 96-bit PSK1 frame at RF/32 on an fc/2 subcarrier — the same air layer as
// Indala26, IDTECK and Keri, measured rather than assumed (C164). 12 bytes, three T5577
// blocks.
//
// ⭐ THE FRAME IS BLOCK-ALIGNED, and that is not an assumption either: a Proxmark clone's
// own blocks are `56000000 / 00436455 / 121E6000`, so block 1 begins with the frame's
// `0x56` preamble. ⛔ Keri's are NOT — it holds `(id << 3) | 7`, three bits out of phase
// with its air frame (C158) — so this writer transcribes where Keri's rotates. Getting that
// backwards is what cost a stable wrong credential 6 of 6 (C160).
#define NEXWATCH_DATA_SIZE  (12)   // 12 bytes = 96 bits on air
#define NEXWATCH_BIT_COUNT  (96)

typedef struct {
    uint8_t data[NEXWATCH_DATA_SIZE];
} nexwatch_codec;

extern const protocol nexwatch;

/** @brief T5577 blocks for a 96-bit NexWatch frame: config + 3 data words. Writes 4. */
uint8_t nexwatch_t55xx_writer(uint8_t *frame12, uint32_t *blks);
