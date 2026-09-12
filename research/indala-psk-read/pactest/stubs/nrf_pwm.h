#pragma once
/* Host stub: the decoder path needs none of this, but pac.c's modulator declares
 * these types, so the file will not compile without them. */
#include <stdint.h>
typedef struct { uint16_t channel_0, channel_1, channel_2, counter_top; } nrf_pwm_values_wave_form_t;
typedef struct { union { const nrf_pwm_values_wave_form_t *p_wave_form; } values;
                 uint16_t length; uint32_t repeats, end_delay; } nrf_pwm_sequence_t;
#define NRF_PWM_VALUES_LENGTH(a) (sizeof(a) / sizeof(uint16_t))
