#pragma once

#include <stdbool.h>

#include "rfid_main.h"
#include "tag_emulation.h"

#define LF_EM410X_TAG_ID_SIZE 5
#define LF_EM410X_ELECTRA_TAG_ID_SIZE 13
#define LF_IOPROX_TAG_ID_SIZE 16
#define LF_HIDPROX_TAG_ID_SIZE 13
#define LF_VIKING_TAG_ID_SIZE 4
#define LF_PAC_TAG_ID_SIZE 8
#define LF_JABLOTRON_TAG_ID_SIZE 5
#define LF_IDTECK_TAG_ID_SIZE 8
#define LF_INDALA_TAG_ID_SIZE 8
// ⚠ 28 bytes — the largest LF tag id there is, and what sizes the shared LF slot
// buffer in tag_emulation.c. Anything longer needs that buffer grown with it.
#define LF_INDALA224_TAG_ID_SIZE 28
#define LF_KERI_TAG_ID_SIZE 8
#define LF_NEXWATCH_TAG_ID_SIZE 12
#define LF_GALLAGHER_TAG_ID_SIZE 12
#define LF_AWID_TAG_ID_SIZE 12
#define LF_GPROXII_TAG_ID_SIZE 12
/* ⚠ SIXTEEN bytes — the largest LF frame here bar Indala224, and the shared LF buffer is
 * sized at LF_INDALA224_TAG_ID_SIZE (28), so it fits. Checked, not assumed (F13's class). */
#define LF_FDXB_TAG_ID_SIZE 16
#define LF_SECURAKEY_TAG_ID_SIZE 12
#define LF_NORALSY_TAG_ID_SIZE 12

void lf_tag_125khz_sense_switch(bool enable);
int lf_tag_data_loadcb(tag_specific_type_t type, tag_data_buffer_t *buffer);
int lf_tag_em410x_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer);
bool lf_tag_em410x_data_factory(uint8_t slot, tag_specific_type_t tag_type);
int lf_tag_hidprox_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer);
bool lf_tag_hidprox_data_factory(uint8_t slot, tag_specific_type_t tag_type);
int lf_tag_ioprox_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer);
bool lf_tag_ioprox_data_factory(uint8_t slot, tag_specific_type_t tag_type);
int lf_tag_viking_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer);
bool lf_tag_viking_data_factory(uint8_t slot, tag_specific_type_t tag_type);
int lf_tag_pac_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer);
bool lf_tag_pac_data_factory(uint8_t slot, tag_specific_type_t tag_type);
int lf_tag_jablotron_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer);
bool lf_tag_jablotron_data_factory(uint8_t slot, tag_specific_type_t tag_type);
int lf_tag_idteck_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer);
bool lf_tag_idteck_data_factory(uint8_t slot, tag_specific_type_t tag_type);
int lf_tag_indala_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer);
bool lf_tag_indala_data_factory(uint8_t slot, tag_specific_type_t tag_type);
int lf_tag_indala224_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer);
bool lf_tag_indala224_data_factory(uint8_t slot, tag_specific_type_t tag_type);
int lf_tag_keri_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer);
bool lf_tag_keri_data_factory(uint8_t slot, tag_specific_type_t tag_type);
int lf_tag_nexwatch_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer);
bool lf_tag_nexwatch_data_factory(uint8_t slot, tag_specific_type_t tag_type);
int lf_tag_gallagher_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer);
bool lf_tag_gallagher_data_factory(uint8_t slot, tag_specific_type_t tag_type);
int lf_tag_awid_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer);
bool lf_tag_awid_data_factory(uint8_t slot, tag_specific_type_t tag_type);
int lf_tag_gproxii_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer);
bool lf_tag_gproxii_data_factory(uint8_t slot, tag_specific_type_t tag_type);
int lf_tag_fdxb_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer);
bool lf_tag_fdxb_data_factory(uint8_t slot, tag_specific_type_t tag_type);
int lf_tag_securakey_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer);
bool lf_tag_securakey_data_factory(uint8_t slot, tag_specific_type_t tag_type);
int lf_tag_noralsy_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer);
bool lf_tag_noralsy_data_factory(uint8_t slot, tag_specific_type_t tag_type);
bool is_lf_field_exists(void);

/** ⚠ §3 instrumentation: 12 bytes of LF emulation state. ⛔ Remove before upstreaming. */
#define LF_TAG_EM_DEBUG_SIZE 14
void lf_tag_em_debug_get(uint8_t *out);

/** ⭐ §3 instrumentation: dump the LIVE PWM wave-form buffer through `m_pwm_seq` — the one step
 * of the PAC path that no air-side measurement can reach (see DATA_CMD_LF_EMU_SEQDUMP).
 *
 * ⛔⛔ A DUMP TAKEN WITH NO READER FIELD IS VOID (M56). The modulator runs on field detection,
 * so with the field off this returns whatever the last armed protocol left — and C454 is the
 * precedent: a register read the same for all three arms and the constant was mistaken for an
 * answer until the controls, which had to differ, caught it. The header therefore carries
 * `emulating` and `playbacks` so a void dump is visible in its own output, and `entries`
 * differs per arm (pac 128, gproxii 96) so the controls can still fail. */
#define LF_TAG_EM_SEQ_HEADER_SIZE   12
#define LF_TAG_EM_SEQ_MAX_ENTRIES   256
#define LF_TAG_EM_SEQ_DUMP_MAX      (LF_TAG_EM_SEQ_HEADER_SIZE + LF_TAG_EM_SEQ_MAX_ENTRIES * 8)
uint16_t lf_tag_em_seq_get(uint16_t start, uint16_t count, uint8_t *out);

/** ⭐ §3 instrumentation: install a SYNTHETIC wave-form of pure alternating static runs, so the
 * long-DC question can be asked of the playback path directly instead of through a credential.
 *
 * `holdsweep.py` fixed the criterion before this existed: `pac.c:363-367` writes one entry per
 * bit at `counter_top = PAC_RF_PER_BIT` (32) with `channel_0 = bits[i] ? 33 : 0`, and at the
 * 125 kHz base clock one tick is one 8us carrier cycle — so one entry is 256us and a run of N
 * identical entries is a level held for exactly N * 256us. This builds that shape and nothing
 * else: `entries_per_run` at compare 33, then `entries_per_run` at compare 0, repeated to fill
 * the buffer, every entry `counter_top` 32 and `channel_1`/`channel_2` zero (pac.c writes
 * neither). ⇒ PREDICTION: measured max static run = N * 256us, slope 1 through the origin.
 *
 * ⛔⛔ REVERSIBLE BY CONSTRUCTION, AND THAT IS A REQUIREMENT, NOT A PROPERTY. The synthetic
 * buffer is a separate array; `m_pwm_seq` is merely re-pointed at it. Arming ANY slot through
 * the normal path calls that protocol's modulator and re-points `m_pwm_seq` back at the
 * protocol's own values, so an unattended tick cannot leave a device emitting this.
 *
 * ⛔ REFUSED (returns 0) unless an emulation is already armed at the 125 kHz base clock. At
 * 1 MHz a `counter_top` of 32 is 32us, not 256us, and the whole criterion above silently
 * becomes eight times wrong — the exact shape of trap this project keeps paying for.
 *
 * @param entries_per_run 1..LF_TAG_EM_HOLD_MAX_RUN
 * @return entries actually installed, 0 if refused. */
#define LF_TAG_EM_HOLD_MAX_RUN      64
#define LF_TAG_EM_HOLD_MAX_ENTRIES  256
uint16_t lf_tag_em_seq_hold(uint16_t entries_per_run);
