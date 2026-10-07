#include "lf_selftrim.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "app_timer.h"
#include "ble_main.h"
#include "nrf_saadc.h"
#include "settings.h"
#include "tag_emulation.h"
#include "utils/psk1.h"
#include "utils/selftrim.h"

#define NRF_LOG_MODULE_NAME lf_selftrim
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

#define CAPTURE_CC        800                     // 16 MHz / 800 = 20 kHz
#define CAPTURE_ACQ       NRF_SAADC_ACQTIME_40US
#define CAPTURE_SAMPLES   SELFTRIM_N_MAX          // 4096: 205 ms
#define RETRY_MS          300                     // after an invalid result, while the field lasts
#define REMEASURE_MS      5000                    // once settled, while the field lasts
#define AGREE_STEPS       3                       // two results within this many steps agree
#define MOVE_STEPS        3                       // once trimmed, move only by at least this many steps
#define BACKOFF_AFTER     3                       // invalid results in a row before backing off
#define BACKOFF_MS        5000                    // then measure at most this often
#define PPM_PER_STEP      (1e6f / 262144.0f)      // one 16 MHz tick per 1024-cycle frame

static int16_t m_buf[CAPTURE_SAMPLES];
static volatile size_t m_len;
static volatile bool m_capturing;
static volatile bool m_ready;

static volatile uint32_t m_epoch;                 // counts fields
static uint32_t m_measured_epoch = 0xFFFFFFFF;
static uint32_t m_last_tick;
static bool m_settled;
static volatile int16_t m_target;                 // trim to apply at the next burst boundary
static volatile int16_t m_applied;                // trim in the playing sequence
static bool m_have_prev;                          // previous valid result, for the agreement rule
static int16_t m_prev;
static uint16_t m_invalid_run;
static selftrim_result_t m_res;
static uint16_t m_count;
static int16_t m_last_ppm10, m_last_snr10;        // last measurement, for the status
static uint8_t m_last_why;

// Kept across sleep and slot changes: on battery the device sleeps a few seconds after a field and wakes with a reset,
// which would otherwise lose the learned trim, and a quick tap is shorter than learning it again (~1 s). The noinit
// region is retained in System OFF (app_main.c). The kept trim belongs to the slot (and type) it was learned on: any
// load of that slot restores it (after a wake, or after a client browsed the slots), and a trim learned on another
// slot replaces it. It is a property of the reader, not the card, so new data in the same slot keeps it. A power-on
// leaves garbage, which the check rejects.
typedef struct {
    uint32_t magic;
    uint8_t slot;
    uint16_t type;
    int16_t trim;
    int16_t last_ppm10;
    int16_t last_snr10;
    uint8_t last_why;
    uint16_t count;
    uint32_t check;
} lf_selftrim_keep_t;
#define KEEP_MAGIC 0x53544B31u  // "STK1"
static __attribute__((section(".noinit_selftrim"))) lf_selftrim_keep_t m_keep;

static uint32_t keep_check(const lf_selftrim_keep_t *k) {
    const uint8_t *b = (const uint8_t *)k;
    uint32_t h = 2166136261u;   // FNV-1a over everything before the check
    for (size_t i = 0; i < offsetof(lf_selftrim_keep_t, check); i++) {
        h = (h ^ b[i]) * 16777619u;
    }
    return h;
}

static void active_slot(uint8_t *slot, uint16_t *type) {
    tag_slot_specific_type_t types;
    *slot = tag_emulation_get_slot();
    tag_emulation_get_specific_types_by_slot(*slot, &types);
    *type = (uint16_t)types.tag_lf;
}

static void keep_save(void) {
    lf_selftrim_keep_t k;
    memset(&k, 0, sizeof(k));
    k.magic = KEEP_MAGIC;
    active_slot(&k.slot, &k.type);
    k.trim = m_target;
    k.last_ppm10 = m_last_ppm10;
    k.last_snr10 = m_last_snr10;
    k.last_why = m_last_why;
    k.count = m_count;
    k.check = keep_check(&k);
    m_keep = k;
}

static bool keep_valid_for_active(void) {
    uint8_t slot;
    uint16_t type;
    active_slot(&slot, &type);
    return m_keep.magic == KEEP_MAGIC && m_keep.check == keep_check(&m_keep) && m_keep.slot == slot &&
           m_keep.type == type;
}

static void adc_cb(nrf_saadc_value_t *v, size_t n) {
    if (!m_capturing) {
        return;
    }
    size_t room = CAPTURE_SAMPLES - m_len;
    size_t k = n < room ? n : room;
    for (size_t i = 0; i < k; i++) {
        m_buf[m_len + i] = v[i];
    }
    m_len += k;
    if (m_len >= CAPTURE_SAMPLES) {
        nrf_saadc_continuous_mode_disable();
        m_capturing = false;
        m_ready = true;
    }
}

static void stop_adc(void) {
    m_capturing = false;
    nrf_saadc_continuous_mode_disable();
    unregister_lf_adc_callback();
}

void lf_selftrim_abort(void) {
    if (m_capturing || m_ready) {
        stop_adc();
        m_ready = false;
    }
}

// Between bursts (PWM stopped): bring the sequence to the target trim.
void lf_selftrim_apply(const nrf_pwm_sequence_t *seq, bool psk1) {
    if (!psk1 || seq == NULL || m_target == m_applied) {
        return;
    }
    lf_psk1_apply_trim((nrf_pwm_values_wave_form_t *)seq->values.p_wave_form, seq->length / 4u, m_target);
    m_applied = m_target;
}

// A result is acted on only when two consecutive valid results agree (within AGREE_STEPS, across fields: some
// readers give a new short field per read); once trimmed, only a move of at least MOVE_STEPS is made. A reader
// restarting its field can bias a single measurement, which this rule doesn't follow.
static void decide(void) {
    bool have = false;
    int16_t cand = 0;
    if (m_res.valid) {
        const int16_t t = m_res.trim;
        if (m_have_prev && abs(t - m_prev) <= AGREE_STEPS) {
            cand = (int16_t)lroundf((t + m_prev) / 2.0f);
            have = true;
        }
        m_prev = t;
        m_have_prev = true;
    } else {
        m_have_prev = false;
    }
    m_settled = m_res.why == SELFTRIM_MATCHED;
    if (have) {
        const int16_t d = cand - m_applied;
        if ((m_applied == 0) != (cand == 0) || d >= MOVE_STEPS || d <= -MOVE_STEPS) {
            m_target = cand;
        }
        m_settled = true;
    }
    if (m_res.valid || m_res.why == SELFTRIM_MATCHED) {
        m_invalid_run = 0;
    } else if (m_invalid_run < 0xFFFF) {
        m_invalid_run++;
    }
}

void lf_selftrim_process(bool emulating, bool psk1) {
    if (!settings_get_lf_selftrim() || !psk1) {
        lf_selftrim_abort();
        if (m_target != 0) {
            m_target = 0;       // turned off: back to the nominal rate at the next burst boundary
            keep_save();
        }
        m_settled = false;
        m_have_prev = false;
        return;
    }
    if (m_capturing && !emulating) {
        lf_selftrim_abort();    // the field went away mid-capture
        return;
    }
    if (m_ready) {
        stop_adc();
        m_ready = false;
        if (!emulating) {
            return;
        }
        selftrim_detect(m_buf, CAPTURE_SAMPLES, 16e6f / CAPTURE_CC, 125000.0f, m_applied * PPM_PER_STEP, &m_res);
        m_count++;
        m_last_tick = app_timer_cnt_get();
        decide();
        m_last_ppm10 = (int16_t)lroundf(m_res.ppm * 10.0f);
        m_last_snr10 = (int16_t)lroundf(m_res.snr_db * 10.0f);
        m_last_why = m_res.why;
        if (m_res.valid || m_res.why == SELFTRIM_MATCHED || keep_valid_for_active()) {
            keep_save();    // a result on another slot that found nothing leaves the kept trim alone
        }
        NRF_LOG_INFO("selftrim: %d ppm/10, %d dB/10, why %d, target %d", (int)lroundf(m_res.ppm * 10.0f),
                     (int)lroundf(m_res.snr_db * 10.0f), m_res.why, m_target);
        return;
    }
    if (m_capturing || !emulating) {
        return;
    }
    const uint32_t since = app_timer_cnt_diff_compute(app_timer_cnt_get(), m_last_tick);
    bool due;
    if (m_invalid_run >= BACKOFF_AFTER) {
        due = since >= APP_TIMER_TICKS(BACKOFF_MS);   // backed off: a new field doesn't override it
    } else {
        due = m_measured_epoch != m_epoch || since >= APP_TIMER_TICKS(m_settled ? REMEASURE_MS : RETRY_MS);
    }
    if (!due) {
        return;
    }
    m_measured_epoch = m_epoch;
    m_len = 0;
    m_capturing = true;
    register_lf_adc_callback_input(adc_cb, NRF_SAADC_INPUT_AIN0, CAPTURE_ACQ);
    nrf_saadc_continuous_mode_enable(CAPTURE_CC);
    nrf_saadc_task_trigger(NRF_SAADC_TASK_SAMPLE);
}

void lf_selftrim_on_field(void) {
    m_epoch++;
}

// The sequence is rebuilt at the nominal rate: start over, then restore the trim kept for this slot, if any (it is
// written into the sequence when the next field arrives).
void lf_selftrim_on_load(void) {
    m_target = 0;
    m_applied = 0;
    m_settled = false;
    m_have_prev = false;
    m_invalid_run = 0;
    m_count = 0;
    m_measured_epoch = 0xFFFFFFFF;
    memset(&m_res, 0, sizeof(m_res));
    m_last_ppm10 = 0;
    m_last_snr10 = 0;
    m_last_why = 0;
    if (keep_valid_for_active()) {
        m_target = m_keep.trim;
        m_settled = m_target != 0;
        m_count = m_keep.count;
        m_last_ppm10 = m_keep.last_ppm10;
        m_last_snr10 = m_keep.last_snr10;
        m_last_why = m_keep.last_why;
        NRF_LOG_INFO("selftrim: kept trim %d restored", m_target);
    }
}

void lf_selftrim_get_status(lf_selftrim_status_t *s) {
    s->enabled = settings_get_lf_selftrim();
    s->applied = m_target;
    s->last_ppm10 = m_last_ppm10;
    s->last_snr10 = m_last_snr10;
    s->last_why = m_last_why;
    s->measurements = m_count;
}
