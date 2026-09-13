#pragma once

#include "protocols.h"

// GProxII: a 96-bit ASK/BIPHASE frame at RF/64 — the first emulated protocol of that family.
#define GPROXII_DATA_SIZE  (12)
#define GPROXII_BIT_COUNT  (96)

/* ⚠ ONE entry per bit — see the long note in gproxii.c. The first version spent two, one per
 * half-bit, and was silent to the Flipper. */

typedef struct {
    uint8_t data[GPROXII_DATA_SIZE];
} gproxii_codec;

extern const protocol gproxii;
