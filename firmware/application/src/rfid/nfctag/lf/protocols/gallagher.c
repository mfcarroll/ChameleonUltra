#include <string.h>

#include "gallagher.h"
#include "t55xx.h"

// T5577 writer: block 0 the ASK RF/32 configuration, blocks 1-3 the 96 bits big-endian.
//
// ⭐ A STRAIGHT TRANSCRIPTION, and as with NexWatch that is the MEASURED answer rather than
// the assumed one. A Proxmark clone of region 1 / facility 4321 / card 6789 / issue 2 holds
// `7FEAA31E / 76D86C6D / 868CC249`, whose first block begins with the frame's own `0x7FEA`
// preamble — so the block form and the air frame coincide. ⛔ Keri's do NOT: it holds
// `(id << 3) | 7`, three bits out of phase with its air frame (C158), and emitting the wrong
// one of the two gave a stable WRONG credential 6 of 6 (C160). Which case a protocol is in is
// read off a real clone's block dump, never assumed from a neighbour.
//
// ⚠ THERE IS NO EMULATOR HERE YET, and that is deliberate rather than an omission. The shared
// PSK1 modulator cannot carry this: it emits a phase-modulated fc/2 subcarrier, where ASK
// needs the field amplitude keyed. An ASK/Manchester emitter is its own piece of work.
uint8_t gallagher_t55xx_writer(uint8_t *frame12, uint32_t *blks) {
    blks[0] = T5577_GALLAGHER_CONFIG;
    for (int w = 0; w < 3; w++) {
        uint32_t v = 0;
        for (int i = 0; i < 4; i++) v = (v << 8) | frame12[w * 4 + i];
        blks[1 + w] = v;
    }
    return 4;
}
