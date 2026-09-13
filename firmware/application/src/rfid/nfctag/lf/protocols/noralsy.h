#pragma once

#include "protocols.h"

// Noralsy: a 96-bit ASK/Manchester frame at RF/32 — the third protocol of the biphase family.
// 12 bytes, three T5577 blocks, 12-bit preamble plus two computed nibble checksums (C181).
#define NORALSY_DATA_SIZE  (12)
#define NORALSY_BIT_COUNT  (96)

typedef struct {
    uint8_t data[NORALSY_DATA_SIZE];
} noralsy_codec;

extern const protocol noralsy;

/** @brief T5577 blocks for a 96-bit Noralsy frame: config + 3 data words. Writes 4. */
uint8_t noralsy_t55xx_writer(uint8_t *frame12, uint32_t *blks);
