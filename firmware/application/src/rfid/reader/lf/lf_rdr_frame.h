#pragma once

// The bytes the interactive LF reader's bench commands (LF_RDR_TRANSACT, LF_RDR_LAST) return for an answer, with no
// hardware. A host parser depends on this layout, and a host test checks it.

#include <stddef.h>
#include <stdint.h>

#include "lf_rdr_ia.h"

#define LF_RDR_FRAME_RUNS (400u)  // runs past this many are left out of the frame
#define LF_RDR_FRAME_MAX  (44u + LF_RDR_FRAME_RUNS * 2u)

// Writes the answer's bit count (u16), 32 bytes of bits, the envelope's first loaded sample (u16), idle, load and
// threshold levels (i16 each), the run count (u16), then up to LF_RDR_FRAME_RUNS runs (u16 each), all little-endian.
// The run count is n_runs, even if the frame holds fewer. out must hold LF_RDR_FRAME_MAX bytes. Returns the length.
size_t lf_rdr_frame_pack(const lf_rdr_reply_t *rx, const uint16_t *runs, size_t n_runs, uint8_t *out);
