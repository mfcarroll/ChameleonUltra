#include "lf_selftrim_fsk.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "app_timer.h"
#include "ble_main.h"
#include "lf_tag_em.h"
#include "tag_emulation.h"
#include "nrf_saadc.h"
#include "utils/fsktrim.h"
#include "utils/selftrim.h"

#define NRF_LOG_MODULE_NAME lf_selftrim_fsk
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

#define CAPTURE_CC        800                     // 16 MHz / 800 = 20 kHz
#define CAPTURE_FS        (16e6f / CAPTURE_CC)
#define CAPTURE_ACQ       NRF_SAADC_ACQTIME_40US
#define CAPTURE_SAMPLES   SELFTRIM_N_MAX          // 8192: 410 ms
#define RETRY_MS          300                     // after an invalid result, while the field lasts
#define REMEASURE_MS      5000                    // once settled: field time (summed over fields) between checks
#define WAIT_BURST_MS     1500                    // give up waiting for a burst in the measure drive
#define AGREE_PPM         11.4f                   // two results within this agree (PSK: 3 steps of 3.8 ppm)
#define MOVE_PPM          11.4f                   // once trimmed, move only by at least this
#define MATCH_PPM         15.0f                   // reader this close to our nominal clock: no trim
#define BACKOFF_AFTER     3                       // invalid results in a row before backing off
#define BACKOFF_MS        5000                    // then measure at most this often

typedef enum { ST_IDLE = 0, ST_WAIT_BURST = 1, ST_CAPTURING = 2 } st_t;

static int16_t *m_buf;                            // lf_tag_em's 16384-sample alias buffer (also the float work)
static volatile size_t m_len;
static volatile bool m_capturing;
static volatile bool m_ready;
static st_t m_state = ST_IDLE;
static uint32_t m_t_state;                        // app_timer ticks when the state was entered

static bool m_enabled = false;
static int16_t m_residual = 25;                   // ppm, reader minus our trimmed clock (st5/st6: 25 avoids stuck-bad-phase reads)
static uint8_t m_measure_drive = LF_FSK_DRIVE_U1_SAME;

static uint16_t m_det_seen, m_loads_seen;
static bool m_seen_init = false;
static uint32_t m_epoch, m_measured_epoch = 0xFFFFFFFF;
static uint32_t m_last_tick;
static uint32_t m_field_ms;                       // field (emulating) time since the last measurement, summed
static uint32_t m_pass_tick;                      // previous process pass, for m_field_ms
static bool m_pass_emulating;
static bool m_settled, m_have_prev;
static int16_t m_prev;                            // previous valid candidate, steps
static int16_t m_target;                          // trim we want, steps
static uint16_t m_invalid_run, m_count, m_aborts;
static selftrim_result_t m_res;
static int16_t m_last_ppm10, m_last_snr10;
static uint8_t m_last_why;
static uint16_t m_last_ms;

static size_t m_want = CAPTURE_SAMPLES;           // samples this capture takes (phase select: PH_SAMPLES)

static void adc_cb(nrf_saadc_value_t *v, size_t n) {
    if (!m_capturing) {
        return;
    }
    size_t room = m_want - m_len;
    size_t k = n < room ? n : room;
    for (size_t i = 0; i < k; i++) {
        m_buf[m_len + i] = v[i];
    }
    m_len += k;
    if (m_len >= m_want) {
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

static void enter(st_t s) {
    m_state = s;
    m_t_state = app_timer_cnt_get();
}

static uint32_t ms_since(uint32_t tick) {
    return app_timer_cnt_diff_compute(app_timer_cnt_get(), tick) * 1000u / APP_TIMER_CLOCK_FREQ;
}

static void abort_measure(void) {
    if (m_state == ST_CAPTURING || m_ready) {
        stop_adc();
        m_ready = false;
    }
    if (m_state != ST_IDLE) {
        m_aborts++;
        m_last_tick = app_timer_cnt_get();
    }
    lf_tag_em_fsk_measure(false, m_measure_drive);
    enter(ST_IDLE);
}

bool lf_selftrim_fsk_busy(void) {
    return m_state != ST_IDLE;
}

static void reset_decisions(void) {
    m_settled = false;
    m_have_prev = false;
    m_invalid_run = 0;
    m_measured_epoch = 0xFFFFFFFF;
    memset(&m_res, 0, sizeof(m_res));
}

static void set_trim(int16_t t) {
    int16_t applied;
    uint32_t ticks;
    m_target = t;
    lf_tag_em_trim_set(true, t, &applied, &ticks);
}

// As PSK: act only when two consecutive valid results agree (across fields: some readers give a new short field
// per read); once trimmed, move only by at least MOVE_PPM. In FSK units: steps of pps ppm, aiming at residual r.
static void decide(float pps, float applied_ppm) {
    const int16_t agree = (int16_t)ceilf(AGREE_PPM / pps);
    const int16_t move = (int16_t)ceilf(MOVE_PPM / pps);
    bool have = false;
    int16_t cand = 0;
    bool matched = m_res.why == SELFTRIM_MATCHED;
    if (m_res.valid) {
        // already where we want it (reader r ppm from our trimmed clock): same as matched
        if (m_target != 0 && fabsf(m_res.ppm - applied_ppm - (float)m_residual) < MATCH_PPM) {
            matched = true;
        }
        const int16_t t = fabsf(m_res.ppm) < MATCH_PPM ? 0 : (int16_t)lroundf((m_res.ppm - (float)m_residual) / pps);
        if (m_have_prev && abs(t - m_prev) <= agree) {
            cand = (int16_t)lroundf((t + m_prev) / 2.0f);
            have = true;
        }
        m_prev = t;
        m_have_prev = true;
    } else {
        m_have_prev = false;
    }
    m_settled = matched;
    if (have && !matched) {
        const int16_t d = cand - m_target;
        if ((m_target == 0) != (cand == 0) || d >= move || d <= -move) {
            set_trim(cand);
        }
        m_settled = true;
    }
    if (m_res.valid || m_res.why == SELFTRIM_MATCHED) {
        m_invalid_run = 0;
    } else if (m_invalid_run < 0xFFFF) {
        m_invalid_run++;
    }
}

// research exp/fsk-phasecont: phase select (hw lfcont mode 2). lf_tag_em plays a field's first burst in the measure
// drive with the trim detuned by -100 ppm; here the alias of the reader's carrier is captured (PH_SAMPLES at 20 kHz,
// first sample started from TIMER3), its frequency fp found near where the (matched) emulate trim puts the reader, and
// its phase at the window centre taken from a complex DFT. Sampling at 20 kHz = 16 MHz / 800 on TIMER3's clock, the
// alias's phase at sample k is the reader's carrier phase at t0 + 800 k (6 whole cycles of 2 pi per sample drop out),
// and the reader's carrier is 120000 + fp Hz on that clock. The anchor is the TIMER3 tick where the carrier is at the
// chosen phase K / 128 cycle; lf_tag_em places the following restarts on it. Self-trim is idle meanwhile (manual trim).
#define PH_SAMPLES   4000                          // 200 ms: done well inside the 376 ms HID burst
#define PH_SCAN_HZ   2.0f                          // search +/- around the expected reader line
#define PH_SCAN_WIDE_HZ 6.0f                       // the first narrow measurement after a wide lock or a restore
#define PH_STEP_HZ   0.25f
// a capture is 200 ms: 5 Hz bins under the Hann window, whose first sidelobe (-31.5 dB) is 2.5 bins out
#define PH_MIN_SNR   14.0f                         // phase error ~ +-3/128 at 15 dB; the Flipper at 30 mm gives 14-21 dB
typedef enum { PH_IDLE = 0, PH_CAPTURING = 1 } ph_t;
static ph_t m_ph = PH_IDLE;
// the field counter as last seen, from 0 at boot: lf_tag_em applies the kept mode only at the first detection after a
// wake, so the first call here already sees det 1, and that waking field must count as new (its first burst is the
// measure burst, which only this file ends)
static uint16_t m_ph_det_seen;
static bool m_ph_pending;
static uint32_t m_ph_t0, m_ph_tick;
static uint16_t m_ph_count, m_ph_invalid, m_ph_fail, m_ph_ms;
static uint8_t m_ph_k128, m_ph_valid;
static int16_t m_ph_snr10;
// With no reader line in the window, a strong line nearby can read as a valid 14-24 dB line (NOTES-lostreader F4): the
// measure burst's ~75 ms structure puts sidebands ~13-15 Hz either side of the reader's line at about -17 dB (a
// PM3/Flipper pair is ~15 Hz apart), and a line 2-4 bins away leaks in at up to -31.5 dB. The window's peak is not
// valid if a line within PH_LEAK_HZ, away from our own measure line, is PH_LEAK_DB stronger (a reader's own line
// in the window has its main lobe 5 Hz out at -6 dB).
#define PH_LEAK_HZ   25.0f
#define PH_LEAK_STEP 2.5f
#define PH_LEAK_DB   10.0f
static int16_t m_ph_self_snr10, m_ph_other_db10;  // our own measure line's SNR; the strongest other line over the peak
static uint16_t m_ph_leaks;
static uint32_t m_ph_fp_mhz;                       // alias line, mHz
static double m_ph_ph, m_ph_cyc;                   // placed-phase telemetry: phase (cycles) at tick m_ph_tc, cycle
static uint32_t m_ph_tc;
// mode 2: the reader's fp averaged over fields (up to PH_AVG_N), restarted when a field's fp is PH_AVG_JUMP_HZ away
#define PH_AVG_N        16
#define PH_AVG_JUMP_HZ  1.0
static double m_fp_avg;
static uint16_t m_fp_n;
// Mode 9: find and hold the trim. Unlocked (a new reader): the field's capture also goes through the wide detector
// (selftrim_detect, +-300 ppm) and the trim is set to the reader. Locked: a field whose fp is AUTO_GUARD_HZ from the
// average is rejected (no anchor; a corrupted capture can still read "valid", NOTES-burst1 section 7); once
// AUTO_MIN_N fields are averaged the trim follows the average (beyond 0.6 of a step). AUTO_UNLOCK_RUN rejected or
// invalid fields in a row unlock (a different reader).
#define AUTO_GUARD_HZ    0.5
#define AUTO_UNLOCK_RUN  3
#define AUTO_MIN_N       4
#define AUTO_LOST_SNR    10.0f  // locked, and nothing near the expected line: a different reader; search again now
static bool m_auto_remeasure;
#define AUTO_WIDE_SNR    20.0f  // the wide search's floor (default 22 dB is for a 410 ms capture; 16 let an artifact line near -216 ppm lock)
#define AUTO_WIDE_SELF_HZ 2.5f  // +-20 ppm around our own measure line (a result at 11.6 ppm from it recurred)
#define AUTO_WIDE_TRIES  4      // wide searches per field before waiting for the next field
static uint8_t m_auto_tries;
// research exp/fsk-selftrim-wide: the wide search covers +/-SELFTRIM_WIDE_MAX_PPM by FFT (selftrim_detect_fft). The
// 20 kHz capture puts the reader at 5000 Hz + 0.125 Hz/ppm, so the band edges (0 and 10 kHz) are +/-40000 ppm; a real
// reader was found at -4187 (READERS-pavilion-2026-10-28). LF_FSK_TRIM_MAX_PPM must cover it.
#define SELFTRIM_WIDE_MAX_PPM 35000.0f

// Trim (ticks per frame) <-> ppm, exactly: a trim t makes the frame N - t ticks of a nominal N (= the frame as played
// plus the trim in it), so the subcarrier runs N / (N - t) times its rate. The linear t * 1e6 / frame is ~1e-6 ppm^2
// out: 18 ppm at -4200, 1300 at 3.5 %.
static double trim_ppm(const lf_tag_em_fsk_info_t *i, int32_t t) {
    const double n = (double)i->frame_ticks + i->trim_act;
    return 1e6 * (n / (n - t) - 1.0);
}

static int16_t ppm_trim(const lf_tag_em_fsk_info_t *i, double ppm) {
    const double n = (double)i->frame_ticks + i->trim_act;
    return (int16_t)lround(n * (1.0 - 1.0 / (1.0 + ppm * 1e-6)));
}
// exp/fsk-selftrim-wide: the wide search's capture A (auto_wide): its |X[k]|, field and our own line
static float m_wide_a[SELFTRIM_FFT_N / 2];
static bool m_wide_have_a;
static float m_wide_a_own;
// exp/fsk-selftrim-wide: the first narrow measurement after a wide lock or a restore scans PH_SCAN_WIDE_HZ: the wide
// result is good to ~10 ppm (1.2 Hz), but one 23 ppm off (2.9 Hz) missed the +-2 Hz scan and cost 4 more pairs
static bool m_ph_scan_wide;
// exp/fsk-selftrim-wide: what mode 9 did, last WLOG_N events (3100 at offset 0xFFFE; 0xFFFC clears). Kinds: 1 wide A
// stored, 2 wide B searched (why/valid as selftrim), 3 kept reader restored, 4 narrow failed and unlocked (why =
// outcome), 5 narrow locked an unlocked reader, 6 trim moved to follow the average, 7 a new field (why = slot; trim_req
// the trim its first burst plays; ppm the slot's kept reader, 0 none). ppm: B's result, the restored / narrow / new
// trim's reader; own: our own line in that capture. In the noinit RAM (as keep9), so a field test that sleeps between
// presentations keeps it; each event carries the wake it happened in (counted from the first event after each boot) and
// the time since that boot (RTC1, wraps at 512 s).
#define WLOG_N     32
#define WLOG_MAGIC 0x574C4F02u
typedef struct {
    uint8_t kind, why;
    uint16_t det;
    int16_t trim_req;
    int32_t own10, ppm10;
    int16_t snr10;
    uint16_t wake;
    uint16_t ms;
} wlog_t;
typedef struct {
    uint32_t magic;
    uint16_t n;     // events since the log was cleared (wraps); the ring holds the last WLOG_N
    uint16_t wake;  // boots that logged anything
    wlog_t e[WLOG_N];
} wlog_store_t;
static __attribute__((section(".noinit_fsk9log"))) wlog_store_t m_wlog;
static bool m_wlog_woke;  // this boot has counted its wake

static void wlog_clear(void) {
    memset(&m_wlog, 0, sizeof(m_wlog));
    m_wlog.magic = WLOG_MAGIC;
    m_wlog_woke = false;
}

static void wlog(uint8_t kind, uint8_t why, const lf_tag_em_fsk_info_t *i, double own, double ppm, double snr) {
    if (m_wlog.magic != WLOG_MAGIC) {
        wlog_clear();
    }
    if (!m_wlog_woke) {
        m_wlog_woke = true;
        m_wlog.wake++;
    }
    wlog_t *e = &m_wlog.e[m_wlog.n % WLOG_N];
    const uint32_t ms = app_timer_cnt_get() * 1000u / APP_TIMER_CLOCK_FREQ;
    e->kind = kind;
    e->why = why;
    e->det = i->det;
    e->trim_req = i->trim_req;
    e->own10 = (int32_t)lround(own * 10.0);
    e->ppm10 = (int32_t)lround(ppm * 10.0);
    e->snr10 = (int16_t)lround(snr * 10.0);
    e->wake = m_wlog.wake;
    e->ms = (uint16_t)(ms > 0xFFFFu ? 0xFFFFu : ms);
    m_wlog.n++;
}
static bool m_auto_locked;
static uint16_t m_auto_gen_seen = 0xFFFF;
static uint16_t m_auto_moves, m_auto_rejects, m_auto_wides, m_auto_bad_run;
static int16_t m_auto_wide_ppm10;

static uint16_t m_keep_restores;
static bool m_keep_try;  // a new field: its first measurement may start from the slot's kept values
static bool m_keep_fresh;  // locked by a restore, not yet confirmed: its first measurement must be valid and in the guard

// Mode 9: a reader can restart its carrier without dropping the field, and the new carrier's phase has nothing to do
// with the measured one (the Flipper's auto read switches between its ASK and PSK windows every ~2.5 s with 50 ms
// off, which the field check between bursts rarely sees). Measured: a field whose one measurement fell in the PSK
// window then failed or read late in the ASK window (NOTES-stalephase). So the field is measured again (one idle
// burst) PH_REMEASURE_MS after its latest measurement started. A re-measure PH_JUMP_128 from where the previous
// measurement puts the carrier counts as a restart.
#define PH_REMEASURE_MS  2000
#define PH_JUMP_128      16
static uint32_t m_ph_meas_tick;  // the latest measure capture's start
static bool m_ph_remeas;         // the pending measurement is a periodic one
static uint16_t m_ph_remeasures, m_ph_jumps;
static uint8_t m_ph_jump128 = 0xFF;  // the latest re-measure's step from the prediction (1/128 cycle); 0xFF none

// Per-slot store (as the PSK self-trim's E52): what mode 9 learned for each slot's credential, in the noinit RAM that
// is retained in System OFF and cleared only on a cold power-on (app_main.c), so a later presentation after a sleep
// starts locked and narrow at the learned trim. A different reader falls back through the lost-reader path. Also keeps
// hw lfcont's mode and K (otherwise RAM only), so a wake comes back in the same mode. A check value rejects garbage.
#define KEEP9_MAGIC 0x464B3902u
#define KEEP9_SLOTS 8
typedef struct {
    uint16_t type;    // LF tag type learned on
    int16_t trim;     // matched trim, steps
    uint32_t fp_mhz;  // averaged reader line, mHz
    uint16_t n;       // fields averaged
    uint8_t valid;
    uint8_t pad;
} keep9_slot_t;
typedef struct {
    uint32_t magic;
    uint8_t cont_mode, cont_k, cont_set, pad;
    keep9_slot_t slot[KEEP9_SLOTS];
    uint32_t check;
} keep9_t;
static __attribute__((section(".noinit_fsk9"))) keep9_t m_keep9;

static uint32_t keep9_check(void) {
    const uint8_t *b = (const uint8_t *)&m_keep9;
    uint32_t h = 2166136261u;  // FNV-1a over everything before the check
    for (size_t i = 0; i < offsetof(keep9_t, check); i++) {
        h = (h ^ b[i]) * 16777619u;
    }
    return h;
}

static bool keep9_ok(void) {
    return m_keep9.magic == KEEP9_MAGIC && m_keep9.check == keep9_check();
}

static void keep9_init_if_bad(void) {
    if (!keep9_ok()) {
        memset(&m_keep9, 0, sizeof(m_keep9));
        m_keep9.magic = KEEP9_MAGIC;
        m_keep9.check = keep9_check();
    }
}

static void keep9_active(uint8_t *slot, uint16_t *type) {
    tag_slot_specific_type_t types;
    *slot = tag_emulation_get_slot();
    tag_emulation_get_specific_types_by_slot(*slot, &types);
    *type = (uint16_t)types.tag_lf;
}

static void keep9_save_slot(int16_t trim, double fp, uint16_t n) {
    uint8_t slot;
    uint16_t type;
    keep9_active(&slot, &type);
    if (slot >= KEEP9_SLOTS) {
        return;
    }
    keep9_init_if_bad();
    keep9_slot_t *e = &m_keep9.slot[slot];
    e->type = type;
    e->trim = trim;
    e->fp_mhz = (uint32_t)lround(fp * 1000.0);
    e->n = n;
    e->valid = 1;
    m_keep9.check = keep9_check();
}

// The kept trim for the active slot, if any (IRQ-safe: reads only). lf_tag_em applies it before the field's first burst.
bool lf_selftrim_fsk_keep_trim(int16_t *trim) {
    uint8_t slot;
    uint16_t type;
    keep9_active(&slot, &type);
    if (slot >= KEEP9_SLOTS || !keep9_ok() || !m_keep9.slot[slot].valid || m_keep9.slot[slot].type != type) {
        return false;
    }
    *trim = m_keep9.slot[slot].trim;
    return true;
}

void lf_selftrim_fsk_keep_cont(uint8_t mode, uint8_t k) {
    keep9_init_if_bad();
    m_keep9.cont_mode = mode;
    m_keep9.cont_k = k;
    m_keep9.cont_set = 1;
    m_keep9.check = keep9_check();
}

bool lf_selftrim_fsk_kept_cont(uint8_t *mode, uint8_t *k) {
    if (!keep9_ok() || !m_keep9.cont_set) {
        return false;
    }
    *mode = m_keep9.cont_mode;
    *k = m_keep9.cont_k;
    return true;
}

// EXPERIMENT: forget every slot's kept reader (lf_exp_fsk.c's clear). The mode set with hw lfcont stays.
void lf_selftrim_fsk_keep_clear(void) {
    keep9_init_if_bad();
    memset(m_keep9.slot, 0, sizeof(m_keep9.slot));
    m_keep9.check = keep9_check();
}

// EXPERIMENT: what mode 9 kept for a slot (lf_exp_fsk.c's report). False if nothing.
bool lf_selftrim_fsk_kept_slot(uint8_t slot, uint16_t *type, int16_t *trim, uint32_t *fp_mhz, uint16_t *n) {
    if (slot >= KEEP9_SLOTS || !keep9_ok() || !m_keep9.slot[slot].valid) {
        return false;
    }
    *type = m_keep9.slot[slot].type;
    *trim = m_keep9.slot[slot].trim;
    *fp_mhz = m_keep9.slot[slot].fp_mhz;
    *n = m_keep9.slot[slot].n;
    return true;
}

// Thread, at a new field in mode 9 while unlocked: start from the kept average, locked (no wide search).
static bool keep9_restore(void) {
    uint8_t slot;
    uint16_t type;
    keep9_active(&slot, &type);
    if (slot >= KEEP9_SLOTS || !keep9_ok() || !m_keep9.slot[slot].valid || m_keep9.slot[slot].type != type) {
        return false;
    }
    const keep9_slot_t *e = &m_keep9.slot[slot];
    m_fp_avg = e->fp_mhz / 1000.0;
    m_fp_n = e->n < AUTO_MIN_N ? e->n : AUTO_MIN_N;
    m_auto_locked = true;
    m_auto_bad_run = 0;
    m_wide_have_a = false;
    m_keep_fresh = true;
    m_keep_restores++;
    return true;
}

static float ph_mag(const float *y, uint16_t n, float f, float *re_out, float *im_out) {
    // complex DFT at f: X = sum y_k exp(-i w k), the phasor by rotation (renormalised every 512)
    const float w = 2.0f * 3.14159265358979f * f / CAPTURE_FS;
    const float rc = cosf(w), rs = sinf(w);
    float c = 1.0f, sn = 0.0f, re = 0.0f, im = 0.0f;
    for (uint16_t k = 0; k < n; k++) {
        re += y[k] * c;
        im -= y[k] * sn;
        const float t = c * rc - sn * rs;
        sn = sn * rc + c * rs;
        c = t;
        if ((k & 511) == 511) {
            const float g = 1.0f / sqrtf(c * c + sn * sn);
            c *= g;
            sn *= g;
        }
    }
    if (re_out) {
        *re_out = re;
        *im_out = im;
    }
    return sqrtf(re * re + im * im);
}

// Research (command 3100): the raw capture of the latest first measurement after a restore, and what it found (F4: a
// valid-looking line in a restored window with no reader there).
static int16_t m_rdump[PH_SAMPLES];
static struct {
    uint8_t valid, outcome;  // outcome: 0 valid, 1 invalid (SNR, edge), 2 leakage, 3 outside the guard
    uint16_t det;
    int16_t trim_req, trim_act;
    uint32_t frame_ticks, f_exp_mhz, fp_mhz;
    int16_t snr10;
    int8_t bi;
    int16_t other_db10, self_snr10;
} m_rdump_meta;

uint16_t lf_selftrim_fsk_rdump(uint16_t off, uint16_t cnt, uint8_t *o) {
    uint16_t n = 0;
    if (off == 0xFFFC) {  // clear the event log
        wlog_clear();
        return 0;
    }
    if (off == 0xFFFE) {  // exp/fsk-selftrim-wide: the event log, oldest first: count u16, entry size u8, wake u16, then
                          // 20 bytes per event (kind, why, det, trim_req, own10, ppm10, snr10, wake, ms; big-endian)
        const bool ok = m_wlog.magic == WLOG_MAGIC;
        const uint16_t total = ok ? m_wlog.n : 0;
        o[n++] = (uint8_t)(total >> 8);
        o[n++] = (uint8_t)total;
        o[n++] = 20;
        o[n++] = (uint8_t)((ok ? m_wlog.wake : 0) >> 8);
        o[n++] = (uint8_t)(ok ? m_wlog.wake : 0);
        const uint16_t have = total < WLOG_N ? total : WLOG_N;
        for (uint16_t k = 0; k < have; k++) {
            const wlog_t *e = &m_wlog.e[(uint16_t)(total - have + k) % WLOG_N];
            const uint32_t w[2] = {(uint32_t)e->own10, (uint32_t)e->ppm10};
            o[n++] = e->kind;
            o[n++] = e->why;
            o[n++] = (uint8_t)(e->det >> 8);
            o[n++] = (uint8_t)e->det;
            o[n++] = (uint8_t)((uint16_t)e->trim_req >> 8);
            o[n++] = (uint8_t)e->trim_req;
            for (int j = 0; j < 2; j++) {
                o[n++] = (uint8_t)(w[j] >> 24);
                o[n++] = (uint8_t)(w[j] >> 16);
                o[n++] = (uint8_t)(w[j] >> 8);
                o[n++] = (uint8_t)w[j];
            }
            o[n++] = (uint8_t)((uint16_t)e->snr10 >> 8);
            o[n++] = (uint8_t)e->snr10;
            o[n++] = (uint8_t)(e->wake >> 8);
            o[n++] = (uint8_t)e->wake;
            o[n++] = (uint8_t)(e->ms >> 8);
            o[n++] = (uint8_t)e->ms;
        }
        return n;
    }
    if (off == 0xFFFF) {
        o[n++] = m_rdump_meta.valid;
        o[n++] = m_rdump_meta.outcome;
        o[n++] = (uint8_t)(m_rdump_meta.det >> 8);
        o[n++] = (uint8_t)m_rdump_meta.det;
        o[n++] = (uint8_t)((uint16_t)m_rdump_meta.trim_req >> 8);
        o[n++] = (uint8_t)m_rdump_meta.trim_req;
        o[n++] = (uint8_t)((uint16_t)m_rdump_meta.trim_act >> 8);
        o[n++] = (uint8_t)m_rdump_meta.trim_act;
        const uint32_t w[3] = {m_rdump_meta.frame_ticks, m_rdump_meta.f_exp_mhz, m_rdump_meta.fp_mhz};
        for (int k = 0; k < 3; k++) {
            for (int b = 3; b >= 0; b--) {
                o[n++] = (uint8_t)(w[k] >> (8 * b));
            }
        }
        o[n++] = (uint8_t)((uint16_t)m_rdump_meta.snr10 >> 8);
        o[n++] = (uint8_t)m_rdump_meta.snr10;
        o[n++] = (uint8_t)m_rdump_meta.bi;
        o[n++] = (uint8_t)((uint16_t)m_rdump_meta.other_db10 >> 8);
        o[n++] = (uint8_t)m_rdump_meta.other_db10;
        o[n++] = (uint8_t)((uint16_t)m_rdump_meta.self_snr10 >> 8);
        o[n++] = (uint8_t)m_rdump_meta.self_snr10;
        o[n++] = (uint8_t)(PH_SAMPLES >> 8);
        o[n++] = (uint8_t)PH_SAMPLES;
        return n;  // 29
    }
    for (uint16_t k = off; k < PH_SAMPLES && k < (uint32_t)off + cnt; k++) {
        o[n++] = (uint8_t)((uint16_t)m_rdump[k] >> 8);
        o[n++] = (uint8_t)m_rdump[k];
    }
    return n;
}

static void ph_compute(const lf_tag_em_fsk_info_t *i, uint8_t k128) {
    const uint16_t n = PH_SAMPLES;
    if (m_keep_fresh) {
        memcpy(m_rdump, m_buf, sizeof(m_rdump));  // before the in-place transform below
        m_rdump_meta.valid = 1;
        m_rdump_meta.det = i->det;
        m_rdump_meta.trim_req = i->trim_req;
        m_rdump_meta.trim_act = i->trim_act;
        m_rdump_meta.frame_ticks = i->frame_ticks;
    }
    float *y = (float *)(void *)m_buf;  // in place: filled from the end (as selftrim_detect)
    float mean = 0.0f;
    for (uint16_t j = 0; j < n; j++) {
        mean += m_buf[j];
    }
    mean /= n;
    for (uint16_t j = 0; j < n; j++) {
        const uint16_t q = (uint16_t)(n - 1u - j);
        const float xq = m_buf[q];
        __asm__ __volatile__("" ::: "memory");
        y[q] = (xq - mean) * (0.5f - 0.5f * cosf(2.0f * 3.14159265358979f * q / (n - 1)));
    }
    // the reader sits where the emulate trim (assumed matched) puts our own line: 5000 Hz + ppm * 0.125 Hz
    const float f_exp = 5000.0f + (float)trim_ppm(i, i->trim_req) * 0.125f;
    const int ns = (int)((m_ph_scan_wide ? PH_SCAN_WIDE_HZ : PH_SCAN_HZ) / PH_STEP_HZ);
    m_ph_scan_wide = false;
    float bm = 0.0f, ma = 0.0f, mc = 0.0f;
    int bi = 0;
    float prev = 0.0f;
    for (int j = -ns; j <= ns; j++) {
        const float m = ph_mag(y, n, f_exp + j * PH_STEP_HZ, NULL, NULL);
        if (m > bm) {
            bm = m;
            bi = j;
            ma = prev;
            mc = 0.0f;
        } else if (j == bi + 1) {
            mc = m;
        }
        prev = m;
    }
    float fp = f_exp + bi * PH_STEP_HZ;
    if (bi > -ns && bi < ns && ma > 0.0f && mc > 0.0f) {
        const float la = logf(ma), lb = logf(bm), lc = logf(mc);
        const float d = la - 2.0f * lb + lc;
        if (d < 0.0f) {
            fp += 0.5f * (la - lc) / d * PH_STEP_HZ;
        }
    }
    // noise: mean magnitude at 8 points 40-70 Hz above the expected line
    float nsum = 0.0f;
    int nn = 0;
    for (int j = 0; j < 8; j++) {
        const float f = f_exp + 40.0f + j * 3.9f;
        nsum += ph_mag(y, n, f, NULL, NULL);
        nn++;
    }
    const float noise = nsum / nn;
    float re, im;
    const float pk = ph_mag(y, n, fp, &re, &im);
    const float snr = noise > 0.0f ? 20.0f * log10f(pk / noise) : 99.0f;
    m_ph_snr10 = (int16_t)lroundf(snr * 10.0f);
    m_ph_fp_mhz = (uint32_t)lroundf(fp * 1000.0f);
    m_ph_count++;
    // our own line: the measure burst's trim puts it at 5000 Hz + ppm * 0.125 Hz, the detune away from f_exp
    const float f_self = 5000.0f + (float)trim_ppm(i, i->trim_act) * 0.125f;
    const float self = ph_mag(y, n, f_self, NULL, NULL);
    m_ph_self_snr10 = (int16_t)lroundf(noise > 0.0f && self > 0.0f ? 200.0f * log10f(self / noise) : 990.0f);
    float other = 0.0f;
    for (float d = 2.0f * PH_LEAK_STEP; d <= PH_LEAK_HZ + 0.01f; d += PH_LEAK_STEP) {
        for (int s = -1; s <= 1; s += 2) {
            const float f = f_exp + (float)s * d;
            if (fabsf(f - f_self) >= 8.0f) {  // our own line's main lobe (+-10 Hz) is not another reader
                const float m = ph_mag(y, n, f, NULL, NULL);
                other = m > other ? m : other;
            }
        }
    }
    m_ph_other_db10 = (int16_t)lroundf(pk > 0.0f && other > 0.0f ? 200.0f * log10f(other / pk) : -990.0f);
    const bool leak = other > pk * powf(10.0f, PH_LEAK_DB / 20.0f);
    if (leak && snr >= PH_MIN_SNR && bi != -ns && bi != ns) {
        m_ph_leaks++;  // would have been valid
    }
    uint16_t gen;
    const bool autom = lf_tag_em_cont_auto(&gen);
    // a restored entry is the last reader, maybe not this one: if its window has no clean line (a weak line above
    // AUTO_LOST_SNR, or one outside the guard), search now, or each later field restores it again
    const bool fresh = m_keep_fresh;
    m_keep_fresh = false;
    if (fresh) {
        m_rdump_meta.f_exp_mhz = (uint32_t)lroundf(f_exp * 1000.0f);
        m_rdump_meta.fp_mhz = m_ph_fp_mhz;
        m_rdump_meta.snr10 = m_ph_snr10;
        m_rdump_meta.bi = (int8_t)bi;
        m_rdump_meta.other_db10 = m_ph_other_db10;
        m_rdump_meta.self_snr10 = m_ph_self_snr10;
        m_rdump_meta.outcome = (snr < PH_MIN_SNR || bi == -ns || bi == ns) ? 1u
                               : leak                                      ? 2u
                               : (autom && m_fp_n >= AUTO_MIN_N && fabs((double)fp - m_fp_avg) > AUTO_GUARD_HZ) ? 3u
                                                                                                                 : 0u;
    }
    if (snr < PH_MIN_SNR || bi == -ns || bi == ns || leak) {
        m_ph_invalid++;
        m_ph_valid = 0;
        if (autom && m_auto_locked && (snr < AUTO_LOST_SNR || fresh || leak)) {
            m_auto_locked = false;  // no line where the trim expects the reader: wide search on the next burst
            m_auto_remeasure = true;
            m_auto_bad_run = 0;
            wlog(4, leak ? 2u : 1u, i, (f_self - 5000.0f) / 0.125f, (fp - 5000.0f) / 0.125f, snr);
        } else if (autom && ++m_auto_bad_run >= AUTO_UNLOCK_RUN) {
            m_auto_locked = false;
            wlog(4, leak ? 2u : 1u, i, (f_self - 5000.0f) / 0.125f, (fp - 5000.0f) / 0.125f, snr);
        }
        return;
    }
    if (autom && m_fp_n >= AUTO_MIN_N && fabs((double)fp - m_fp_avg) > AUTO_GUARD_HZ) {
        m_auto_rejects++;
        m_ph_valid = 0;
        if (fresh) {
            m_auto_locked = false;  // as an invalid line above
            m_auto_remeasure = true;
            m_auto_bad_run = 0;
            wlog(4, 3, i, (f_self - 5000.0f) / 0.125f, (fp - 5000.0f) / 0.125f, snr);
        } else if (++m_auto_bad_run >= AUTO_UNLOCK_RUN) {
            m_auto_locked = false;
            wlog(4, 3, i, (f_self - 5000.0f) / 0.125f, (fp - 5000.0f) / 0.125f, snr);
        }
        return;
    }
    m_auto_bad_run = 0;
    if (autom && !m_auto_locked) {
        // a valid narrow line where the trim expects the reader is a lock too (at a weak coupling the wide search may
        // never get two agreeing results)
        m_auto_locked = true;
        m_wide_have_a = false;
        wlog(5, 0, i, (f_self - 5000.0f) / 0.125f, (fp - 5000.0f) / 0.125f, snr);
    }
    // phase of the line at the window centre k_c = (n - 1) / 2, in cycles
    const double kc = (n - 1) / 2.0;
    double ph = (atan2((double)im, (double)re) + 2.0 * 3.14159265358979 * (double)fp / CAPTURE_FS * kc) /
                (2.0 * 3.14159265358979);
    ph -= floor(ph);
    m_ph_k128 = (uint8_t)((int)lround(ph * 128.0) & 127);
    if (m_fp_n == 0 || fabs((double)fp - m_fp_avg) > PH_AVG_JUMP_HZ) {
        m_fp_avg = fp;
        m_fp_n = 1;
    } else {
        if (m_fp_n < 0xFFFF) {
            m_fp_n++;
        }
        m_fp_avg += ((double)fp - m_fp_avg) / (m_fp_n < PH_AVG_N ? m_fp_n : PH_AVG_N);
    }
    if (autom) {
        // coarse: the wide search is good to ~10 ppm, so act on the first narrow result if it is > 1.5 steps off;
        // fine: once AUTO_MIN_N fields are averaged, beyond 0.6 of a step
        const double step_hz = 125000.0 / (double)i->frame_ticks;  // alias Hz per trim step
        const int16_t want = ppm_trim(i, (m_fp_avg - 5000.0) / 0.125);
        const double off = fabs(m_fp_avg - (5000.0 + trim_ppm(i, i->trim_req) * 0.125));
        int16_t trim_now = i->trim_req;
        if (want != i->trim_req && (off > 1.5 * step_hz || (m_fp_n >= AUTO_MIN_N && off > 0.6 * step_hz))) {
            set_trim(want);
            m_auto_moves++;
            trim_now = want;
            wlog(6, 0, i, trim_ppm(i, want), (m_fp_avg - 5000.0) / 0.125, (double)m_fp_n);
        }
        keep9_save_slot(trim_now, m_fp_avg, m_fp_n);  // the slot's credential remembers this reader
    }
    const double fr_hz = lf_tag_em_cont_period() == 0 ? m_fp_avg : (double)fp;  // the period's source
    const double cyc = 16e6 / (120000.0 + fr_hz);  // reader cycle, TIMER3 ticks
    const double tc = (double)m_ph_t0 + kc * CAPTURE_CC;  // integer: kc * 800 = 1599600
    m_ph_ph = ph;
    m_ph_cyc = cyc;
    m_ph_tc = m_ph_t0 + (uint32_t)(kc * CAPTURE_CC);
    double fr = (double)k128 / 128.0 - ph;
    fr -= floor(fr);
    const double a = fr * cyc;  // ticks after tc
    const uint64_t a_int = (uint64_t)(uint32_t)((uint32_t)tc + (uint32_t)floor(a));
    const uint64_t a_q32 = (a_int << 32) | (uint64_t)((a - floor(a)) * 4294967296.0);
    lf_tag_em_cont_anchor_set(a_q32, (uint64_t)(cyc * 4294967296.0));
    m_ph_valid = 1;
}

static void ph_stop(void) {
    if (m_ph == PH_CAPTURING) {
        stop_adc();
        m_ready = false;
    }
    m_ph = PH_IDLE;
    lf_tag_em_fsk_measure(false, 0xFF);
}

// Mode 9, unlocked: the wide detector on a copy of the capture (ph_compute then reuses the raw samples). On a valid
// result the trim is set to the reader and the narrow search below (f_exp from trim_req) looks there.
// exp/fsk-selftrim-wide: in pairs within a field. Capture A keeps its spectrum and asks for the next measure burst with
// our tone at another rate (lf_tag_em_fsk_measure_alt); capture B, in this field or a later one (a PM3's fields are
// short), is searched against it, so only lines that did not move with our tone count. A and B from two different
// readers have nothing in common: no lock, and the next capture starts a new pair. A valid B result locks: the pair replaces the two agreeing results the +-300 search waited
// for. The 3100 dump holds capture B.
#define WIDE_PAIR_MIN_PPM 2000.0f  // our tone must have moved this much between A and B (the pair is 4000 apart)
static void auto_wide(lf_tag_em_fsk_info_t *iv) {
    size_t cap;
    int16_t *buf = lf_tag_em_alias_buffer(&cap);
    if (cap < 16384u) {
        return;
    }
    int16_t *x = buf + 12288;  // copy: 4000 samples; the detector's floats go to buf[4096..12288)
    memcpy(x, buf, PH_SAMPLES * sizeof(int16_t));
    float *work = (float *)(void *)(buf + 4096);
    const float own = (float)trim_ppm(iv, iv->trim_act);  // our own line in this capture (the measure detune included)
    if (!m_wide_have_a || fabsf(own - m_wide_a_own) < WIDE_PAIR_MIN_PPM) {
        // capture A (or a B whose tone did not move: a new A); the next one at the other rate
        selftrim_fft_mags(x, PH_SAMPLES, work);
        memcpy(m_wide_a, work, sizeof(m_wide_a));
        m_wide_have_a = true;
        m_wide_a_own = own;
        lf_tag_em_fsk_measure_alt(iv->trim_act <= iv->trim_req);  // detuned down now (the usual): up next
        wlog(1, 0, iv, own, 0.0, 0.0);
        return;
    }
    m_wide_have_a = false;
    lf_tag_em_fsk_measure_alt(false);
    memcpy(m_rdump, x, sizeof(m_rdump));  // 3100: the latest wide search's capture (outcome 4)
    selftrim_result_t r;
    const float floor_db = g_selftrim_min_snr_db;
    g_selftrim_min_snr_db = AUTO_WIDE_SNR;
    const float self_hz = g_selftrim_self_hz;
    g_selftrim_self_hz = AUTO_WIDE_SELF_HZ;
    selftrim_detect_fft(x, PH_SAMPLES, CAPTURE_FS, 125000.0f, own, m_wide_a_own, SELFTRIM_WIDE_MAX_PPM, m_wide_a, work,
                        &r);
    g_selftrim_min_snr_db = floor_db;
    g_selftrim_self_hz = self_hz;
    m_auto_wides++;
    wlog(2, (uint8_t)(r.why | (r.valid ? 0x80 : 0)), iv, own, r.ppm, r.snr_db);
    const float p10 = r.ppm * 10.0f;  // s16 telemetry: +/-3276.7 ppm, saturated
    m_auto_wide_ppm10 = (int16_t)(p10 > 32767.0f ? 32767 : p10 < -32767.0f ? -32767 : lroundf(p10));
    m_rdump_meta.valid = 1;
    m_rdump_meta.outcome = 4;
    m_rdump_meta.det = iv->det;
    m_rdump_meta.trim_req = iv->trim_req;
    m_rdump_meta.trim_act = iv->trim_act;
    m_rdump_meta.frame_ticks = iv->frame_ticks;
    m_rdump_meta.f_exp_mhz = (uint32_t)lroundf((5000.0f + 0.125f * m_wide_a_own) * 1000.0f);  // capture A's own line
    m_rdump_meta.fp_mhz = (uint32_t)lroundf(r.fpk * 1000.0f);
    m_rdump_meta.snr10 = (int16_t)lroundf(r.snr_db * 10.0f);
    m_rdump_meta.bi = (int8_t)r.why;
    m_rdump_meta.other_db10 = r.valid;
    m_rdump_meta.self_snr10 = 0;
    if (r.valid) {
        const float ppm = r.ppm;
        m_wide_have_a = false;
        const int16_t t = ppm_trim(iv, ppm);
        set_trim(t);
        iv->trim_req = t;
        m_fp_n = 0;  // a new reader: restart the average
        m_auto_locked = true;
        m_ph_scan_wide = true;
        m_auto_bad_run = 0;
        // keep it now: the narrow measurement that confirms it needs another burst in this field, and a short field
        // that ends first would otherwise leave the old reader's entry to be restored at every field
        keep9_save_slot(t, 5000.0 + 0.125 * (double)ppm, 1);
    }
}

static void ph_process(const lf_tag_em_fsk_info_t *i, uint8_t k128) {
    if (i->det != m_ph_det_seen) {
        m_ph_det_seen = i->det;
        m_ph_pending = true;  // a new field: lf_tag_em plays its first burst in the measure drive
        m_auto_tries = 0;
        m_keep_try = true;
        m_ph_remeas = false;
        uint16_t g;
        if (lf_tag_em_cont_auto(&g)) {
            uint8_t slot;
            uint16_t type;
            keep9_active(&slot, &type);
            double kept = 0.0;
            if (slot < KEEP9_SLOTS && keep9_ok() && m_keep9.slot[slot].valid && m_keep9.slot[slot].type == type) {
                kept = (m_keep9.slot[slot].fp_mhz / 1000.0 - 5000.0) / 0.125;
            }
            wlog(7, slot, i, trim_ppm(i, i->trim_act), kept, 0.0);  // own: the trim the first burst plays
        }
        m_ph_valid = 0;       // the measurement (and phase_at) is per field: TIMER3 restarts at each field
        m_ph_tick = app_timer_cnt_get();
        if (m_ph == PH_CAPTURING) {
            stop_adc();
            m_ready = false;
            m_ph = PH_IDLE;
        }
    }
    if (!i->emulating) {
        if (m_ph != PH_IDLE || m_ph_pending) {
            ph_stop();
            m_ph_pending = false;
        }
        return;
    }
    if (m_ph == PH_IDLE) {
        if (!m_ph_pending) {
            uint16_t g;
            if (lf_tag_em_cont_auto(&g) && ms_since(m_ph_meas_tick) >= PH_REMEASURE_MS) {
                m_ph_remeasures++;
                m_ph_remeas = true;
                lf_tag_em_fsk_measure(true, 0xFF);  // the next burst is an idle measure burst
                m_ph_pending = true;
                m_ph_tick = app_timer_cnt_get();
            }
            return;
        }
        if (lf_tag_em_fsk_measure_playing()) {
            size_t cap;
            m_buf = lf_tag_em_alias_buffer(&cap);
            m_len = 0;
            m_want = PH_SAMPLES;
            m_ready = false;
            m_capturing = true;
            register_lf_adc_callback_input(adc_cb, NRF_SAADC_INPUT_AIN0, CAPTURE_ACQ);
            nrf_saadc_continuous_mode_enable(CAPTURE_CC);
            m_ph_pending = false;
            if (!lf_tag_em_cont_saadc_start(&m_ph_t0)) {
                m_ph_fail++;
                m_capturing = false;
                stop_adc();
                lf_tag_em_fsk_measure(false, 0xFF);
                return;
            }
            m_ph = PH_CAPTURING;
            m_ph_tick = app_timer_cnt_get();
            m_ph_meas_tick = m_ph_tick;
        } else if (ms_since(m_ph_tick) > WAIT_BURST_MS) {
            m_ph_fail++;
            m_ph_pending = false;
            lf_tag_em_fsk_measure(false, 0xFF);
        }
        return;
    }
    if (!m_ready) {
        if (ms_since(m_ph_tick) > 1000) {
            m_ph_fail++;
            ph_stop();
        }
        return;
    }
    stop_adc();
    m_ready = false;
    m_ph = PH_IDLE;
    const uint32_t t = app_timer_cnt_get();
    lf_tag_em_fsk_info_t iv = *i;
    uint16_t gen;
    if (lf_tag_em_cont_auto(&gen)) {
        if (gen != m_auto_gen_seen) {
            m_auto_gen_seen = gen;
            m_auto_locked = false;
        }
        if (!m_auto_locked && m_keep_try && keep9_restore()) {
            // the slot remembers a reader: locked and narrow from this first burst (lf_tag_em applied the trim)
            int16_t kt;
            if (lf_selftrim_fsk_keep_trim(&kt)) {
                iv.trim_req = kt;
            }
            m_ph_scan_wide = true;
            wlog(3, 0, &iv, trim_ppm(&iv, iv.trim_act), (m_fp_avg - 5000.0) / 0.125, 0.0);
        }
        m_keep_try = false;
        if (!m_auto_locked) {
            auto_wide(&iv);
            if (!m_auto_locked && ++m_auto_tries < AUTO_WIDE_TRIES) {
                // nothing found: try again on the next burst of this field (a reader's field may never drop)
                m_ph_ms = (uint16_t)ms_since(t);
                m_ph_valid = 0;
                lf_tag_em_fsk_measure(false, 0xFF);
                lf_tag_em_fsk_measure(true, 0xFF);
                m_ph_pending = true;
                m_ph_tick = app_timer_cnt_get();
                return;
            }
            if (m_auto_locked) {
                // this field's burst was built at the old trim: our own line may sit near the reader's and pull
                // the narrow result; no anchor and no average from it. Measure again on the next burst (at the new
                // trim), so a reader whose field never drops (one long field) still gets an anchor.
                m_ph_ms = (uint16_t)ms_since(t);
                m_ph_valid = 0;
                lf_tag_em_fsk_measure(false, 0xFF);  // clears "playing": the capture waits for the next burst
                lf_tag_em_fsk_measure(true, 0xFF);
                m_ph_pending = true;
                m_ph_tick = app_timer_cnt_get();
                return;
            }
        }
    }
    m_auto_remeasure = false;
    const bool remeas = m_ph_remeas && m_ph_valid;
    const double old_ph = m_ph_ph, old_cyc = m_ph_cyc;
    const uint32_t old_tc = m_ph_tc;
    m_ph_remeas = false;
    ph_compute(&iv, k128);
    if (remeas && m_ph_valid && old_cyc > 0.0) {
        double d = m_ph_ph - (old_ph + (double)(int32_t)(m_ph_tc - old_tc) / old_cyc);
        d -= floor(d + 0.5);
        m_ph_jump128 = (uint8_t)lround(fabs(d) * 128.0);
        if (m_ph_jump128 >= PH_JUMP_128) {
            m_ph_jumps++;
        }
    }
    m_ph_ms = (uint16_t)ms_since(t);
    lf_tag_em_fsk_measure(false, 0xFF);  // the next burst emulates, on the anchor
    if (m_auto_remeasure) {
        lf_tag_em_fsk_measure(true, 0xFF);  // ...unless the reader was lost: measure (wide) on the next burst
        m_ph_pending = true;
        m_ph_tick = app_timer_cnt_get();
    }
}

uint8_t lf_selftrim_fsk_phase_at(uint32_t cc) {
    if (!m_ph_valid || m_ph_cyc <= 0.0) {
        return 0xFF;
    }
    double p = m_ph_ph + (double)(int32_t)(cc - m_ph_tc) / m_ph_cyc;
    p -= floor(p);
    return (uint8_t)((int)lround(p * 128.0) & 127);
}

uint16_t lf_selftrim_fsk_phsel_status(uint8_t *o) {
    uint16_t n = 0;
    o[n++] = m_ph_valid;
    o[n++] = m_ph_k128;
    o[n++] = (uint8_t)(m_ph_count >> 8);
    o[n++] = (uint8_t)m_ph_count;
    o[n++] = (uint8_t)(m_ph_invalid >> 8);
    o[n++] = (uint8_t)m_ph_invalid;
    o[n++] = (uint8_t)(m_ph_fail >> 8);
    o[n++] = (uint8_t)m_ph_fail;
    o[n++] = (uint8_t)((uint16_t)m_ph_snr10 >> 8);
    o[n++] = (uint8_t)m_ph_snr10;
    for (int b = 3; b >= 0; b--) {
        o[n++] = (uint8_t)(m_ph_fp_mhz >> (8 * b));
    }
    o[n++] = (uint8_t)(m_ph_ms >> 8);
    o[n++] = (uint8_t)m_ph_ms;
    return n;  // LF_SELFTRIM_FSK_PHSEL_LEN
}

uint16_t lf_selftrim_fsk_auto_status(uint8_t *o) {
    uint16_t n = 0;
    o[n++] = m_auto_locked;
    o[n++] = (uint8_t)(m_auto_moves >> 8);
    o[n++] = (uint8_t)m_auto_moves;
    o[n++] = (uint8_t)(m_auto_rejects >> 8);
    o[n++] = (uint8_t)m_auto_rejects;
    o[n++] = (uint8_t)((uint16_t)m_auto_wide_ppm10 >> 8);
    o[n++] = (uint8_t)m_auto_wide_ppm10;
    o[n++] = (uint8_t)(m_auto_wides >> 8);
    o[n++] = (uint8_t)m_auto_wides;
    int16_t kt = 0;
    const bool kv = lf_selftrim_fsk_keep_trim(&kt);
    o[n++] = kv;                                  // v7: the active slot has a kept reader
    o[n++] = (uint8_t)((uint16_t)kt >> 8);        // its trim
    o[n++] = (uint8_t)kt;
    o[n++] = (uint8_t)(m_keep_restores >> 8);     // fields started from the kept values
    o[n++] = (uint8_t)m_keep_restores;
    o[n++] = (uint8_t)((uint16_t)m_ph_self_snr10 >> 8);   // v8: our own measure line, dB x10 over the noise
    o[n++] = (uint8_t)m_ph_self_snr10;
    o[n++] = (uint8_t)((uint16_t)m_ph_other_db10 >> 8);  // the strongest other line near the window over its peak
    o[n++] = (uint8_t)m_ph_other_db10;
    o[n++] = (uint8_t)(m_ph_leaks >> 8);                  // measurements refused as leakage
    o[n++] = (uint8_t)m_ph_leaks;
    o[n++] = (uint8_t)(m_ph_remeasures >> 8);             // v9: periodic re-measures in a field
    o[n++] = (uint8_t)m_ph_remeasures;
    o[n++] = (uint8_t)(m_ph_jumps >> 8);                  // re-measures that found the carrier restarted
    o[n++] = (uint8_t)m_ph_jumps;
    o[n++] = m_ph_jump128;                                // the latest re-measure's step (1/128 cycle)
    return n;  // 25
}

uint16_t lf_selftrim_fsk_phavg_status(uint8_t *o) {
    const uint32_t mhz = (uint32_t)lround(m_fp_avg * 1000.0);
    uint16_t n = 0;
    for (int b = 3; b >= 0; b--) {
        o[n++] = (uint8_t)(mhz >> (8 * b));
    }
    o[n++] = (uint8_t)(m_fp_n >> 8);
    o[n++] = (uint8_t)m_fp_n;
    return n;  // 6
}

void lf_selftrim_fsk_process(void) {
    lf_tag_em_fsk_info_t i;
    lf_tag_em_fsk_info(&i);
    {
        uint8_t k128;
        if (lf_tag_em_cont_phsel(&k128) && i.fsk) {
            if (m_state != ST_IDLE) {
                abort_measure();
            }
            ph_process(&i, k128);
            return;
        }
        if (m_ph != PH_IDLE || m_ph_pending) {
            ph_stop();
            m_ph_pending = false;
        }
    }
    if (!m_seen_init) {
        m_det_seen = i.det;
        m_loads_seen = i.loads;
        m_seen_init = true;
    }
    if (i.loads != m_loads_seen) {  // the slot was (re)loaded: the reader may differ, start over
        m_loads_seen = i.loads;
        abort_measure();
        reset_decisions();
        if (m_enabled) {
            set_trim(0);
        }
    }
    if (m_state == ST_IDLE) {
        m_target = i.trim_req;  // follow manual hw lftrim changes
    }
    if (!m_enabled || !i.fsk) {
        if (m_state != ST_IDLE) {
            abort_measure();
        }
        return;
    }
    if (i.det != m_det_seen) {
        m_det_seen = i.det;
        m_epoch++;
    }
    {
        const uint32_t now = app_timer_cnt_get();
        if (i.emulating && m_pass_emulating) {
            m_field_ms += app_timer_cnt_diff_compute(now, m_pass_tick) * 1000u / APP_TIMER_CLOCK_FREQ;
        }
        m_pass_tick = now;
        m_pass_emulating = i.emulating;
    }
    if (!i.emulating) {
        if (m_state != ST_IDLE) {
            abort_measure();  // the field went away
        }
        return;
    }
    switch (m_state) {
        case ST_IDLE: {
            if (i.frame_ticks == 0) {
                return;
            }
            const uint32_t since = ms_since(m_last_tick);
            bool due;
            if (m_invalid_run >= BACKOFF_AFTER) {
                due = since >= BACKOFF_MS;
            } else if (m_settled) {
                // a learned trim is kept across fields: don't spend the start of every presentation (a reader's
                // read) in the measure drive; check again only after REMEASURE_MS of field time
                due = m_field_ms >= REMEASURE_MS;
            } else {
                due = m_measured_epoch != m_epoch || since >= RETRY_MS;
            }
            if (!due) {
                return;
            }
            m_measured_epoch = m_epoch;
            m_field_ms = 0;
            lf_tag_em_fsk_measure(true, m_measure_drive);
            enter(ST_WAIT_BURST);
            return;
        }
        case ST_WAIT_BURST: {
            if (lf_tag_em_fsk_measure_playing()) {
                size_t cap;
                m_buf = lf_tag_em_alias_buffer(&cap);
                m_len = 0;
                m_want = CAPTURE_SAMPLES;
                m_ready = false;
                m_capturing = true;
                register_lf_adc_callback_input(adc_cb, NRF_SAADC_INPUT_AIN0, CAPTURE_ACQ);
                nrf_saadc_continuous_mode_enable(CAPTURE_CC);
                nrf_saadc_task_trigger(NRF_SAADC_TASK_SAMPLE);
                enter(ST_CAPTURING);
            } else if (ms_since(m_t_state) > WAIT_BURST_MS) {
                abort_measure();
            }
            return;
        }
        case ST_CAPTURING: {
            if (!m_ready) {
                if (ms_since(m_t_state) > 2000) {
                    abort_measure();  // the ADC never filled the window
                }
                return;
            }
            stop_adc();
            m_ready = false;
            lf_tag_em_fsk_measure(false, m_measure_drive);  // back to the emulate drive at the next burst
            enter(ST_IDLE);
            const float pps = 1e6f / (float)i.frame_ticks;
            const float applied_ppm = (float)i.trim_act * pps;
            const uint32_t t0 = app_timer_cnt_get();
            selftrim_detect(m_buf, CAPTURE_SAMPLES, CAPTURE_FS, 125000.0f, applied_ppm, 16e6f / (float)i.frame_ticks,
                            (float *)(void *)m_buf, &m_res);
            m_last_ms = (uint16_t)ms_since(t0);
            m_count++;
            m_last_tick = app_timer_cnt_get();
            decide(pps, applied_ppm);
            m_last_ppm10 = (int16_t)lroundf(m_res.ppm * 10.0f);
            m_last_snr10 = (int16_t)lroundf(m_res.snr_db * 10.0f);
            m_last_why = m_res.why;
            NRF_LOG_INFO("selftrim fsk: %d ppm/10, %d dB/10, why %d, target %d", m_last_ppm10, m_last_snr10,
                         m_res.why, m_target);
            return;
        }
    }
}

static uint16_t put16(uint8_t *o, uint16_t n, uint16_t v) {
    o[n] = (uint8_t)(v >> 8);
    o[n + 1] = (uint8_t)v;
    return n + 2;
}

uint16_t lf_selftrim_fsk_command(const uint8_t *req, uint16_t len, uint8_t *out) {
    if (len == 4) {
        const bool en = req[0] != 0;
        m_residual = (int16_t)((req[1] << 8) | req[2]);
        if (req[3] <= LF_FSK_DRIVE_MAX) {
            m_measure_drive = req[3];
        }
        if (en != m_enabled) {
            if (!en) {
                abort_measure();
            }
            reset_decisions();
            m_enabled = en;
        }
    }
    lf_tag_em_fsk_info_t i;
    lf_tag_em_fsk_info(&i);
    if (len == 4) {
        m_target = i.trim_req;  // the trim may have been set by hand (hw lftrim) since we last acted
    }
    uint16_t n = 0;
    out[n++] = m_enabled ? 1u : 0u;
    n = put16(out, n, (uint16_t)m_residual);
    out[n++] = m_measure_drive;
    out[n++] = (uint8_t)m_state;
    n = put16(out, n, (uint16_t)m_target);
    n = put16(out, n, (uint16_t)i.trim_act);
    n = put16(out, n, (uint16_t)m_last_ppm10);
    n = put16(out, n, (uint16_t)m_last_snr10);
    out[n++] = m_last_why;
    n = put16(out, n, m_count);
    n = put16(out, n, m_invalid_run);
    n = put16(out, n, i.measure_bursts);
    n = put16(out, n, m_aborts);
    n = put16(out, n, m_res.evals);
    n = put16(out, n, m_last_ms);
    return n;  // LF_SELFTRIM_FSK_STATUS_LEN
}
