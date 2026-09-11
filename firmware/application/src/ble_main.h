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

void register_lf_adc_callback(lf_adc_callback_t cb);
void unregister_lf_adc_callback(void);

#endif
