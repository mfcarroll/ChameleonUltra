#include "field_probe.h"

#include <math.h>
#include <string.h>

#include "app_timer.h"
#include "ble_main.h"
#include "bsp_delay.h"
#include "bsp_wdt.h"
#include "hw_connect.h"
#include "lf_tag_em.h"
#include "nrf_saadc.h"
#include "syssleep.h"

#define NRF_LOG_MODULE_NAME field_probe
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
NRF_LOG_MODULE_REGISTER();

#define FP_MAGIC        (0x31425046u)  // "FPB1"
#define FP_ROUNDS       (3)            // measurement rounds per LF field, at the start and then FP_ROUND_AT_MS
#define FP_CHECK_MS     (50)           // how often the field is checked between rounds
#define FP_ABSENT_CHECKS (2)           // consecutive checks without the field that end it
#define FP_TONE_HZ      (125000.0f / 8.0f)
#define FP_TONE_HARMS   (24)

// Sample rates as SAADC internal timer counts of 16 MHz.
#define CC_20K          (800)
#define CC_100K         (160)
#define CC_SECOND       (832)          // 19.23 kHz: the tone capture's second rate, to tell the carrier's aliases apart
#define FS_OF(cc)       (16000000.0f / (float)(cc))

extern bool g_is_tag_emulating;
extern bool g_usb_led_marquee_enable;

// The LF sniff buffer (app_cmd.c): the probe's captures and its FFT work share it. Reader mode never runs alongside.
uint8_t *lf_sniff_buffer(size_t *cap_bytes);
void lf_sniff_set_len(size_t bytes);

static __attribute__((section(".noinit_fprobe"))) fp_store_t m_store;

static const uint32_t m_round_at_ms[FP_ROUNDS] = {0, 1000, 4000};

// ---------------------------------------------------------------- time

// app_timer's counter is 24 bits at 32768 Hz and wraps every 512 s. Each call adds the ticks since the last one, and
// a repeating timer calls it often enough that no wrap is missed.
static uint32_t m_last_ticks;
static uint64_t m_ticks;
APP_TIMER_DEF(m_clock_timer);
// While an LF field lasts, this timer wakes the main loop for field_probe_process.
APP_TIMER_DEF(m_check_timer);

static uint32_t now_ms(void) {
    __disable_irq();
    const uint32_t t = app_timer_cnt_get();
    m_ticks += app_timer_cnt_diff_compute(t, m_last_ticks);
    m_last_ticks = t;
    const uint64_t ticks = m_ticks;
    __enable_irq();
    return (uint32_t)(ticks * 1000u / APP_TIMER_CLOCK_FREQ);
}

static void clock_tick(void *ctx) {
    (void)now_ms();
}

static void check_tick(void *ctx) {
}

// ---------------------------------------------------------------- the log

static bool store_valid(void) {
    return m_store.magic == FP_MAGIC && m_store.version == FP_VERSION && m_store.lf_rec_size == sizeof(fp_lf_rec_t);
}

static void store_reset(bool enabled, uint8_t env_mode) {
    memset(&m_store, 0, sizeof(m_store));
    m_store.magic = FP_MAGIC;
    m_store.version = FP_VERSION;
    m_store.enabled = enabled;
    m_store.env_mode = env_mode;
    m_store.lf_rec_size = sizeof(fp_lf_rec_t);
}

void field_probe_init(void) {
    if (!store_valid()) {
        store_reset(false, FP_MOD_HIGH);
    }
    if (m_store.enabled) {
        m_store.wake++;
    }
    m_last_ticks = app_timer_cnt_get();
    APP_ERROR_CHECK(app_timer_create(&m_clock_timer, APP_TIMER_MODE_REPEATED, clock_tick));
    APP_ERROR_CHECK(app_timer_start(m_clock_timer, APP_TIMER_TICKS(60000), NULL));
    APP_ERROR_CHECK(app_timer_create(&m_check_timer, APP_TIMER_MODE_REPEATED, check_tick));
}

bool field_probe_enabled(void) {
    return m_store.enabled != 0;
}

uint8_t field_probe_env_mode(void) {
    return m_store.env_mode;
}

static void field_abandon(void);

void field_probe_set(bool enabled, uint8_t env_mode) {
    if (!store_valid()) {
        store_reset(enabled, env_mode);
    }
    if (!enabled) {
        field_abandon();
    }
    m_store.enabled = enabled;
    m_store.env_mode = env_mode <= FP_MOD_TONE ? env_mode : FP_MOD_HIGH;
}

void field_probe_clear(void) {
    store_reset(m_store.enabled, m_store.env_mode);
}

const fp_store_t *field_probe_store(void) {
    return &m_store;
}

// ---------------------------------------------------------------- captures

// A raw capture shares the buffer with the rounds, so the rounds wait FP_HOLD_MS after one for the host to read it.
#define FP_HOLD_MS (10000)
static uint32_t m_hold_until_ms = 0;

static int16_t *volatile m_cap;
static volatile uint32_t m_cap_len, m_cap_want;

static void cap_cb(nrf_saadc_value_t *v, size_t n) {
    uint32_t room = m_cap_want - m_cap_len;
    uint32_t k = n < room ? n : room;
    memcpy(m_cap + m_cap_len, v, k * sizeof(int16_t));
    m_cap_len += k;
}

// Continuous sampling on the SAADC's own timer at 16 MHz / cc. The ADC hands over whole 2048-sample buffers, so want
// should be a multiple of 2048.
static uint32_t capture(int16_t *dst, uint32_t want, uint16_t cc, uint8_t ain, uint8_t gain) {
    m_cap = dst;
    m_cap_len = 0;
    m_cap_want = want;
    lf_adc_set_input((nrf_saadc_input_t)(NRF_SAADC_INPUT_AIN0 + ain));
    lf_adc_set_gain(gain);
    lf_adc_set_acq_fast(cc < 112);
    register_lf_adc_callback(cap_cb);
    nrf_saadc_continuous_mode_enable(cc);
    nrf_saadc_task_trigger(NRF_SAADC_TASK_SAMPLE);
    const uint32_t limit = want * cc / 16000u + 50u;
    for (uint32_t ms = 0; ms < limit && m_cap_len < m_cap_want; ms++) {
        bsp_delay_ms(1);
        bsp_wdt_feed();
    }
    nrf_saadc_continuous_mode_disable();
    unregister_lf_adc_callback();
    lf_adc_set_input(NRF_SAADC_INPUT_AIN5);
    lf_adc_set_gain(6);
    lf_adc_set_acq_fast(false);
    return m_cap_len;
}

uint32_t field_probe_capture(uint8_t mod, uint16_t rate_khz, uint32_t n, uint8_t ain, uint8_t gain) {
    size_t cap_bytes;
    int16_t *buf = (int16_t *)lf_sniff_buffer(&cap_bytes);
    if (!lf_tag_em_probe_ready() || rate_khz == 0 || rate_khz > 200 || ain > 7) {
        return 0;
    }
    n = (n + 2047u) / 2048u * 2048u;
    if (n * 2u > cap_bytes) {
        n = cap_bytes / 2u / 2048u * 2048u;
    }
    lf_tag_em_probe_mod(mod);
    bsp_delay_ms(3);
    const uint32_t got = capture(buf, n, (uint16_t)(16000u / rate_khz), ain, gain);
    lf_tag_em_probe_mod(FP_MOD_LOW);
    lf_sniff_set_len(got * 2u);
    m_hold_until_ms = now_ms() + FP_HOLD_MS;
    return got;
}

static void cap_summary(fp_cap_t *c, const int16_t *x, uint32_t n, uint8_t mode, uint16_t cc, fp_stats_t *s) {
    fp_stats(x, n, s);
    c->mode = mode;
    c->rate_khz = (uint8_t)(16000u / cc);
    c->n = (uint16_t)n;
    c->min = s->min;
    c->p10 = s->p10;
    c->p50 = s->p50;
    c->p90 = s->p90;
    c->max = s->max;
    c->noise = s->noise;
}

// In place: each sample becomes the mean of itself and the three before it.
static void smooth4(int16_t *x, uint32_t n) {
    int32_t a = n ? x[0] : 0, b = a, c = a;
    for (uint32_t i = 0; i < n; i++) {
        const int32_t v = x[i];
        x[i] = (int16_t)((v + a + b + c) / 4);
        c = b;
        b = a;
        a = v;
    }
}

static int16_t snr10(float db) {
    return (int16_t)lrintf(db * 10.0f);
}

// ---------------------------------------------------------------- one round

// The buffer holds 14336 samples. The envelope capture fills the first 10240, and its first 4096 are copied to the
// last 4096 for the spectrum, whose work (4096 floats) then takes the first 16 KB. The 20 kHz captures sit at sample
// 8192, clear of that work.
#define ENV_N       (10240)
#define SPEC_N      (4096)
#define LEVEL_N     (2048)
#define TONE_N      (4096)
#define SECOND_N    (2048)

static void measure(fp_lf_rec_t *r) {
    size_t cap_bytes;
    int16_t *buf = (int16_t *)lf_sniff_buffer(&cap_bytes);
    float *work = (float *)buf;
    int16_t *low = buf + 8192;
    int16_t *spec = buf + ENV_N;
    fp_stats_t s;
    float snr;

    // The level with LF_MOD off.
    lf_tag_em_probe_mod(FP_MOD_LOW);
    bsp_delay_ms(3);
    uint32_t n = capture(low, LEVEL_N, CC_20K, 0, 6);
    cap_summary(&r->cap[0], low, n, FP_MOD_LOW, CC_20K, &s);
    if (s.p90 >= 16000) {
        r->flags |= FP_F_RAILED;
    }

    // The envelope: dips, and the carrier's alias at 100 kHz, where 100..150 kHz folds to 0..50 kHz one to one.
    const uint8_t env_mode = m_store.env_mode;
    lf_tag_em_probe_mod(env_mode);
    bsp_delay_ms(3);
    n = capture(buf, ENV_N, CC_100K, 0, 6);
    lf_tag_em_probe_mod(FP_MOD_LOW);
    const uint32_t env_n = n;
    if (env_n >= SPEC_N) {
        memmove(spec, buf, SPEC_N * sizeof(int16_t));
    }
    // With the load on, the carrier's ripple swings the envelope by thousands of counts. A 125 kHz carrier aliases to
    // 25 kHz at 100 kHz, and the mean of 4 samples (one alias period) cancels it, leaving the field's own changes.
    smooth4(buf, env_n);
    cap_summary(&r->cap[1], buf, env_n, env_mode, CC_100K, &s);
    static fp_dip_t dips[FP_DIPS];
    uint16_t total;
    r->n_dips = (uint8_t)fp_dips(buf, env_n, &s, 32, dips, FP_DIPS, &total);
    r->dips_total = total;
    memcpy(r->dips, dips, sizeof(dips));
    float env_hz = 0.0f;
    if (env_n >= SPEC_N) {
        const float a = fp_line(spec, SPEC_N, SPEC_N, FS_OF(CC_100K), 2000.0f, NULL, 0, 0.0f, work, &snr);
        r->cap[1].line_mhz = (uint32_t)lrintf(a * 1000.0f);
        r->cap[1].line_snr10 = snr10(snr);
        if (a > 0.0f && snr >= 20.0f) {
            env_hz = 100000.0f + a;
        }
    }

    // The tone: our load modulation makes the detector recharge on the reader's carrier peaks, which puts the carrier
    // on LF_RSSI. Its alias at 20 kHz is precise, but every carrier k x 20 kHz +- alias shares it.
    // The tone's 8th harmonic is 125 kHz on our clock, which folds next to any reader's carrier, so the multiples of
    // 8 stay in the search. A square wave of even duty has little energy there.
    float own[FP_TONE_HARMS];
    int n_own = 0;
    for (int h = 1; h <= FP_TONE_HARMS; h++) {
        if (h % 8 != 0) {
            own[n_own++] = fp_fold((double)FP_TONE_HZ * h, FS_OF(CC_20K));
        }
    }
    lf_tag_em_probe_mod(FP_MOD_TONE);
    bsp_delay_ms(3);
    n = capture(low, TONE_N, CC_20K, 0, 6);
    lf_tag_em_probe_mod(FP_MOD_LOW);
    cap_summary(&r->cap[2], low, n, FP_MOD_TONE, CC_20K, &s);
    const float binw = FS_OF(CC_20K) / TONE_N;
    const float alias = n >= TONE_N ? fp_line(low, TONE_N, TONE_N, FS_OF(CC_20K), 100.0f, own, n_own, 3.0f * binw,
                                              work, &snr)
                                    : 0.0f;
    r->cap[2].line_mhz = (uint32_t)lrintf(alias * 1000.0f);
    r->cap[2].line_snr10 = snr10(snr);

    // The envelope's line places the carrier on its own, unambiguously.
    if (env_hz > 0.0f) {
        r->carrier_mhz = (uint32_t)lrintf(env_hz * 1000.0f);
        r->carrier_snr10 = r->cap[1].line_snr10;
        r->carrier_how = FP_CARRIER_ENVELOPE;
        return;
    }
    if (alias <= 0.0f || snr < 15.0f) {
        return;
    }
    const float tone_snr = snr;

    // The candidates between 90 and 160 kHz, checked in a second capture at another rate, where they fold apart.
    float cand[8];
    int nc = 0;
    for (int k = 4; k <= 8 && nc < 8; k++) {
        const float base = k * FS_OF(CC_20K);
        const float f2[2] = {base - alias, base + alias};
        for (int j = 0; j < 2 && nc < 8; j++) {
            if (f2[j] >= 90000.0f && f2[j] <= 160000.0f) {
                cand[nc++] = f2[j];
            }
        }
    }
    lf_tag_em_probe_mod(FP_MOD_TONE);
    bsp_delay_ms(3);
    n = capture(low, SECOND_N, CC_SECOND, 0, 6);
    lf_tag_em_probe_mod(FP_MOD_LOW);
    float best = 0.0f, second = 0.0f;
    int bi = -1;
    for (int i = 0; n >= SECOND_N && i < nc; i++) {
        const float a2 = fp_fold(cand[i], FS_OF(CC_SECOND));
        bool own_line = false;
        for (int h = 1; h <= FP_TONE_HARMS; h++) {
            if (h % 8 != 0 && fabsf(a2 - fp_fold((double)FP_TONE_HZ * h, FS_OF(CC_SECOND))) < 30.0f) {
                own_line = true;
            }
        }
        if (own_line || a2 < 100.0f) {
            continue;
        }
        const float noise = fp_noise_near(low, SECOND_N, a2, FS_OF(CC_SECOND), 400.0f);
        const float ratio = noise > 0.0f ? fp_goertzel(low, SECOND_N, a2, FS_OF(CC_SECOND)) / noise : 0.0f;
        if (ratio > best) {
            second = best;
            best = ratio;
            bi = i;
        } else if (ratio > second) {
            second = ratio;
        }
    }

    float f = 0.0f;
    if (bi >= 0 && best >= 4.0f && best >= 2.0f * second) {
        f = cand[bi];
        r->carrier_how = FP_CARRIER_TWO_RATES;
    } else {
        if (bi >= 0 && best >= 4.0f) {
            r->flags |= FP_F_AMBIGUOUS;
        }
        for (int i = 0; i < nc; i++) {
            if (f == 0.0f || fabsf(cand[i] - 125000.0f) < fabsf(f - 125000.0f)) {
                f = cand[i];
            }
        }
        r->carrier_how = FP_CARRIER_ASSUMED;
    }
    r->carrier_mhz = (uint32_t)lrintf(f * 1000.0f);
    r->carrier_snr10 = snr10(tone_snr);
}

// ---------------------------------------------------------------- the LF field

static volatile bool m_lf_pending = false;
static bool m_lf_active = false;
static uint8_t m_round = 0;
static uint32_t m_field_start_ms = 0, m_next_check_ms = 0;
static uint8_t m_absent = 0;
static uint16_t m_field_slot = 0;  // index into lf_field of the field in progress

void field_probe_lf_up(void) {
    if (!m_lf_active) {
        m_lf_pending = true;
    }
}

static fp_lf_rec_t *next_rec(void) {
    fp_lf_rec_t *r = &m_store.lf[m_store.lf_recs % FP_LF_RECS];
    memset(r, 0, sizeof(*r));
    m_store.lf_recs++;
    return r;
}

static void field_start(void) {
    m_lf_active = true;
    m_round = 0;
    m_absent = 0;
    m_field_start_ms = now_ms();
    m_next_check_ms = m_field_start_ms;
    m_field_slot = m_store.lf_fields % FP_LF_FIELDS;
    fp_field_t *f = &m_store.lf_field[m_field_slot];
    f->wake = m_store.wake;
    f->t_ms = m_field_start_ms;
    f->dur_ms = 0xFFFFFFFFu;
    m_store.lf_fields++;

    sleep_timer_stop();
    APP_ERROR_CHECK(app_timer_start(m_check_timer, APP_TIMER_TICKS(FP_CHECK_MS), NULL));
    g_is_tag_emulating = true;
    g_usb_led_marquee_enable = false;
    set_slot_light_color(RGB_CYAN);
    TAG_FIELD_LED_ON()
    NRF_LOG_INFO("probe: LF field %d", m_store.lf_fields);
}

static void field_end(uint32_t t) {
    m_lf_active = false;
    app_timer_stop(m_check_timer);
    m_store.lf_field[m_field_slot].dur_ms = t - m_field_start_ms;
    g_is_tag_emulating = false;
    TAG_FIELD_LED_OFF()
    lf_tag_em_probe_rearm();
    sleep_timer_start(SLEEP_DELAY_MS_FIELD_125KHZ_LOST);
    NRF_LOG_INFO("probe: LF field gone after %d ms", t - m_field_start_ms);
}

// The probe was switched off during a field: the field's duration stays unknown.
static void field_abandon(void) {
    m_lf_pending = false;
    if (m_lf_active) {
        m_lf_active = false;
        app_timer_stop(m_check_timer);
        g_is_tag_emulating = false;
        TAG_FIELD_LED_OFF()
    }
}

void field_probe_process(void) {
    if (!m_store.enabled || !lf_tag_em_probe_ready()) {
        return;
    }
    if (!m_lf_active) {
        if (!m_lf_pending) {
            return;
        }
        m_lf_pending = false;
        field_start();
    }
    const uint32_t t = now_ms();
    if ((int32_t)(t - m_next_check_ms) < 0) {
        return;
    }
    m_next_check_ms = t + FP_CHECK_MS;
    sleep_timer_stop();
    if (m_round < FP_ROUNDS && t - m_field_start_ms >= m_round_at_ms[m_round] && (int32_t)(t - m_hold_until_ms) >= 0) {
        fp_lf_rec_t *r = next_rec();
        r->wake = m_store.wake;
        r->field = m_store.lf_fields;
        r->t_ms = t;
        r->round = m_round++;
        measure(r);
        bsp_delay_ms(3);
        if (!is_lf_field_exists()) {
            r->flags |= FP_F_ENDED;
        }
        return;
    }
    if (is_lf_field_exists()) {
        m_absent = 0;
    } else if (++m_absent >= FP_ABSENT_CHECKS) {
        field_end(t);
    }
}

bool field_probe_lf_measure_now(fp_lf_rec_t *out) {
    if (!lf_tag_em_probe_ready()) {
        return false;
    }
    fp_lf_rec_t *r = next_rec();
    r->wake = m_store.wake;
    r->field = m_store.lf_fields;
    r->t_ms = now_ms();
    r->flags = FP_F_MANUAL;
    measure(r);
    bsp_delay_ms(3);
    if (!is_lf_field_exists()) {
        r->flags |= FP_F_ENDED;
    }
    memcpy(out, r, sizeof(*r));
    return true;
}

// ---------------------------------------------------------------- the HF field

static fp_hf_t *volatile m_hf = NULL;

void field_probe_hf_field(bool on) {
    const uint32_t t = now_ms();
    if (on) {
        fp_hf_t *h = &m_store.hf[m_store.hf_fields % FP_HF_FIELDS];
        memset(h, 0, sizeof(*h));
        h->wake = m_store.wake;
        h->t_ms = t;
        h->dur_ms = 0xFFFFFFFFu;
        m_store.hf_fields++;
        m_hf = h;
    } else if (m_hf != NULL) {
        m_hf->dur_ms = t - m_hf->t_ms;
        m_hf = NULL;
    }
}

void field_probe_hf_frame(const uint8_t *data, uint16_t bits, uint32_t rx_status) {
    fp_hf_t *h = m_hf;
    if (h == NULL) {
        return;
    }
    if (h->frames < 0xFFFF) h->frames++;
    if (rx_status != 0) {
        if (h->rx_errors < 0xFFFF) h->rx_errors++;
    }
    if (bits == 7 && data[0] == 0x26) {
        if (h->reqa < 0xFFFF) h->reqa++;
    } else if (bits == 7 && data[0] == 0x52) {
        if (h->wupa < 0xFFFF) h->wupa++;
    } else {
        if (h->other < 0xFFFF) h->other++;
        h->other_bits = bits;
        const uint16_t bytes = (bits + 7) / 8;
        memcpy(h->other_data, data, bytes < 4 ? bytes : 4);
    }
}
