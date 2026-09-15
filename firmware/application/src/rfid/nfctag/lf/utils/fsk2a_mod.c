#include "fsk2a_mod.h"

/* ⭐ See fsk2a_mod.h for WHY this shape — the varying-`counter_top` encoding it replaces does
 * not reach the air (C382/C383), and the 1MHz base clock is what makes a constant one legal
 * (C385). */

static nrf_pwm_values_wave_form_t m_fsk2a_vals[LF_FSK2A_MAX_ENTRIES] = {};

static nrf_pwm_sequence_t m_fsk2a_seq = {
    .values.p_wave_form = m_fsk2a_vals,
    .length = NRF_PWM_VALUES_LENGTH(m_fsk2a_vals),
    .repeats = 0,
    .end_delay = 0,
};

/* One entry. `on` is a full-height mark for the whole entry, `off` is silence for the whole
 * entry — the two duty extremes, never an intermediate.
 *
 * ⚠ The nRF PWM compares CH0 against the counter and drives the pin HIGH until the compare,
 * then LOW (polarity bit 15 clear, which is what every emitter here relies on). So a compare
 * equal to `counter_top` never falls within the period and holds HIGH, and a compare of 0
 * falls immediately and holds LOW. Those are the only two values this module ever writes. */
static void put(uint16_t *n, uint16_t counter_top, bool on) {
    m_fsk2a_vals[*n].channel_0 = on ? counter_top : 0u;
    m_fsk2a_vals[*n].channel_1 = 0;
    m_fsk2a_vals[*n].channel_2 = 0;
    m_fsk2a_vals[*n].counter_top = counter_top;
    (*n)++;
}

const nrf_pwm_sequence_t *lf_fsk2a_build(const lf_fsk2a_params_t *p,
                                         const uint8_t *frame, uint16_t nbits) {
    if (p == NULL || frame == NULL || p->counter_top == 0u) {
        return NULL;
    }
    if (nbits == 0u || nbits > LF_FSK2A_MAX_FRAME_BITS) {
        return NULL;
    }

    const uint16_t short_ticks = (uint16_t)(p->short_cycles * LF_FSK2A_TICKS_PER_CYCLE);
    const uint16_t long_ticks  = (uint16_t)(p->long_cycles * LF_FSK2A_TICKS_PER_CYCLE);
    const uint16_t mark_ticks  = (uint16_t)(p->mark_cycles * LF_FSK2A_TICKS_PER_CYCLE);

    /* ⛔ The whole scheme rests on `counter_top` dividing BOTH tone periods and the mark. If it
     * does not, the tone this emits is not the tone asked for — refuse rather than emit a
     * plausible wrong frequency, which is the failure this module exists to end. */
    if ((short_ticks % p->counter_top) != 0u || (long_ticks % p->counter_top) != 0u ||
        (mark_ticks % p->counter_top) != 0u) {
        return NULL;
    }
    /* Nordic PS: COUNTERTOP in WaveForm mode has a minimum valid value of 3 (C385). */
    if (p->counter_top < 3u) {
        return NULL;
    }

    const uint16_t short_entries = (uint16_t)(short_ticks / p->counter_top);
    const uint16_t long_entries  = (uint16_t)(long_ticks / p->counter_top);
    const uint16_t mark_entries  = (uint16_t)(mark_ticks / p->counter_top);

    /* Refuse up front rather than discovering the overflow partway through a frame. */
    const uint32_t worst = (uint32_t)nbits *
                           (uint32_t)((p->short_pulses * short_entries) > (p->long_pulses * long_entries)
                                      ? (p->short_pulses * short_entries)
                                      : (p->long_pulses * long_entries));
    if (worst > LF_FSK2A_MAX_ENTRIES) {
        return NULL;
    }

    uint16_t n = 0;
    for (uint16_t i = 0; i < nbits; i++) {
        const bool bit = (frame[i / 8u] >> (7u - (i % 8u))) & 1u;
        /* ⚠ WHICH TONE IS WHICH IS NOT A GUESS. The shipping decoder calls the LONG period
         * pulse 1 and the SHORT one pulse 0, so a 1 bit is the long tone. Inverting this
         * produces a frame whose every bit is flipped, which a preamble search fails silently
         * rather than flagging — the mode C160 paid for on Keri. */
        const uint16_t per_tone = bit ? long_entries : short_entries;
        const uint8_t  pulses   = bit ? p->long_pulses : p->short_pulses;
        for (uint8_t t = 0; t < pulses; t++) {
            for (uint16_t e = 0; e < per_tone; e++) {
                put(&n, p->counter_top, e < mark_entries);
            }
        }
    }
    m_fsk2a_seq.length = (uint16_t)(n * 4u);   /* NRF_PWM_VALUES_LENGTH counts uint16s */
    return &m_fsk2a_seq;
}
