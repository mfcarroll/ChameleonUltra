#pragma once

// The exchange log: every field session (a presentation) and every command, in compact entries, saved to a flash area
// of its own and read over USB. An entry says what the engine captured, how it decoded it, what the module answered,
// and when the answer went out, which is what tells a capture fault from a decode, module or modulation fault.
//
// The engine fills one full record (lf_ia_trace_rec_t) at a time. When the record is final, the log encodes it into
// a byte stream of entries, in a buffer in retained RAM. While no field is present, lf_ia_trace_process() saves the
// buffer to flash in small pieces. The read-out sees one stream: the saved bytes in flash, then the bytes in RAM.
//
// Flash writes halt the CPU, so the log writes and erases only while no field is present: about 85 ms per page erase,
// about 41 µs per word written. lf_ia_trace_save_all() saves the rest before System OFF.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lf_ia.h"
#include "lf_ia_debug.h"

#define LF_IA_TRACE_GAPS (48u)  // gaps kept per command in the full record

enum {
    LF_IA_TR_FIELD_OFF = 2,
    LF_IA_TR_COMMAND = 3,
};

// Record flags.
#define LF_IA_TRF_LATE          (1u << 0)  // the answer started after its due time
#define LF_IA_TRF_GAPS_DROPPED  (1u << 1)  // the command had more gaps than the engine keeps
#define LF_IA_TRF_ENCODE_FAILED (1u << 2)  // the response did not fit the run buffer
#define LF_IA_TRF_FIELD_BACK    (1u << 3)  // FIELD_OFF only: the recheck found the field, so a new session started

// The record the engine fills. It lives in RAM only, until the log encodes it.
typedef struct {
    uint8_t type;
    uint8_t verdict;
    uint8_t decode_status;
    uint8_t flags;
    uint32_t t_us;         // FIELD_OFF: when the field went; COMMAND: the end of its last gap
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
    uint16_t max_gap_us;   // the command's widest gap, of all its gaps
    uint8_t bits[16];      // the first 128 decoded bits
    uint8_t tx_bits[16];   // the first 128 sent bits
    struct {
        uint16_t start_us;  // from the first gap's start
        uint16_t width_us;
    } gaps[LF_IA_TRACE_GAPS];
    // Timing (lf_ia.h, LF_IA_MODE_): the carrier the answer was timed by, and what the measurement found.
    uint32_t fc_used_dhz;      // in 0.1 Hz: the measured carrier with LF_IA_MODE_SCALE, else 125 kHz
    uint32_t fc_measured_dhz;  // in 0.1 Hz, 0 if no measurement succeeded yet in this session
    int16_t meas_snr_db10;     // the last measurement's line against the band, in 0.1 dB
    uint8_t meas_status;       // the last measurement's lf_ia_carrier_status_t, 255 if none ran
    uint8_t mode;              // the slot's LF_IA_MODE_ flags
    uint8_t slot;              // the active slot, 1 to 8
    uint8_t meas_us32;         // how long the last measurement took in the main loop, in 32 µs units
    // COMMAND with an answer: the last time the engine saw the reader's field, from the command's end to the answer's
    // end. The watch sees up to about 35 µs before t_tx_end_us (the ADC's conversion and the 4-sample mean's lag). If
    // it is earlier than that, the reader dropped its field while the CU was answering. 0: never seen.
    uint32_t t_field_seen_us;
} lf_ia_trace_rec_t;

// Called once at start-up, after the SoftDevice. If the retained RAM lost the log's state (a power-on reset or a
// flash), the log takes its write position, its counters and its totals from the entries in flash.
void lf_ia_trace_init(void);

// Field sessions. session_start counts the session and logs its header. accept_bits is the module's
// lf_ia_module_t.accept_bits. session_end takes the session's FIELD_OFF record, which the engine may still change
// until lf_ia_trace_flush() or the next session_start.
void lf_ia_trace_session_start(uint32_t t_us, uint8_t slot, uint8_t mode, uint16_t accept_bits);
lf_ia_trace_rec_t *lf_ia_trace_session_end(uint32_t t_us, uint16_t level);

// Takes the record, zeroed. The engine fills it and calls lf_ia_trace_commit. The log encodes the previous record
// first.
lf_ia_trace_rec_t *lf_ia_trace_begin(uint8_t type);
void lf_ia_trace_commit(void);
// The record most recently committed, for the engine to complete after the answer goes out. NULL if none.
lf_ia_trace_rec_t *lf_ia_trace_last(void);
// Encodes the committed record into the log. The engine calls it once the field has gone and it no longer changes the
// record.
void lf_ia_trace_flush(void);

// The engine's counters, kept with the log.
lf_ia_stats_t *lf_ia_trace_stats(void);

// From the main loop: erases or saves one small piece if quiet (no field present and the engine idle), and if the log
// has work to do.
void lf_ia_trace_process(bool quiet);
// Before System OFF: saves everything in RAM, waiting for each write. Does nothing while the log is erasing.
void lf_ia_trace_save_all(void);

// Restarts the log: clears the counters and the RAM buffer, and erases the flash area page by page while quiet. The
// log saves nothing until the erase has finished. The engine's records keep going to RAM meanwhile.
void lf_ia_trace_clear(void);

typedef enum {
    LF_IA_LOG_ERASING = 1,  // checking and erasing the flash area, page by page
    LF_IA_LOG_READY = 2,    // saving
    LF_IA_LOG_FULL = 3,     // the flash area has no room for the next piece
    LF_IA_LOG_BROKEN = 4,   // the flash area overlaps other data, or a write failed: the log saves nothing
} lf_ia_log_state_t;

#define LF_IA_LOGI_RECOVERED (1u << 0)  // at the last power-on, the counters came from flash
#define LF_IA_LOGI_FILLING   (1u << 1)  // the log is nearly full: presentations are kept only as lf_ia_trace.c says

// The read-out's summary, as sent over USB (little-endian).
typedef struct __attribute__((packed)) {
    uint8_t format;        // LF_IA_LOG_FORMAT: the entries' layout
    uint8_t state;         // lf_ia_log_state_t
    uint8_t flags;         // LF_IA_LOGI_
    uint8_t reserved;
    uint16_t erase_left;   // pages still to check or erase
    uint16_t boot;         // the current boot (wake), counted since the clear
    uint32_t session;      // the last session, counted since the clear
    uint32_t capacity;     // stream bytes the flash area holds
    uint32_t saved;        // stream bytes in flash
    uint32_t pending;      // stream bytes in RAM, after the saved ones
    uint32_t ram_size;     // the RAM buffer's size
    uint32_t dropped;      // entries lost because the RAM buffer was full
    uint32_t not_kept;     // presentations left out because the log was nearly full
    uint16_t slot_done;    // bit 2s: slot s+1 has a kept success; bit 2s+1: a kept failure
    uint16_t recover_bad;  // entries with a bad checksum found at the last recovery from flash
    lf_ia_stats_t stats;
} lf_ia_trace_info_t;

void lf_ia_trace_info(lf_ia_trace_info_t *out);
// Copies up to max stream bytes from offset into out. Returns how many. end_out receives the stream's end (saved
// plus pending). A byte keeps its offset and value when it moves from RAM to flash, so a read in several pieces
// gives one consistent stream, unless a clear comes between them.
size_t lf_ia_trace_read(uint32_t offset, uint8_t *out, size_t max, uint32_t *end_out);
