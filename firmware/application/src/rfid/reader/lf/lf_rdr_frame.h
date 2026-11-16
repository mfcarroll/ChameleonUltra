#pragma once

// The bytes the interactive LF reader's bench commands return, with no hardware: an answer (LF_RDR_TRANSACT,
// LF_RDR_LAST) and a trace record (LF_RDR_TRACE_READ). Host parsers depend on these layouts, and a host test checks
// them.

#include <stddef.h>
#include <stdint.h>

#include "lf_rdr_ia.h"

#define LF_RDR_FRAME_RUNS (400u)  // runs past this many are left out of the frame
#define LF_RDR_FRAME_MAX  (44u + LF_RDR_FRAME_RUNS * 2u)

// Writes the answer's bit count (u16), 32 bytes of bits, the envelope's first loaded sample (u16), idle, load and
// threshold levels (i16 each), the run count (u16), then up to LF_RDR_FRAME_RUNS runs (u16 each), all little-endian.
// The run count is n_runs, even if the frame holds fewer. out must hold LF_RDR_FRAME_MAX bytes. Returns the length.
size_t lf_rdr_frame_pack(const lf_rdr_reply_t *rx, const uint16_t *runs, size_t n_runs, uint8_t *out);

#define LF_RDR_TRACE_TX_BITS (32u)  // command bits a record keeps
#define LF_RDR_TRACE_RUNS    (96u)  // enough for a 38-bit Manchester answer with its start-of-frame
#define LF_RDR_TRACE_REC_SIZE (20u + LF_RDR_TRACE_RUNS * 2u)

// Writes one transaction's trace record, LF_RDR_TRACE_REC_SIZE bytes, all little-endian: the command's bit count (u8),
// its first 32 bits packed first-bit-first in 4 bytes, the power-up time (u16 µs, at most 65535), flags (u8, bit 0 =
// clean), the answer's bit count (u16), the envelope's first loaded sample (u16), idle, load and threshold levels (i16
// each), the run count (u16), then LF_RDR_TRACE_RUNS runs (u16 each, zero past n_runs). The run count is n_runs, even
// if the record holds fewer.
void lf_rdr_trace_pack(const uint8_t *tx, size_t n_tx, uint32_t power_up_us, const lf_rdr_reply_t *rx,
                       const uint16_t *runs, size_t n_runs, uint8_t *out);
