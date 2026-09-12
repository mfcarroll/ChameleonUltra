#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nrf_pwm.h"

// Shared PSK1 (BPSK on a carrier/2 subcarrier) tag-emulation parameters for the
// tag-emulation PWM path. The PWM base clock is set to 1MHz by pwm_init when an
// active PSK1 tag type is loaded (see tag_base_type.h IS_PSK1_TYPE), so one tick
// is 1us. With counter_top=16 each PWM entry spans one 16us subcarrier cycle.
// Nordic PS: COUNTERTOP in WaveForm mode has a minimum valid value of 3; 16 is
// comfortably above that.
#define LF_PSK1_SUBCARRIER_TOP          (16)
#define LF_PSK1_SUBCARRIER_DUTY         (8)
#define LF_PSK1_RF32_SUBCYCLES_PER_BIT  (16)

// A standard 64-bit PSK1/RF-32 frame, which is what most PSK1 tag types on this
// device transmit (Indala26 and IDTECK are identical at this layer — same T5577
// config word, same bit rate, same subcarrier, same frame length; only the
// preamble and payload interpretation differ).
#define LF_PSK1_RF32_FRAME_BITS   (64)

// ⭐ Indala224 is the same air layer with a 224-bit frame, so it shares this buffer
// rather than carrying its own. See the sizing note below for why that matters.
#define LF_PSK1_MAX_FRAME_BITS    (224)

// ⭐⭐ ONE PWM ENTRY PER BIT, NOT ONE PER SUBCARRIER CYCLE — the sequence's own `repeats`
// field holds each entry for LF_PSK1_RF32_SUBCYCLES_PER_BIT PWM periods.
//
// All 16 entries within a bit are IDENTICAL by construction: same polarity, same duty,
// same counter_top. Storing 16 copies of each cost 16x the RAM for no information, which
// was affordable at 64 bits (8KB) and is not at 224 (28KB, on a part where the Indala
// reader already holds 28KB of capture buffer). SEQ[n].REFRESH is the hardware's own
// answer: nrf_pwm_sequence_set() writes `repeats` there, and the peripheral holds each
// loaded sample for REFRESH+1 PWM periods with COUNTERTOP unchanged — which is exactly a
// 16us subcarrier cycle repeated 16 times, one RF/32 bit.
//
// ⚠ ANYTHING READING THIS SEQUENCE'S DURATION MUST MULTIPLY BY repeats+1. The frame is
// 224 entries of 16us but lasts 224 x 256us; recompute_frames_per_burst() in lf_tag_em.c
// is the one caller that cares and it reads `repeats` off the sequence to do it.
//
// ⚠ The Nordic PS notes REFRESH is ignored for the final sample of a playback, at
// LOOPSDONE. That shortens the last bit of the last frame in a burst and nothing else.
// ⛔ TWO COPIES OF THE LONGEST FRAME. An odd-parity PSK2 frame repeats at twice the
// frame period, and the second copy is phase-inverted — see the note at the end of
// lf_psk1_build_sequence. 448 entries x 8 bytes is 3584 bytes.
#define LF_PSK1_PWM_ENTRIES       (LF_PSK1_MAX_FRAME_BITS * 2)
#define LF_PSK1_SEQ_REPEATS       (LF_PSK1_RF32_SUBCYCLES_PER_BIT - 1)

// ⛔⛔ PSK1 AND PSK2 ARE DIFFERENT MODULATIONS AND A TAG TYPE MUST NAME WHICH IT IS.
// This is not a decode-time fallback: a reader cannot tell them apart from the signal,
// because the direct view of a PSK2 frame is the running XOR of its data and repeats at
// the frame period exactly as well as the data does (C107). Indala26 and IDTECK are PSK1;
// Indala224 is PSK2, which is what the Proxmark's own clone writes and what
// `lf t55xx detect` reports for it (C99).
typedef enum {
    LF_PSK1_PHASE_DIRECT = 0,       // PSK1: the subcarrier phase IS the data bit
    LF_PSK1_PHASE_DIFFERENTIAL,     // PSK2: the phase CHANGES on a 1 bit
} lf_psk1_phase_mode_t;

// Build a PSK1 or PSK2 wave-form PWM sequence for a frame transmitted MSB first.
//
// Each bit produces ONE entry, held for LF_PSK1_RF32_SUBCYCLES_PER_BIT PWM periods by
// the sequence's `repeats`.
//
// ⛔ THE DIRECT MODE IS PSK1 AND ITS TOGGLE FORM IS CORRECT — DO NOT "FIX" IT. PSK1 is NOT
// differential: the subcarrier phase IS the data, bit 1 one phase and bit 0 the other.
// Differential is PSK2 and is the OTHER mode here. Confusing the two is not hypothetical —
// this project spent a fortnight on a reader that demodulated PSK2 against a PSK1 tag, and
// the wrong verdict survived because its unit test encoded with the same mistake it
// decoded with.
//
// The toggle form used for DIRECT telescopes to the non-differential answer:
//     phase[k] = phase[k-1] XOR (bit[k] XOR bit[k-1])  =  bit[k] XOR bit[N-1]
// which is the data, inverted or not depending on the frame's last bit. A global
// inversion is unobservable — the absolute subcarrier phase is not knowable, which is
// why every reader searches both polarities.
//
// DIFFERENTIAL is the plain running XOR, phase[k] = phase[k-1] XOR bit[k], seeded at 0.
// ⚠ WHEN THE FRAME HAS ODD PARITY THE WHOLE SEQUENCE INVERTS ON EACH LOOP, so one bit per
// frame — the first — decodes wrong at the wrap. That is not a defect to seed around: no
// seed can fix it (the loop needs phase[last] == phase[-1], which forces even parity), and
// a real T5577 does the same thing, clocking its blocks through a free-running PSK
// modulator that is not reset at the frame boundary. It is C14 in the 64-bit path, where
// odd parity inverts the subcarrier every frame, and the readers already cope.
//
// ⇒ Verify a change here by DECODING it, never by reading it. `lf indala read` on a
// second device is the check; research/indala-psk-read/ctest runs the same decoder on
// the host.
//
// The initial "last bit" used for the first transition check in DIRECT mode is the LSB of
// the packed frame, i.e. the final bit of the frame in transmission order. This makes the
// wrap from the last to the first PWM entry of the looped sequence produce the same phase
// relation a continuously-emitting passive tag would present to the reader.
//
// Parameters:
//   frame_bytes   - frame bytes, MSB first, bit 0 of byte 0 is the first bit on air
//   bit_count     - number of bits to emit (64 for Indala26/IDTECK, 224 for Indala224)
//   mode          - PSK1 (direct) or PSK2 (differential)
//   out_buf       - destination buffer for wave_form entries
//   out_capacity  - number of entries available in out_buf
//
// Returns the number of entries written (one per bit), or 0 if out_buf is too small.
size_t lf_psk1_build_sequence(const uint8_t *frame_bytes,
                              size_t bit_count,
                              lf_psk1_phase_mode_t mode,
                              nrf_pwm_values_wave_form_t *out_buf,
                              size_t out_capacity);

// ⭐ ONE PWM BUFFER FOR ALL PSK1 TAG TYPES, because only one tag is ever emulated at a
// time. lf_tag_em.c calls a protocol's modulator, keeps the returned sequence pointer in
// m_pwm_seq and frees the codec immediately, so the buffer has to outlive the codec but
// never has to coexist with another protocol's.
//
// ⚠ 224 entries x 8 bytes is 1792 bytes, which is small enough that the longest frame can
// simply be the size. That is a recent change: one entry per subcarrier cycle would have
// made this 28KB, against the 8KB the 64-bit-only version used.
//
// Returns a sequence ready for nrfx_pwm_simple_playback, or NULL if the frame could not
// be built. `frame8` is 8 bytes, MSB first on air.
const nrf_pwm_sequence_t *lf_psk1_rf32_modulator(const uint8_t *frame8);

// The general form: any frame length up to LF_PSK1_MAX_FRAME_BITS, either modulation.
const nrf_pwm_sequence_t *lf_psk1_modulator(const uint8_t *frame,
                                            size_t bit_count,
                                            lf_psk1_phase_mode_t mode);
