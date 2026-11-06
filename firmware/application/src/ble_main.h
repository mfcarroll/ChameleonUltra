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
bool advertising_pause(void);
void advertising_resume(void);
void delete_bonds_all(void);
void nus_data_response(uint8_t *p_data, uint16_t length);
bool is_nus_working(void);
void set_ble_connect_key(uint8_t *key);

/* Settings for the next register_lf_adc_callback(). The gain divisor is 1..6, and anything
 * else selects the default 1/6. A fast acquisition (3us) allows sampling above 143kHz. */
void lf_adc_set_gain(uint8_t divisor);
void lf_adc_set_acq_fast(bool fast);
/* The analog input for the next register_lf_adc_callback(): AIN5 (LF_OA_OUT, the reader) unless set. */
void lf_adc_set_input(nrf_saadc_input_t input);
void register_lf_adc_callback(lf_adc_callback_t cb);
void unregister_lf_adc_callback(void);

#endif
