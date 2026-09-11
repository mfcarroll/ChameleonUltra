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

void register_lf_adc_callback(lf_adc_callback_t cb);
void unregister_lf_adc_callback(void);

#endif
