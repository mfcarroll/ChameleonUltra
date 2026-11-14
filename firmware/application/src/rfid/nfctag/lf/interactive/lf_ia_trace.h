#pragma once

// The exchange trace: one record per field event or command, kept in RAM and read over USB. A record says what the
// engine captured, how it decoded it, what the module answered, and when the answer went out, which is what tells a
// capture fault from a decode, module or modulation fault.

#include <stddef.h>
#include <stdint.h>

#include "lf_ia.h"

#define LF_IA_TRACE_RECORDS (32u)
#define LF_IA_TRACE_GAPS    (48u)

enum {
    LF_IA_TR_FIELD_ON = 1,
    LF_IA_TR_FIELD_OFF = 2,
    LF_IA_TR_COMMAND = 3,
};

// Record flags.
#define LF_IA_TRF_LATE          (1u << 0)  // the answer started after its due time
#define LF_IA_TRF_GAPS_DROPPED  (1u << 1)  // the command had more gaps than the engine keeps
#define LF_IA_TRF_ENCODE_FAILED (1u << 2)  // the response did not fit the run buffer
#define LF_IA_TRF_FIELD_BACK    (1u << 3)  // FIELD_OFF only: the recheck found the field, so a new session started

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t verdict;
    uint8_t decode_status;
    uint8_t flags;
    uint32_t t_us;         // FIELD_ON/OFF: the event; COMMAND: the end of its last gap
    uint32_t t_detect_us;  // when the engine saw the command's end
    uint32_t t_ready_us;   // when the answer was armed, after decode, module and encode
    uint32_t t_due_us;     // when the answer was due to start
    uint32_t t_tx_us;      // when the modulator started it
    uint32_t t_tx_end_us;  // when the modulator finished
    uint16_t n_gaps;
    uint16_t n_bits;
    uint16_t n_tx_bits;
    uint16_t n_runs;
    uint32_t split;        // the decode's split point, or its one class's mean
    uint16_t level;        // the field-on level the gap detector tracked, in ADC counts after smoothing
    uint16_t n_noise;      // dips shorter than the minimum gap, skipped
    uint8_t bits[16];      // the first 128 decoded bits
    uint8_t tx_bits[16];   // the first 128 sent bits
    struct __attribute__((packed)) {
        uint16_t start_us;  // from the first gap's start
        uint16_t width_us;
    } gaps[LF_IA_TRACE_GAPS];
} lf_ia_trace_rec_t;

// Takes the next record slot, zeroed. The engine fills it and calls lf_ia_trace_commit.
lf_ia_trace_rec_t *lf_ia_trace_begin(uint8_t type);
void lf_ia_trace_commit(void);
// The record most recently committed, for the engine to complete after the answer goes out. NULL if none.
lf_ia_trace_rec_t *lf_ia_trace_last(void);

void lf_ia_trace_clear(void);
// Copies records from index first (counted from the oldest kept) into out. Returns how many. total_out receives the
// number of records written since the last clear, which may exceed the number kept.
size_t lf_ia_trace_read(size_t first, lf_ia_trace_rec_t *out, size_t max, uint32_t *total_out);
