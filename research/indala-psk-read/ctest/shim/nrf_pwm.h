#pragma once
/*
 * Minimal stand-in for the nRF5 SDK's nrf_pwm.h, so the SHIPPING emulation encoder
 * (firmware/application/src/rfid/nfctag/lf/utils/psk1.c) host-compiles unchanged.
 *
 * ⛔ Only the two types and one macro psk1.c actually touches. If it ever needs more of
 * the SDK than this, that is a signal the encoder has grown a hardware dependency and the
 * round trip below can no longer test the real thing — which would be worth knowing.
 */
#include <stdint.h>

typedef struct {
    uint16_t channel_0;
    uint16_t channel_1;
    uint16_t channel_2;
    uint16_t counter_top;
} nrf_pwm_values_wave_form_t;

typedef struct {
    union {
        nrf_pwm_values_wave_form_t const *p_wave_form;
    } values;
    uint16_t length;
    uint32_t repeats;
    uint32_t end_delay;
} nrf_pwm_sequence_t;

/* Parenthesised exactly as the SDK's is; the division by uint16_t across a struct
 * array is deliberate — the peripheral counts 16-bit fields, four per entry. */
#define NRF_PWM_VALUES_LENGTH(array) (sizeof(array) / (sizeof(uint16_t)))
