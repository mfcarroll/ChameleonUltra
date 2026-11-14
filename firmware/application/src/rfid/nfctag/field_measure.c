#include "field_measure.h"

#include <math.h>
#include <string.h>

#include "app_error.h"
#include "app_timer.h"
#include "app_usbd.h"
#include "ble_main.h"
#include "bsp_delay.h"
#include "bsp_wdt.h"
#include "hw_connect.h"
#include "lf_tag_em.h"
#include "nrf_saadc.h"
#include "nrfx_power.h"
#include "syssleep.h"

#define NRF_LOG_MODULE_NAME field_measure
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
NRF_LOG_MODULE_REGISTER();

#define FM_MAGIC        (0x31425046u)  // "FPB1"
#define FM_CRASH_MARK   (0x43525348u)  // "CRSH": the fault handler's reset, for the next boot
#define FM_ROUNDS       (3)            // measurement rounds per LF field, at the start and then FM_ROUND_AT_MS
#define FM_CHECK_MS     (50)           // how often the field is checked between rounds
#define FM_ABSENT_CHECKS (2)           // consecutive checks without the field that end it
#define FM_TONE_HZ      (125000.0f / 8.0f)
#define FM_TONE_HARMS   (24)
#define FM_DROP_LED_MS  (150)          // a drop shows red this long, so a 2 ms drop can be seen
#define FM_FIELD_MIN    (200)          // ADC counts: a field's level at least this

// Sample rates as SAADC internal timer counts of 16 MHz.
#define CC_20K          (800)
#define CC_100K         (160)
#define CC_SECOND       (832)          // 19.23 kHz: the tone capture's second rate, to tell the carrier's aliases apart
#define CC_TIMING       (2000)         // 8 kHz: the timing capture
#define FS_OF(cc)       (16000000.0f / (float)(cc))

extern bool g_is_tag_emulating;
extern bool g_usb_led_marquee_enable;

// The LF sniff buffer (app_cmd.c): the probe's captures and its FFT work share it. Reader mode never runs alongside.
uint8_t *lf_sniff_buffer(size_t *cap_bytes);
void lf_sniff_set_len(size_t bytes);

static __attribute__((section(".noinit_fprobe"))) fm_store_t m_store;
// A firmware error resets the CU the same way as a DFU reboot does (SREQ), so the fault handler leaves this mark.
static __attribute__((section(".noinit_fprobe"))) uint32_t m_crash_mark;

static const uint32_t m_round_at_ms[FM_ROUNDS] = {0, 1000, 4000};

// ---------------------------------------------------------------- time

// app_timer stops its RTC while no timer is due, and on USB with no field the probe's clock built on it counted 6.3 s
// of 12.6 s. So the probe keeps its own clock on RTC2, which nothing else uses: 1024 Hz, 24 bits, overflows counted.
#define FM_RTC          NRF_RTC2
#define FM_RTC_HZ       (1024u)
static volatile uint32_t m_rtc_wraps;
// While an LF field lasts, this timer wakes the main loop for field_measure_process.
APP_TIMER_DEF(m_check_timer);

void RTC2_IRQHandler(void) {
    if (FM_RTC->EVENTS_OVRFLW) {
        FM_RTC->EVENTS_OVRFLW = 0;
        m_rtc_wraps++;
    }
}

static void clock_init(void) {
    FM_RTC->TASKS_STOP = 1;
    FM_RTC->TASKS_CLEAR = 1;
    FM_RTC->PRESCALER = 32768u / FM_RTC_HZ - 1u;
    FM_RTC->EVENTS_OVRFLW = 0;
    FM_RTC->INTENSET = RTC_INTENSET_OVRFLW_Msk;
    NVIC_SetPriority(RTC2_IRQn, APP_IRQ_PRIORITY_LOW);
    NVIC_ClearPendingIRQ(RTC2_IRQn);
    NVIC_EnableIRQ(RTC2_IRQn);
    FM_RTC->TASKS_START = 1;
}

// Safe in interrupt context. An overflow whose interrupt hasn't run yet still counts.
static uint32_t now_ms(void) {
    uint32_t w, c;
    do {
        w = m_rtc_wraps;
        c = FM_RTC->COUNTER;
    } while (w != m_rtc_wraps);
    if (FM_RTC->EVENTS_OVRFLW && c < 0x800000u) {
        w++;
    }
    return (uint32_t)((((uint64_t)w << 24) | c) * 1000u / FM_RTC_HZ);
}

static void check_tick(void *ctx) {
}

// ---------------------------------------------------------------- the log

static bool store_valid(void) {
    return m_store.magic == FM_MAGIC && m_store.version == FM_VERSION && m_store.lf_rec_size == sizeof(fm_lf_rec_t);
}

static void store_reset(bool enabled, uint8_t env_mode) {
    memset(&m_store, 0, sizeof(m_store));
    m_store.magic = FM_MAGIC;
    m_store.version = FM_VERSION;
    m_store.enabled = enabled;
    m_store.env_mode = env_mode;
    m_store.lf_rec_size = sizeof(fm_lf_rec_t);
}

static bool crashed(uint32_t resetreas) {
    return (resetreas & (POWER_RESETREAS_DOG_Msk | POWER_RESETREAS_LOCKUP_Msk)) || m_crash_mark == FM_CRASH_MARK;
}

void field_measure_noinit_wipe(uint32_t resetreas) {
    const bool keep = crashed(resetreas) && store_valid();
    const uint8_t *lo = (const uint8_t *)&m_store, *hi = lo + sizeof(m_store);
    for (uint32_t *p = (uint32_t *)0x20038000; p < (uint32_t *)0x20040000; p++) {
        const bool ours = (const uint8_t *)p >= lo && (const uint8_t *)p < hi;
        if (!(keep && ours) && p != &m_crash_mark) {
            *p = 0xFFFFFFFFu;
        }
    }
}

// The SDK's handler, without its log flush: record the error in the log, then reset.
void app_error_fault_handler(uint32_t id, uint32_t pc, uint32_t info) {
    __disable_irq();
    if (store_valid()) {
        // APP_ERROR_CHECK passes no PC, only its file and line.
        const error_info_t *e = id == NRF_FAULT_ID_SDK_ERROR ? (const error_info_t *)info : NULL;
        m_store.crash_err = e ? e->err_code : id;
        m_store.crash_pc = (pc == 0 && e) ? e->line_num : pc;
        m_crash_mark = FM_CRASH_MARK;
    }
    NVIC_SystemReset();
}

void field_measure_init(uint32_t resetreas) {
    // A fresh log, after a flash or a first power-up, starts with measurement on: this firmware is for measuring.
    if (!store_valid()) {
        store_reset(true, FM_MOD_HIGH);
    } else if (crashed(resetreas)) {
        if (m_store.crashes < 0xFF) m_store.crashes++;
        m_store.crash_wake = m_store.wake;
        if (m_crash_mark == FM_CRASH_MARK) {
            m_store.crash_kind = FM_CRASH_ERROR;
        } else {
            m_store.crash_kind = (resetreas & POWER_RESETREAS_DOG_Msk) ? FM_CRASH_WATCHDOG : FM_CRASH_LOCKUP;
            m_store.crash_err = 0;
            m_store.crash_pc = 0;
        }
    }
    m_crash_mark = 0;
    m_store.boot_reason = (uint16_t)((resetreas & 0xF) | ((resetreas >> 12) & 0x1F0));
    if (m_store.enabled) {
        m_store.wake++;
    }
    clock_init();
    APP_ERROR_CHECK(app_timer_create(&m_check_timer, APP_TIMER_MODE_REPEATED, check_tick));
}

bool field_measure_enabled(void) {
    return m_store.enabled != 0;
}

uint8_t field_measure_env_mode(void) {
    return m_store.env_mode;
}

static void field_abandon(void);

void field_measure_set(bool enabled, uint8_t env_mode) {
    if (!store_valid()) {
        store_reset(enabled, env_mode);
    }
    if (!enabled) {
        field_abandon();
    }
    m_store.enabled = enabled;
    m_store.env_mode = env_mode <= FM_MOD_TONE ? env_mode : FM_MOD_HIGH;
}

void field_measure_clear(void) {
    store_reset(m_store.enabled, m_store.env_mode);
}

void field_measure_usb_dropped(uint32_t err) {
    if (store_valid()) {
        if (m_store.usb_dropped < 0xFFFF) m_store.usb_dropped++;
        m_store.usb_err = (uint16_t)err;
    }
}

const fm_store_t *field_measure_store(void) {
    return &m_store;
}

// ---------------------------------------------------------------- captures

// A round holds the main loop for a few hundred ms. USB events run only there, so the round processes them itself:
// a reply already going out finishes, and a command arriving waits in the frame buffer for the main loop.
static void usb_keepalive(void) {
    while (app_usbd_event_queue_process());
    bsp_wdt_feed();
}

static void drop_led_poll(void);

// A raw capture shares the buffer with the rounds, so the rounds wait FM_HOLD_MS after one for the host to read it.
#define FM_HOLD_MS (10000)
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
        usb_keepalive();
        drop_led_poll();
    }
    nrf_saadc_continuous_mode_disable();
    unregister_lf_adc_callback();
    lf_adc_set_input(NRF_SAADC_INPUT_AIN5);
    lf_adc_set_gain(6);
    lf_adc_set_acq_fast(false);
    return m_cap_len;
}

uint32_t field_measure_capture(uint8_t mod, uint16_t rate_khz, uint32_t n, uint8_t ain, uint8_t gain) {
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
    lf_tag_em_probe_mod(FM_MOD_LOW);
    lf_sniff_set_len(got * 2u);
    m_hold_until_ms = now_ms() + FM_HOLD_MS;
    return got;
}

static void cap_summary(fm_cap_t *c, const int16_t *x, uint32_t n, uint8_t mode, uint16_t cc, fm_stats_t *s) {
    fm_stats(x, n, s);
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
#define BLOCK_N     (64)    // the envelope's on-off blocks: 0.64 ms, 16 periods of the carrier's alias
#define GATED_MIN_N (2000)  // 20 ms: the shortest on-stretch the carrier is fitted on
#define TIMING_N    (14336) // 1.79 s at 8 kHz
#define TIMING_BLK  (8)     // 1 ms

// The longest stretch of x with the field on: 64-sample means at least half the highest. A stretch that meets a
// block with the field off loses its edge block, where the field was rising or falling. Returns whether the field
// went off anywhere (or never came on).
static bool longest_on(const int16_t *x, uint32_t n, uint32_t *at, uint32_t *len) {
    static int32_t m[ENV_N / BLOCK_N];
    const uint32_t nb = n / BLOCK_N;
    int32_t top = 0;
    for (uint32_t b = 0; b < nb; b++) {
        int32_t sum = 0;
        for (uint32_t i = 0; i < BLOCK_N; i++) {
            sum += x[b * BLOCK_N + i];
        }
        m[b] = sum / BLOCK_N;
        if (m[b] > top) top = m[b];
    }
    *at = 0;
    *len = 0;
    if (top < FM_FIELD_MIN) {
        return true;
    }
    bool off_seen = false;
    uint32_t start = 0;
    bool in = false;
    for (uint32_t b = 0; b <= nb; b++) {
        const bool on = b < nb && 2 * m[b] >= top;
        if (b < nb && !on) off_seen = true;
        if (on && !in) {
            in = true;
            start = b;
        } else if (!on && in) {
            in = false;
            const uint32_t a = start + (start > 0 ? 1 : 0), e = b - (b < nb ? 1 : 0);
            if (e > a && (e - a) * BLOCK_N > *len) {
                *at = a * BLOCK_N;
                *len = (e - a) * BLOCK_N;
            }
        }
    }
    return off_seen;
}

// One of the timing capture's dips, from ms start to ms end. The record is packed, so the dip is copied in.
static void level_dip(fm_lf_rec_t *r, uint16_t *stored, uint16_t *total, uint32_t start, uint32_t end, int32_t depth,
                      bool open) {
    (*total)++;
    if (*stored < FM_LEVEL_DIPS) {
        const fm_dip_t d = {
            .start = (uint16_t)start,
            .width = (uint16_t)(end - start),
            .depth = (uint16_t)((depth > 0x7FFF ? 0x7FFF : depth) | (open ? FM_DIP_OPEN : 0)),
        };
        memcpy(&r->level_dips[(*stored)++], &d, sizeof(d));
    }
}

// One on or off stretch of the timing capture, in ms.
static void timing_run(fm_lf_rec_t *r, uint16_t *stored, uint16_t *total, uint32_t ms) {
    (*total)++;
    if (*stored < FM_RUNS) {
        r->runs[(*stored)++] = (uint16_t)(ms > 0xFFFF ? 0xFFFF : ms);
    }
}

// The field's on and off stretches over 1.79 s, with LF_MOD off: what a reader that pulses its field does, and the
// dips of a field that stays on.
//
// With the load off, LF_RSSI falls slowly when the field goes off (a time constant of about 4.5 ms on a bench CU). A
// drop that reaches the off level is an off. The off level is the larger of FM_FIELD_MIN and a tenth of the full level,
// because a strong field takes too long to fall as far as FM_FIELD_MIN. Every other drop is a dip. An off shorter
// than about 10 ms can't reach the off level and reads as a dip. The page compares such a dip's depth with its length.
static void timing(fm_lf_rec_t *r) {
    size_t cap_bytes;
    int16_t *buf = (int16_t *)lf_sniff_buffer(&cap_bytes);
    lf_tag_em_probe_mod(FM_MOD_LOW);
    bsp_delay_ms(3);
    const uint32_t n = capture(buf, TIMING_N, CC_TIMING, 0, 6);
    usb_keepalive();
    // 1 ms means, in place: block b is written at b, before anything at b is read again.
    const uint32_t nb = n / TIMING_BLK;
    int32_t lo = 0x7FFF, hi = -0x8000;
    for (uint32_t b = 0; b < nb; b++) {
        int32_t sum = 0;
        for (uint32_t i = 0; i < TIMING_BLK; i++) {
            sum += buf[b * TIMING_BLK + i];
        }
        buf[b] = (int16_t)(sum / TIMING_BLK);
        if (buf[b] < lo) lo = buf[b];
        if (buf[b] > hi) hi = buf[b];
    }
    r->timing_low = (int16_t)lo;
    r->timing_high = (int16_t)hi;
    if (nb == 0 || hi < FM_FIELD_MIN) {
        return;
    }
    // The full level is the 90th percentile of the 1 ms means with the field on. A field that dips for most of the
    // capture keeps its top level as full. The means at or above FM_FIELD_MIN are copied past the means, over the raw
    // samples.
    int16_t *on_ms = buf + TIMING_N / 4;
    uint32_t n_on = 0;
    for (uint32_t b = 0; b < nb; b++) {
        if (buf[b] >= FM_FIELD_MIN) {
            on_ms[n_on++] = buf[b];
        }
    }
    const int32_t full = fm_percentile(on_ms, n_on, 0.9f);
    const int32_t off_at = full / 10 > FM_FIELD_MIN ? full / 10 : FM_FIELD_MIN;
    const int32_t half = full / 2;
    int32_t enter = full / 8;
    if (enter < 32) {
        enter = 32;
    }
    const int32_t drop_lo = full - enter, drop_hi = full - enter / 2;
    r->timing_full = (int16_t)full;
    r->timing_off_at = (int16_t)off_at;

    // A drop starts below drop_lo and ends back above drop_hi. The gap between the two thresholds keeps noise from
    // splitting a drop, and it makes the field's fall and its return after an off one drop. If a drop reaches off_at,
    // it is an off stretch, timed where the level crosses half of full. The level falls to half in about 3 ms and to
    // off_at in about 10, so the half crossing times an off more closely. Every other drop is a dip, timed between
    // the two thresholds.
    bool in = false;
    uint32_t start = 0, half_first = 0, half_last = 0, on_from = 0;
    bool below_half = false;
    int32_t bottom = 0;
    uint16_t stored = 0, total = 0, dips_stored = 0, dips_total = 0;
    r->timing_first_on = 1;
    for (uint32_t b = 0; b <= nb; b++) {
        const bool end = b == nb;
        const int32_t v = end ? full : buf[b];
        if (!in) {
            if (end || v >= drop_lo) {
                continue;
            }
            in = true;
            start = b;
            bottom = v;
            below_half = false;
        }
        if (!end) {
            if (v < bottom) bottom = v;
            if (v < half) {
                if (!below_half) half_first = b;
                below_half = true;
                half_last = b;
            }
        }
        if (!end && v < drop_hi) {
            continue;
        }
        in = false;
        if (bottom < off_at) {
            const uint32_t off_end = half_last + 1;
            if (half_first == 0) {
                r->timing_first_on = 0;
            } else {
                timing_run(r, &stored, &total, half_first - on_from);
            }
            timing_run(r, &stored, &total, off_end - half_first);
            on_from = off_end;
        } else {
            level_dip(r, &dips_stored, &dips_total, start, b, full - bottom, start == 0 || end);
        }
    }
    if (on_from < nb) {
        timing_run(r, &stored, &total, nb - on_from);
    }
    r->n_runs = (uint8_t)stored;
    r->runs_total = total;
    r->n_level_dips = (uint8_t)dips_stored;
    r->level_dips_total = dips_total;
}

static void measure_carrier(fm_lf_rec_t *r) {
    size_t cap_bytes;
    int16_t *buf = (int16_t *)lf_sniff_buffer(&cap_bytes);
    float *work = (float *)buf;
    int16_t *low = buf + 8192;
    int16_t *spec = buf + ENV_N;
    fm_stats_t s;
    float snr;

    // The envelope first, while a reader that sends one command and stops may still be sending: dips, and the
    // carrier's alias at 100 kHz, where 100..150 kHz folds to 0..50 kHz one to one.
    const uint8_t env_mode = m_store.env_mode;
    lf_tag_em_probe_mod(env_mode);
    bsp_delay_ms(1);
    uint32_t n = capture(buf, ENV_N, CC_100K, 0, 6);
    lf_tag_em_probe_mod(FM_MOD_LOW);
    const uint32_t env_n = n;
    // The carrier's line comes from the longest stretch with the field on, at most SPEC_N from its middle. A field
    // that switches puts its own on-off pattern in the spectrum of a capture that spans the switch.
    uint32_t run_at, run_n;
    const bool switched = longest_on(buf, env_n, &run_at, &run_n);
    if (switched) {
        r->flags |= FM_F_SWITCHED;
    }
    if (run_n > SPEC_N) {
        run_at += (run_n - SPEC_N) / 2;
        run_n = SPEC_N;
    }
    memcpy(spec, buf + run_at, run_n * sizeof(int16_t));
    // With the load on, the carrier's ripple swings the envelope by thousands of counts. A 125 kHz carrier aliases to
    // 25 kHz at 100 kHz, and the mean of 4 samples (one alias period) cancels it, leaving the field's own changes.
    smooth4(buf, env_n);
    cap_summary(&r->cap[1], buf, env_n, env_mode, CC_100K, &s);
    usb_keepalive();
    static fm_dip_t dips[FM_DIPS];
    uint16_t total;
    r->n_dips = (uint8_t)fm_dips(buf, env_n, &s, 32, dips, FM_DIPS, &total);
    r->dips_total = total;
    memcpy(r->dips, dips, sizeof(dips));
    // The Hitag 2 authentication mode from the reader's command lengths. Runs on the whole envelope, so the 64-bit
    // (crypto) frame is measured even though it overflows the 40 stored dips. Reads the frame's length, not its content.
    // A round taken after the field has gone is skipped: its envelope is noise, and the scan anchors START_AUTH in it.
    fm_hitag_t ht = {0};
    if (s.p90 >= FM_FIELD_MIN) {
        fm_hitag_scan(buf, env_n, &s, 32, &ht);
    }
    r->auth_mode = ht.mode;
    r->auth_start_gaps = ht.start_gaps;
    r->auth_gaps = ht.auth_gaps;
    r->auth_span = ht.auth_span;
    float env_hz = 0.0f;
    if (run_n >= GATED_MIN_N) {
        const float a = fm_line(spec, run_n, SPEC_N, FS_OF(CC_100K), 2000.0f, NULL, 0, 0.0f, work, &snr);
        r->cap[1].line_mhz = (uint32_t)lrintf(a * 1000.0f);
        r->cap[1].line_snr10 = snr10(snr);
        // Only the load on puts the carrier's ripple in the envelope. With the load off or the tone, the strongest
        // line is something else: the tone's own 15.6 kHz, which read as a 115.6 kHz carrier.
        if (env_mode == FM_MOD_HIGH && a > 0.0f && snr >= 20.0f) {
            env_hz = 100000.0f + a;
        }
    }

    // The level with LF_MOD off. The envelope's spectrum work has finished with the buffer it shares.
    lf_tag_em_probe_mod(FM_MOD_LOW);
    bsp_delay_ms(3);
    n = capture(low, LEVEL_N, CC_20K, 0, 6);
    cap_summary(&r->cap[0], low, n, FM_MOD_LOW, CC_20K, &s);
    if (s.p90 >= 16000) {
        r->flags |= FM_F_RAILED;
    }

    // The tone: our load modulation makes the detector recharge on the reader's carrier peaks, which puts the carrier
    // on LF_RSSI. Its alias at 20 kHz is precise, but every carrier k x 20 kHz +- alias shares it.
    // The tone's 8th harmonic is 125 kHz on our clock, which folds next to any reader's carrier, so the multiples of
    // 8 stay in the search. A square wave of even duty has little energy there.
    float own[FM_TONE_HARMS];
    int n_own = 0;
    for (int h = 1; h <= FM_TONE_HARMS; h++) {
        if (h % 8 != 0) {
            own[n_own++] = fm_fold((double)FM_TONE_HZ * h, FS_OF(CC_20K));
        }
    }
    lf_tag_em_probe_mod(FM_MOD_TONE);
    bsp_delay_ms(3);
    n = capture(low, TONE_N, CC_20K, 0, 6);
    lf_tag_em_probe_mod(FM_MOD_LOW);
    cap_summary(&r->cap[2], low, n, FM_MOD_TONE, CC_20K, &s);
    usb_keepalive();
    const float binw = FS_OF(CC_20K) / TONE_N;
    const float alias = n >= TONE_N ? fm_line(low, TONE_N, TONE_N, FS_OF(CC_20K), 100.0f, own, n_own, 3.0f * binw,
                                              work, &snr)
                                    : 0.0f;
    r->cap[2].line_mhz = (uint32_t)lrintf(alias * 1000.0f);
    r->cap[2].line_snr10 = snr10(snr);

    // The envelope's line places the carrier on its own, unambiguously.
    if (env_hz > 0.0f) {
        r->carrier_mhz = (uint32_t)lrintf(env_hz * 1000.0f);
        r->carrier_snr10 = r->cap[1].line_snr10;
        r->carrier_how = switched ? FM_CARRIER_GATED : FM_CARRIER_ENVELOPE;
        r->carrier_n = (uint16_t)run_n;
        return;
    }
    // A switching field spreads the tone's own line, and its on-off pattern gives a line near the search's floor:
    // on HS's door readers these read as 124.4 and 120.1 kHz. So the tone only places a steady field's carrier.
    if (switched || alias <= 0.0f || snr < 15.0f) {
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
    lf_tag_em_probe_mod(FM_MOD_TONE);
    bsp_delay_ms(3);
    n = capture(low, SECOND_N, CC_SECOND, 0, 6);
    lf_tag_em_probe_mod(FM_MOD_LOW);
    float best = 0.0f, second = 0.0f;
    int bi = -1;
    for (int i = 0; n >= SECOND_N && i < nc; i++) {
        const float a2 = fm_fold(cand[i], FS_OF(CC_SECOND));
        bool own_line = false;
        for (int h = 1; h <= FM_TONE_HARMS; h++) {
            if (h % 8 != 0 && fabsf(a2 - fm_fold((double)FM_TONE_HZ * h, FS_OF(CC_SECOND))) < 30.0f) {
                own_line = true;
            }
        }
        if (own_line || a2 < 100.0f) {
            continue;
        }
        usb_keepalive();
        const float noise = fm_noise_near(low, SECOND_N, a2, FS_OF(CC_SECOND), 400.0f);
        const float ratio = noise > 0.0f ? fm_goertzel(low, SECOND_N, a2, FS_OF(CC_SECOND)) / noise : 0.0f;
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
        r->carrier_how = FM_CARRIER_TWO_RATES;
    } else {
        if (bi >= 0 && best >= 4.0f) {
            r->flags |= FM_F_AMBIGUOUS;
        }
        for (int i = 0; i < nc; i++) {
            if (f == 0.0f || fabsf(cand[i] - 125000.0f) < fabsf(f - 125000.0f)) {
                f = cand[i];
            }
        }
        r->carrier_how = FM_CARRIER_ASSUMED;
    }
    r->carrier_mhz = (uint32_t)lrintf(f * 1000.0f);
    r->carrier_snr10 = snr10(tone_snr);
}

static void measure(fm_lf_rec_t *r) {
    measure_carrier(r);
    timing(r);
}

// ---------------------------------------------------------------- the LF field

static volatile bool m_lf_pending = false;
static bool m_lf_active = false;
static uint8_t m_round = 0;
static uint32_t m_field_start_ms = 0, m_next_check_ms = 0;
static uint8_t m_absent = 0;
static uint16_t m_field_slot = 0;  // index into lf_field of the field in progress
static volatile uint16_t m_drops;  // LPCOMP's drops in the field in progress
static volatile uint32_t m_drop_ms;
static volatile bool m_drop_shown = false;

// The field LED follows the field while the drop watch runs; a drop also turns the slots red for FM_DROP_LED_MS.
void field_measure_lf_drop(void) {
    if (!m_lf_active) {
        return;
    }
    m_drops++;
    m_drop_ms = now_ms();
    m_drop_shown = true;
    TAG_FIELD_LED_OFF()
    set_slot_light_color(RGB_RED);
}

void field_measure_lf_seen(bool on) {
    if (!m_lf_active) {
        return;
    }
    if (on) {
        TAG_FIELD_LED_ON()
    } else {
        TAG_FIELD_LED_OFF()
    }
}

static void drop_led_restore(void) {
    m_drop_shown = false;
    if (m_lf_active) {
        set_slot_light_color(RGB_CYAN);
    }
}

// After a field, USB's light pattern comes back, as it would after the port closes. Stock tag mode leaves it off.
static void usb_lights_back(void) {
    if (nrfx_power_usbstatus_get() != NRFX_POWER_USB_STATE_DISCONNECTED) {
        g_usb_led_marquee_enable = true;
    }
}

static void drop_led_poll(void) {
    if (m_drop_shown && now_ms() - m_drop_ms >= FM_DROP_LED_MS) {
        drop_led_restore();
    }
}

void field_measure_lf_up(void) {
    if (!m_lf_active) {
        m_lf_pending = true;
    }
}

static fm_lf_rec_t *next_rec(void) {
    fm_lf_rec_t *r = &m_store.lf[m_store.lf_recs % FM_LF_RECS];
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
    m_field_slot = m_store.lf_fields % FM_LF_FIELDS;
    fm_field_t *f = &m_store.lf_field[m_field_slot];
    f->wake = m_store.wake;
    f->t_ms = m_field_start_ms;
    f->dur_ms = 0xFFFFFFFFu;
    f->drops = 0;
    m_store.lf_fields++;
    m_drops = 0;

    sleep_timer_stop();
    APP_ERROR_CHECK(app_timer_start(m_check_timer, APP_TIMER_TICKS(FM_CHECK_MS), NULL));
    g_is_tag_emulating = true;
    g_usb_led_marquee_enable = false;
    set_slot_light_color(RGB_CYAN);
    TAG_FIELD_LED_ON()
    lf_tag_em_probe_watch(true);
    NRF_LOG_INFO("probe: LF field %d", m_store.lf_fields);
}

static void field_end(uint32_t t) {
    lf_tag_em_probe_watch(false);
    m_lf_active = false;
    app_timer_stop(m_check_timer);
    m_store.lf_field[m_field_slot].dur_ms = t - m_field_start_ms;
    m_store.lf_field[m_field_slot].drops = m_drops;
    g_is_tag_emulating = false;
    m_drop_shown = false;
    set_slot_light_color(RGB_CYAN);
    TAG_FIELD_LED_OFF()
    usb_lights_back();
    lf_tag_em_probe_rearm();
    sleep_timer_start(SLEEP_DELAY_MS_FIELD_125KHZ_LOST);
    NRF_LOG_INFO("probe: LF field gone after %d ms", t - m_field_start_ms);
}

// The probe was switched off during a field: the field's duration stays unknown.
static void field_abandon(void) {
    m_lf_pending = false;
    if (m_lf_active) {
        lf_tag_em_probe_watch(false);
        m_lf_active = false;
        m_store.lf_field[m_field_slot].drops = m_drops;
        m_drop_shown = false;
        app_timer_stop(m_check_timer);
        g_is_tag_emulating = false;
        TAG_FIELD_LED_OFF()
        usb_lights_back();
    }
}

void field_measure_process(void) {
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
    drop_led_poll();
    m_store.lf_field[m_field_slot].drops = m_drops;
    const uint32_t t = now_ms();
    if ((int32_t)(t - m_next_check_ms) < 0) {
        return;
    }
    m_next_check_ms = t + FM_CHECK_MS;
    sleep_timer_stop();
    if (m_round < FM_ROUNDS && t - m_field_start_ms >= m_round_at_ms[m_round] && (int32_t)(t - m_hold_until_ms) >= 0) {
        fm_lf_rec_t *r = next_rec();
        r->wake = m_store.wake;
        r->field = m_store.lf_fields;
        r->t_ms = t;
        r->round = m_round++;
        measure(r);
        bsp_delay_ms(3);
        if (!lf_tag_em_probe_field()) {
            r->flags |= FM_F_ENDED;
        }
        return;
    }
    if (lf_tag_em_probe_field()) {
        m_absent = 0;
    } else if (++m_absent >= FM_ABSENT_CHECKS) {
        field_end(t);
    }
}

bool field_measure_lf_measure_now(fm_lf_rec_t *out) {
    if (!lf_tag_em_probe_ready()) {
        return false;
    }
    fm_lf_rec_t *r = next_rec();
    r->wake = m_store.wake;
    r->field = m_store.lf_fields;
    r->t_ms = now_ms();
    r->flags = FM_F_MANUAL;
    const bool active = m_lf_active;
    measure(r);
    bsp_delay_ms(3);
    if (!lf_tag_em_probe_field()) {
        r->flags |= FM_F_ENDED;
        // With no field, the measurement's own LF_MOD changes can raise LPCOMP's UP, and the main loop would then
        // run a field's rounds on nothing. lf_tag_em_probe_field() also masked UP, so it is armed again here for the
        // next real field.
        if (!active) {
            m_lf_pending = false;
            lf_tag_em_probe_rearm();
        }
    }
    memcpy(out, r, sizeof(*r));
    return true;
}

// ---------------------------------------------------------------- the HF field

static fm_hf_t *volatile m_hf = NULL;

void field_measure_hf_field(bool on) {
    const uint32_t t = now_ms();
    if (on) {
        fm_hf_t *h = &m_store.hf[m_store.hf_fields % FM_HF_FIELDS];
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

void field_measure_hf_frame(const uint8_t *data, uint16_t bits, uint32_t rx_status) {
    fm_hf_t *h = m_hf;
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
