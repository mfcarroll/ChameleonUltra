#ifndef BLE_MAIN_H
#define BLE_MAIN_H

#include "ble_bas.h"
#include "ble_gatts.h"
#include "ble_nus.h"
#include "nrfx_saadc.h"

extern uint16_t batt_lvl_in_milli_volts;
extern uint8_t percentage_batt_lvl;

typedef void (*lf_adc_callback_t)(nrf_saadc_value_t *, size_t);

void ble_slave_init(void);
/* Defined in ble_main.c. Declared here because it was previously reachable only by
 * re-declaring it locally, which is how a definition drifts out of step with its users. */
extern volatile bool g_is_ble_connected;

void advertising_start(bool erase_bonds);
void advertising_stop(void);
void delete_bonds_all(void);
void nus_data_response(uint8_t *p_data, uint16_t length);
bool is_nus_working(void);
void set_ble_connect_key(uint8_t *key);

/* Shorten the SAADC acquisition window so a faster sample trigger can be serviced.
 * Conversion takes roughly acq_time + 2us, so the default 5us caps the usable rate at
 * about 143kHz — fine for the carrier-locked 125kHz trigger, not for oversampling.
 * 3us brings the ceiling to ~200kHz. The LF source is an op-amp output, so the shorter
 * window costs little settling accuracy. Call BEFORE register_lf_adc_callback(). */
void lf_adc_set_acq_fast(bool fast);

/* SAADC input gain for the LF channel, as the DIVISOR: 6 is the default (GAIN1_6).
 *
 * ⭐ WHY IT IS WORTH A KNOB. With gain 1/6 against the 0.6V internal reference, full
 * scale is 3.6V and one 14-bit count is 220uV. The LF signal sits on ~1.2V of LF_VBIAS
 * DC, and the fc/2 subcarrier measures ~17 counts — about 1/3000th of the range. The DC
 * is consuming the headroom, so most of the converter is spent representing a constant.
 *
 * ⚠ SINGLE-ENDED, THE DC SETS THE CEILING. 1.2V against full scale means divisors below
 * 3 clip: 1/3 gives 1.8V full scale (DC at 67%), 1/2 gives 1.2V and saturates. Going
 * further needs differential mode against LF_RSSI to subtract the DC first.
 *
 * ⇒ What this knob is FOR is one measurement: whether the empty-field noise floor scales
 * with gain. If it does, the floor is analog and gain buys nothing. If it does not, the
 * floor is ADC-referred and gain is real SNR. Call BEFORE register_lf_adc_callback().
 */
void lf_adc_set_gain(uint8_t divisor);

/* SAADC input node for the LF channel: 5 = AIN5 (default), 0 = AIN0.
 *
 * ⭐ WHY IT IS WORTH A KNOB — it is the only way to get UPSTREAM of the filter poles.
 *
 *     ANT -> VD1 detector -> LF_OA -> [C28 10n / R9 82 / C36 33n] -> IC1A (R17 4k7 / C38 1n)
 *                              |                                      -> IC1B -> AIN5 (P0.29)
 *                              \-> R12 470k -> LF_RSSI -> AIN0 (P0.02)
 *
 * AIN5 sits after BOTH RC poles (R9/C36 = 58.8kHz, R17/C38 = 33.9kHz). At an fc/2 = 62.5kHz
 * subcarrier those cost -3.3dB and -6.4dB, so ~9.7dB of the measured deficit is spent
 * before the converter ever sees the signal. AIN0 hangs off LF_OA itself, ahead of both.
 *
 * ⚠ Whether that is RECOVERABLE depends on where the noise is made. The poles attenuate
 * upstream noise along with the signal, so they only cost SNR for noise added at or after
 * IC1A. Measuring AIN0 against AIN5 on the same tag is what separates the two.
 *
 * ⚠ 470k IS FAR ABOVE WHAT THE SAADC WANTS. Nordic's table asks ~20-40us of acquisition at
 * this source impedance; a 125kHz carrier-locked trigger has 8us of period, so 5us is the
 * ceiling and the sample cap cannot fully charge. Each conversion becomes a one-pole IIR of
 * the input rather than a sample of it, which attenuates fc/2 hardest of all. That
 * attenuation hits signal and floor alike, so the fc/2-to-floor RATIO is still the number
 * to read — but a dead result here is a real finding, not a null. Call BEFORE
 * register_lf_adc_callback(). */
void lf_adc_set_input(uint8_t ain);

void register_lf_adc_callback(lf_adc_callback_t cb);
void unregister_lf_adc_callback(void);

#endif
