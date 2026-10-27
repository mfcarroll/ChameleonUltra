#include "lf_tag_em.h"

#include <stdint.h>

#include "app_timer.h"
#include "app_util_platform.h"
#include "bsp_delay.h"
#include "fds_util.h"
#include "nrf_clock.h"
#include "nrf_gpio.h"
#include "nrf_sdh.h"
#include "nrf_soc.h"
#include "nrfx_lpcomp.h"
#include "nrfx_pwm.h"
#include "nrfx_ppi.h"
#include "nrf_timer.h"
#include "ble_main.h"
#include "bsp_wdt.h"
#include "nrf_saadc.h"
#include "protocols/em410x.h"
#include "protocols/hidprox.h"
#include "protocols/idteck.h"
#include "protocols/ioprox.h"
#include "protocols/jablotron.h"
#include "protocols/pac.h"
#include "protocols/paradox.h"
#include "lf_exp_fsk.h"
#include "protocols/viking.h"
#include "syssleep.h"
#include "tag_emulation.h"
#include "tag_persistence.h"
#include "utils/fsktrim.h"
#include "lf_selftrim_fsk.h"

#define NRF_LOG_MODULE_NAME tag_em410x
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
NRF_LOG_MODULE_REGISTER();

#define ANT_NO_MOD() nrf_gpio_pin_clear(LF_MOD)

// Whether the USB light effect is allowed to enable
extern bool g_usb_led_marquee_enable;

// Whether it is currently in the low -frequency card number of broadcasting
static volatile bool m_is_lf_emulating = false;
// Cache tag type
static tag_specific_type_t m_tag_type = TAG_TYPE_UNDEFINED;

// The pwm to broadcast modulated card id
const nrfx_pwm_t m_broadcast = NRFX_PWM_INSTANCE(0);
const nrf_pwm_sequence_t *m_pwm_seq = NULL;

// volatile: read by pwm_handler (IRQ) while lf_sense_disable (thread) changes it
static volatile enum {
    LF_SENSE_STATE_NONE,
    LF_SENSE_STATE_DISABLE,
    LF_SENSE_STATE_ENABLE,
} m_lf_sense_state = LF_SENSE_STATE_NONE;

// research: burst watchdog and state counters for the lock-up hunt (REVIEW-lockup.md §4b/§5).
// The PWM, LPCOMP and app_timer IRQs all run at priority 6, so none preempts another.
#define LF_WD_PERIOD_MS 50
APP_TIMER_DEF(m_lf_wd_timer);
static bool m_lf_wd_created = false;
static volatile bool m_lf_wd_busy = false;  // pwm_handler ignores STOPPED while the watchdog resets PWM0
static volatile uint32_t m_lf_wd_ticks = 0;
static const nrf_pwm_sequence_t *m_lf_wd_seq = NULL;  // sequence the limit below was computed for
static uint32_t m_lf_wd_limit_ms = 0;

static volatile uint16_t d_irq_up, d_det, d_det_null, d_play, d_stopped, d_stopped_ign, d_chk_hi, d_chk_lo,
                         d_lost, d_en, d_dis, d_stop_to, d_wd_fire, d_wd_pwr;
static volatile uint32_t d_t_play, d_t_stop;  // m_lf_wd_ticks at the last playback start / STOPPED
// research: wait between a burst's STOPPED and the field check; settable (hw lfsettle) to test whether
// drvC's own emission is what keeps the check reading 'present'
static volatile uint16_t m_lf_settle_ms = 20;
// research drvC-p4: drvC's own residual holds LF_RSSI above threshold for ~13 ms after a burst
// (NOTES-hard-2026-10-10.md), so a 2 ms check alone never sees the reader leave. Check after 2 ms on
// most bursts, and after m_lf_settle_ms on every LF_LONG_CHECK_EVERY-th burst since the detection, so
// emission ends within ~4 bursts (~1.3 s for ioProx) of the field going away.
#define LF_SHORT_SETTLE_MS 2
#define LF_LONG_CHECK_EVERY 4
static volatile uint16_t m_lf_bursts = 0;  // bursts completed since the last detection
static volatile uint16_t d_lo_short = 0;   // short (2 ms) checks that read low (lfstate v3 'lo_short')

// research exp/fsk-drvtrim: FSK emulation (ioProx, HID Prox) at a 16 MHz PWM base, with a runtime drive
// mode (hw lfdrive, 3093) and subcarrier trim (hw lftrim, 3094); see utils/fsktrim.h. The commands only set
// the *_req values. The sequence and the pins are brought to them in lf_fsk_sync(), which runs right before
// every playback start, i.e. only while PWM0 is stopped (pwm_handler at STOPPED, the LPCOMP UP handler while
// idle, the watchdog after its stop) or from pwm_init. So a change takes effect at the next burst.
#define IS_FSK_EMU_TYPE(t) ((t) == TAG_TYPE_IOPROX || (t) == TAG_TYPE_HID_PROX || (t) == TAG_TYPE_PARADOX)
static volatile uint8_t m_fsk_drive_req = LF_FSK_DRIVE_DEFAULT;
static void lf_exp_fsk_sync(void);
static uint8_t m_fsk_drive_act = LF_FSK_DRIVE_DEFAULT;    // the mode PWM0's pins were set up for
static tag_specific_type_t m_pwm_init_type = TAG_TYPE_UNDEFINED;  // m_tag_type when pwm_init last ran
static volatile int16_t m_fsk_trim_req = 0;
static int16_t m_fsk_trim_built = 0;   // trim_req the sequence was last prepared with
static int16_t m_fsk_trim_act = 0;     // trim actually in the sequence (after clamping)
static uint8_t m_fsk_drive_built = LF_FSK_DRIVE_DEFAULT;  // drive mode the sequence's channel_1 was set for
static volatile bool m_fsk_seq_dirty = true;  // a modulator rewrote the sequence (nominal tops, stale channel_1)
static uint32_t m_fsk_frame_ticks = 0;  // ticks per frame of the sequence as prepared
static uint16_t d_fsk_prep = 0, d_fsk_reinit = 0;
// research exp/fsk-selftrim: while a self-trim measurement is due (lf_selftrim_fsk.c), FSK bursts play in the
// measure drive instead of the emulate drive (m_fsk_drive_req): the reader's carrier alias reaches LF_RSSI only
// while LF_MOD switches (modes 0, 2), and the Flipper reads best in mode 1. m_fsk_measure_playing is set when a
// burst starts in the measure drive, so the capture lines up with it.
static volatile bool m_fsk_measure_on = false;
static volatile uint8_t m_fsk_measure_drive = LF_FSK_DRIVE_U1_SAME;
static volatile bool m_fsk_measure_playing = false;
static volatile uint16_t d_fsk_measure_bursts = 0;
static volatile uint16_t m_fsk_loads = 0;  // counts FSK sequence loads (slot load / econfig)
// research exp/fsk-selftrim: a fixed delay (us) added before every burst. Each restart advances the U1 phase vs the
// reader by a roughly fixed step (NOTES-burstphase); the same delay on every gap shifts that step, so a sweep can
// drive it toward 0 (phase held across a field) and then pick the phase. 0 = off.
static volatile uint8_t m_fsk_phase_us = 0;
// research exp/fsk-phasecont: phase-continuous burst restart (hw lfcont, 3097). The software gap (bsp_delay_ms in
// pwm_handler, then the nrfx start) lands each restart at an arbitrary point of the reader's carrier cycle, so the U1
// phase steps at every burst (NOTES-burstphase). Here TIMER3 free-runs at 16 MHz (the PWM's clock) and each burst
// starts on TIMER3 COMPARE0 -> PPI -> PWM0 SEQSTART; after the field check the next start is armed at the first
// whole number of reader carrier cycles after now. A reader cycle is the trimmed sequence length over its cycle count,
// so with the trim matched the PWM resumes as if it had kept playing. A PPI fork disables the channel's group, so each
// arm fires once. A field's first burst starts m_cont_offset ticks (of 128 per cycle) after its detection.
#define LF_CONT_MARGIN_TICKS 800u  // 50 us: arm no closer than this to now
static volatile bool m_cont_req = false;
static volatile uint8_t m_cont_offset = 0;
static bool m_cont_ready = false;  // PPI channel and group allocated
static nrf_ppi_channel_t m_cont_ch;
static nrf_ppi_channel_group_t m_cont_grp;
static volatile bool m_cont_run = false;  // TIMER3 is running and m_cont_start is the current burst's start
static uint32_t m_cont_start;             // TIMER3 tick the current burst started at
static volatile uint16_t d_cont_arm, d_cont_first, d_cont_late, d_cont_fail;
static volatile uint32_t d_cont_gap_cycles;  // reader cycles from the last burst's start to the next one's
// research exp/fsk-phasecont: phase select (hw lfcont mode 2). A field's first burst plays in the measure drive with
// the trim detuned (m_fsk_measure_detune steps), so the reader's alias line on LF_RSSI clears our own line; the
// self-trim module captures it with the first sample started from TIMER3 (PPI), takes its frequency and phase, and
// sets an anchor: the TIMER3 tick (q32) where the reader's carrier is at the chosen phase, and the reader's cycle
// in ticks (q32). While the anchor is valid, every restart lands on anchor + n cycles (not the trimmed grid).
static volatile bool m_cont_phsel = false;
static volatile bool m_anchor_valid = false;
static uint64_t m_anchor_q32;  // TIMER3 ticks << 32 (integer part mod 2^32)
static uint64_t m_cycle_q32;   // reader carrier cycle, TIMER3 ticks << 32
static volatile int16_t m_fsk_measure_detune = 0;  // trim steps added while the measure drive plays
static volatile uint16_t d_cont_anchored;  // restarts placed from the anchor
// research exp/fsk-phasecont: placed-phase telemetry. Mode 3 measures as mode 2 but keeps the restarts on the
// trimmed grid (no anchor), so the start phase is the field's natural one and the measurement can be checked against
// the level it gives. Per field: the start tick of the first burst, of the first burst after the measurement, and of
// the last burst; hw lfcont converts them to the measured reader phase.
static volatile bool m_cont_noanchor = false;
// The anchored restarts' period. Mode 2: the reader's cycle from fp averaged over fields (lf_selftrim_fsk.c): the
// trimmed grid is only matched to a trim step (0.21 Hz) and the reader drifts, and each 0.1 Hz of grid - reader
// error moves the placed phase ~4/128 by burst 2 (NOTES-burst1 section 4); one field's fp scatters +-0.2 Hz
// (NOTES-phplace section 4). Mode 4: the per-field fp. Mode 5: the trimmed grid's cycle. 4 and 5 are for A/B.
// Mode 6: mode 2 with the measure burst in LF_MOD only (drive 0) and no detune. Without U1 there is no line of
// our own on LF_RSSI to clear, so burst 1 plays at the matched rate and the switch to emulate changes only the drive
// (NOTES-burst1 section 3: the measure -> emulate junction cost 3 misframes in 15).
static volatile bool m_cont_measure_lfmod = false;
// The measure burst's detune (ppm below the emulate trim; hw lfcont's optional 3rd byte) and mode 7: the measure burst
// plays LF_CONT_SHORT_LOOPS of the usual 10 sequence loops (ends on a frame boundary, after the 200 ms capture).
#define LF_CONT_SHORT_LOOPS 7u
static volatile uint8_t m_cont_detune_ppm = 240;  // 30 Hz = 6 bins of the 200 ms capture: our own line's leakage has a
                                                   // null at the window and it sits outside the leakage check (F4)
static volatile bool m_cont_short_measure = false;
// Mode 8: mode 7 whose measure burst carries no ID: a constant fc/8 tone (LF_CONT_IDLE_PERIODS periods, the same
// 4096 carrier cycles as an HID/ioProx frame, so a trim step is the same ppm), trimmed and driven like the emulate
// sequence. The capture needs LF_MOD switching, not our data; a reader then sees no frame for ~260 ms instead of a
// detuned, fading one it can bit-slip into a wrong ID (NOTES-burst1 section 8).
#define LF_CONT_IDLE_PERIODS 512u
#define LF_CONT_IDLE_LOOPS   8u  // 8 x 32.8 ms: room for the 200 ms capture and its compute
static volatile bool m_cont_idle_measure = false;
// Mode 9: mode 8 that finds and holds the trim itself (lf_selftrim_fsk.c: a wide search on a new reader, then the
// field-averaged line). m_cont_auto_gen counts switch-ons, so a re-enable starts from a wide search again.
static volatile bool m_cont_auto = false;
static volatile uint16_t m_cont_auto_gen = 0;
static bool m_cont_kept_loaded = false;  // hw lfcont's mode and K restored from the retained store (after a wake)
static void lf_cont_apply(uint8_t mode, uint8_t offset);
static nrf_pwm_values_wave_form_t m_idle_vals[LF_CONT_IDLE_PERIODS];
static nrf_pwm_sequence_t m_idle_seq;

static inline int16_t fsk_trim_want(void);

// Build the idle tone for the current trim and drive. PWM0 is stopped (between bursts).
static const nrf_pwm_sequence_t *lf_cont_idle_seq(void) {
    const nrf_pwm_values_wave_form_t *v0 = m_pwm_seq->values.p_wave_form;
    for (uint32_t i = 0; i < LF_CONT_IDLE_PERIODS; i++) {
        m_idle_vals[i].counter_top = 8u * LF_FSK_TICKS_PER_CYCLE;
        m_idle_vals[i].channel_0 = 4u * LF_FSK_TICKS_PER_CYCLE;
        m_idle_vals[i].channel_1 = 0;
        m_idle_vals[i].channel_2 = v0[0].channel_2;
    }
    uint32_t ft;
    lf_fsk_prepare(m_idle_vals, LF_CONT_IDLE_PERIODS, fsk_trim_want(), m_fsk_drive_act, &ft);
    m_idle_seq.values.p_wave_form = m_idle_vals;
    m_idle_seq.length = LF_CONT_IDLE_PERIODS * 4u;
    m_idle_seq.repeats = m_pwm_seq->repeats;
    m_idle_seq.end_delay = 0;
    return &m_idle_seq;
}
static volatile uint8_t m_cont_period = 0;  // 0 averaged measured (mode 2), 1 per-field measured (4), 2 grid (5)
static volatile uint16_t d_cont_burst, d_cont_post_idx = 0xFFFF;
static volatile uint8_t d_cont_post_how;  // 1 grid, 2 anchored
static volatile uint32_t d_cont_first_cc, d_cont_post_cc, d_cont_last_cc;
static bool m_saadc_ppi_ready = false;
static nrf_ppi_channel_t m_saadc_ch;
static inline uint8_t fsk_drive_want(void) {
    return m_fsk_measure_on ? m_fsk_measure_drive : m_fsk_drive_req;
}
static inline int16_t fsk_trim_want(void) {
    return (int16_t)(m_fsk_trim_req + (m_fsk_measure_on ? m_fsk_measure_detune : 0));
}

static void pwm_init(void);
static void lf_fsk_prepare_seq(void);

// drive mode 4 (IDLEHI) leaves U1's GPIO high between bursts; every other path expects it low (tag mode
// sets it low, U1 not selected). Put it back when PWM0 is re-created for another mode or sensing ends.
static bool m_lf_u1_idle_high = false;
static void lf_u1_idle_low(void) {
    if (m_lf_u1_idle_high) {
        nrf_gpio_pin_clear(LF_ANT_DRIVER);
        m_lf_u1_idle_high = false;
    }
}

static void lf_fsk_sync(void) {
    if (!IS_FSK_EMU_TYPE(m_tag_type) && !IS_FSK_EMU_TYPE(m_pwm_init_type)) {
        return;  // other tag types: unchanged
    }
    if (m_tag_type != m_pwm_init_type || fsk_drive_want() != m_fsk_drive_act) {
        // pins or base clock differ: re-create PWM0 (as the watchdog does; PWM0 is stopped here).
        // pwm_init prepares the sequence too.
        d_fsk_reinit++;
        nrfx_pwm_uninit(&m_broadcast);
        pwm_init();
        return;
    }
    if (IS_FSK_EMU_TYPE(m_tag_type) &&
        (m_fsk_seq_dirty || fsk_trim_want() != m_fsk_trim_built || m_fsk_drive_built != m_fsk_drive_act)) {
        lf_fsk_prepare_seq();
    }
}

#define CONT_TIMER NRF_TIMER3

static bool lf_cont_setup(void) {
    if (!m_cont_ready) {
        if (nrfx_ppi_channel_alloc(&m_cont_ch) != NRFX_SUCCESS) {
            return false;
        }
        if (nrfx_ppi_group_alloc(&m_cont_grp) != NRFX_SUCCESS) {
            nrfx_ppi_channel_free(m_cont_ch);
            return false;
        }
        nrfx_ppi_channel_include_in_group(m_cont_ch, m_cont_grp);
        m_cont_ready = true;
    }
    return true;
}

// Stop TIMER3 and drop a pending arm. The next burst then starts a new grid (as a field's first burst).
static void lf_cont_disarm(void) {
    if (m_cont_ready) {
        nrfx_ppi_group_disable(m_cont_grp);
    }
    nrf_timer_task_trigger(CONT_TIMER, NRF_TIMER_TASK_STOP);
    m_cont_run = false;
    m_anchor_valid = false;  // TIMER3 restarts from 0 at the next field
}

static uint32_t lf_cont_now(void) {
    nrf_timer_task_trigger(CONT_TIMER, NRF_TIMER_TASK_CAPTURE1);
    return nrf_timer_cc_read(CONT_TIMER, NRF_TIMER_CC_CHANNEL1);
}

// placed-phase telemetry: a burst armed to start at cc (how: 0 first of the field, 1 grid, 2 anchored)
static void lf_cont_note(uint32_t cc, uint8_t how) {
    if (how == 0) {
        d_cont_burst = 0;
        d_cont_post_idx = 0xFFFF;
        d_cont_first_cc = cc;
    } else {
        d_cont_burst++;
        if (d_cont_post_idx == 0xFFFF && !m_fsk_measure_on) {
            d_cont_post_idx = d_cont_burst;
            d_cont_post_cc = cc;
            d_cont_post_how = how;
        }
    }
    d_cont_last_cc = cc;
}

// Set up the next burst to start on TIMER3 COMPARE0. Returns false if it could not (caller starts it directly).
// TIMER3 free-runs (32 bits at 16 MHz, wraps after 268 s; all differences below are mod 2^32 and < 1 s).
static bool lf_cont_arm(void) {
    if (!lf_cont_setup() || m_pwm_seq == NULL || m_fsk_frame_ticks == 0) {
        d_cont_fail++;
        return false;
    }
    // one sequence = seq_ticks ticks = seq_cycles reader cycles (each nominal period is a whole number of cycles)
    uint32_t reps = m_pwm_seq->repeats + 1u;
    uint64_t seq_ticks = (uint64_t)m_fsk_frame_ticks * reps;
    uint64_t seq_cycles = (uint64_t)((int64_t)m_fsk_frame_ticks + m_fsk_trim_act) / LF_FSK_TICKS_PER_CYCLE * reps;
    if (seq_cycles == 0) {
        d_cont_fail++;
        return false;
    }
    const bool idle = m_fsk_measure_on && m_cont_idle_measure;
    const uint16_t loops = idle ? LF_CONT_IDLE_LOOPS : (m_fsk_measure_on && m_cont_short_measure) ? LF_CONT_SHORT_LOOPS : 10u;
    uint32_t task = nrfx_pwm_simple_playback(&m_broadcast, idle ? lf_cont_idle_seq() : m_pwm_seq, loops,
                                             NRFX_PWM_FLAG_STOP | NRFX_PWM_FLAG_START_VIA_TASK);
    NRF_PPI->CH[m_cont_ch].EEP = (uint32_t)nrf_timer_event_address_get(CONT_TIMER, NRF_TIMER_EVENT_COMPARE0);
    NRF_PPI->CH[m_cont_ch].TEP = task;
    NRF_PPI->FORK[m_cont_ch].TEP = nrfx_ppi_task_addr_group_disable_get(m_cont_grp);
    d_cont_arm++;

    if (!m_cont_run) {
        // (re)start TIMER3 free-running; the field's first burst starts a new grid
        nrf_timer_task_trigger(CONT_TIMER, NRF_TIMER_TASK_STOP);
        nrf_timer_mode_set(CONT_TIMER, NRF_TIMER_MODE_TIMER);
        nrf_timer_bit_width_set(CONT_TIMER, NRF_TIMER_BIT_WIDTH_32);
        nrf_timer_frequency_set(CONT_TIMER, NRF_TIMER_FREQ_16MHz);
        nrf_timer_int_disable(CONT_TIMER, 0xFFFFFFFFu);
        nrf_timer_shorts_disable(CONT_TIMER, 0xFFFFFFFFu);
        nrf_timer_task_trigger(CONT_TIMER, NRF_TIMER_TASK_CLEAR);
        uint32_t cc = LF_CONT_MARGIN_TICKS + m_cont_offset;
        nrf_timer_cc_write(CONT_TIMER, NRF_TIMER_CC_CHANNEL0, cc);
        nrf_timer_event_clear(CONT_TIMER, NRF_TIMER_EVENT_COMPARE0);
        nrfx_ppi_group_enable(m_cont_grp);
        nrf_timer_task_trigger(CONT_TIMER, NRF_TIMER_TASK_START);
        m_cont_start = cc;
        m_cont_run = true;
        d_cont_first++;
        d_cont_gap_cycles = 0;
        lf_cont_note(cc, 0);
        return true;
    }
    if (m_anchor_valid && !m_fsk_measure_on) {
        // phase select: the first anchor + n reader cycles at least LF_CONT_MARGIN_TICKS from now
        const uint64_t cyc_q32 = m_cont_period != 2 ? m_cycle_q32 : (seq_ticks << 32) / seq_cycles;
        nrf_timer_event_clear(CONT_TIMER, NRF_TIMER_EVENT_COMPARE0);
        nrfx_ppi_group_enable(m_cont_grp);
        const uint32_t a_int = (uint32_t)(m_anchor_q32 >> 32);
        const uint64_t a_frac = m_anchor_q32 & 0xFFFFFFFFull;
        for (uint32_t tries = 0; tries < 8; tries++) {
            int32_t rel = (int32_t)(lf_cont_now() + LF_CONT_MARGIN_TICKS * (1u + tries) - a_int);
            int64_t el = ((int64_t)rel << 32) - (int64_t)a_frac;  // target - anchor, q32
            uint64_t n = el <= 0 ? 0 : ((uint64_t)el + cyc_q32 - 1) / cyc_q32;
            uint32_t cc = a_int + (uint32_t)((a_frac + n * cyc_q32 + 0x80000000ull) >> 32);
            nrf_timer_cc_write(CONT_TIMER, NRF_TIMER_CC_CHANNEL0, cc);
            if (nrf_timer_event_check(CONT_TIMER, NRF_TIMER_EVENT_COMPARE0) || (int32_t)(cc - lf_cont_now()) > 0) {
                d_cont_gap_cycles = (uint32_t)(((uint64_t)(uint32_t)(cc - m_cont_start) << 32) / cyc_q32);
                m_cont_start = cc;
                d_cont_anchored++;
                lf_cont_note(cc, 2);
                return true;
            }
            d_cont_late++;
        }
        nrfx_ppi_group_disable(m_cont_grp);
        m_cont_run = false;
        m_anchor_valid = false;
        d_cont_fail++;
        *(volatile uint32_t *)task = 1u;
        return true;
    }
    // Next start: reader cycle n after the last start, at tick round(n * seq_ticks / seq_cycles), the first one at
    // least LF_CONT_MARGIN_TICKS from now. If now passes it before the compare is written, take a later one
    // (counted; it should only happen under a long higher-priority interrupt).
    nrf_timer_event_clear(CONT_TIMER, NRF_TIMER_EVENT_COMPARE0);
    nrfx_ppi_group_enable(m_cont_grp);
    for (uint32_t tries = 0; tries < 8; tries++) {
        uint64_t target = (uint64_t)(uint32_t)(lf_cont_now() - m_cont_start) + LF_CONT_MARGIN_TICKS * (1u + tries);
        uint64_t n = (target * seq_cycles + seq_ticks - 1) / seq_ticks;
        uint64_t off = (n * seq_ticks + seq_cycles / 2) / seq_cycles;
        if (off < target) {
            n++;
            off = (n * seq_ticks + seq_cycles / 2) / seq_cycles;
        }
        uint32_t cc = m_cont_start + (uint32_t)off;
        nrf_timer_cc_write(CONT_TIMER, NRF_TIMER_CC_CHANNEL0, cc);
        if (nrf_timer_event_check(CONT_TIMER, NRF_TIMER_EVENT_COMPARE0) || (int32_t)(cc - lf_cont_now()) > 0) {
            m_cont_start = cc;
            d_cont_gap_cycles = (uint32_t)n;
            lf_cont_note(cc, 1);
            return true;  // fired (now reached cc after the write) or still ahead
        }
        d_cont_late++;
    }
    // give up on the grid: start now, new grid at the next field
    nrfx_ppi_group_disable(m_cont_grp);
    m_cont_run = false;
    d_cont_fail++;
    *(volatile uint32_t *)task = 1u;
    return true;
}

static void lf_playback(void) {
    if (m_fsk_phase_us && IS_FSK_EMU_TYPE(m_tag_type)) {
        bsp_delay_us(m_fsk_phase_us);
    }
    lf_fsk_sync();
    if (m_fsk_measure_on && IS_FSK_EMU_TYPE(m_tag_type) && m_fsk_drive_act == m_fsk_measure_drive) {
        m_fsk_measure_playing = true;
        d_fsk_measure_bursts++;
    }
    d_play++;
    d_t_play = m_lf_wd_ticks;
    if (m_cont_req && IS_FSK_EMU_TYPE(m_tag_type) && lf_cont_arm()) {
        return;  // research exp/fsk-phasecont: starts on TIMER3's compare
    }
    if (m_cont_run) {
        lf_cont_disarm();
    }
    nrfx_pwm_simple_playback(&m_broadcast, m_pwm_seq, 10, NRFX_PWM_FLAG_STOP);
}

static void lf_field_lost(void) {
    // Open the incident interruption, so that the next event can be in and out normally
    d_lost++;
    lf_cont_disarm();            // research exp/fsk-phasecont: the next field starts a new grid
    g_is_tag_emulating = false;  // Reset the flag in the emulation
    m_is_lf_emulating = false;
    TAG_FIELD_LED_OFF()  // Make sure the indicator light of the LF field status
    // Re-arm LPCOMP so the next field appearance triggers lpcomp_event_handler.
    NRF_LPCOMP->INTENSET = LPCOMP_INTENSET_UP_Msk;
    // call sleep_timer_start *after* unsetting g_is_tag_emulating
    sleep_timer_start(SLEEP_DELAY_MS_FIELD_125KHZ_LOST);  // Start the timer to enter the sleep
    NRF_LOG_INFO("LF FIELD LOST");
}

/**
 * @brief Judge field status
 */
bool is_lf_field_exists(void) {
    nrfx_lpcomp_enable();
    bsp_delay_us(30);  // Display for a period of time and sampling to avoid misjudgment
    nrf_lpcomp_task_trigger(NRF_LPCOMP_TASK_SAMPLE);
    return nrf_lpcomp_result_get() == 1;  // Determine the sampling results of the LF field status
}

/**
 * @brief LPCOMP event handler is called when LPCOMP detects voltage drop.
 *
 * This function is called from interrupt context so it is very important
 * to return quickly. Don't put busy loops or any other CPU intensive actions here.
 * It is also not allowed to call soft device functions from it (if LPCOMP IRQ
 * priority is set to APP_IRQ_PRIORITY_HIGH).
 */
static void lpcomp_event_handler(nrf_lpcomp_event_t event) {
    // Only when the lf-frequency emulation is not launched, and the analog card is started
    if (m_is_lf_emulating || event != NRF_LPCOMP_EVENT_UP) {
        return;
    }
    d_irq_up++;
    // research C129: with no waveform loaded, stay in sense mode rather than play from address 0
    if (m_pwm_seq == NULL) {
        d_det_null++;
        return;
    }
    d_det++;
    m_lf_bursts = 0;
    if (IS_FSK_EMU_TYPE(m_tag_type)) {
        lf_exp_fsk_sync();  // EXPERIMENT: the slot's preset (lf_exp_fsk.h), before the field's first burst
        lf_exp_fsk_note_field();
    }
    if (!m_cont_kept_loaded) {
        uint8_t km, kk;
        m_cont_kept_loaded = true;
        if (lf_selftrim_fsk_kept_cont(&km, &kk)) {
            lf_cont_apply(km, kk);
        }
    }
    if (m_cont_auto && IS_FSK_EMU_TYPE(m_tag_type)) {
        int16_t kt;
        if (lf_selftrim_fsk_keep_trim(&kt)) {
            m_fsk_trim_req = kt;  // the slot's kept reader: this first burst is built at its trim
        }
    }
    if (m_cont_req && m_cont_phsel && IS_FSK_EMU_TYPE(m_tag_type)) {
        // research exp/fsk-phasecont: the field's first burst is the phase measurement (lf_selftrim_fsk.c)
        m_fsk_measure_playing = false;
        m_fsk_measure_drive = m_cont_measure_lfmod ? LF_FSK_DRIVE_LFMOD : LF_FSK_DRIVE_U1_SAME;
        m_fsk_measure_detune = m_cont_measure_lfmod
                                   ? 0
                                   : -(int16_t)(((uint64_t)m_fsk_frame_ticks * m_cont_detune_ppm + 500000u) / 1000000u);
        m_fsk_measure_on = true;
    } else {
        m_fsk_measure_detune = 0;  // self-trim's measure bursts are not detuned (a phase-select field left it set)
    }

    sleep_timer_stop();  // turn off dormant delay
    // Disable LPCOMP during emulation — LF_RSSI fluctuates during load
    // modulation and would trigger spurious DOWN events with DETECT_CROSS.
    // Field-loss is checked periodically via EVT_END_SEQ0 in pwm_handler.
    nrfx_lpcomp_disable();

    // set the emulation status logo bit
    d_t_play = m_lf_wd_ticks;  // research: lf_sense_enable calls this from thread context; keep the watchdog quiet
    m_is_lf_emulating = true;
    g_is_tag_emulating = true;
    // turn off USB light effect when emulating cards
    g_usb_led_marquee_enable = false;

    // LED status update
    set_slot_light_color(RGB_BLUE);
    TAG_FIELD_LED_ON()

    // Play a finite burst then stop — field check happens in EVT_STOPPED after
    // PWM has fully released LF_MOD, so ANT_NO_MOD() and the settle delay are
    // effective. NRFX_PWM_FLAG_LOOP kept the pin owned by the peripheral,
    // making the field check always read "present" due to self-drive on LF_RSSI.
    lf_playback();

    NRF_LOG_INFO("LF FIELD DETECTED");
}

static void lpcomp_init(void) {
    nrfx_lpcomp_config_t cfg = NRFX_LPCOMP_DEFAULT_CONFIG;
    cfg.input = LF_RSSI;
    cfg.hal.reference = NRF_LPCOMP_REF_SUPPLY_1_16;
    cfg.hal.detection = NRF_LPCOMP_DETECT_UP;
    cfg.hal.hyst = NRF_LPCOMP_HYST_50mV;

    ret_code_t err_code = nrfx_lpcomp_init(&cfg, lpcomp_event_handler);
    APP_ERROR_CHECK(err_code);
}

static void pwm_handler(nrfx_pwm_evt_type_t event_type) {
    if (event_type != NRFX_PWM_EVT_STOPPED) {
        return;
    }
    d_t_stop = m_lf_wd_ticks;
    // PWM has fully stopped — LF_MOD is released back to GPIO.
    // Now ANT_NO_MOD() and the settle delay are effective.
    ANT_NO_MOD();
    // research: a STOPPED caused by lf_sense_disable or the watchdog must not start another burst
    if (m_lf_sense_state != LF_SENSE_STATE_ENABLE || m_lf_wd_busy) {
        d_stopped_ign++;
        return;
    }
    d_stopped++;
    m_lf_bursts++;
    // let peak detector drain: ~2 ms time constant on LF_RSSI (longer after drvC's own emission)
    bool long_check = m_lf_bursts % LF_LONG_CHECK_EVERY == 0;
    bsp_delay_ms(long_check ? m_lf_settle_ms : LF_SHORT_SETTLE_MS);
    if (is_lf_field_exists() && m_pwm_seq != NULL) {
        // Field still present — play another finite burst then check again.
        d_chk_hi++;
        lf_playback();
    } else {
        // Field gone — clean up.
        if (!long_check) {
            d_lo_short++;
        }
        d_chk_lo++;
        lf_field_lost();
    }
}

// research: stop PWM0 and wait for it, bounded. A junk sequence can hold one period for up to
// 0x7FFF ticks (262 ms at 125 kHz; 2 ms at 16 MHz), so allow 300 ms. Returns false if it never stopped.
static bool lf_pwm_stop_bounded(void) {
    nrfx_pwm_stop(&m_broadcast, false);
    for (uint32_t i = 0; i < 3000; i++) {
        if (nrfx_pwm_is_stopped(&m_broadcast)) {
            return true;
        }
        bsp_delay_us(100);
    }
    d_stop_to++;
    return false;
}

static void pwm_init(void) {
    nrfx_pwm_config_t cfg = NRFX_PWM_DEFAULT_CONFIG;
    cfg.output_pins[0] = LF_MOD;
    for (uint8_t i = 1; i < NRF_PWM_CHANNEL_COUNT; i++) {
        cfg.output_pins[i] = NRFX_PWM_PIN_NOT_USED;
    }
    // research exp/fsk-drvtrim: FSK types (ioProx, HID Prox) take their pins from the drive mode (hw lfdrive).
    // Channel 0 carries channel_0 (stock compare), channel 1 carries channel_1 (set by lf_fsk_prepare).
    m_pwm_init_type = m_tag_type;
    lf_u1_idle_low();
    if (IS_FSK_EMU_TYPE(m_tag_type)) {
        m_fsk_drive_act = fsk_drive_want();
        switch (m_fsk_drive_act) {
            case LF_FSK_DRIVE_LFMOD:
                break;
            case LF_FSK_DRIVE_U1:
                cfg.output_pins[0] = LF_ANT_DRIVER;
                break;
            case LF_FSK_DRIVE_U1_IDLEHI:
                // nrfx's INVERTED only sets the pin's idle (GPIO) level high; the waveform is not inverted
                cfg.output_pins[1] = LF_ANT_DRIVER | NRFX_PWM_PIN_INVERTED;
                m_lf_u1_idle_high = true;
                break;
            default:  // LF_FSK_DRIVE_U1_SAME, LF_FSK_DRIVE_U1_INV
                cfg.output_pins[1] = LF_ANT_DRIVER;
                break;
        }
    }
    cfg.irq_priority = APP_IRQ_PRIORITY_LOW;
    // Base clock depends on the currently-loaded tag type. Legacy ASK/FSK
    // protocols (EM410x, HID, ioProx, Viking, PAC) use 125kHz base so that
    // their hardcoded counter_top values (8-64 range) produce the correct
    // absolute timing. Paradox uses the same FSK2a timing as HID Prox.
    // PSK1 protocols need finer resolution for the 16us
    // subcarrier period, so pwm_init uses 1MHz base with counter_top=16.
    // See tag_base_type.h IS_PSK1_TYPE for the list of qualifying types.
    // research exp/fsk-drvtrim: ioProx and HID Prox run at 16 MHz (128 ticks per carrier cycle) so the
    // subcarrier can be trimmed by single ticks; their modulators write 16 MHz tops.
    cfg.base_clock = IS_PSK1_TYPE(m_tag_type)       ? NRF_PWM_CLK_1MHz
                     : IS_FSK_EMU_TYPE(m_tag_type) ? NRF_PWM_CLK_16MHz
                                                   : NRF_PWM_CLK_125kHz;
    cfg.count_mode = NRF_PWM_MODE_UP;
    cfg.load_mode = NRF_PWM_LOAD_WAVE_FORM;
    cfg.step_mode = NRF_PWM_STEP_AUTO;

    nrfx_err_t err_code = nrfx_pwm_init(&m_broadcast, &cfg, pwm_handler);
    APP_ERROR_CHECK(err_code);
    if (IS_FSK_EMU_TYPE(m_tag_type)) {
        lf_fsk_prepare_seq();  // PWM0 is idle here
    }
}

// Bring the loaded FSK sequence to the drive mode PWM0 was set up for and to the requested trim.
// Only while PWM0 is not playing it.
static void lf_fsk_prepare_seq(void) {
    if (m_pwm_seq == NULL) {
        return;
    }
    int16_t req = fsk_trim_want();
    m_fsk_trim_act = (int16_t)lf_fsk_prepare((nrf_pwm_values_wave_form_t *)m_pwm_seq->values.p_wave_form,
                                             m_pwm_seq->length / 4u, req, m_fsk_drive_act, &m_fsk_frame_ticks);
    m_fsk_trim_built = req;
    m_fsk_drive_built = m_fsk_drive_act;
    m_fsk_seq_dirty = false;
    m_lf_wd_seq = NULL;  // recompute the watchdog limit from the new lengths
    d_fsk_prep++;
}

// research: how long one 10-frame burst of the loaded sequence should take, times 3, plus margin.
static uint32_t lf_wd_limit_ms(void) {
    if (m_pwm_seq != m_lf_wd_seq) {
        m_lf_wd_seq = m_pwm_seq;
        uint64_t ticks = 0;
        if (m_pwm_seq != NULL) {
            const nrf_pwm_values_wave_form_t *v = m_pwm_seq->values.p_wave_form;
            for (uint32_t i = 0; i < m_pwm_seq->length / 4u; i++) {
                ticks += (uint64_t)(v[i].counter_top & 0x7FFF) * (m_pwm_seq->repeats + 1u);
            }
        }
        // PWM ticks per ms for the base clock pwm_init chose (exp/fsk-drvtrim: 16 MHz for ioProx and HID;
        // keyed on the type pwm_init ran for, so a slot type change without a re-arm still gets the right base)
        uint32_t ticks_per_ms = IS_PSK1_TYPE(m_pwm_init_type)       ? 1000u
                                : IS_FSK_EMU_TYPE(m_pwm_init_type) ? 16000u
                                                                   : 125u;
        uint32_t burst_ms = (uint32_t)(ticks * 10u / ticks_per_ms);
        m_lf_wd_limit_ms = 3u * burst_ms + 100u;
        if (m_lf_wd_limit_ms < 300u) {
            m_lf_wd_limit_ms = 300u;
        }
    }
    return m_lf_wd_limit_ms;
}

// research: a burst that never reaches STOPPED leaves the emulator 'emulating' with LPCOMP off,
// deaf to the field (REVIEW-lockup.md §3a). Stop and re-create PWM0, then resume as pwm_handler
// would. Counted, so a dump says how often it happened.
static void lf_wd_handler(void *p_context) {
    m_lf_wd_ticks++;
    if (m_lf_sense_state != LF_SENSE_STATE_ENABLE || !m_is_lf_emulating) {
        return;
    }
    if ((m_lf_wd_ticks - d_t_play) * LF_WD_PERIOD_MS < lf_wd_limit_ms()) {
        return;
    }
    d_wd_fire++;
    m_lf_wd_busy = true;
    lf_cont_disarm();  // research exp/fsk-phasecont: an arm that never fired lands here; restart the grid
    if (!lf_pwm_stop_bounded()) {
        // [PS?] undocumented peripheral power-cycle (offset 0xFFC), as in Nordic's errata workarounds
        d_wd_pwr++;
        volatile uint32_t *power = (volatile uint32_t *)((uint32_t)NRF_PWM0 + 0xFFC);
        *power = 0;
        (void)*power;
        *power = 1;
    }
    nrfx_pwm_uninit(&m_broadcast);
    pwm_init();
    m_lf_wd_busy = false;
    ANT_NO_MOD();
    bsp_delay_ms(m_lf_settle_ms);
    if (is_lf_field_exists() && m_pwm_seq != NULL) {
        d_chk_hi++;
        lf_playback();
    } else {
        d_chk_lo++;
        lf_field_lost();
    }
}

static void lf_wd_start(void) {
    if (!m_lf_wd_created) {
        APP_ERROR_CHECK(app_timer_create(&m_lf_wd_timer, APP_TIMER_MODE_REPEATED, lf_wd_handler));
        m_lf_wd_created = true;
    }
    APP_ERROR_CHECK(app_timer_start(m_lf_wd_timer, APP_TIMER_TICKS(LF_WD_PERIOD_MS), NULL));
}

// HFXO started without the SoftDevice: a field wake-up emulates before the SoftDevice is enabled
static bool m_hfclk_direct = false;

static void hfclk_request(void) {
    if (nrf_sdh_is_enabled()) {
        sd_clock_hfclk_request();
        uint32_t hfclk_running = 0;
        while (!hfclk_running) {
            sd_clock_hfclk_is_running(&hfclk_running);
        }
    } else {
        nrf_clock_task_trigger(NRF_CLOCK_TASK_HFCLKSTART);
        while (!nrf_clock_hf_is_running(NRF_CLOCK_HFCLK_HIGH_ACCURACY)) {
        }
        m_hfclk_direct = true;
    }
}

static void hfclk_release(void) {
    if (m_hfclk_direct) {
        m_hfclk_direct = false;
        nrf_clock_task_trigger(NRF_CLOCK_TASK_HFCLKSTOP);
    } else {
        sd_clock_hfclk_release();
    }
}

// The SoftDevice now owns the clock: move the direct request to it so it keeps HFXO running
static void lf_sdh_state_handler(nrf_sdh_state_evt_t state, void *p_context) {
    if (state == NRF_SDH_EVT_STATE_ENABLED && m_hfclk_direct) {
        m_hfclk_direct = false;
        sd_clock_hfclk_request();
    }
}

NRF_SDH_STATE_OBSERVER(m_lf_sdh_state_observer, 0) = {
    .handler = lf_sdh_state_handler,
};

static void lf_sense_enable(void) {
    // PWM bit timing divides HFCLK by a fixed ratio. On HFINT (64 MHz RC,
    // ±1.5% at 25°C after factory trim, wider over temperature) this gives a
    // chip-to-chip spread that NRZ readers — which see cumulative error across
    // runs of same-polarity bits with no intra-run resync — reject even when
    // Manchester/FSK readers don't. Holding HFXO brings the PWM clock to
    // ±40 ppm, which is also tight enough for differential PSK encodings
    // (e.g. IDTECK) where what the reader decodes are bit-to-bit phase
    // transitions, so absolute phase lock to the reader's carrier is not
    // required. The tag-mode antenna taps on this board are envelope-only,
    // which rules out coherent demodulation or phase-lock-based approaches,
    // but does not preclude the differential-phase encodings supported here.
    //
    // Paired release in lf_sense_disable(). SD reference-counts HFXO requests,
    // so this coexists with BLE. Both functions run from thread context
    // (tag_mode_enter/tag_emulation_sense_end) where SVCs are safe.
    hfclk_request();

    d_en++;
    lpcomp_init();
    pwm_init();  // use precise hardware pwm to broadcast card id
    lf_wd_start();
    if (is_lf_field_exists()) {
        lpcomp_event_handler(NRF_LPCOMP_EVENT_UP);
    }
}

static void lf_sense_disable(void) {
    d_dis++;
    app_timer_stop(m_lf_wd_timer);
    // research: LPCOMP first, so no UP can start a playback below; then STOP before ENABLE=0
    // (nrfx_pwm_uninit only clears ENABLE). m_lf_sense_state is already DISABLE here, so the
    // STOPPED event does not start another burst.
    nrfx_lpcomp_uninit();
    lf_cont_disarm();
    lf_pwm_stop_bounded();
    nrfx_pwm_uninit(&m_broadcast);
    lf_u1_idle_low();
    // research C129: keep m_pwm_seq. The sequences are static, and hw mode -r then -e re-enables
    // sensing without reloading the slot, which left nothing to play.
    m_is_lf_emulating = false;
    hfclk_release();
}

static uint16_t lf_em410x_id_size(tag_specific_type_t type) {
    return type == TAG_TYPE_EM410X_ELECTRA ? LF_EM410X_ELECTRA_TAG_ID_SIZE : LF_EM410X_TAG_ID_SIZE;
}

/**
 * @brief switchLfFieldInductionToEnableTheState
 */
void lf_tag_125khz_sense_switch(bool enable) {
    // init modulation PIN as output PIN
    nrf_gpio_cfg_output(LF_MOD);
    // turn off mod, otherwise its hard to judge RSSI
    ANT_NO_MOD();

    if ((m_lf_sense_state == LF_SENSE_STATE_NONE || m_lf_sense_state == LF_SENSE_STATE_DISABLE) && enable) {
        // switch from disable -> enable
        m_lf_sense_state = LF_SENSE_STATE_ENABLE;
        lf_sense_enable();
    } else if (m_lf_sense_state == LF_SENSE_STATE_ENABLE && !enable) {
        // switch from enable -> disable
        m_lf_sense_state = LF_SENSE_STATE_DISABLE;
        lf_sense_disable();
    }
}

/** @brief lf card data loader
 * @param type     Refined tag type
 * @param buffer   Data buffer
 */
int lf_tag_data_loadcb(tag_specific_type_t type, tag_data_buffer_t *buffer) {
    // ensure buffer size is large enough for specific tag type,
    // so that tag data (e.g., card numbers) can be converted to corresponding pwm sequence here.
    if ((type == TAG_TYPE_EM410X || type == TAG_TYPE_EM410X_ELECTRA) && buffer->length >= lf_em410x_id_size(type)) {
        const protocol *p = type == TAG_TYPE_EM410X_ELECTRA ? &em410x_electra : &em410x_64;
        m_tag_type = type;
        void *codec = p->alloc();
        m_pwm_seq = p->modulator(codec, buffer->buffer);
        p->free(codec);
        NRF_LOG_INFO("load lf em410x%s data finish.", type == TAG_TYPE_EM410X_ELECTRA ? " electra" : "");
        return lf_em410x_id_size(type);
    }

    if (type == TAG_TYPE_HID_PROX && buffer->length >= LF_HIDPROX_TAG_ID_SIZE) {
        m_tag_type = type;
        g_lf_fsk_exact_bits = (lf_exp_fsk_preset(tag_emulation_get_slot()) & LF_EXP_FSK_EXACT_BITS) != 0;  // EXPERIMENT
        void *codec = hidprox.alloc();
        m_pwm_seq = hidprox.modulator(codec, buffer->buffer);
        hidprox.free(codec);
        m_fsk_seq_dirty = true;  // research exp/fsk-drvtrim: nominal tops; drive and trim applied before playing
        m_fsk_loads++;
        lf_exp_fsk_sync();  // EXPERIMENT: the slot's preset
        NRF_LOG_INFO("load lf hidprox data finish.");
        return LF_HIDPROX_TAG_ID_SIZE;
    }

    if (type == TAG_TYPE_IOPROX && buffer->length >= LF_IOPROX_TAG_ID_SIZE) {
        m_tag_type = type;
        void *codec = ioprox.alloc();
        m_pwm_seq = ioprox.modulator(codec, buffer->buffer);
        ioprox.free(codec);
        m_fsk_seq_dirty = true;  // research exp/fsk-drvtrim: nominal tops; drive and trim applied before playing
        m_fsk_loads++;
        lf_exp_fsk_sync();  // EXPERIMENT: the slot's preset
        NRF_LOG_INFO("load lf ioprox data finish.");
        return LF_IOPROX_TAG_ID_SIZE;
    }

    if (type == TAG_TYPE_PARADOX && buffer->length >= LF_PARADOX_TAG_ID_SIZE) {
        m_tag_type = type;
        g_lf_fsk_exact_bits = (lf_exp_fsk_preset(tag_emulation_get_slot()) & LF_EXP_FSK_EXACT_BITS) != 0;  // EXPERIMENT
        void *codec = paradox.alloc();
        m_pwm_seq = paradox.modulator(codec, buffer->buffer);
        paradox.free(codec);
        m_fsk_seq_dirty = true;  // EXPERIMENT: as HID Prox, drive and trim applied before playing
        m_fsk_loads++;
        lf_exp_fsk_sync();  // EXPERIMENT: the slot's preset
        NRF_LOG_INFO("load lf paradox data finish.");
        return LF_PARADOX_TAG_ID_SIZE;
    }

    if (type == TAG_TYPE_VIKING && buffer->length >= LF_VIKING_TAG_ID_SIZE) {
        m_tag_type = type;
        void *codec = viking.alloc();
        m_pwm_seq = viking.modulator(codec, buffer->buffer);
        viking.free(codec);
        NRF_LOG_INFO("load lf viking data finish.");
        return LF_VIKING_TAG_ID_SIZE;
    }

    if (type == TAG_TYPE_PAC && buffer->length >= LF_PAC_TAG_ID_SIZE) {
        m_tag_type = type;
        void *codec = pac.alloc();
        m_pwm_seq = pac.modulator(codec, buffer->buffer);
        pac.free(codec);
        NRF_LOG_INFO("load lf pac data finish.");
        return LF_PAC_TAG_ID_SIZE;
    }

    if (type == TAG_TYPE_JABLOTRON && buffer->length >= LF_JABLOTRON_TAG_ID_SIZE) {
        m_tag_type = type;
        void *codec = jablotron.alloc();
        m_pwm_seq = jablotron.modulator(codec, buffer->buffer);
        jablotron.free(codec);
        NRF_LOG_INFO("load lf jablotron data finish.");
        return LF_JABLOTRON_TAG_ID_SIZE;
    }

    if (type == TAG_TYPE_IDTECK && buffer->length >= LF_IDTECK_TAG_ID_SIZE) {
        m_tag_type = type;
        void *codec = idteck.alloc();
        m_pwm_seq = idteck.modulator(codec, buffer->buffer);
        idteck.free(codec);
        NRF_LOG_INFO("load lf idteck data finish.");
        return LF_IDTECK_TAG_ID_SIZE;
    }

    NRF_LOG_ERROR("no valid data exists in buffer for tag type: %d.", type);
    return 0;
}

/** @brief Id card deposit card number before callback
 * @param type      Refined tag type
 * @param buffer    Data buffer
 * @return The length of the data that needs to be saved is that it does not save when 0
 */
int lf_tag_em410x_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer) {
    // Make sure to load this tag before allowing saving
    // Just save the original card package directly
    if (m_tag_type == TAG_TYPE_EM410X) {
        return LF_EM410X_TAG_ID_SIZE;
    }
    if (m_tag_type == TAG_TYPE_EM410X_ELECTRA) {
        return LF_EM410X_ELECTRA_TAG_ID_SIZE;
    }
    return 0;
}

/** @brief Id card deposit card number before callback
 * @param type      Refined tag type
 * @param buffer    Data buffer
 * @return The length of the data that needs to be saved is that it does not save when 0
 */
int lf_tag_hidprox_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer) {
    // Make sure to load this tag before allowing saving
    // Just save the original card package directly
    return m_tag_type == TAG_TYPE_HID_PROX ? LF_HIDPROX_TAG_ID_SIZE : 0;
}

/** @brief Id card deposit card number before callback
 * @param type      Refined tag type
 * @param buffer    Data buffer
 * @return The length of the data that needs to be saved is that it does not save when 0
 */
int lf_tag_ioprox_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer) {
    // Make sure to load this tag before allowing saving
    // Just save the original card package directly
    return m_tag_type == TAG_TYPE_IOPROX ? LF_IOPROX_TAG_ID_SIZE : 0;
}

/** @brief Paradox data save callback. */
int lf_tag_paradox_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer) {
    return m_tag_type == TAG_TYPE_PARADOX ? LF_PARADOX_TAG_ID_SIZE : 0;
}

/** @brief Id card deposit card number before callback
 * @param type      Refined tag type
 * @param buffer    Data buffer
 * @return The length of the data that needs to be saved is that it does not save when 0
 */
int lf_tag_viking_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer) {
    // Make sure to load this tag before allowing saving
    // Just save the original card package directly
    return m_tag_type == TAG_TYPE_VIKING ? LF_VIKING_TAG_ID_SIZE : 0;
}

bool lf_tag_data_factory(uint8_t slot, tag_specific_type_t tag_type, uint8_t *tag_id, uint16_t length) {
    // write data to flash
    tag_sense_type_t sense_type = get_sense_type_from_tag_type(tag_type);
    fds_slot_record_map_t map_info;  // Get the special card slot FDS record information
    get_fds_map_by_slot_sense_type_for_dump(slot, sense_type, &map_info);
    // Call the blocked FDS to write the function, and write the data of the specified field type of the card slot into the Flash
    bool ret = fds_write_sync(map_info.id, map_info.key, length, (uint8_t *)tag_id);
    if (ret) {
        NRF_LOG_INFO("Factory slot data success.");
    } else {
        NRF_LOG_ERROR("Factory slot data error.");
    }
    return ret;
}

/** @brief Id card deposit card number before callback
 * @param slot      Card slot number
 * @param tag_type  Refined tag type
 * @return Whether the format is successful, if the formatting is successful, it will return to True, otherwise False will be returned
 */
bool lf_tag_em410x_data_factory(uint8_t slot, tag_specific_type_t tag_type) {
    static const uint8_t tag_id_base[LF_EM410X_TAG_ID_SIZE] = {0xDE, 0xAD, 0xBE, 0xEF, 0x88};
    static const uint8_t tag_id_electra[LF_EM410X_ELECTRA_TAG_ID_SIZE] = {0xDE, 0xAD, 0xBE, 0xEF, 0x88,
                                                                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                                                                         };

    switch (tag_type) {
        case TAG_TYPE_EM410X_ELECTRA:
            return lf_tag_data_factory(slot, tag_type, (uint8_t *)tag_id_electra, sizeof(tag_id_electra));
        case TAG_TYPE_EM410X:
            return lf_tag_data_factory(slot, tag_type, (uint8_t *)tag_id_base, sizeof(tag_id_base));
        default:
            return false;
    }
}

/** @brief Id card deposit card number before callback
 * @param slot      Card slot number
 * @param tag_type  Refined tag type
 * @return Whether the format is successful, if the formatting is successful, it will return to True, otherwise False will be returned
 */
bool lf_tag_hidprox_data_factory(uint8_t slot, tag_specific_type_t tag_type) {
    // default id, must to align(4), more word...
    uint8_t tag_id[13] = {0x01, 0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x51, 0x45, 0x00, 0x00, 0x00};
    return lf_tag_data_factory(slot, tag_type, tag_id, sizeof(tag_id));
}

/** @brief Id card deposit card number before callback
 * @param slot      Card slot number
 * @param tag_type  Refined tag type
 * @return Whether the format is successful, if the formatting is successful, it will return to True, otherwise False will be returned
 */
bool lf_tag_ioprox_data_factory(uint8_t slot, tag_specific_type_t tag_type) {
    uint8_t tag_id[16] = {
        0x01, 0xAA, 0x30, 0x39, 0x00, 0x78, 0x6A, 0xA0, 0x33, 0x09, 0xCF, 0xEF, 0x00, 0x00, 0x00, 0x00
    };
    return lf_tag_data_factory(slot, tag_type, tag_id, sizeof(tag_id));
}

/** @brief Paradox default data. */
bool lf_tag_paradox_data_factory(uint8_t slot, tag_specific_type_t tag_type) {
    static const uint8_t tag_id[LF_PARADOX_TAG_ID_SIZE] = {
        0x00, 0x00, 0x40, 0x00, 0x7A, 0xC0
    };
    return lf_tag_data_factory(slot, tag_type, (uint8_t *)tag_id, sizeof(tag_id));
}

/** @brief Id card deposit card number before callback
 * @param slot      Card slot number
 * @param tag_type  Refined tag type
 * @return Whether the format is successful, if the formatting is successful, it will return to True, otherwise False will be returned
 */
bool lf_tag_viking_data_factory(uint8_t slot, tag_specific_type_t tag_type) {
    // default id
    uint8_t tag_id[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    return lf_tag_data_factory(slot, tag_type, tag_id, sizeof(tag_id));
}

int lf_tag_pac_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer) {
    return m_tag_type == TAG_TYPE_PAC ? LF_PAC_TAG_ID_SIZE : 0;
}

bool lf_tag_pac_data_factory(uint8_t slot, tag_specific_type_t tag_type) {
    // default id: 8 ASCII bytes
    uint8_t tag_id[8] = {'C', 'A', 'R', 'D', '0', '0', '0', '1'};
    return lf_tag_data_factory(slot, tag_type, tag_id, sizeof(tag_id));
}

int lf_tag_jablotron_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer) {
    return m_tag_type == TAG_TYPE_JABLOTRON ? LF_JABLOTRON_TAG_ID_SIZE : 0;
}

bool lf_tag_jablotron_data_factory(uint8_t slot, tag_specific_type_t tag_type) {
    // default id: 5 bytes (top bit must be 0)
    uint8_t tag_id[5] = {0x01, 0xB6, 0x69, 0x00, 0x00};
    return lf_tag_data_factory(slot, tag_type, tag_id, sizeof(tag_id));
}

/** @brief IDTECK data save callback. */
int lf_tag_idteck_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer) {
    return m_tag_type == TAG_TYPE_IDTECK ? LF_IDTECK_TAG_ID_SIZE : 0;
}

/** @brief IDTECK default frame: preamble "IDTK" + 32-bit placeholder card data. */
bool lf_tag_idteck_data_factory(uint8_t slot, tag_specific_type_t tag_type) {
    uint8_t tag_id[LF_IDTECK_TAG_ID_SIZE] = {
        0x49, 0x44, 0x54, 0x4B,   // "IDTK" preamble (MSB first)
        0xDE, 0xAD, 0xBE, 0xEF,   // default card data
    };
    return lf_tag_data_factory(slot, tag_type, tag_id, sizeof(tag_id));
}

// research: hw lfstate. Read-only snapshot of the LF emulation state for the lock-up hunt
// (REVIEW-lockup.md §4b). Deliberately does not call is_lf_field_exists(), which enables LPCOMP
// and samples it, changing the state being measured. Big-endian; layout in lfstate.py.
static uint16_t put_u16(uint8_t *o, uint16_t n, uint16_t v) {
    o[n] = (uint8_t)(v >> 8);
    o[n + 1] = (uint8_t)v;
    return n + 2;
}

static uint16_t put_u32(uint8_t *o, uint16_t n, uint32_t v) {
    n = put_u16(o, n, (uint16_t)(v >> 16));
    return put_u16(o, n, (uint16_t)v);
}

static uint16_t put_pin(uint8_t *o, uint16_t n, uint32_t pin) {
    uint32_t p = pin;
    NRF_GPIO_Type *reg = nrf_gpio_pin_port_decode(&p);
    n = put_u32(o, n, reg->PIN_CNF[p]);
    o[n++] = (uint8_t)((reg->OUT >> p) & 1u);
    o[n++] = (uint8_t)((reg->IN >> p) & 1u);
    return n;
}

// research: hw lfstall, a test hook for the watchdog. Puts the emulator into REVIEW-lockup.md state (a):
// 'emulating', LPCOMP off, and a looping playback that never reaches STOPPED.
bool lf_tag_em_stall(void) {
    if (m_lf_sense_state != LF_SENSE_STATE_ENABLE || m_pwm_seq == NULL) {
        return false;
    }
    CRITICAL_REGION_ENTER();
    m_lf_wd_busy = true;
    lf_pwm_stop_bounded();
    if (!m_is_lf_emulating) {
        nrfx_lpcomp_disable();
        m_is_lf_emulating = true;
        g_is_tag_emulating = true;
        TAG_FIELD_LED_ON()
    }
    d_t_play = m_lf_wd_ticks;
    nrfx_pwm_simple_playback(&m_broadcast, m_pwm_seq, 1, NRFX_PWM_FLAG_LOOP);
    m_lf_wd_busy = false;
    CRITICAL_REGION_EXIT();
    return true;
}

uint16_t lf_tag_em_settle_set(uint16_t ms) {
    if (ms != 0) {
        m_lf_settle_ms = ms > 50 ? 50 : ms;
    }
    return m_lf_settle_ms;
}

// research exp/fsk-drvtrim: hw lfdrive. mode > LF_FSK_DRIVE_MAX queries. Takes effect at the next burst
// start (PWM0 is re-created then, between bursts). Returns the requested mode; *active gets the mode PWM0
// is currently set up for.
uint8_t lf_tag_em_phase_set(uint8_t set, uint8_t us) {
    if (set) {
        m_fsk_phase_us = us;
    }
    return m_fsk_phase_us;
}

// research exp/fsk-phasecont: hw lfcont. Sets (if set) on/off and the first-burst offset (ticks, of 128 per carrier
// cycle); takes effect at the next burst start. Fills the reply (LF_TAG_EM_CONT_LEN bytes, BE).
static void lf_cont_apply(uint8_t mode, uint8_t offset) {
    m_cont_offset = offset;
    m_cont_noanchor = mode == 3;
    m_cont_period = mode == 4 ? 1u : mode == 5 ? 2u : 0u;
    m_cont_measure_lfmod = mode == 6;
    m_cont_short_measure = mode == 7;
    m_cont_idle_measure = mode == 8 || mode == 9;
    if (mode == 9 && !m_cont_auto) {
        m_cont_auto_gen++;
    }
    m_cont_auto = mode == 9;
    m_cont_phsel = mode >= 2;
    m_cont_req = mode >= 1;
}

// EXPERIMENT (lf_exp_fsk.h): bring drive, trim and mode 9 to the active slot's preset, if it or the slot changed.
// From the load callback and at a field's start (LPCOMP IRQ); takes effect at the next burst, as the commands do.
static uint8_t m_exp_slot = 0xFF, m_exp_preset = 0xFF;
static void lf_exp_fsk_sync(void) {
    const uint8_t slot = tag_emulation_get_slot();
    const uint8_t preset = lf_exp_fsk_preset(slot);
    m_cont_kept_loaded = true;  // the preset decides the mode, not the research store's kept mode
    if (slot == m_exp_slot && preset == m_exp_preset) {
        return;
    }
    m_exp_slot = slot;
    m_exp_preset = preset;
    const uint8_t mode = preset & LF_EXP_FSK_MODE_MASK;
    m_fsk_drive_req = mode == LF_EXP_FSK_STOCK ? LF_FSK_DRIVE_LFMOD : LF_FSK_DRIVE_U1;
    if (mode == LF_EXP_FSK_SELFTRIM) {
        lf_cont_apply(0, 0);  // off first, so mode 9 starts over (and restores this slot's kept reader)
        lf_cont_apply(9, LF_EXP_FSK_K);
    } else {
        lf_cont_apply(0, 0);
        m_fsk_trim_req = 0;
    }
}

uint16_t lf_tag_em_cont_set(bool set, uint8_t mode, uint8_t offset, uint8_t detune_ppm, uint8_t *o) {
    if (detune_ppm != 0) {
        m_cont_detune_ppm = detune_ppm;
    }
    if (!m_cont_kept_loaded) {
        uint8_t km, kk;
        m_cont_kept_loaded = true;
        if (!set && lf_selftrim_fsk_kept_cont(&km, &kk)) {
            lf_cont_apply(km, kk);
        }
    }
    if (set) {
        lf_cont_apply(mode, offset);
        lf_selftrim_fsk_keep_cont(mode, offset);  // kept across sleep with the per-slot store
    }
    uint16_t n = 0;
    o[n++] = m_cont_req ? (m_cont_phsel ? (m_cont_noanchor ? 3u : m_cont_period == 1 ? 4u : m_cont_period == 2 ? 5u : m_cont_measure_lfmod ? 6u : m_cont_short_measure ? 7u : m_cont_auto ? 9u : m_cont_idle_measure ? 8u : 2u) : 1u) : 0u;
    o[n++] = m_cont_offset;
    o[n++] = (uint8_t)(m_cont_run | (m_cont_ready << 1) | (m_anchor_valid << 2));
    n = put_u16(o, n, d_cont_arm);
    n = put_u16(o, n, d_cont_first);
    n = put_u16(o, n, d_cont_late);
    n = put_u16(o, n, d_cont_fail);
    n = put_u32(o, n, d_cont_gap_cycles);
    n = put_u16(o, n, d_cont_anchored);
    n += lf_selftrim_fsk_phsel_status(&o[n]);
    // placed-phase telemetry (the last field): burst indices, start ticks, and the measured reader phase at each
    // start (1/128 cycle, from the field's own measurement; 0xFF none)
    n = put_u16(o, n, d_cont_burst);
    n = put_u16(o, n, d_cont_post_idx);
    o[n++] = d_cont_post_how;
    n = put_u32(o, n, d_cont_first_cc);
    n = put_u32(o, n, d_cont_post_cc);
    n = put_u32(o, n, d_cont_last_cc);
    o[n++] = lf_selftrim_fsk_phase_at(d_cont_first_cc);
    o[n++] = d_cont_post_idx == 0xFFFF ? 0xFFu : lf_selftrim_fsk_phase_at(d_cont_post_cc);
    o[n++] = lf_selftrim_fsk_phase_at(d_cont_last_cc);
    n += lf_selftrim_fsk_phavg_status(&o[n]);  // v4: the averaged reader line
    o[n++] = m_cont_detune_ppm;                  // v5
    n += lf_selftrim_fsk_auto_status(&o[n]);     // v6: mode 9's trim finding
    return n;
}

// research exp/fsk-phasecont: for lf_selftrim_fsk.c's phase measurement.
// Phase select requested and the field's measure burst due? *k = the chosen phase (1/128 cycle).
bool lf_tag_em_cont_phsel(uint8_t *k) {
    *k = m_cont_offset;
    return m_cont_req && m_cont_phsel;
}

// Restart period source for lf_selftrim_fsk.c: 0 averaged measured, 1 per-field measured, 2 grid (unused there).
// Mode 9 on? *gen counts its switch-ons (a change means: start over with a wide search).
bool lf_tag_em_cont_auto(uint16_t *gen) {
    *gen = m_cont_auto_gen;
    return m_cont_auto;
}

uint8_t lf_tag_em_cont_period(void) {
    return m_cont_period;
}

// Start the SAADC (already set up, continuous mode) with its SAMPLE task from TIMER3 COMPARE2, so the first sample's
// time is known on TIMER3. Thread context. Returns false if TIMER3 is not running or the compare was missed.
bool lf_tag_em_cont_saadc_start(uint32_t *t0) {
    if (!m_cont_run) {
        return false;
    }
    if (!m_saadc_ppi_ready) {
        if (nrfx_ppi_channel_alloc(&m_saadc_ch) != NRFX_SUCCESS) {
            return false;
        }
        m_saadc_ppi_ready = true;
    }
    NRF_PPI->CH[m_saadc_ch].EEP = (uint32_t)nrf_timer_event_address_get(CONT_TIMER, NRF_TIMER_EVENT_COMPARE2);
    NRF_PPI->CH[m_saadc_ch].TEP = (uint32_t)nrf_saadc_task_address_get(NRF_SAADC_TASK_SAMPLE);
    for (int tries = 0; tries < 4; tries++) {
        nrf_timer_event_clear(CONT_TIMER, NRF_TIMER_EVENT_COMPARE2);
        nrfx_ppi_channel_enable(m_saadc_ch);
        uint32_t cc = lf_cont_now() + 3200u;  // 200 us
        nrf_timer_cc_write(CONT_TIMER, NRF_TIMER_CC_CHANNEL2, cc);
        for (int i = 0; i < 100 && !nrf_timer_event_check(CONT_TIMER, NRF_TIMER_EVENT_COMPARE2); i++) {
            bsp_delay_us(10);
        }
        nrfx_ppi_channel_disable(m_saadc_ch);
        if (nrf_timer_event_check(CONT_TIMER, NRF_TIMER_EVENT_COMPARE2)) {
            *t0 = cc;
            return true;
        }
    }
    return false;
}

// Place the following restarts on the reader: anchor (TIMER3 tick q32 at the chosen phase), cycle (ticks q32).
void lf_tag_em_cont_anchor_set(uint64_t anchor_q32, uint64_t cycle_q32) {
    CRITICAL_REGION_ENTER();
    m_anchor_q32 = anchor_q32;
    m_cycle_q32 = cycle_q32;
    m_anchor_valid = m_cont_run && cycle_q32 != 0 && !m_cont_noanchor;  // mode 3: measure only
    CRITICAL_REGION_EXIT();
}

uint8_t lf_tag_em_drive_set(uint8_t mode, uint8_t *active) {
    if (mode <= LF_FSK_DRIVE_MAX) {
        m_fsk_drive_req = mode;
    }
    *active = m_fsk_drive_act;
    return m_fsk_drive_req;
}

// research exp/fsk-drvtrim: hw lftrim. Sets the requested trim (if set); applied (clamped to +-1000 ppm of
// the frame) at the next burst start. Returns the requested trim; *applied / *frame_ticks describe the
// sequence as last prepared.
int16_t lf_tag_em_trim_set(bool set, int16_t trim, int16_t *applied, uint32_t *frame_ticks) {
    if (set) {
        m_fsk_trim_req = trim;
    }
    *applied = m_fsk_trim_act;
    *frame_ticks = m_fsk_frame_ticks;
    return m_fsk_trim_req;
}

uint16_t lf_tag_em_state_get(uint8_t *o) {
    uint16_t n = 0;
    o[n++] = 4;  // layout version
    o[n++] = (uint8_t)m_lf_sense_state;
    o[n++] = m_is_lf_emulating ? 1u : 0u;
    o[n++] = (uint8_t)((m_pwm_seq != NULL) | (m_lf_wd_busy << 1) | (m_lf_wd_created << 2));
    n = put_u16(o, n, (uint16_t)m_tag_type);

    const uint16_t counters[] = {d_irq_up, d_det, d_det_null, d_play, d_stopped, d_stopped_ign, d_chk_hi,
                                 d_chk_lo, d_lost, d_en, d_dis, d_stop_to, d_wd_fire, d_wd_pwr};
    for (uint32_t i = 0; i < sizeof(counters) / sizeof(counters[0]); i++) {
        n = put_u16(o, n, counters[i]);
    }
    n = put_u32(o, n, m_lf_wd_ticks);
    n = put_u32(o, n, d_t_play);
    n = put_u32(o, n, d_t_stop);
    n = put_u32(o, n, m_lf_wd_limit_ms);
    n = put_u32(o, n, (uint32_t)m_pwm_seq);

    NRF_PWM_Type *pwm = NRF_PWM0;
    const uint32_t pwm_regs[] = {pwm->ENABLE, pwm->SEQ[0].PTR, pwm->SEQ[0].CNT, pwm->SEQ[0].REFRESH,
                                 pwm->SEQ[0].ENDDELAY, pwm->LOOP, pwm->SHORTS, pwm->INTEN, pwm->PSEL.OUT[0],
                                 pwm->COUNTERTOP, pwm->PRESCALER, pwm->DECODER, pwm->EVENTS_SEQSTARTED[0],
                                 pwm->EVENTS_SEQEND[0], pwm->EVENTS_SEQEND[1], pwm->EVENTS_LOOPSDONE,
                                 pwm->EVENTS_STOPPED, pwm->EVENTS_PWMPERIODEND};
    for (uint32_t i = 0; i < sizeof(pwm_regs) / sizeof(pwm_regs[0]); i++) {
        n = put_u32(o, n, pwm_regs[i]);
    }
    const uint32_t comp_regs[] = {NRF_LPCOMP->ENABLE, NRF_LPCOMP->RESULT, NRF_LPCOMP->INTENSET,
                                  NRF_LPCOMP->SHORTS, NRF_LPCOMP->PSEL, NRF_LPCOMP->REFSEL, NRF_LPCOMP->HYST,
                                  NRF_LPCOMP->ANADETECT, NRF_LPCOMP->EVENTS_READY, NRF_LPCOMP->EVENTS_UP,
                                  NRF_LPCOMP->EVENTS_DOWN, NRF_LPCOMP->EVENTS_CROSS};
    for (uint32_t i = 0; i < sizeof(comp_regs) / sizeof(comp_regs[0]); i++) {
        n = put_u32(o, n, comp_regs[i]);
    }
    o[n++] = (uint8_t)NVIC_GetEnableIRQ(PWM0_IRQn);
    o[n++] = (uint8_t)NVIC_GetPendingIRQ(PWM0_IRQn);
    o[n++] = (uint8_t)NVIC_GetEnableIRQ(LPCOMP_IRQn);
    o[n++] = (uint8_t)NVIC_GetPendingIRQ(LPCOMP_IRQn);

    n = put_pin(o, n, LF_MOD);
    n = put_pin(o, n, LF_ANT_DRIVER);
    n = put_pin(o, n, LF_RSSI_PIN);
    n = put_pin(o, n, LF_OA_OUT);
    n = put_pin(o, n, READER_POWER);
    n = put_pin(o, n, LED_FIELD);
    n = put_u16(o, n, m_lf_settle_ms);
    n = put_u32(o, n, m_pwm_seq != NULL ? (uint32_t)m_pwm_seq->values.p_raw : 0);
    // v3 block (as exp/drvc-p4l2 / p4poll). This build has the p4 check schedule: no idle poll, no
    // held 'lost', so poll_det, chk_lo_held and rescue stay 0; lo_short is counted.
    n = put_u16(o, n, 0);           // poll_det
    n = put_u16(o, n, 0);           // chk_lo_held
    n = put_u16(o, n, d_lo_short);  // lo_short
    n = put_u16(o, n, 0);           // rescue
    o[n++] = LF_LONG_CHECK_EVERY;   // long_every
    o[n++] = 1;                     // lost_after
    o[n++] = 0;                     // poll
    // v4 (exp/fsk-drvtrim): drive mode and trim, requested and in force; the prepared frame
    n = put_u16(o, n, (uint16_t)((m_fsk_drive_req << 8) | m_fsk_drive_act));
    n = put_u16(o, n, (uint16_t)m_fsk_trim_req);
    n = put_u16(o, n, (uint16_t)m_fsk_trim_act);
    n = put_u32(o, n, m_fsk_frame_ticks);
    n = put_u16(o, n, m_pwm_seq != NULL ? (uint16_t)(m_pwm_seq->length / 4u) : 0);  // periods per frame
    n = put_u32(o, n, NRF_PWM0->PSEL.OUT[1]);
    n = put_u16(o, n, d_fsk_prep);
    n = put_u16(o, n, d_fsk_reinit);
    o[n++] = m_fsk_seq_dirty ? 1u : 0u;
    return n;
}

// EXPERIMENT (research/lf/emulation-ceiling E24): continuous raw capture of LF_RSSI (AIN0), to look for
// the reader's carrier ripple aliased by the SAADC. Free-running at 16 MHz / cc with the given acquisition
// time; with force_play our waveform plays meanwhile (LPCOMP off). Samples stay in RAM for read-out.
#define ALIAS_MAX 16384
static int16_t m_alias_buf[ALIAS_MAX] __attribute__((aligned(4)));  // also the self-trim capture + float work
static volatile size_t m_alias_len;
static size_t m_alias_want;
static void alias_adc_cb(nrf_saadc_value_t *v, size_t n) {
    size_t room = m_alias_want - m_alias_len;
    size_t k = n < room ? n : room;
    for (size_t i = 0; i < k; i++) {
        m_alias_buf[m_alias_len + i] = v[i];
    }
    m_alias_len += k;
}
bool lf_selftrim_fsk_busy(void);
size_t lf_tag_em_alias_capture(uint16_t cc, uint8_t acq, bool force_play, size_t want) {
    bool started = false;
    if (lf_selftrim_fsk_busy()) {
        return 0;  // research exp/fsk-selftrim: the self-trim capture owns the buffer and the LF ADC callback
    }
    m_alias_want = want < ALIAS_MAX ? want : ALIAS_MAX;
    m_alias_len = 0;
    if (force_play && !m_is_lf_emulating && m_pwm_seq != NULL) {
        nrfx_lpcomp_disable();
        nrfx_pwm_simple_playback(&m_broadcast, m_pwm_seq, 255, NRFX_PWM_FLAG_STOP);
        started = true;
        bsp_delay_ms(5);
    }
    register_lf_adc_callback_input(alias_adc_cb, NRF_SAADC_INPUT_AIN0, (nrf_saadc_acqtime_t)acq);
    nrf_saadc_continuous_mode_enable(cc);
    nrf_saadc_task_trigger(NRF_SAADC_TASK_SAMPLE);
    for (int i = 0; i < 3000 && m_alias_len < m_alias_want; i++) {
        bsp_delay_ms(1);
        bsp_wdt_feed();
    }
    nrf_saadc_continuous_mode_disable();
    unregister_lf_adc_callback();
    if (started) {
        nrfx_pwm_stop(&m_broadcast, true);
        ANT_NO_MOD();
    }
    return m_alias_len;
}
size_t lf_tag_em_alias_read(size_t offset, int16_t **out) {
    *out = &m_alias_buf[offset < ALIAS_MAX ? offset : ALIAS_MAX];
    return offset < m_alias_len ? m_alias_len - offset : 0;
}

// research exp/fsk-selftrim: accessors for lf_selftrim_fsk.c.
int16_t *lf_tag_em_alias_buffer(size_t *cap_samples) {
    *cap_samples = ALIAS_MAX;
    return m_alias_buf;
}

void lf_tag_em_fsk_info(lf_tag_em_fsk_info_t *i) {
    i->emulating = m_is_lf_emulating;
    i->fsk = IS_FSK_EMU_TYPE(m_tag_type) && m_pwm_seq != NULL;
    i->frame_ticks = m_fsk_frame_ticks;
    i->trim_req = m_fsk_trim_req;
    i->trim_act = m_fsk_trim_act;
    i->drive_act = m_fsk_drive_act;
    i->det = d_det;
    i->loads = m_fsk_loads;
    i->measure_bursts = d_fsk_measure_bursts;
}

void lf_tag_em_fsk_measure(bool on, uint8_t drive) {
    if (drive <= LF_FSK_DRIVE_MAX) {
        m_fsk_measure_drive = drive;
    }
    if (on && !m_fsk_measure_on) {
        m_fsk_measure_playing = false;
    }
    m_fsk_measure_on = on;
    if (!on) {
        m_fsk_measure_playing = false;
    }
}

bool lf_tag_em_fsk_measure_playing(void) {
    return m_fsk_measure_playing;
}
