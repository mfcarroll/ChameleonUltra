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

void lf_rdr_trace_pack(const uint8_t *tx, size_t n_tx, uint32_t power_up_us, const lf_rdr_reply_t *rx,
                       const uint16_t *runs, size_t n_runs, uint8_t *out) {
    memset(out, 0, LF_RDR_TRACE_REC_SIZE);
    // tx holds only the command's bytes, so copy no more than those, and clear the bits past the command.
    const size_t kept = n_tx < LF_RDR_TRACE_TX_BITS ? n_tx : LF_RDR_TRACE_TX_BITS;
    out[0] = (uint8_t)n_tx;
    memcpy(out + 1, tx, (kept + 7u) / 8u);
    if (kept % 8u) {
        out[1 + kept / 8u] &= (uint8_t)(0xFFu << (8u - kept % 8u));
    }
    put_u16(out + 5, power_up_us < 0xFFFFu ? (uint16_t)power_up_us : 0xFFFFu);
    out[7] = rx->clean ? 1u : 0u;
    put_u16(out + 8, (uint16_t)rx->n_bits);
    put_u16(out + 10, (uint16_t)rx->env.first);
    put_u16(out + 12, (uint16_t)(int16_t)rx->env.idle);
    put_u16(out + 14, (uint16_t)(int16_t)rx->env.load);
    put_u16(out + 16, (uint16_t)(int16_t)rx->env.threshold);
    put_u16(out + 18, (uint16_t)n_runs);
    const size_t keep = n_runs < LF_RDR_TRACE_RUNS ? n_runs : LF_RDR_TRACE_RUNS;
    for (size_t i = 0; i < keep; i++) {
        put_u16(out + 20 + 2 * i, runs[i]);
    }
}
