#include "fsk2a_t55xx.h"

uint8_t fsk2a_t55xx_blocks(const uint8_t *frame, uint8_t words, uint32_t config, uint32_t *blks) {
    blks[0] = config;
    for (uint8_t w = 0; w < words; w++) {
        uint32_t v = 0;
        for (int i = 0; i < 4; i++) {
            v = (v << 8) | frame[w * 4 + i];
        }
        blks[1 + w] = v;
    }
    return (uint8_t)(words + 1);
}
