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

// A standard 64-bit PSK1/RF-32 frame, which is what every PSK1 tag type on this
// device transmits (Indala and IDTECK are identical at this layer — same T5577
// config word, same bit rate, same subcarrier, same frame length; only the
// preamble and payload interpretation differ).
#define LF_PSK1_RF32_FRAME_BITS   (64)
#define LF_PSK1_RF32_PWM_ENTRIES  (LF_PSK1_RF32_FRAME_BITS * LF_PSK1_RF32_SUBCYCLES_PER_BIT)

// Build a PSK1 wave-form PWM sequence for a frame transmitted MSB first.
//
// Each bit spans LF_PSK1_RF32_SUBCYCLES_PER_BIT PWM entries (one subcarrier
// cycle per entry). The implementation toggles the channel_0 polarity MSB on every
// bit CHANGE, which looks differential but is not.
//
// ⛔ THIS IS PSK1 AND THE TOGGLE FORM IS CORRECT — DO NOT "FIX" IT. PSK1 is NOT
// differential: the subcarrier phase IS the data, bit 1 one phase and bit 0 the other.
// Differential is PSK2. Confusing the two is not hypothetical — this project spent a
// fortnight on a reader that demodulated PSK2 against a PSK1 tag, and the wrong verdict
// survived because its unit test encoded with the same mistake it decoded with.
//
// The toggle form here telescopes to the non-differential answer:
//     phase[k] = phase[k-1] XOR (bit[k] XOR bit[k-1])  =  bit[k] XOR bit[N-1]
// which is the data, inverted or not depending on the frame's last bit. A global
// inversion is unobservable — the absolute subcarrier phase is not knowable, which is
// why every reader searches both polarities.
//
// ⇒ Verify a change here by DECODING it, never by reading it. `lf indala read` on a
// second device is the check; research/indala-psk-read/ctest runs the same decoder on
// the host.
//
// The initial "last bit" used for the first transition check is the LSB of
// the packed frame, i.e. the final bit of the frame in transmission order.
// This makes the wrap from the last to the first PWM entry of the looped
// sequence produce the same phase relation a continuously-emitting passive
// tag would present to the reader.
//
// Parameters:
//   frame_bytes   - frame bytes, MSB first, bit 0 of byte 0 is the first bit on air
//   bit_count     - number of bits to emit (e.g. 64 for standard IDTECK)
//   out_buf       - destination buffer for wave_form entries
//   out_capacity  - number of entries available in out_buf
//
// Returns the number of entries written, or 0 if out_buf is too small for
// (bit_count * LF_PSK1_RF32_SUBCYCLES_PER_BIT) entries.
size_t lf_psk1_build_sequence(const uint8_t *frame_bytes,
                              size_t bit_count,
                              nrf_pwm_values_wave_form_t *out_buf,
                              size_t out_capacity);

// ⭐ ONE 8KB PWM BUFFER FOR ALL 64-BIT PSK1 TAG TYPES, because only one tag is ever
// emulated at a time. lf_tag_em.c calls a protocol's modulator, keeps the returned
// sequence pointer in m_pwm_seq and frees the codec immediately, so the buffer has to
// outlive the codec but never has to coexist with another protocol's.
//
// ⚠ 1024 entries x 8 bytes is 8KB, which is not small on this part — `lf indala read`
// alone holds 8KB of samples, 8KB of scratch and two 16KB accumulators. Giving each
// PSK1 protocol its own identical buffer would cost another 8KB per protocol for no
// benefit, which is why this lives here rather than in each protocols/*.c.
//
// Returns a sequence ready for nrfx_pwm_simple_playback, or NULL if the frame could not
// be built. `frame8` is 8 bytes, MSB first on air.
const nrf_pwm_sequence_t *lf_psk1_rf32_modulator(const uint8_t *frame8);
