#pragma once

#include "protocols.h"

// FDX-B (ISO 11784/11785, the animal tag): a 128-bit ASK/BIPHASE frame at RF/32 — the SECOND
// emulated protocol of the biphase family, and the first emulate arm built after C429 proved
// the family emits at all.
#define FDXB_DATA_SIZE  (16)
#define FDXB_BIT_COUNT  (128)

/* ⚠ ONE entry per bit, exactly as gproxii.c — see the note there. Two entries per bit (one per
 * half-bit) is the shape that was silent to the Flipper. */

typedef struct {
    uint8_t data[FDXB_DATA_SIZE];
} fdxb_codec;

extern const protocol fdxb;
