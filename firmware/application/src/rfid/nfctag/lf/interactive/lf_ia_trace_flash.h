#pragma once

// The log's flash area, as lf_ia_trace.c uses it: memory-mapped reads, and one asynchronous write or page erase at a
// time. lf_ia_trace_flash.c implements it with nrf_fstorage and the SoftDevice. The host tests implement it with an
// array.

#include <stdbool.h>
#include <stdint.h>

#define LF_IA_FLASH_PAGE (4096u)

// The area, or NULL if the log may not use it (it overlaps other data).
const uint8_t *lf_ia_flash_base(void);
uint32_t lf_ia_flash_size(void);

// Start one operation. src stays valid until it completes. len and offset are multiples of 4. Return false if the
// operation could not start, for example while another one runs.
bool lf_ia_flash_write(uint32_t offset, const void *src, uint32_t len);
bool lf_ia_flash_erase_page(uint32_t offset);
// An operation is in progress.
bool lf_ia_flash_busy(void);
// Waits until no operation is in progress.
void lf_ia_flash_wait(void);

// Called by the port when an operation completes, from its event context.
void lf_ia_trace_flash_done(bool ok);
