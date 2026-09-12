#include "psk1.h"

// Extract bit at index `bit_idx` from a MSB-first bit stream stored in
// frame_bytes. bit_idx=0 is the MSB of frame_bytes[0].
static inline bool read_bit_msb_first(const uint8_t *frame_bytes, size_t bit_idx) {
    return (frame_bytes[bit_idx / 8] >> (7 - (bit_idx % 8))) & 1U;
}

// Extract the last bit of the frame (LSB of the last byte that contains a
// bit). For frames whose bit_count is not a multiple of 8 this still points
// at the final transmitted bit.
static inline bool read_last_bit(const uint8_t *frame_bytes, size_t bit_count) {
    size_t last_idx = bit_count - 1;
    return read_bit_msb_first(frame_bytes, last_idx);
}

// One PWM entry: the subcarrier polarity in the top bit of channel_0, one 16us period.
static inline void emit_entry(nrf_pwm_values_wave_form_t *e, bool phase) {
    e->channel_0 = (phase ? (1U << 15) : 0) | LF_PSK1_SUBCARRIER_DUTY;
    e->channel_1 = 0;
    e->channel_2 = 0;
    e->counter_top = LF_PSK1_SUBCARRIER_TOP;
}

size_t lf_psk1_build_sequence(const uint8_t *frame_bytes,
                              size_t bit_count,
                              lf_psk1_phase_mode_t mode,
                              nrf_pwm_values_wave_form_t *out_buf,
                              size_t out_capacity) {
    if (bit_count == 0 || bit_count > out_capacity) {
        return 0;
    }

    bool phase = false;
    // ⛔ DIRECT seeds from the frame's LAST bit so the loop wrap is continuous; the
    // telescoping identity in psk1.h depends on it. DIFFERENTIAL has no such seed to
    // choose — see the odd-parity note there.
    bool last_bit = read_last_bit(frame_bytes, bit_count);

    for (size_t bit_idx = 0; bit_idx < bit_count; bit_idx++) {
        bool cur_bit = read_bit_msb_first(frame_bytes, bit_idx);
        if (mode == LF_PSK1_PHASE_DIFFERENTIAL) {
            if (cur_bit) {
                phase = !phase;
            }
        } else {
            if (cur_bit != last_bit) {
                phase = !phase;
            }
            last_bit = cur_bit;
        }

        // ⭐ ONE ENTRY PER BIT. The sequence's `repeats` holds it for
        // LF_PSK1_RF32_SUBCYCLES_PER_BIT PWM periods — see psk1.h.
        emit_entry(&out_buf[bit_idx], phase);
    }

    /* ⛔⛔ AN ODD-PARITY PSK2 FRAME DOES NOT REPEAT AT THE FRAME PERIOD — IT REPEATS AT
     * TWICE IT, AND A BUFFER HOLDING ONE COPY TRANSMITS SOMETHING NO READER WANTS.
     *
     * `phase` here is the running XOR of every data bit. If it has not returned to its
     * starting value the frame has odd parity, so a real tag — whose modulator is never
     * reset at the frame boundary — emits the whole next frame INVERTED. The PWM replays
     * one buffer verbatim, so a single-frame buffer instead jumps back to the starting
     * phase: a discontinuity once per frame that is not in the data.
     *
     * ⚠ IT DOES NOT MERELY COST ONE BIT. Momentum decoded such a buffer 6 of 6 — a
     * confident, stable, WRONG 224-bit credential, and a different wrong one for each
     * encoding tried (C152). The wrap error moves where the preamble search lands, so the
     * whole frame re-frames around it. ⇒ Appending the inverted copy is not a refinement,
     * it is the difference between emulating the credential and emulating a plausible
     * impostor.
     *
     * ⭐ The DIRECT (PSK1) mode needs none of this: its telescoping form is
     * phase[k] = bit[k] XOR bit[N-1], which is periodic at the frame period whatever the
     * parity. That is why Indala26 and IDTECK were right with a single copy. */
    if (mode == LF_PSK1_PHASE_DIFFERENTIAL && phase) {
        if (bit_count * 2u > out_capacity) {
            return 0;
        }
        for (size_t k = 0; k < bit_count; k++) {
            bool inverted = ((out_buf[k].channel_0 & (1U << 15)) == 0);
            emit_entry(&out_buf[bit_count + k], inverted);
        }
        return bit_count * 2u;
    }

    return bit_count;
}

// The shared PSK1/PSK2 sequence. See the note in psk1.h for why there is exactly one of
// these rather than one per protocol.
static nrf_pwm_values_wave_form_t m_psk1_vals[LF_PSK1_PWM_ENTRIES] = {};

static nrf_pwm_sequence_t m_psk1_seq = {
    .values.p_wave_form = m_psk1_vals,
    .length = NRF_PWM_VALUES_LENGTH(m_psk1_vals),
    .repeats = LF_PSK1_SEQ_REPEATS,
    .end_delay = 0,
};

const nrf_pwm_sequence_t *lf_psk1_modulator(const uint8_t *frame,
                                            size_t bit_count,
                                            lf_psk1_phase_mode_t mode) {
    size_t n = lf_psk1_build_sequence(frame, bit_count, mode,
                                      m_psk1_vals, LF_PSK1_PWM_ENTRIES);
    if (n == 0) {
        return NULL;
    }
    // 4 uint16 fields per wave-form entry, which is what nrf_pwm_sequence_t counts.
    m_psk1_seq.length = (uint16_t)(n * 4);
    return &m_psk1_seq;
}

const nrf_pwm_sequence_t *lf_psk1_rf32_modulator(const uint8_t *frame8) {
    return lf_psk1_modulator(frame8, LF_PSK1_RF32_FRAME_BITS, LF_PSK1_PHASE_DIRECT);
}
