#include "lf_slicer.h"
#include "lf_indala_psk.h"   /* LF_SAMPLED_MAX_CAPTURE_SAMPLES */

#define DC_MAX_BLOCKS ((LF_SAMPLED_MAX_CAPTURE_SAMPLES / LF_SLICER_BLOCK) + 1)

static int32_t m_dc[DC_MAX_BLOCKS];

void lf_slicer_build_dc(const int16_t *samples, size_t n) {
    size_t nb = (n + LF_SLICER_BLOCK - 1) / LF_SLICER_BLOCK;
    if (nb > DC_MAX_BLOCKS) {
        nb = DC_MAX_BLOCKS;
    }
    for (size_t b = 0; b < nb; b++) {
        size_t from = b * LF_SLICER_BLOCK;
        size_t to = from + LF_SLICER_BLOCK;
        if (to > n) {
            to = n;
        }
        int32_t acc = 0;
        for (size_t i = from; i < to; i++) {
            acc += samples[i];
        }
        m_dc[b] = acc / (int32_t)(to - from);
    }
}

/* ⚠ lp==1 is raw, and that is what the clean captures want: the level path needs no low-pass
 * at all, which is the opposite of what the edge route needed to suppress its glitches
 * (C171). lp==3 is kept because the CLIPPED captures preferred it. */
static inline int32_t smp(const int16_t *s, size_t n, size_t i, uint8_t lp) {
    if (lp <= 1 || i == 0 || i + 1 >= n) {
        return s[i];
    }
    return ((int32_t)s[i - 1] + s[i] + s[i + 1]) / 3;
}

bool lf_slicer_level(const int16_t *samples, size_t n, size_t i, uint8_t lp) {
    return smp(samples, n, i, lp) > m_dc[i >> LF_SLICER_BLOCK_SHIFT];
}
