#include "lf_tag_em.h"

#include <stdint.h>

#include "bsp_delay.h"
#include "fds_util.h"
#include "nrf_gpio.h"
#include "nrf_soc.h"
#include "nrfx_lpcomp.h"
#include "nrfx_pwm.h"
#include "protocols/em410x.h"
#include "protocols/hidprox.h"
#include "protocols/idteck.h"
#include "protocols/indala.h"
#include "protocols/ioprox.h"
#include "protocols/jablotron.h"
#include "protocols/keri.h"
#include "protocols/nexwatch.h"
#include "protocols/pac.h"
#include "protocols/viking.h"
#include "syssleep.h"
#include "tag_emulation.h"
#include "tag_persistence.h"

#define NRF_LOG_MODULE_NAME tag_em410x
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
NRF_LOG_MODULE_REGISTER();

#define ANT_NO_MOD() nrf_gpio_pin_clear(LF_MOD)

/* ⭐ HOW LONG TO PLAY THE FRAME BEFORE PAUSING TO CHECK THE FIELD — A TIME BUDGET, NOT A
 * FRAME COUNT.
 *
 * The device cannot watch for field loss while it modulates: LF_RSSI swings with the load
 * modulation, so LPCOMP is disabled during emulation and the field is only re-checked between
 * bursts. The burst length therefore buys two things against each other:
 *
 *   short  → notices the reader leaving quickly, but the transmission is interrupted often,
 *            and a reader that demodulates ONE long capture fails across a boundary;
 *   long   → fewer boundaries, but the device keeps modulating for up to a whole burst after
 *            the reader has gone, which costs battery and responsiveness.
 *
 * ⭐ It was `32 FRAMES`, and a frame count is the wrong unit because a frame is not a fixed
 * duration. At RF/32 on a 125kHz carrier:
 *
 *     Indala 64-bit    16.4 ms/frame   → 32 frames =  524 ms
 *     PAC, EM410x      32.8 ms/frame   → 32 frames = 1.05 s
 *     Indala 224-bit   57.3 ms/frame   → 32 frames = 1.83 s   ⛔
 *
 * So the same constant meant a third of a second for one protocol and nearly two seconds for
 * another, and the longest-frame protocol — the one where latency hurts most — got the worst
 * of it. Nobody had noticed because Indala224 emulation does not exist yet.
 *
 * ⇒ Budget in MILLISECONDS and convert per protocol, using the sequence's own timing so it
 * stays right for any protocol added later.
 *
 * ⚠ 500ms is the value that 32 Indala frames happened to give (524ms), which is the only
 * length with evidence behind it: at 10 frames / 164ms a Proxmark failed beyond ~163ms of
 * capture, and at 32 frames / 524ms it decodes to 262ms (C70, C75). Changing the number needs
 * that measurement repeated, not an opinion.
 *
 * ⛔ DO NOT SIMPLY MAXIMISE IT. `NRFX_PWM_FLAG_LOOP` removes boundaries altogether and was
 * tried: it breaks field detection, because the device's own drive feeds back into LF_RSSI.
 * ⭐ The real fix is to stop needing bursts at all — detect field loss by counting carrier
 * edges, the way the Proxmark and Flipper both stay locked to the reader's clock while
 * emulating. See NEXT.md §4. */
#define LF_TAG_BURST_TARGET_MS   (500)
#define LF_TAG_BURST_MIN_FRAMES  (2)
#define LF_TAG_BURST_MAX_FRAMES  (255)

/** Frames per burst for the currently loaded sequence; recomputed whenever it changes. */
static uint16_t m_frames_per_burst = LF_TAG_BURST_MIN_FRAMES;

// Whether the USB light effect is allowed to enable
extern bool g_usb_led_marquee_enable;

// Whether it is currently in the low -frequency card number of broadcasting
static volatile bool m_is_lf_emulating = false;
// Cache tag type
static tag_specific_type_t m_tag_type = TAG_TYPE_UNDEFINED;

/* ⚠ §3 INSTRUMENTATION. Changing a slot's LF tag type kills emulation until a power cycle
 * (C126), and the stale-base-clock explanation is refuted because forcing pwm_init does not
 * fix it. These counters exist to find what IS stuck. Every test costs a power cycle, so the
 * point is to read everything at once. ⛔ Remove before upstreaming. */
static uint8_t  m_dbg_pwm_clk = 0xFF;   /* base clock pwm_init last applied */
static uint16_t m_dbg_pwm_inits = 0;    /* how many times pwm_init has run */
static uint16_t m_dbg_playbacks = 0;    /* how many times playback was started */
static uint16_t m_dbg_hf_req = 0;       /* sd_clock_hfclk_request calls */
static uint16_t m_dbg_hf_rel = 0;       /* ...and releases; these must stay balanced */

// The pwm to broadcast modulated card id
const nrfx_pwm_t m_broadcast = NRFX_PWM_INSTANCE(0);
const nrf_pwm_sequence_t *m_pwm_seq = NULL;

static void lf_field_lost(void) {
    // Open the incident interruption, so that the next event can be in and out normally
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

    sleep_timer_stop();  // turn off dormant delay
    // Disable LPCOMP during emulation — LF_RSSI fluctuates during load
    // modulation and would trigger spurious DOWN events with DETECT_CROSS.
    // Field-loss is checked periodically via EVT_END_SEQ0 in pwm_handler.
    nrfx_lpcomp_disable();

    // set the emulation status logo bit
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
    m_dbg_playbacks++;
    nrfx_pwm_simple_playback(&m_broadcast, m_pwm_seq, m_frames_per_burst,
                             NRFX_PWM_FLAG_STOP);

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
    // PWM has fully stopped — LF_MOD is released back to GPIO.
    // Now ANT_NO_MOD() and the settle delay are effective.
    ANT_NO_MOD();
    bsp_delay_ms(2);  // let peak detector drain: ~2 ms time constant on LF_RSSI
    if (is_lf_field_exists()) {
        // Field still present — play another finite burst then check again.
        m_dbg_playbacks++;
    nrfx_pwm_simple_playback(&m_broadcast, m_pwm_seq, m_frames_per_burst,
                             NRFX_PWM_FLAG_STOP);
    } else {
        // Field gone — clean up.
        lf_field_lost();
    }
}

static void pwm_init(void) {
    nrfx_pwm_config_t cfg = NRFX_PWM_DEFAULT_CONFIG;
    cfg.output_pins[0] = LF_MOD;
    for (uint8_t i = 1; i < NRF_PWM_CHANNEL_COUNT; i++) {
        cfg.output_pins[i] = NRFX_PWM_PIN_NOT_USED;
    }
    cfg.irq_priority = APP_IRQ_PRIORITY_LOW;
    // Base clock depends on the currently-loaded tag type. Legacy ASK/FSK
    // protocols (EM410x, HID, ioProx, Viking, PAC) use 125kHz base so that
    // their hardcoded counter_top values (8-64 range) produce the correct
    // absolute timing. PSK1 protocols need finer resolution for the 16us
    // subcarrier period, so pwm_init uses 1MHz base with counter_top=16.
    // See tag_base_type.h IS_PSK1_TYPE for the list of qualifying types.
    cfg.base_clock = IS_PSK1_TYPE(m_tag_type) ? NRF_PWM_CLK_1MHz : NRF_PWM_CLK_125kHz;
    /* ⚠ DEBUG BOOKKEEPING for §3. Records what was actually applied rather than what the
     * current tag type would ask for now, which is the whole question. */
    m_dbg_pwm_clk = (uint8_t)cfg.base_clock;
    m_dbg_pwm_inits++;
    cfg.count_mode = NRF_PWM_MODE_UP;
    cfg.load_mode = NRF_PWM_LOAD_WAVE_FORM;
    cfg.step_mode = NRF_PWM_STEP_AUTO;

    nrfx_err_t err_code = nrfx_pwm_init(&m_broadcast, &cfg, pwm_handler);
    APP_ERROR_CHECK(err_code);
}

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
    sd_clock_hfclk_request();
    m_dbg_hf_req++;
    uint32_t hfclk_running = 0;
    while (!hfclk_running) {
        sd_clock_hfclk_is_running(&hfclk_running);
    }

    lpcomp_init();
    pwm_init();  // use precise hardware pwm to broadcast card id
    if (is_lf_field_exists()) {
        lpcomp_event_handler(NRF_LPCOMP_EVENT_UP);
    }
}

static void lf_sense_disable(void) {
    nrfx_pwm_uninit(&m_broadcast);
    nrfx_lpcomp_uninit();
    /* ⛔⛔ DO NOT NULL m_pwm_seq HERE. It used to, and the result was that ANY sense cycle —
     * `hw mode -r` then `hw mode -e`, which is what a host driver does every time it alternates
     * reading and emulating — left the device in Tag Emulator mode, sense ENABLED, clock
     * correct, and emitting NOTHING, because there was no waveform left to play. Nothing
     * reloaded it: `lf_sense_enable()` re-runs `pwm_init()` but not the slot load, so only a
     * reboot or an explicit slot reload brought emulation back (C129).
     *
     * ⭐ It is safe to keep, because a modulator returns a pointer to a STATIC sequence owned
     * by its protocol module, not to anything the codec owns — see the note in utils/psk1.h,
     * which exists precisely because the codec is freed immediately after. The buffer outlives
     * the uninit.
     *
     * ⚠ What must NOT go stale is the pairing of sequence and CLOCK. A sequence built for a
     * PSK1 type played at 125kHz is unreadable and vice versa, which is C130; that is handled
     * where the tag type changes, in lf_tag_data_loadcb(). */
    m_is_lf_emulating = false;
    sd_clock_hfclk_release();
    m_dbg_hf_rel++;
}

static enum {
    LF_SENSE_STATE_NONE,
    LF_SENSE_STATE_DISABLE,
    LF_SENSE_STATE_ENABLE,
} m_lf_sense_state = LF_SENSE_STATE_NONE;

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

/* ⛔⛔ C130: THE PWM BASE CLOCK IS CHOSEN BY TAG TYPE AND WAS ONLY EVER SET AT SENSE-ENABLE.
 *
 * `pwm_init()` picks 1MHz for PSK1 types and 125kHz for everything else, and it ran only from
 * `lf_sense_enable()`. `hw slot type` changes the loaded type long afterwards, so the waveform
 * played at the PREVIOUS protocol's rate — 8x out, and unreadable. Measured: the same EM410X
 * slot with the same id reads 0 of 4 at a stale 1MHz and 5 of 5 at the correct 125kHz.
 *
 * ⚠ The two defects interlocked, which is why this went unnoticed for so long: the only thing
 * that re-ran `pwm_init()` was a sense cycle, and a sense cycle used to destroy the sequence
 * (C129). "Correct clock AND loaded waveform" was unreachable without a reboot.
 *
 * ⇒ Re-init here, where the type is known to have changed, and only when the required clock
 * actually differs so an unchanged type costs nothing. */
static void pwm_reinit_if_clock_changed(void) {
    if (m_lf_sense_state != LF_SENSE_STATE_ENABLE) {
        return;   /* pwm_init has not run yet; sense-enable will pick the right clock */
    }
    const uint8_t want = (uint8_t)(IS_PSK1_TYPE(m_tag_type) ? NRF_PWM_CLK_1MHz
                                                            : NRF_PWM_CLK_125kHz);
    if (want == m_dbg_pwm_clk) {
        return;
    }
    nrfx_pwm_uninit(&m_broadcast);
    /* uninit stops any playback, so the emulation flag no longer reflects reality */
    m_is_lf_emulating = false;
    pwm_init();
    /* ⚠ If a reader is already in the field, nothing else will re-trigger playback: the
     * LPCOMP UP event that normally starts it has long since fired. */
    if (is_lf_field_exists()) {
        lpcomp_event_handler(NRF_LPCOMP_EVENT_UP);
    }
}

/** @brief lf card data loader
 * @param type     Refined tag type
 * @param buffer   Data buffer
 */
static int lf_tag_data_loadcb_inner(tag_specific_type_t type, tag_data_buffer_t *buffer) {
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
        void *codec = hidprox.alloc();
        m_pwm_seq = hidprox.modulator(codec, buffer->buffer);
        hidprox.free(codec);
        NRF_LOG_INFO("load lf hidprox data finish.");
        return LF_HIDPROX_TAG_ID_SIZE;
    }

    if (type == TAG_TYPE_IOPROX && buffer->length >= LF_IOPROX_TAG_ID_SIZE) {
        m_tag_type = type;
        void *codec = ioprox.alloc();
        m_pwm_seq = ioprox.modulator(codec, buffer->buffer);
        ioprox.free(codec);
        NRF_LOG_INFO("load lf ioprox data finish.");
        return LF_IOPROX_TAG_ID_SIZE;
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

    if (type == TAG_TYPE_INDALA && buffer->length >= LF_INDALA_TAG_ID_SIZE) {
        m_tag_type = type;
        void *codec = indala.alloc();
        m_pwm_seq = indala.modulator(codec, buffer->buffer);
        indala.free(codec);
        NRF_LOG_INFO("load lf indala data finish.");
        return LF_INDALA_TAG_ID_SIZE;
    }

    if (type == TAG_TYPE_INDALA224 && buffer->length >= LF_INDALA224_TAG_ID_SIZE) {
        m_tag_type = type;
        void *codec = indala224.alloc();
        m_pwm_seq = indala224.modulator(codec, buffer->buffer);
        indala224.free(codec);
        NRF_LOG_INFO("load lf indala224 data finish.");
        return LF_INDALA224_TAG_ID_SIZE;
    }

    if (type == TAG_TYPE_KERI && buffer->length >= LF_KERI_TAG_ID_SIZE) {
        m_tag_type = type;
        void *codec = keri.alloc();
        m_pwm_seq = keri.modulator(codec, buffer->buffer);
        keri.free(codec);
        NRF_LOG_INFO("load lf keri data finish.");
        return LF_KERI_TAG_ID_SIZE;
    }

    if (type == TAG_TYPE_NEXWATCH && buffer->length >= LF_NEXWATCH_TAG_ID_SIZE) {
        m_tag_type = type;
        void *codec = nexwatch.alloc();
        m_pwm_seq = nexwatch.modulator(codec, buffer->buffer);
        nexwatch.free(codec);
        NRF_LOG_INFO("load lf nexwatch data finish.");
        return LF_NEXWATCH_TAG_ID_SIZE;
    }

    NRF_LOG_ERROR("no valid data exists in buffer for tag type: %d.", type);
    return 0;
}

/* ⭐ FRAME DURATION FROM THE SEQUENCE ITSELF, so a protocol added later is right for free.
 *
 * In wave-form mode every PWM entry carries its own `counter_top`, so the frame's duration is
 * the SUM of those tops divided by the base clock — no per-protocol table to forget to
 * update. `length` counts uint16 fields and there are four per entry.
 *
 * ⚠ Computed once per load rather than per playback: the playback call runs from the PWM
 * handler, and a 224-bit frame is 3584 entries to walk. */
static void recompute_frames_per_burst(void) {
    m_frames_per_burst = LF_TAG_BURST_MIN_FRAMES;
    if (m_pwm_seq == NULL || m_pwm_seq->values.p_wave_form == NULL) {
        return;
    }
    const size_t entries = (size_t)m_pwm_seq->length / 4u;
    uint64_t ticks = 0;
    for (size_t i = 0; i < entries; i++) {
        ticks += m_pwm_seq->values.p_wave_form[i].counter_top;
    }
    /* ⛔ `repeats` IS PART OF THE DURATION, not a detail of the buffer. SEQ[n].REFRESH
     * holds each loaded sample for repeats+1 PWM periods, so a PSK1 sequence storing one
     * entry per BIT (repeats 15) lasts sixteen times what its counter_tops sum to. Reading
     * it off the sequence keeps this generic: an ASK protocol storing one entry per symbol
     * leaves repeats at 0 and the arithmetic is unchanged. */
    ticks *= (uint64_t)m_pwm_seq->repeats + 1u;
    const uint32_t hz = IS_PSK1_TYPE(m_tag_type) ? 1000000u : 125000u;
    const uint64_t frame_us = (ticks * 1000000u) / hz;
    if (frame_us == 0) {
        return;
    }
    uint64_t n = ((uint64_t)LF_TAG_BURST_TARGET_MS * 1000u + frame_us - 1u) / frame_us;
    if (n < LF_TAG_BURST_MIN_FRAMES) {
        n = LF_TAG_BURST_MIN_FRAMES;
    }
    if (n > LF_TAG_BURST_MAX_FRAMES) {
        n = LF_TAG_BURST_MAX_FRAMES;
    }
    m_frames_per_burst = (uint16_t)n;
    NRF_LOG_INFO("lf burst: frame %lu us, %u frames per burst",
                 (unsigned long)frame_us, m_frames_per_burst);
}

/* ⭐ The public loader is the inner one plus the clock re-init. Keeping them separate means
 * every early return in the loader still gets the re-init, which a check bolted onto each
 * `return` would not. */
int lf_tag_data_loadcb(tag_specific_type_t type, tag_data_buffer_t *buffer) {
    int ret = lf_tag_data_loadcb_inner(type, buffer);
    recompute_frames_per_burst();
    pwm_reinit_if_clock_changed();
    return ret;
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

/** @brief Indala data save callback. */
int lf_tag_indala_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer) {
    return m_tag_type == TAG_TYPE_INDALA ? LF_INDALA_TAG_ID_SIZE : 0;
}

/** @brief Indala default frame: the fixed 33-bit preamble plus a placeholder payload.
 *
 * ⭐ a0000000e6bd0e92 is the bench credential this protocol was developed against —
 * Fmt 26, FC 52, Card 63612 — so a factory-reset slot emulates something a reader will
 * actually recognise, and something `lf indala read` is known to decode 20 times in 20.
 * The leading a0000000 is not a choice: bits 0-32 are Indala's fixed preamble (1010, then
 * 28 zeros, then a 1), which is why every Indala raw word begins that way. */
bool lf_tag_indala_data_factory(uint8_t slot, tag_specific_type_t tag_type) {
    uint8_t tag_id[LF_INDALA_TAG_ID_SIZE] = {
        0xA0, 0x00, 0x00, 0x00,   // fixed preamble: 1010, 28 zeros, then the leading 1 of
        0xE6, 0xBD, 0x0E, 0x92,   // ...the payload. Fmt 26 FC 52 Card 63612.
    };
    return lf_tag_data_factory(slot, tag_type, tag_id, sizeof(tag_id));
}

/** @brief Indala224 data save callback. */
int lf_tag_indala224_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer) {
    return m_tag_type == TAG_TYPE_INDALA224 ? LF_INDALA224_TAG_ID_SIZE : 0;
}

/** @brief Indala224 default frame: the 224-bit credential this format was developed
 * against, `80000001b23523a6c2e31eba3cbee4afb3c6ad1fcf649393928c14e5` — the Proxmark's own
 * documented 224-bit example, written to the bench T5577 and read back 6 of 6 by
 * `lf indala read --224` (C107). ⭐ A factory-reset slot therefore emulates something this
 * bench has independently decoded, rather than a pattern nothing has ever verified.
 * The leading `80000001` carries the 30-bit preamble: a 1 followed by 29 zeros. */
bool lf_tag_indala224_data_factory(uint8_t slot, tag_specific_type_t tag_type) {
    uint8_t tag_id[LF_INDALA224_TAG_ID_SIZE] = {
        0x80, 0x00, 0x00, 0x01,   // 1 then 29 zeros: the Indala224 preamble, then payload
        0xB2, 0x35, 0x23, 0xA6,
        0xC2, 0xE3, 0x1E, 0xBA,
        0x3C, 0xBE, 0xE4, 0xAF,
        0xB3, 0xC6, 0xAD, 0x1F,
        0xCF, 0x64, 0x93, 0x93,
        0x92, 0x8C, 0x14, 0xE5,
    };
    return lf_tag_data_factory(slot, tag_type, tag_id, sizeof(tag_id));
}

/** @brief Keri data save callback. */
int lf_tag_keri_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer) {
    return m_tag_type == TAG_TYPE_KERI ? LF_KERI_TAG_ID_SIZE : 0;
}

/** @brief NexWatch data save callback. */
int lf_tag_nexwatch_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer) {
    return m_tag_type == TAG_TYPE_NEXWATCH ? LF_NEXWATCH_TAG_ID_SIZE : 0;
}

/** @brief NexWatch default: the 96-bit frame of card 12345678, mode 1, Nexkey — the exact
 * bytes a Proxmark `lf nexwatch clone --cn 12345678 -m 1 --nc` leaves in T5577 blocks 1-3.
 *
 * ⭐ NO ROTATION, and that is measured rather than assumed: those blocks are
 * `56000000 / 00436455 / 121E6000`, so the frame's `0x56` preamble IS the top of block 1 and
 * the block form and the air frame coincide. ⛔ Keri's do NOT — see the note on its factory
 * — and emitting the wrong one of the two gave a stable wrong credential 6 of 6 (C160). The
 * rule is the same either way: emulate what the tag puts on the wire, and check which that
 * is against a real clone's own block dump. */
bool lf_tag_nexwatch_data_factory(uint8_t slot, tag_specific_type_t tag_type) {
    uint8_t tag_id[LF_NEXWATCH_TAG_ID_SIZE] = {
        0x56, 0x00, 0x00, 0x00,
        0x00, 0x43, 0x64, 0x55,
        0x12, 0x1E, 0x60, 0x00,
    };
    return lf_tag_data_factory(slot, tag_type, tag_id, sizeof(tag_id));
}

/** @brief Keri default: the BLOCK form of internal id 0x80003039, `(id << 3) | 7` — what
 * `lf keri clone -t i --cn 12345` leaves in T5577 blocks 1-2, and what a real tag therefore
 * puts on the wire.
 *
 * ⛔⛔ NOT THE READER'S FRAME VIEW `E0000000 80003039`, THOUGH THE TWO ARE THE SAME 64-BIT
 * CYCLE THREE BITS APART. Emulating the frame view gives Momentum a stable, confident,
 * WRONG credential — `FD9FD9FB` against the true `80003039`, 6 reads out of 6 — while the
 * block form reads correctly 5 of 5, with an Indala26 control correct on the same encoder
 * in both directions (C160).
 *
 * ⭐ The cyclic sequences are identical, so this can only be the BURST BOUNDARY: the
 * emulation plays 31 frames and pauses for field detection, and playback always begins at
 * buffer index 0. Rotation decides where in the frame that seam falls, and Momentum's Keri
 * decoder wants its preamble at bit 0 AND bit 64 of its own window. ⚠ The measurement is
 * solid; that mechanism is inferred and has not been isolated.
 *
 * ⇒ GENERAL RULE FOR THIS FAMILY: emulate what the TAG puts on the wire, not what the
 * reader's frame view is. They differ whenever the preamble straddles a block boundary. */
bool lf_tag_keri_data_factory(uint8_t slot, tag_specific_type_t tag_type) {
    uint8_t tag_id[LF_KERI_TAG_ID_SIZE] = {
        0x00, 0x00, 0x00, 0x04,   // (0x80003039 << 3) | 7, big-endian: the block form a
        0x00, 0x01, 0x81, 0xCF,   // ...T5577 holds and clocks out continuously
    };
    return lf_tag_data_factory(slot, tag_type, tag_id, sizeof(tag_id));
}

/* ⚠ §3 INSTRUMENTATION — see the counter declarations above. Reads everything in one call
 * because each test costs a power cycle. ⛔ Deliberately does NOT call is_lf_field_exists():
 * that enables LPCOMP and triggers a sample, so measuring with it would change the state
 * being measured. ⛔ Remove before upstreaming. */
void lf_tag_em_debug_get(uint8_t *out) {
    uint32_t hf = 0;
    sd_clock_hfclk_is_running(&hf);
    out[0]  = (uint8_t)m_lf_sense_state;
    out[1]  = m_is_lf_emulating ? 1u : 0u;
    out[2]  = (uint8_t)((uint16_t)m_tag_type >> 8);
    out[3]  = (uint8_t)((uint16_t)m_tag_type & 0xFF);
    out[4]  = m_dbg_pwm_clk;
    out[5]  = (uint8_t)(m_dbg_pwm_inits >> 8);
    out[6]  = (uint8_t)(m_dbg_pwm_inits & 0xFF);
    out[7]  = (uint8_t)(m_dbg_playbacks >> 8);
    out[8]  = (uint8_t)(m_dbg_playbacks & 0xFF);
    out[9]  = (uint8_t)(m_dbg_hf_req - m_dbg_hf_rel);
    out[10] = (uint8_t)hf;
    out[11] = (m_pwm_seq != NULL) ? 1u : 0u;
    out[12] = (uint8_t)(m_frames_per_burst >> 8);
    out[13] = (uint8_t)(m_frames_per_burst & 0xFF);
}
