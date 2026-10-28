#include "fsktrim.h"

uint32_t lf_fsk_trim_limit(uint64_t nominal_ticks) {
    return (uint32_t)(nominal_ticks * LF_FSK_TRIM_MAX_PPM / 1000000u);
}

static uint16_t lf_fsk_ch1(uint16_t ch0, uint8_t drive) {
    switch (drive) {
        case LF_FSK_DRIVE_U1_SAME:
        case LF_FSK_DRIVE_U1_IDLEHI:
            return ch0;
        case LF_FSK_DRIVE_U1_INV:
            return (uint16_t)(ch0 | 0x8000u);  // bit 15: the period starts on the opposite edge
        default:
            return 0;  // channel 1 has no pin in this mode
    }
}

int32_t lf_fsk_prepare(nrf_pwm_values_wave_form_t *v, size_t k, int32_t trim, uint8_t drive,
                       uint32_t *frame_ticks) {
    uint64_t nominal = 0;
    for (size_t i = 0; i < k; i++) {
        // nominal = nearest whole carrier cycle; a trimmed period is off by at most a few ticks
        uint16_t top = (uint16_t)(((v[i].counter_top + LF_FSK_TICKS_PER_CYCLE / 2) / LF_FSK_TICKS_PER_CYCLE) *
                                  LF_FSK_TICKS_PER_CYCLE);
        v[i].counter_top = top;
        v[i].channel_0 = top / 2;
        v[i].channel_1 = lf_fsk_ch1(top / 2, drive);
        nominal += top;
    }
    int32_t lim = (int32_t)lf_fsk_trim_limit(nominal);
    if (trim > lim) {
        trim = lim;
    } else if (trim < -lim) {
        trim = -lim;
    }
    uint32_t n = (uint32_t)(trim < 0 ? -trim : trim);
    if (k != 0 && n != 0) {
        // in proportion to each period's length (exp/fsk-selftrim-wide), so every edge stays within a tick of the
        // whole waveform scaled; per period index gave fc/8 and fc/10 the same ticks, which at thousands of ppm
        // (several ticks a period) puts runs of one tone off the reader's carrier
        uint32_t prev = 0;
        uint64_t upto = 0;
        for (size_t i = 0; i < k; i++) {
            upto += v[i].counter_top;  // still the nominal top (the loop above), until this period's adj below
            uint32_t next = (uint32_t)((upto * n) / nominal);
            uint16_t adj = (uint16_t)(next - prev);
            prev = next;
            v[i].counter_top = (uint16_t)(trim > 0 ? v[i].counter_top - adj : v[i].counter_top + adj);
        }
    }
    if (frame_ticks != NULL) {
        *frame_ticks = (uint32_t)(trim > 0 ? nominal - n : nominal + n);
    }
    return trim;
}

bool g_lf_fsk_exact_bits = false;

void lf_fsk_emit_bit(nrf_pwm_values_wave_form_t *v, uint16_t *k, lf_fsk_bitclock_t *c, uint16_t top, uint16_t loops,
                     uint16_t bit_cycles) {
    const uint32_t target = (c->bits + 1) * bit_cycles;
    for (uint16_t n = 0; g_lf_fsk_exact_bits ? c->cycles + top / 2 < target : n < loops; n++) {
        v[*k].channel_0 = top * LF_FSK_TICKS_PER_CYCLE / 2;
        v[*k].counter_top = top * LF_FSK_TICKS_PER_CYCLE;
        (*k)++;
        c->cycles += top;
    }
    c->bits++;
}
