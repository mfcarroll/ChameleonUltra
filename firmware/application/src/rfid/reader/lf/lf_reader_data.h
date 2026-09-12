#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*RIO_CALLBACK_S)(void);  // Call the function format

void register_rio_callback(RIO_CALLBACK_S P);
void unregister_rio_callback(void);
void gpio_int0_irq_handler(void);

// Counter
uint32_t get_lf_counter_value(void);
void clear_lf_counter_value(void);

/* ⭐ ONE COPY OF THE BLE ADVERTISING GUARD, because four LF readers need it and only one
 * had it.
 *
 * ⛔ EACH ADVERTISING EVENT COLLAPSES THE 125kHz FIELD for ~1.6ms (C47). The envelope drops
 * to near zero and returns — the sniff output's own "real field drops" line has been
 * reporting it all along, and it hit 4 captures in 10. `lf_reader_generic.c` suspends
 * advertising for the duration and its reader (Indala) runs at 60/60 on real tags; the
 * hidprox, ioProx and PAC readers each start the field with no such guard, and two of those
 * three are the ones measured failing on loud tags (C45, C46).
 *
 * ⚠ THAT CORRELATION IS NOT PROOF, and the third reader is the reason to say so out loud:
 * ioProx lacks the guard too and is not on the unreliable list. If the guard were the whole
 * story ioProx should fail as well, so either its FSK decode tolerates a 1.6ms dropout that
 * HID's does not, or something else is also wrong. ⇒ Adding the guard is the clean test:
 * it changes one variable, and a rate that does not move refutes C47 rather than confirming
 * a fix.
 *
 * ⚠ Only when not connected — dropping advertising is harmless, dropping a live link is not.
 */
typedef struct {
    bool paused;
} lf_adv_guard_t;

void lf_adv_suspend(lf_adv_guard_t *guard);
void lf_adv_resume(lf_adv_guard_t *guard);

bool em410x_read(uint8_t *data, uint32_t timeout_ms);
bool ioprox_read(uint8_t *data, uint8_t format_hint, uint32_t timeout_ms);
bool hidprox_read(uint8_t *data, uint8_t format_hint, uint32_t timeout_ms);
bool pac_read(uint8_t *data, uint32_t timeout_ms);
bool viking_read(uint8_t *data, uint32_t timeout_ms);
bool jablotron_read(uint8_t *data, uint32_t timeout_ms);

/* ⛔ DO NOT RE-DECLARE raw_read_to_buffer HERE. This header carried a hand-maintained
 * copy of the prototype, and it went stale on BOTH occasions the signature changed --
 * once adding raw16, once adding settle_ms -- each time as a build break that looked
 * like it came from the file actually being edited. Include the owner instead. */
#include "lf_reader_generic.h"

#ifdef __cplusplus
}
#endif
