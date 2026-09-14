#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nrf_pwm.h"

/* ⭐⭐⭐ FSK2a TAG EMULATION — ONE CONSTANT `counter_top`, THE FREQUENCY CARRIED BY THE DUTY
 * PATTERN. This exists because the obvious encoding does not reach the air (C382, C383).
 *
 * ⛔ WHAT WENT WRONG WITH THE OBVIOUS ENCODING. Each protocol used to spend ONE PWM entry per
 * tone period and vary `counter_top` between 8 and 10 to carry the frequency. That is what the
 * peripheral is documented to support in WaveForm mode, and it round-trips through our own
 * decoder exactly — but it does not happen. Captured off the air with the Flipper's raw reader:
 *
 *     HID Prox   2257 periods in the RF/8 band, ZERO in the RF/10 band
 *     AWID       3620 periods in the RF/8 band, TWO  in the RF/10 band
 *
 * The emission is a CONSTANT TONE, so an FSK decoder has no data to find — which is exactly the
 * `Protocol: not found` both captures returned while an ASK control decoded byte-exact from the
 * same pipeline. ⭐ Confirmed by forcing both tones to `counter_top` 10 and re-capturing: the
 * whole emission moved to ~79us (RF/10 periods 0 -> 2640). ⇒ The peripheral plays the sequence
 * at ONE period. A `counter_top` that VARIES within a sequence is not applied per entry.
 *
 * ⭐ That also unifies the emulate column: every emitter on this device that WORKS uses a single
 * constant `counter_top` — Gallagher 32, Securakey 40, Noralsy 32, GProxII, em410x 64, and every
 * PSK1 type through psk1.c. The FSK ones were the only ones that varied it.
 *
 * ⇒ SO KEEP IT CONSTANT AND SPEND SEVERAL ENTRIES PER TONE, switching the duty instead:
 *
 *     RF/8  tone = 4 entries of 16 ticks   ON, ON, OFF, OFF            mark 32us, gap 32us
 *     RF/10 tone = 5 entries of 16 ticks   ON, ON, OFF, OFF, OFF       mark 32us, gap 48us
 *
 * ⛔ THE BASE CLOCK MUST BE 1MHz FOR THIS TO BE LEGAL, and that is not a free choice (C385).
 * COUNTERTOP in WaveForm mode has a MINIMUM VALID VALUE OF 3 (Nordic PS; the same note is in
 * psk1.h). At the 125kHz clock these types used to run at, one tick is one carrier cycle, the
 * tone periods are 8 and 10 ticks, and their gcd is 2 — below the minimum, so no constant value
 * exists. At 1MHz a carrier cycle is 8 ticks, the tones are 64 and 80 ticks, and gcd is 16.
 * ⇒ `IS_1MHZ_PWM_TYPE` in tag_base_type.h must include every type that uses this module.
 *
 * ⭐ THE MARK FALLS OUT CORRECT FOR FREE. C226 measured a real FSK2a emission: the HIGH run is a
 * fixed 4 carrier cycles on every tone and only the gap varies. 4 cycles = 32us = exactly 2
 * entries of 16, for both tones. C380 is why that matters — it is a shape defect no round trip
 * can see, because a demodulator only ever looks at the tone's PERIOD.
 *
 * ⚠ COST: ~25 entries per bit against the old 5-6, so a 96-bit frame needs 2,400 entries. The
 * buffer is SHARED for that reason — see the note on LF_FSK2A_MAX_ENTRIES. */

/* One carrier cycle at the 125kHz carrier, in 1MHz PWM ticks. */
#define LF_FSK2A_TICKS_PER_CYCLE   (8u)

/* ⭐ The measured fixed mark (C226/C380), in carrier cycles. Not half the period. */
#define LF_FSK2A_MARK_CYCLES       (4u)

/* ⭐⭐ ONE SHARED BUFFER, NOT ONE PER PROTOCOL — the RAM budget decides this (C384).
 *
 * At 8 bytes per entry a private 2,400-entry buffer per protocol costs 19,200 B each. Measured
 * off `objects/application.map`, this part has 91,748 B of RAM left for heap and stack, so three
 * private buffers would take 64% of everything remaining. That is refused.
 *
 * ⭐ Sharing is SAFE rather than merely cheap: `lf_tag_data_loadcb_inner()` sets exactly one
 * `m_tag_type` and one `m_pwm_seq`, so two FSK protocols can never be loaded at once. F5 is the
 * precedent — it fixed this same shape, two 28 KB capture buffers resident simultaneously.
 *
 * ⚠ Sized for a 96-bit frame of ALL ONES, the worst case: 96 x 5 tones x 5 entries = 2,400. A
 * frame of all zeros is 96 x 6 x 4 = 2,304, so the length is set per call, not once. */
#define LF_FSK2A_MAX_FRAME_BITS    (96u)
#define LF_FSK2A_MAX_ENTRIES       (2400u)

typedef struct {
    uint16_t counter_top;    /* ticks per entry — the gcd of the two tone periods */
    uint8_t  short_cycles;   /* carrier cycles in the SHORT tone (the 0 bit's tone) */
    uint8_t  long_cycles;    /* ...and in the LONG one */
    uint8_t  short_pulses;   /* how many short tones make a 0 bit */
    uint8_t  long_pulses;    /* how many long tones make a 1 bit */
} lf_fsk2a_params_t;

/** @brief Build the FSK2a PWM sequence for one frame.
 *  @param p      tone geometry; `counter_top` must divide both tone periods in ticks
 *  @param frame  the on-air frame, MSB first, preamble included
 *  @param nbits  frame length in bits, at most LF_FSK2A_MAX_FRAME_BITS
 *  @return the sequence, or NULL if the parameters do not fit the shared buffer.
 *
 *  ⚠ Returns NULL rather than truncating. A short buffer that silently emits a partial frame is
 *  the failure mode U18 spent a whole unit on — it looks exactly like a dead emitter.
 */
const nrf_pwm_sequence_t *lf_fsk2a_build(const lf_fsk2a_params_t *p,
                                         const uint8_t *frame, uint16_t nbits);
