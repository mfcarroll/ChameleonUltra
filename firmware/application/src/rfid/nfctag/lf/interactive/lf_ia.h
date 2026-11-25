#pragma once

// The interactive LF engine and the interface a protocol module implements.
//
// The engine owns the hardware and the timing: it listens for a reader's command as field gaps, hands the command to
// the module, and sends the module's answer by load modulation at the delay the module asks for. The module owns
// every piece of protocol knowledge and state. All times are in microseconds of the CU's own clock, with the
// high-frequency crystal held on.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lf_ia_codec.h"

#define LF_IA_US_PER_FC         (8u)    // one carrier period at 125 kHz
#define LF_IA_MAX_GAPS          (128u)  // gaps kept per command
#define LF_IA_MAX_BITS          (LF_IA_MAX_GAPS)
#define LF_IA_MAX_RESPONSE_BITS (256u)

// One field gap: when the field dropped, and for how long.
typedef struct {
    uint32_t start_us;
    uint16_t width_us;
} lf_ia_gap_t;

// One reader command: the gaps from its first to its last, ended by a field-on stretch of the module's eof_us.
typedef struct {
    const lf_ia_gap_t *gaps;
    size_t n_gaps;
    uint32_t end_us;  // the end of the last gap, which response delays count from

    // Filled when the module set want_decode: one bit per interval between consecutive gap starts, so n_gaps - 1 bits.
    lf_ia_decode_status_t decode_status;
    const uint8_t *bits;
    size_t n_bits;

    uint8_t mode;  // the slot's LF_IA_MODE_ flags, for a module that offers LF_IA_MODE_ALT_PROTOCOL
} lf_ia_command_t;

typedef enum {
    LF_IA_IGNORE,    // no answer, keep listening
    LF_IA_RESPOND,   // send the response, then keep listening
    LF_IA_END,       // no answer, and the exchange is over until the field drops and comes back
} lf_ia_verdict_t;

typedef struct {
    uint8_t bits[LF_IA_MAX_RESPONSE_BITS / 8];
    size_t n_bits;
    lf_ia_coding_t coding;
    uint32_t coding_flags;  // LF_IA_INVERT, LF_IA_BIPHASE_ONE_FLIPS
    uint16_t bit_fc;        // carrier periods per bit, for example 32 for RF/32
    // From the command's end_us to the first half-bit of the answer: in carrier periods if delay_fc is set, which the
    // engine converts with the carrier it is using (see LF_IA_MODE_SCALE), else in µs.
    uint32_t delay_fc;
    uint32_t delay_us;
} lf_ia_response_t;

typedef struct {
    const char *name;

    // Command framing.
    uint32_t eof_us;        // a field-on stretch this long after a gap ends the command
    bool want_decode;       // run lf_ia_decode_intervals on each command before on_command
    lf_ia_decode_cfg_t decode;
    // Carrier periods to hold the load off after an answer (unless LF_IA_MODE_REST_LOADED). 0: one bit of the answer's
    // coding.
    uint32_t rest_fc;
    // If the module answers a decoded command of this many bits, the reader has accepted the tag (lf_ia_trace.h counts
    // a session with one as a success). 0: none does.
    uint16_t accept_bits;

    // Called with the field newly up, before the first command. May be NULL.
    void (*field_on)(void *ctx);
    // Called once per command, in interrupt context: keep it short, because its time comes out of the response window.
    lf_ia_verdict_t (*on_command)(void *ctx, const lf_ia_command_t *cmd, lf_ia_response_t *rsp);
    // Called when the field drops, so the module can reset. May be NULL.
    void (*field_off)(void *ctx);
    void *ctx;
} lf_ia_module_t;

// Per-slot modes, a research test of how the answer should follow the reader.
//
// LF_IA_MODE_SCALE: the engine measures the reader's carrier at the start of each listen (lf_ia_carrier.h) and times
// the answer's bits and delay in that reader's carrier periods. Without it, a carrier period is taken as 8 µs
// (125 kHz). LF_IA_MODE_ALT_PROTOCOL: the module follows its alternative protocol rule (Hitag 2: it answers only every
// second START_AUTH, as the datasheet says a genuine tag does).
// After each answer the engine holds the load off for the module's rest_fc, as a genuine tag rests unloaded, and then
// listens with the load on. The answer's last half-bit then ends cleanly, whatever its level. LF_IA_MODE_REST_LOADED
// skips the rest and listens with the load on as soon as the answer ends. That was the engine's only behaviour until
// 2026-11-18, and a door reader accepted it. It squeezes a final no-load half-bit, which breaks a CU reader on an
// odd UID.
//
// The modes are kept in retained RAM, so they survive sleep (a test runs at a reader on battery) but not a power-on
// reset or a flash, which set every slot to 0.
//
// Bit 1 (0x02) has no meaning, and the engine ignores it. Pages and scripts made for earlier builds still set it, so
// don't give it a new meaning while those are in use.
#define LF_IA_MODE_SCALE     (1u << 0)
#define LF_IA_MODE_ALT_PROTOCOL (1u << 2)
#define LF_IA_MODE_REST_LOADED (1u << 3)
// A slot's mode until one is set, and after a flash or a power-on reset: the answer timed in the reader's carrier.
#define LF_IA_MODE_DEFAULT (LF_IA_MODE_SCALE)
#define LF_IA_SLOTS          (8u)
void lf_ia_slot_modes_get(uint8_t modes[LF_IA_SLOTS]);
// Runs the engine's slow work (the carrier measurement) from the main loop.
void lf_ia_process(void);
void lf_ia_slot_modes_set(const uint8_t modes[LF_IA_SLOTS]);
// Per-slot answer delays, in carrier periods from the end of the reader's stop gap, kept with the modes. 0 leaves the
// module's own delay (Hitag 2: HT2_DELAY_FC). With LF_IA_MODE_SCALE the periods are the reader's measured ones.
#define LF_IA_DELAY_MIN_FC   (100u)
#define LF_IA_DELAY_MAX_FC   (400u)
void lf_ia_slot_delays_get(uint16_t delays_fc[LF_IA_SLOTS]);
// Values outside LF_IA_DELAY_MIN_FC..LF_IA_DELAY_MAX_FC, other than 0, return false and change nothing.
bool lf_ia_slot_delays_set(const uint16_t delays_fc[LF_IA_SLOTS]);

// Registers the module the engine runs while the slot's LF tag type is this module's. NULL clears it.
void lf_ia_set_module(const lf_ia_module_t *module);
const lf_ia_module_t *lf_ia_get_module(void);

// lf_tag_em.c's side: the engine replaces the fixed broadcast while a module is set.
void lf_ia_sense_enable(void);
void lf_ia_sense_disable(void);
void lf_ia_field_detected(void);
// Provided by lf_tag_em.c: the field has gone. Re-arms the wake comparator and starts the sleep timer.
void lf_tag_em_ia_field_lost(void);
