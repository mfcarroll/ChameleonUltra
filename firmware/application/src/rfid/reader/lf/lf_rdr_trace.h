#pragma once

// The reader's transaction trace: one record for each lf_rdr_transact whose capture completed, with its command and
// the answer's timing. A host clears it, runs a scan or a read, and reads back one record for each command that
// exchange sent. It lives in plain RAM, so a reset clears it. No hardware: a host test runs it.

#include <stddef.h>
#include <stdint.h>

#include "lf_rdr_frame.h"

// A Hitag 2 read sends at most 59 commands: 8 START_AUTHs, 3 passwords, and 6 tries at each of 8 pages.
#define LF_RDR_TRACE_RECORDS (64u)

void lf_rdr_trace_clear(void);
// Records one transaction. When the trace is full, the new record replaces the oldest.
void lf_rdr_trace_add(const uint8_t *tx, size_t n_tx, uint32_t power_up_us, const lf_rdr_reply_t *rx,
                      const uint16_t *runs, size_t n_runs);
// Copies up to max records into out, oldest first, starting first records after the oldest one held. Returns how
// many. total_out receives the records added since the clear, at most 65535. If it exceeds LF_RDR_TRACE_RECORDS, the
// oldest ones were replaced.
size_t lf_rdr_trace_read(size_t first, uint8_t *out, size_t max, uint16_t *total_out);
