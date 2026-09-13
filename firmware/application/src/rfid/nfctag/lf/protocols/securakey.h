#pragma once

#include "protocols.h"

// Securakey: a 96-bit ASK/Manchester frame at RF/40 — the second protocol of the biphase
// family. 12 bytes, three T5577 blocks, 19-bit preamble (C175).
#define SECURAKEY_DATA_SIZE  (12)
#define SECURAKEY_BIT_COUNT  (96)

typedef struct {
    uint8_t data[SECURAKEY_DATA_SIZE];
} securakey_codec;

extern const protocol securakey;

/** @brief T5577 blocks for a 96-bit Securakey frame: config + 3 data words. Writes 4. */
uint8_t securakey_t55xx_writer(uint8_t *frame12, uint32_t *blks);
