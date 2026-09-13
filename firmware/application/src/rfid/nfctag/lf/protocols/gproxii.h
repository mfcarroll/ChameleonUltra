#pragma once

#include "protocols.h"

// GProxII: a 96-bit ASK/BIPHASE frame at RF/64 — the first emulated protocol of that family.
#define GPROXII_DATA_SIZE  (12)
#define GPROXII_BIT_COUNT  (96)

/* ⭐ TWO ENTRIES PER BIT, one per HALF-bit, and the count is fixed — unlike AWID's, where a 0
 * costs six entries and a 1 costs five. Biphase spends the same time on both symbols. */
#define GPROXII_PWM_ENTRIES (GPROXII_BIT_COUNT * 2)

typedef struct {
    uint8_t data[GPROXII_DATA_SIZE];
} gproxii_codec;

extern const protocol gproxii;
