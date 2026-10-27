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

#include <stddef.h>
#include <stdint.h>

#include "nrf_pwm.h"

#define LF_FSK_TICKS_PER_CYCLE (128u)  // 16 MHz / 125 kHz
#define LF_FSK_TRIM_MAX_PPM (1000u)    // |trim| is clamped to this fraction of the frame's ticks

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
