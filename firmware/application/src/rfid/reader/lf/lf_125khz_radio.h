#pragma once

#include "ble_main.h"
#include "nrfx_pwm.h"

/* Exposed so lf_gap.c can stop the PWM and drive LF_ANT_DRIVER directly
 * to create clean field gaps without relying on PWM pin release state. */
extern nrfx_pwm_t m_pwm;

void lf_125khz_radio_init(void);
void lf_125khz_radio_uninit(void);

/* ⭐ SAMPLE PHASE, in 62.5ns TIMER ticks after the carrier period boundary.
 *
 * By default the SAADC sample task is PPI-triggered straight from the carrier PWM's
 * PWMPERIODEND, so it samples once per 8us carrier cycle at ONE FIXED PHASE. That is
 * fine for subcarriers well below Nyquist, but a tag's fc/2 subcarrier (62.5kHz, e.g.
 * Indala PSK1) lands at exactly 2 samples/cycle, and because a T5577 derives it by
 * dividing the same field the reader generates, it is phase-LOCKED to that trigger.
 * The recovered amplitude is then proportional to cos(phi) for a constant phi, so an
 * unlucky phi nulls the subcarrier to any depth, on every read, forever.
 *
 * Setting a phase reroutes the trigger through TIMER3: PWMPERIODEND clears it, and its
 * COMPARE[0] at `ticks` fires the sample. 128 ticks span one carrier period, so ticks
 * covers 0-360 degrees of the carrier and 0-720 of an fc/2 subcarrier.
 *
 * @param ticks 0 restores the direct PWMPERIODEND trigger (default, byte-compatible).
 *              1..127 samples that many 62.5ns ticks after the period boundary.
 */
void lf_125khz_radio_saadc_phase_set(uint8_t ticks);

/* ⭐ OVERSAMPLE, decoupling the ADC from the carrier entirely.
 *
 * The default trigger fires once per carrier period, so a tag's fc/2 subcarrier lands at
 * exactly 2 samples/cycle — Nyquist — where recovery depends on sampling phase. Setting a
 * rate instead free-runs TIMER3 at that frequency with a compare-clear short, so sampling
 * is ASYNCHRONOUS to the field and fc/2 is comfortably oversampled: no degeneracy to
 * phase-tune around, and the distinction between "attenuated" and "nulled by the sampler"
 * stops mattering.
 *
 * ⚠ Rates above ~143kHz also need lf_adc_set_acq_fast(true); the enable path does that.
 * The nRF52840 SAADC ceiling with a 3us acquisition is about 200kHz, so do not ask for
 * more — the conversions would simply not keep up with the trigger.
 *
 * @param khz 0 restores the carrier-locked trigger (default). Otherwise the free-running
 *            sample rate in kHz; 200 is the practical maximum.
 */
void lf_125khz_radio_saadc_rate_set(uint16_t khz);

/* Reader field strength as the PWM mark against a top_value of 8: 1..7, stock 4 (50%).
 * Exists so C140's saturation can be reduced without moving the tag — see lf_125khz_radio.c. */
void lf_125khz_radio_drive_set(uint8_t duty);
uint8_t lf_125khz_radio_drive_get(void);

/* ⛔ Instrumentation for C148 — the drive control going inert mid-session. 16 bytes.
 * Remove with `hw emudebug` before upstreaming. See the note in lf_125khz_radio.c. */
#define LF_RADIO_DEBUG_SIZE 20
void lf_125khz_radio_debug_get(uint8_t *out);

void lf_125khz_radio_saadc_enable(lf_adc_callback_t cb);
void lf_125khz_radio_gpiote_enable(void);
void lf_125khz_radio_saadc_disable(void);
void lf_125khz_radio_gpiote_disable(void);

void start_lf_125khz_radio(void);
void stop_lf_125khz_radio(void);
