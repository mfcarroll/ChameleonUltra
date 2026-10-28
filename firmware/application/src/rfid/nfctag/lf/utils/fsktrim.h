#pragma once

// research (exp/fsk-drvtrim): FSK emulation (ioProx, HID Prox) at a 16 MHz PWM base, a runtime
// subcarrier trim, and a runtime choice of which pins carry the modulation (drive mode).
//
// The protocol modulators write each subcarrier period as counter_top = carrier cycles x
// LF_FSK_TICKS_PER_CYCLE (fc/8 = 1024, fc/10 = 1280) and channel_0 = counter_top / 2, so with
// trim 0 the timing is the same as the old 125 kHz base (8 and 10 ticks). lf_fsk_prepare()
// then sets channel_1 for the drive mode and spreads the trim over the frame. It recovers each
// period's nominal length by rounding to a whole carrier cycle, so it can be re-run on an
// already-trimmed sequence with a new trim. Only while the PWM is not playing the sequence.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nrf_pwm.h"

#define LF_FSK_TICKS_PER_CYCLE (128u)  // 16 MHz / 125 kHz
#define LF_FSK_TRIM_MAX_PPM (36000u)   // |trim| is clamped to this fraction of the frame's ticks (exp/fsk-selftrim-wide:
                                       // was 1000; covers the wide search, SELFTRIM_WIDE_MAX_PPM)

// Drive modes (hw lfdrive, command 3093). Pins are chosen in pwm_init (lf_tag_em.c).
#define LF_FSK_DRIVE_LFMOD (0u)       // LF_MOD only (stock path; = fc10-hard)
#define LF_FSK_DRIVE_U1 (1u)          // U1 select (LF_ANT_DRIVER) only (= drvC)
#define LF_FSK_DRIVE_U1_SAME (2u)     // LF_MOD + U1, same compare (= drvA)
#define LF_FSK_DRIVE_U1_INV (3u)      // LF_MOD + U1, U1 inverted every period (polarity bit)
#define LF_FSK_DRIVE_U1_IDLEHI (4u)   // as built for drvB on 2026-10-09: same compare as drvA, U1 idles HIGH
#define LF_FSK_DRIVE_MAX LF_FSK_DRIVE_U1_IDLEHI
#define LF_FSK_DRIVE_DEFAULT LF_FSK_DRIVE_U1

// Largest |trim| allowed for a frame of nominal_ticks (LF_FSK_TRIM_MAX_PPM of it).
uint32_t lf_fsk_trim_limit(uint64_t nominal_ticks);

// Rewrite k periods in place: restore the nominal lengths, set channel_0 = nominal / 2 and
// channel_1 for the drive mode, then make the frame |trim| ticks shorter (trim > 0, faster
// subcarrier) or longer (trim < 0, slower), the ticks spread evenly over the k periods
// (period i gets floor((i+1)|n|/k) - floor(i|n|/k), i.e. 0 or 1 tick while |n| <= k).
// trim is clamped to lf_fsk_trim_limit(); returns the trim applied. *frame_ticks (optional)
// receives the frame's total ticks after trimming.
int32_t lf_fsk_prepare(nrf_pwm_values_wave_form_t *v, size_t k, int32_t trim, uint8_t drive,
                       uint32_t *frame_ticks);

// EXPERIMENT (lf_exp_fsk.h): FSK2a bits of whole subcarrier periods. With exact set, each bit ends at the period
// edge nearest its nominal boundary (bit_cycles carrier cycles per bit, counted from the frame start), so the frame
// keeps the nominal bit rate, as ioprox.c does: fc/8 at RF/50 then takes 6 or 7 periods (48 or 56 cycles), 50 on
// average. Without it, a bit is a fixed number of periods (fc/8: 6 x 8 = 48 cycles, 4 % short of RF/50).
// The loader sets g_lf_fsk_exact_bits before calling a modulator.
extern bool g_lf_fsk_exact_bits;
typedef struct {
    uint32_t cycles;  // carrier cycles emitted in this frame
    uint32_t bits;    // bits emitted in this frame
} lf_fsk_bitclock_t;
// top: the period in carrier cycles; loops: periods per bit when not exact. Writes 16 MHz tops at v[*k...].
void lf_fsk_emit_bit(nrf_pwm_values_wave_form_t *v, uint16_t *k, lf_fsk_bitclock_t *c, uint16_t top, uint16_t loops,
                     uint16_t bit_cycles);
