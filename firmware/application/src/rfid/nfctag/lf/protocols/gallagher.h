#pragma once

#include "protocols.h"

// Gallagher: a 96-bit ASK/Manchester frame at RF/32 — the first protocol of the biphase
// family on this device. 12 bytes, three T5577 blocks, 16-bit preamble 0x7FEA (C171).
#define GALLAGHER_DATA_SIZE  (12)
#define GALLAGHER_BIT_COUNT  (96)

/** @brief T5577 blocks for a 96-bit Gallagher frame: config + 3 data words. Writes 4. */
uint8_t gallagher_t55xx_writer(uint8_t *frame12, uint32_t *blks);
