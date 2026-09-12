#pragma once

#include "protocols.h"

// Keri frame layout: a 33-bit preamble — 111, then 29 zeros, then a 1 — followed by the
// low 31 bits of a 32-bit "internal id" whose top bit is that final preamble 1. So the
// air frame is always E0000000 followed by the internal id, and a valid internal id
// always has its top bit set.
#define KERI_DATA_SIZE   (8)     // 8 bytes = 64 bits on air
#define KERI_BIT_COUNT   (64)

typedef struct {
    uint8_t data[KERI_DATA_SIZE];
} keri_codec;

extern const protocol keri;

uint8_t keri_t55xx_writer(uint8_t *frame8, uint32_t *blks);
