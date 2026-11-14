#include "lf_ia_trace.h"

#include <string.h>

#include "app_util_platform.h"

static lf_ia_trace_rec_t m_recs[LF_IA_TRACE_RECORDS];
static uint32_t m_written;  // records committed since the last clear
static lf_ia_trace_rec_t *m_open;

lf_ia_trace_rec_t *lf_ia_trace_begin(uint8_t type) {
    m_open = &m_recs[m_written % LF_IA_TRACE_RECORDS];
    memset(m_open, 0, sizeof(*m_open));
    m_open->type = type;
    return m_open;
}

void lf_ia_trace_commit(void) {
    if (m_open) {
        m_open = NULL;
        m_written++;
    }
}

lf_ia_trace_rec_t *lf_ia_trace_last(void) {
    return m_written ? &m_recs[(m_written - 1) % LF_IA_TRACE_RECORDS] : NULL;
}

void lf_ia_trace_clear(void) {
    CRITICAL_REGION_ENTER();
    m_written = 0;
    m_open = NULL;
    CRITICAL_REGION_EXIT();
}

size_t lf_ia_trace_read(size_t first, lf_ia_trace_rec_t *out, size_t max, uint32_t *total_out) {
    size_t n = 0;
    CRITICAL_REGION_ENTER();
    const uint32_t kept = m_written < LF_IA_TRACE_RECORDS ? m_written : LF_IA_TRACE_RECORDS;
    const uint32_t oldest = m_written - kept;
    for (size_t i = first; i < kept && n < max; i++, n++) {
        out[n] = m_recs[(oldest + i) % LF_IA_TRACE_RECORDS];
    }
    *total_out = m_written;
    CRITICAL_REGION_EXIT();
    return n;
}
