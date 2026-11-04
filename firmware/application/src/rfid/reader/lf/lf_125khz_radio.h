#pragma once

#include "ble_main.h"
#include "nrfx_pwm.h"

/* Exposed so lf_gap.c can stop the PWM and drive LF_ANT_DRIVER directly
 * to create clean field gaps without relying on PWM pin release state. */
extern nrfx_pwm_t m_pwm;

void lf_125khz_radio_init(void);
void lf_125khz_radio_uninit(void);

void lf_125khz_radio_saadc_enable(lf_adc_callback_t cb);
void lf_125khz_radio_gpiote_enable(void);
void lf_125khz_radio_saadc_disable(void);

/* Where the next SAADC capture samples. The phase is 0..127 ticks of 62.5ns after the
 * carrier period starts (0 = the period boundary). A rate in kHz (1..200) samples on its
 * own clock instead of the carrier's, and 0 restores carrier-locked sampling. The caller
 * resets both to 0 after its capture. */
void lf_125khz_radio_saadc_phase_set(uint8_t ticks);
void lf_125khz_radio_saadc_rate_set(uint8_t khz);
void lf_125khz_radio_gpiote_disable(void);

void start_lf_125khz_radio(void);
void stop_lf_125khz_radio(void);
