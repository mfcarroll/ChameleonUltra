#include "hfxo_count.h"

#include "app_util_platform.h"
#include "nrf.h"
#include "nrf_sdm.h"
#include "nrf_soc.h"

#define TICKS_PER_S 16000000u

static volatile uint32_t m_hi;
static volatile uint16_t m_lapses;
static bool m_running;
// HFXO was already held by someone else (LF emulation or reader) at START: leave it held at STOP
static bool m_hfxo_was_held;

static bool hfxo_running(void) {
    uint32_t running = 0;
    sd_clock_hfclk_is_running(&running);
    return running != 0;
}

static void hfxo_hold(void) {
    sd_clock_hfclk_request();
    while (!hfxo_running()) {
    }
}

void TIMER1_IRQHandler(void) {
    if (NRF_TIMER1->EVENTS_COMPARE[0]) {
        NRF_TIMER1->EVENTS_COMPARE[0] = 0;
        m_hi++;
    }
    if (NRF_TIMER1->EVENTS_COMPARE[1]) {
        NRF_TIMER1->EVENTS_COMPARE[1] = 0;
        NRF_TIMER1->CC[1] += TICKS_PER_S;
        // The app has one HFXO request flag, not a count: another module's release drops ours.
        // Count it so the host can reject the run, and take the clock back.
        if (!hfxo_running()) {
            if (m_lapses != UINT16_MAX) {
                m_lapses++;
            }
            sd_clock_hfclk_request();
        }
    }
}

void hfxo_count_start(void) {
    hfxo_count_stop();
    m_hfxo_was_held = hfxo_running();
    hfxo_hold();

    NRF_TIMER1->MODE = TIMER_MODE_MODE_Timer;
    NRF_TIMER1->BITMODE = TIMER_BITMODE_BITMODE_32Bit;
    NRF_TIMER1->PRESCALER = 0;  // 16 MHz, HFCLK / 1
    NRF_TIMER1->CC[0] = 0;      // wrap
    NRF_TIMER1->CC[1] = TICKS_PER_S;
    NRF_TIMER1->INTENCLR = 0xFFFFFFFF;
    m_hi = 0;
    m_lapses = 0;
    NRF_TIMER1->TASKS_CLEAR = 1;
    NRF_TIMER1->TASKS_START = 1;
    // Drop any compare event from the clear itself before the wrap interrupt is armed
    do {
        NRF_TIMER1->TASKS_CAPTURE[2] = 1;
    } while (NRF_TIMER1->CC[2] == 0);
    NRF_TIMER1->EVENTS_COMPARE[0] = 0;
    NRF_TIMER1->EVENTS_COMPARE[1] = 0;

    NVIC_SetPriority(TIMER1_IRQn, APP_IRQ_PRIORITY_LOW);
    NVIC_ClearPendingIRQ(TIMER1_IRQn);
    NVIC_EnableIRQ(TIMER1_IRQn);
    NRF_TIMER1->INTENSET = TIMER_INTENSET_COMPARE0_Msk | TIMER_INTENSET_COMPARE1_Msk;
    m_running = true;
}

void hfxo_count_read(hfxo_count_t *out) {
    uint32_t lo = 0, hi = 0;
    if (m_running) {
        CRITICAL_REGION_ENTER();
        NRF_TIMER1->TASKS_CAPTURE[2] = 1;
        lo = NRF_TIMER1->CC[2];
        hi = m_hi;
        // A wrap the interrupt hasn't counted yet
        if (NRF_TIMER1->EVENTS_COMPARE[0] && lo < 0x80000000u) {
            hi++;
        }
        CRITICAL_REGION_EXIT();
    }
    out->ticks = ((uint64_t)hi << 32) | lo;
    out->overflows = hi;
    out->raw = lo;
    out->lapses = m_lapses;
    out->running = m_running;
    out->hfxo_now = hfxo_running();
}

void hfxo_count_stop(void) {
    if (!m_running) {
        return;
    }
    NRF_TIMER1->INTENCLR = 0xFFFFFFFF;
    NVIC_DisableIRQ(TIMER1_IRQn);
    NRF_TIMER1->TASKS_STOP = 1;
    NRF_TIMER1->TASKS_SHUTDOWN = 1;
    m_running = false;
    if (!m_hfxo_was_held) {
        sd_clock_hfclk_release();
    }
}
