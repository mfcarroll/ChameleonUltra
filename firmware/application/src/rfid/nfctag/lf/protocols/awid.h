#pragma once

#include "protocols.h"

// AWID: a 96-bit FSK2a frame, the first EMULATED protocol of that family.
#define AWID_DATA_SIZE  (12)
#define AWID_BIT_COUNT  (96)

/* ⛔ WORST-CASE PWM ENTRIES, AND THE WORST CASE IS ALL ZEROS. FSK2a spends SIX tone periods
 * on a 0 and only five on a 1, so a frame of 96 zeros needs 576 entries where a frame of 96
 * ones needs 480. Sizing this at 96x5 would overrun on the first all-zero field. */

typedef struct {
    uint8_t data[AWID_DATA_SIZE];
} awid_codec;

extern const protocol awid;

/** @brief T5577 blocks for a 96-bit AWID frame: config + 3 data words. Writes 4. */
uint8_t awid_t55xx_writer(uint8_t *frame12, uint32_t *blks);
