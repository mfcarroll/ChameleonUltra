#include "lf_rdr_frame.h"

#include <string.h>

static void put_u16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

size_t lf_rdr_frame_pack(const lf_rdr_reply_t *rx, const uint16_t *runs, size_t n_runs, uint8_t *out) {
    const size_t keep = n_runs < LF_RDR_FRAME_RUNS ? n_runs : LF_RDR_FRAME_RUNS;
    put_u16(out, (uint16_t)rx->n_bits);
    memcpy(out + 2, rx->bits, 32);
    put_u16(out + 34, (uint16_t)rx->env.first);
    put_u16(out + 36, (uint16_t)(int16_t)rx->env.idle);
    put_u16(out + 38, (uint16_t)(int16_t)rx->env.load);
    put_u16(out + 40, (uint16_t)(int16_t)rx->env.threshold);
    put_u16(out + 42, (uint16_t)n_runs);
    for (size_t i = 0; i < keep; i++) {
        put_u16(out + 44 + 2 * i, runs[i]);
    }
    return 44 + keep * 2;
}
