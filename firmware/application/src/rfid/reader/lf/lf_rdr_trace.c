#include "lf_rdr_trace.h"

#include <string.h>

static uint8_t m_recs[LF_RDR_TRACE_RECORDS][LF_RDR_TRACE_REC_SIZE];
static size_t m_next;   // the slot the next record goes in
static uint32_t m_total;

void lf_rdr_trace_clear(void) {
    m_next = 0;
    m_total = 0;
}

void lf_rdr_trace_add(const uint8_t *tx, size_t n_tx, uint32_t power_up_us, const lf_rdr_reply_t *rx,
                      const uint16_t *runs, size_t n_runs) {
    lf_rdr_trace_pack(tx, n_tx, power_up_us, rx, runs, n_runs, m_recs[m_next]);
    m_next = (m_next + 1u) % LF_RDR_TRACE_RECORDS;
    m_total++;
}

size_t lf_rdr_trace_read(size_t first, uint8_t *out, size_t max, uint16_t *total_out) {
    *total_out = m_total < 0xFFFFu ? (uint16_t)m_total : 0xFFFFu;
    const size_t held = m_total < LF_RDR_TRACE_RECORDS ? (size_t)m_total : LF_RDR_TRACE_RECORDS;
    const size_t oldest = held < LF_RDR_TRACE_RECORDS ? 0 : m_next;
    size_t n = 0;
    for (size_t i = first; i < held && n < max; i++, n++) {
        memcpy(out + n * LF_RDR_TRACE_REC_SIZE, m_recs[(oldest + i) % LF_RDR_TRACE_RECORDS], LF_RDR_TRACE_REC_SIZE);
    }
    return n;
}
