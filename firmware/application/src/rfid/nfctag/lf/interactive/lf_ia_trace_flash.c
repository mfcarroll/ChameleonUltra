// The log's flash area on the nRF52840, through nrf_fstorage's SoftDevice backend, as FDS uses it. The backend queues
// each operation with the SoftDevice if it is enabled, so a Bluetooth connection keeps its timing, and writes directly
// if it is not.

#include "lf_ia_trace_flash.h"

#include <stddef.h>

#include "bsp_wdt.h"
#include "nrf.h"
#include "nrf_fstorage_sd.h"
#include "sdk_config.h"

// application.ld places the area.
extern const uint8_t __lf_ia_log_start[];
extern const uint8_t __lf_ia_log_end[];

static void fs_evt_handler(nrf_fstorage_evt_t *p_evt);

NRF_FSTORAGE_DEF(nrf_fstorage_t m_log_fs) = {
    .evt_handler = fs_evt_handler,
};

static bool m_ok;  // the area lies below FDS, and fstorage is set up

static void fs_evt_handler(nrf_fstorage_evt_t *p_evt) {
    lf_ia_trace_flash_done(p_evt->result == NRF_SUCCESS);
}

// FDS computes its start the same way (fds.c, flash_end_addr()).
static uint32_t fds_start(void) {
    const uint32_t bootloader = NRF_UICR->NRFFW[0];
    const uint32_t end = bootloader != 0xFFFFFFFFu ? bootloader : NRF_FICR->CODESIZE * NRF_FICR->CODEPAGESIZE;
    return end - FDS_VIRTUAL_PAGES * FDS_VIRTUAL_PAGE_SIZE * 4u;
}

static void flash_init(void) {
    static bool done;
    if (done) {
        return;
    }
    done = true;
    if ((uint32_t)__lf_ia_log_end > fds_start()) {
        return;
    }
    m_log_fs.start_addr = (uint32_t)__lf_ia_log_start;
    m_log_fs.end_addr = (uint32_t)__lf_ia_log_end - 1u;
    m_ok = nrf_fstorage_init(&m_log_fs, &nrf_fstorage_sd, NULL) == NRF_SUCCESS;
}

const uint8_t *lf_ia_flash_base(void) {
    flash_init();
    return m_ok ? __lf_ia_log_start : NULL;
}

uint32_t lf_ia_flash_size(void) {
    return (uint32_t)(__lf_ia_log_end - __lf_ia_log_start);
}

bool lf_ia_flash_write(uint32_t offset, const void *src, uint32_t len) {
    return m_ok && nrf_fstorage_write(&m_log_fs, m_log_fs.start_addr + offset, src, len, NULL) == NRF_SUCCESS;
}

bool lf_ia_flash_erase_page(uint32_t offset) {
    return m_ok && nrf_fstorage_erase(&m_log_fs, m_log_fs.start_addr + offset, 1, NULL) == NRF_SUCCESS;
}

bool lf_ia_flash_busy(void) {
    return m_ok && nrf_fstorage_is_busy(&m_log_fs);
}

void lf_ia_flash_wait(void) {
    while (lf_ia_flash_busy()) {
        bsp_wdt_feed();
    }
}
