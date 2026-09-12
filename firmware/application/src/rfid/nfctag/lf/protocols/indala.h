#pragma once

#include "protocols.h"

// Indala frame layout: a fixed 33-bit preamble followed by 31 bits of payload.
// The preamble is 1,0,1,0 then 28 zeros then 1 — which is why every Indala raw
// word starts a0000000 and why the first bit of the second word is always set.
#define INDALA_DATA_SIZE   (8)     // 8 bytes = 64 bits on air
#define INDALA_BIT_COUNT   (64)

// Indala 224-bit: the same PSK1/RF-32 air layer with a 30-bit preamble (a 1 and 29 zeros)
// and a 224-bit frame — 28 bytes, which is seven T5577 blocks and fills page 0.
// ⛔ On a T5577 it is written PSK2, not PSK1 (C99). See T5577_INDALA224_CONFIG.
#define INDALA224_DATA_SIZE (28)   // 28 bytes = 224 bits on air
#define INDALA224_BIT_COUNT (224)

typedef struct {
    uint8_t data[INDALA_DATA_SIZE];
} indala_codec;

typedef struct {
    uint8_t data[INDALA224_DATA_SIZE];
} indala224_codec;

extern const protocol indala;
extern const protocol indala224;

uint8_t indala_t55xx_writer(uint8_t *uid, uint32_t *blks);

/** @brief T5577 blocks for a 224-bit Indala frame: config + 7 data words. Writes 8. */
uint8_t indala224_t55xx_writer(uint8_t *raw28, uint32_t *blks);
