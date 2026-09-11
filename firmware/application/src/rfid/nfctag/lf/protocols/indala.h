#pragma once

#include "protocols.h"

// Indala frame layout: a fixed 33-bit preamble followed by 31 bits of payload.
// The preamble is 1,0,1,0 then 28 zeros then 1 — which is why every Indala raw
// word starts a0000000 and why the first bit of the second word is always set.
#define INDALA_DATA_SIZE   (8)     // 8 bytes = 64 bits on air
#define INDALA_BIT_COUNT   (64)

typedef struct {
    uint8_t data[INDALA_DATA_SIZE];
} indala_codec;

extern const protocol indala;

uint8_t indala_t55xx_writer(uint8_t *uid, uint32_t *blks);
