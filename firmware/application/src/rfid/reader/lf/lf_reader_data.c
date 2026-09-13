#include "lf_reader_data.h"

#include "ble_main.h"
#include "nrfx_timer.h"

RIO_CALLBACK_S RIO_callback;
extern nrfx_timer_t m_pwm_timer_counter;

// Register recovery function
void register_rio_callback(RIO_CALLBACK_S P) {
    RIO_callback = P;
}

void unregister_rio_callback(void) {
    RIO_callback = NULL;
}

// GPIO interrupt is the RIO pin
void gpio_int0_irq_handler(void) {
    if (RIO_callback != NULL) {
        RIO_callback();
    }
}

// Get the value of the counter
uint32_t get_lf_counter_value(void) {
    return nrfx_timer_capture(&m_pwm_timer_counter, NRF_TIMER_CC_CHANNEL1);
}

// Clear the value of the counter
void clear_lf_counter_value(void) { nrfx_timer_clear(&m_pwm_timer_counter); }

/* ⭐ A SWITCH, SO C47 CAN BE MEASURED RATHER THAN ASSUMED. Setting this to 0 builds the
 * firmware exactly as it was before the guard existed, so the same tag can be read with and
 * without it in one session — the paired test C47 has never had. ⛔ Ship it at 1. */
/* ⛔⛔ PROBE BUILD — THIS IS 0 AND MUST NOT STAY 0. The paired test the comment above
 * asks for is running right now: C248 measured the guard-ON arm at 48/48 on the rig-B
 * T5577, and this build is its OFF arm on the same tag in the same session. The very
 * next commit sets it back to 1. If you are reading this on a checkout that is not
 * mid-probe, the restore was lost — put it back. */
#define LF_ADV_GUARD_ENABLED 0

void lf_adv_suspend(lf_adv_guard_t *guard) {
    guard->paused = false;
#if LF_ADV_GUARD_ENABLED
    if (!g_is_ble_connected) {
        advertising_stop();
        guard->paused = true;
    }
#endif
}

void lf_adv_resume(lf_adv_guard_t *guard) {
    if (guard->paused) {
        advertising_start(false);
        guard->paused = false;
    }
}
