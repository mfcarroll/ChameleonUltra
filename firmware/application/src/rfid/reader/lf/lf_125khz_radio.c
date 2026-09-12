#include "lf_125khz_radio.h"

#include "lf_reader_data.h"
#include "nrf_gpio.h"
#include "nrfx_clock.h"
#include "nrfx_gpiote.h"
#include "nrfx_ppi.h"
#include "nrfx_pwm.h"
#include "nrfx_saadc.h"
#include "nrfx_timer.h"
#include "rfid_main.h"

static bool m_reader_inited = false;

/* ⛔ C148 instrumentation — the value the PWM was ACTUALLY handed, snapshotted at the moment
 * playback starts. A readback after the fact is useless: the capture path deliberately restores
 * the stock drive when it finishes, so querying afterwards always reports the default no matter
 * what the capture ran at. That is not a bug, it is why the snapshot has to be here. */
static uint8_t  m_dbg_drive_at_start = 0xFF;
static uint32_t m_dbg_ptr_at_start = 0;
static uint16_t m_dbg_starts = 0;

nrfx_pwm_t m_pwm = NRFX_PWM_INSTANCE(0);
nrfx_timer_t m_pwm_timer_counter = NRFX_TIMER_INSTANCE(2);
nrf_ppi_channel_t m_pwm_saadc_sample_ppi_channel;
nrf_ppi_channel_t m_pwm_timer_count_ppi_channel;

/* Sample-phase plumbing -- see lf_125khz_radio_saadc_phase_set() in the header.
 * TIMER3 is enabled in sdk_config and used by nothing else. */
static nrfx_timer_t m_phase_timer = NRFX_TIMER_INSTANCE(3);
static nrf_ppi_channel_t m_pwm_phase_clear_ppi_channel;
static nrf_ppi_channel_t m_phase_saadc_sample_ppi_channel;
static uint8_t m_saadc_phase_ticks = 0;   /* 0 = direct PWMPERIODEND trigger */
static uint16_t m_saadc_rate_khz = 0;     /* 0 = carrier-locked; else free-run at this kHz */

/* One carrier period is 8us = 128 ticks of a 16MHz timer. */
#define LF_PHASE_TICKS_PER_PERIOD 128

/* ⭐ THE READER'S OWN FIELD STRENGTH, AND IT IS A DIAGNOSTIC KNOB.
 *
 * The PWM drives LF_ANT_DRIVER, which is the SELECT pin of the antenna's analog switch
 * (C134) — so this value is the mark/space of the square wave that swings the coil terminal
 * between GND and 3V3, against a top_value of 4. Stock is 2, a 50% duty.
 *
 * ⚠ The reason it is adjustable is C140: on a strongly coupled tag the LF op-amp chain
 * SATURATES, a third of every PAC capture is pinned at a rail, and the levels are destroyed
 * before the ADC ever sees them. No gain setting reaches that, because the clipping is
 * upstream of the ADC and 1/6 is already the lowest gain. Driving the field more weakly is
 * the one way to reduce the signal into the amplifier without moving the tag — an air gap
 * done electronically, and unlike an air gap it needs no hands.
 *
 * ⭐ MEASURED, and it is the largest single effect found on PAC: at the stock duty 37.3% of a
 * capture is pinned at the bottom rail and the best any decoder manages is 6 errors of 128;
 * one step weaker it is 25.9% pinned and **1 error**. That is why the range is 1..7 rather
 * than the 1..3 a top_value of 4 allowed — the trend was still improving at the edge of the
 * old range, so the knob needed more turns.
 *
 * ⚠ TOP_VALUE 8 AT 1MHz, not 4 at 500kHz. Same 125kHz carrier, twice the duty resolution.
 * Everything downstream keys off PWMPERIODEND — the sample trigger, the cycle counter and the
 * phase timer's CLEAR — and that still fires once per carrier period, so the extra resolution
 * costs nothing. ⛔ The phase timer is unaffected either way: it runs on TIMER3 at 16MHz where
 * one carrier period is 128 ticks regardless of how the PWM divides its own clock. */
#define LF_DRIVE_TOP          8
#define LF_DRIVE_DUTY_DEFAULT 4

// At present, only channel 1 is used, so only one channel can be configured
static nrf_pwm_values_individual_t m_lf_125khz_pwm_seq_val[] = {
    {LF_DRIVE_DUTY_DEFAULT, 0, 0, 0},
};

void lf_125khz_radio_drive_set(uint8_t duty) {
    if (duty < 1) {
        duty = 1;
    } else if (duty > LF_DRIVE_TOP - 1) {
        duty = LF_DRIVE_TOP - 1;
    }
    /* ⚠ Written into the sequence the PWM plays from, so it takes effect on the next
     * start_lf_125khz_radio(). Changing it mid-playback would shift the carrier phase
     * under a capture that is already running. */
    m_lf_125khz_pwm_seq_val[0].channel_0 = duty;
}

uint8_t lf_125khz_radio_drive_get(void) {
    return (uint8_t)m_lf_125khz_pwm_seq_val[0].channel_0;
}

/* ⭐ A TRAP FOR C148, AND THE POINT IS THE FORK IT SETTLES.
 *
 * The drive control works from a fresh boot and goes inert partway through a session; a reboot
 * restores it exactly. Every test so far has been behavioural — inferring the peripheral's
 * condition from a decode rate — which is how this session produced four wrong explanations
 * and sent the user to check a bench that was fine.
 *
 * ⇒ One readback splits the hypothesis space in two:
 *
 *   `drive` wrong        → the RAM the PWM reads by DMA is being CLOBBERED. `m_lf_125khz_pwm_seq_val`
 *                          is a static, and a write past the end of a neighbouring buffer would
 *                          rewrite it silently. Nothing about the peripheral is at fault.
 *   `drive` right        → the value is correct and the PWM is IGNORING it. Then `seq_ptr` says
 *                          whether the peripheral is still pointed at our array — ⚠ `lf_tag_em.c`
 *                          drives `NRFX_PWM_INSTANCE(0)` too, the same peripheral, with its own
 *                          separate belief about who owns it — and `countertop`/`decoder` say
 *                          whether the config is ours or the tag path's.
 *
 * ⛔ Instrumentation. Remove with `hw emudebug` before upstreaming (§9). */
void lf_125khz_radio_debug_get(uint8_t *out) {
    uint32_t ptr = NRF_PWM0->SEQ[0].PTR;
    uint32_t own = (uint32_t)(uintptr_t)m_lf_125khz_pwm_seq_val;
    out[0]  = (uint8_t)m_lf_125khz_pwm_seq_val[0].channel_0;
    out[1]  = m_reader_inited ? 1 : 0;
    out[2]  = (uint8_t)(NRF_PWM0->COUNTERTOP >> 8);
    out[3]  = (uint8_t)NRF_PWM0->COUNTERTOP;
    out[4]  = (uint8_t)NRF_PWM0->PRESCALER;
    out[5]  = (uint8_t)NRF_PWM0->DECODER;          /* LOAD in bits 0-2, MODE in bit 8 */
    out[6]  = (uint8_t)(NRF_PWM0->DECODER >> 8);
    out[7]  = (uint8_t)NRF_PWM0->ENABLE;
    out[8]  = (uint8_t)(NRF_PWM0->SEQ[0].CNT >> 8);
    out[9]  = (uint8_t)NRF_PWM0->SEQ[0].CNT;
    /* ⭐ The whole question in one bit: is the peripheral still reading OUR array? */
    out[10] = (ptr == own) ? 1 : 0;
    out[11] = (uint8_t)(ptr >> 24);
    out[12] = (uint8_t)(ptr >> 16);
    out[13] = (uint8_t)(ptr >> 8);
    out[14] = (uint8_t)ptr;
    out[15] = (uint8_t)(NRF_PWM0->MODE);
    /* ⭐ The load-bearing fields: what the PWM was handed at the last playback start, which is
     * the only moment the value matters. `drive_at_start` wrong ⇒ the RAM was clobbered before
     * the capture; right, with the capture still inert ⇒ the peripheral ignored a correct value
     * and `ptr_at_start` says whether it was even reading our array at the time. */
    out[16] = m_dbg_drive_at_start;
    out[17] = (m_dbg_ptr_at_start == own) ? 1 : 0;
    out[18] = (uint8_t)(m_dbg_starts >> 8);
    out[19] = (uint8_t)m_dbg_starts;
}

nrf_pwm_sequence_t const m_lf_125khz_pwm_seq_obj = {
    .values.p_individual = m_lf_125khz_pwm_seq_val,
    .length = NRF_PWM_VALUES_LENGTH(m_lf_125khz_pwm_seq_val),
    .repeats = 0,
    .end_delay = 0
};

/**
 * LF reading card decrease along the trigger collection event
 */
static void lf_125khz_gpio_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action) {
    // Directly transfer to the event
    gpio_int0_irq_handler();
}

// The LF collection decline is interrupted, and the GPIO is pulled down
// by default. The trigger method is triggering
static void gpiote_init(void) {
    nrfx_err_t err_code;

    nrfx_gpiote_in_config_t cfg = NRFX_GPIOTE_CONFIG_IN_SENSE_LOTOHI(false);
    err_code = nrfx_gpiote_in_init(LF_OA_OUT, &cfg, lf_125khz_gpio_handler);
    APP_ERROR_CHECK(err_code);
}

/**
 * Start the 125kHz broadcast
 */
void start_lf_125khz_radio(void) {
    nrfx_pwm_simple_playback(&m_pwm, &m_lf_125khz_pwm_seq_obj, 1, NRFX_PWM_FLAG_LOOP);
    m_dbg_drive_at_start = (uint8_t)m_lf_125khz_pwm_seq_val[0].channel_0;
    m_dbg_ptr_at_start = NRF_PWM0->SEQ[0].PTR;
    m_dbg_starts++;
    TAG_FIELD_LED_ON();
}

/**
 * Close 125kHz RF broadcast
 */
void stop_lf_125khz_radio(void) {
    nrfx_pwm_stop(&m_pwm, true);
    TAG_FIELD_LED_OFF();
}

static void pwm_init(void) {
    nrfx_pwm_config_t config = NRFX_PWM_DEFAULT_CONFIG;
    config.output_pins[0] = LF_ANT_DRIVER | NRFX_PWM_PIN_INVERTED;
    for (uint8_t i = 1; i < NRF_PWM_CHANNEL_COUNT; i++) {
        config.output_pins[i] = NRFX_PWM_PIN_NOT_USED;
    }
    config.irq_priority = APP_IRQ_PRIORITY_LOW;
    config.base_clock = (nrf_pwm_clk_t)NRF_PWM_CLK_1MHz;
    config.count_mode = (nrf_pwm_mode_t)NRF_PWM_MODE_UP;
    config.top_value = (uint16_t)LF_DRIVE_TOP;
    config.load_mode = (nrf_pwm_dec_load_t)NRF_PWM_LOAD_INDIVIDUAL;
    config.step_mode = (nrf_pwm_dec_step_t)NRF_PWM_STEP_AUTO;

    nrfx_err_t err_code = nrfx_pwm_init(&m_pwm, &config, NULL);
    APP_ERROR_CHECK(err_code);
}

static void pwm_timer_counter_init(void) {
    nrfx_err_t err_code;

    nrfx_timer_config_t timer_cfg = NRFX_TIMER_DEFAULT_CONFIG;
    timer_cfg.mode = NRF_TIMER_MODE_COUNTER;

    err_code = nrfx_timer_init(&m_pwm_timer_counter, &timer_cfg, NULL);
    APP_ERROR_CHECK(err_code);
}

// trigger timer count task from pwm
static void pwm_timer_count_ppi_init(void) {
    nrfx_err_t err_code;

    err_code = nrfx_ppi_channel_alloc(&m_pwm_timer_count_ppi_channel);
    APP_ERROR_CHECK(err_code);

    err_code = nrfx_ppi_channel_assign(
                   m_pwm_timer_count_ppi_channel,
                   nrfx_pwm_event_address_get(&m_pwm, NRF_PWM_EVENT_PWMPERIODEND),
                   nrfx_timer_task_address_get(&m_pwm_timer_counter, NRF_TIMER_TASK_COUNT));
    APP_ERROR_CHECK(err_code);
}

static void phase_timer_init(void) {
    nrfx_timer_config_t cfg = NRFX_TIMER_DEFAULT_CONFIG;
    cfg.frequency = NRF_TIMER_FREQ_16MHz;      /* 62.5ns per tick */
    cfg.mode = NRF_TIMER_MODE_TIMER;
    cfg.bit_width = NRF_TIMER_BIT_WIDTH_16;
    APP_ERROR_CHECK(nrfx_timer_init(&m_phase_timer, &cfg, NULL));
}

/* PWMPERIODEND clears the phase timer; the timer's COMPARE[0] samples the SAADC. */
static void phase_ppi_init(void) {
    APP_ERROR_CHECK(nrfx_ppi_channel_alloc(&m_pwm_phase_clear_ppi_channel));
    APP_ERROR_CHECK(nrfx_ppi_channel_assign(
                        m_pwm_phase_clear_ppi_channel,
                        nrfx_pwm_event_address_get(&m_pwm, NRF_PWM_EVENT_PWMPERIODEND),
                        nrfx_timer_task_address_get(&m_phase_timer, NRF_TIMER_TASK_CLEAR)));

    APP_ERROR_CHECK(nrfx_ppi_channel_alloc(&m_phase_saadc_sample_ppi_channel));
    APP_ERROR_CHECK(nrfx_ppi_channel_assign(
                        m_phase_saadc_sample_ppi_channel,
                        nrfx_timer_compare_event_address_get(&m_phase_timer, NRF_TIMER_CC_CHANNEL0),
                        nrf_saadc_task_address_get(NRF_SAADC_TASK_SAMPLE)));
}

void lf_125khz_radio_saadc_rate_set(uint16_t khz) {
    /* Clamp to what the SAADC can actually convert (3us acquisition + ~2us conversion).
     * Asking for more would leave the trigger firing faster than conversions complete. */
    if (khz > 200) {
        khz = 200;
    }
    m_saadc_rate_khz = khz;
}

void lf_125khz_radio_saadc_phase_set(uint8_t ticks) {
    /* ⚠ Clamp rather than wrap. A tick count at or beyond one period would leave
     * COMPARE[0] unreachable before the next CLEAR and the SAADC would simply never
     * be triggered -- a silent dead capture rather than a visible error. */
    if (ticks >= LF_PHASE_TICKS_PER_PERIOD) {
        ticks = LF_PHASE_TICKS_PER_PERIOD - 1;
    }
    m_saadc_phase_ticks = ticks;
}

// trigger saadc sample task from pwm
static void pwm_saadc_sample_ppi_init(void) {
    nrfx_err_t err_code;

    err_code = nrfx_ppi_channel_alloc(&m_pwm_saadc_sample_ppi_channel);
    APP_ERROR_CHECK(err_code);

    err_code = nrfx_ppi_channel_assign(
                   m_pwm_saadc_sample_ppi_channel,
                   nrfx_pwm_event_address_get(&m_pwm, NRF_PWM_EVENT_PWMPERIODEND),
                   nrf_saadc_task_address_get(NRF_SAADC_TASK_SAMPLE));
    APP_ERROR_CHECK(err_code);
}

void lf_125khz_radio_saadc_enable(lf_adc_callback_t cb) {
    /* Must precede register_lf_adc_callback(): that is where the channel is configured. */
    lf_adc_set_acq_fast(m_saadc_rate_khz > 143);
    register_lf_adc_callback(cb);

    if (m_saadc_rate_khz != 0) {
        /* Free-running, ASYNCHRONOUS to the carrier. The PWM->CLEAR channel stays OFF --
         * that channel is precisely what locks sampling to the field, and leaving it on
         * would re-impose the degeneracy this mode exists to remove. */
        uint32_t ticks = 16000u / m_saadc_rate_khz;      /* TIMER3 runs at 16MHz */
        nrfx_timer_extended_compare(&m_phase_timer, NRF_TIMER_CC_CHANNEL0, ticks,
                                    NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK, false);
        nrfx_timer_enable(&m_phase_timer);
        APP_ERROR_CHECK(nrfx_ppi_channel_enable(m_phase_saadc_sample_ppi_channel));
        return;
    }

    if (m_saadc_phase_ticks == 0) {
        /* Default: sample straight off the carrier period boundary. */
        APP_ERROR_CHECK(nrfx_ppi_channel_enable(m_pwm_saadc_sample_ppi_channel));
        return;
    }
    /* Phase-shifted: PWMPERIODEND clears TIMER3, COMPARE[0] fires the sample. The
     * compare is set with `false` for the clear-on-compare short -- the PWM does the
     * clearing, so a short here would re-clear mid-period and double-trigger. */
    nrfx_timer_compare(&m_phase_timer, NRF_TIMER_CC_CHANNEL0, m_saadc_phase_ticks, false);
    nrfx_timer_enable(&m_phase_timer);
    APP_ERROR_CHECK(nrfx_ppi_channel_enable(m_pwm_phase_clear_ppi_channel));
    APP_ERROR_CHECK(nrfx_ppi_channel_enable(m_phase_saadc_sample_ppi_channel));
}

void lf_125khz_radio_saadc_disable(void) {
    /* Disable both routes unconditionally: the phase may have been changed between
     * enable and disable, and leaving a live PPI channel pointed at SAADC_TASK_SAMPLE
     * would keep triggering conversions after the reader is done with them. */
    APP_ERROR_CHECK(nrfx_ppi_channel_disable(m_pwm_saadc_sample_ppi_channel));
    APP_ERROR_CHECK(nrfx_ppi_channel_disable(m_pwm_phase_clear_ppi_channel));
    APP_ERROR_CHECK(nrfx_ppi_channel_disable(m_phase_saadc_sample_ppi_channel));
    nrfx_timer_disable(&m_phase_timer);

    unregister_lf_adc_callback();
}

void lf_125khz_radio_gpiote_enable(void) {
    nrfx_err_t err_code;
    err_code = nrfx_ppi_channel_enable(m_pwm_timer_count_ppi_channel);
    APP_ERROR_CHECK(err_code);

    gpiote_init();
    nrfx_timer_enable(&m_pwm_timer_counter);
    nrfx_gpiote_in_event_enable(LF_OA_OUT, true);
}

void lf_125khz_radio_gpiote_disable(void) {
    nrfx_gpiote_in_event_disable(LF_OA_OUT);
    nrfx_gpiote_in_uninit(LF_OA_OUT);
    nrfx_timer_disable(&m_pwm_timer_counter);

    nrfx_err_t err_code;
    err_code = nrfx_ppi_channel_disable(m_pwm_timer_count_ppi_channel);
    APP_ERROR_CHECK(err_code);
}

// init 125kHz signal PWM modulation (use gpiote for ASK & saadc for FSK)
void lf_125khz_radio_init(void) {
    if (!m_reader_inited) {
        pwm_init();
        pwm_timer_counter_init();
        pwm_timer_count_ppi_init();
        pwm_saadc_sample_ppi_init();
        phase_timer_init();
        phase_ppi_init();
        m_reader_inited = true;
    }
}

// uninitialize
void lf_125khz_radio_uninit(void) {
    if (m_reader_inited) {
        nrfx_ppi_channel_free(m_pwm_saadc_sample_ppi_channel);
        nrfx_ppi_channel_free(m_pwm_timer_count_ppi_channel);
        nrfx_ppi_channel_free(m_pwm_phase_clear_ppi_channel);
        nrfx_ppi_channel_free(m_phase_saadc_sample_ppi_channel);
        nrfx_timer_uninit(&m_phase_timer);
        nrfx_timer_uninit(&m_pwm_timer_counter);
        nrfx_pwm_uninit(&m_pwm);
        m_reader_inited = false;
    }
}
