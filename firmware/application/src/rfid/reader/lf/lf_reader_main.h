#pragma once

#include <stddef.h>
#include <stdint.h>

#include "app_status.h"
#include "lf_125khz_radio.h"
#if defined(PROJECT_CHAMELEON_ULTRA)
#include "lf_em4x05_data.h"
#endif
#include "lf_reader_data.h"

void set_scan_tag_timeout(uint32_t ms);
uint8_t scan_em410x(uint8_t *uid);
uint8_t scan_ioprox(uint8_t *uid, uint8_t format_hint);
uint8_t decode_ioprox_raw(uint8_t *raw8, uint8_t *output);
uint8_t encode_ioprox_params(uint8_t ver, uint8_t fc, uint16_t cn, uint8_t *out);
uint8_t scan_hidprox(uint8_t *uid, uint8_t format_hint);
uint8_t scan_indala(uint8_t *data);
uint8_t scan_idteck(uint8_t *data);
uint8_t scan_keri(uint8_t *data);
uint8_t write_keri_to_t55xx(uint8_t *frame8, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count);
uint8_t scan_nexwatch(uint8_t *data);
uint8_t scan_gallagher(uint8_t *data);
uint8_t scan_securakey(uint8_t *data);
uint8_t scan_noralsy(uint8_t *data);
/** ⚠ Read only — see the note on instafob_read for why there is no writer. */
uint8_t scan_instafob(uint8_t *data);
/** ⭐ The BIPHASE family's first protocol — a fourth decode path, not a flag on the ASK one.
 *  See lf_ask_biphase.h for why the level path cannot read RF/64 at all. */
/** ⭐ The biphase family's SECOND protocol, and it needs none of GProxII's overrides — stock
 *  drive, shared rotation, no inter-capture gap. Measured, not assumed (C214). */
uint8_t scan_fdxb(uint8_t *data);
uint8_t write_fdxb_to_t55xx(uint8_t *frame16, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count);
uint8_t scan_gproxii(uint8_t *data);
/** ⚠ Instrumentation — the same read, reporting the decoder's energy on failure. */
uint8_t scan_gproxii_energy(uint8_t *data, int32_t *energy_out);
uint8_t write_gproxii_to_t55xx(uint8_t *frame12, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count);
uint8_t scan_awid(uint8_t *data);
uint8_t scan_paradox(uint8_t *data);
uint8_t scan_pyramid(uint8_t *data);
/** ⛔ Read only, and permanently so far: nothing on this bench can read an FDX-A tag back,
 *  so a writer would be self-certifying. See the note beside the three FSK writers. */
uint8_t scan_fdxa(uint8_t *data);
/** ⚠ `frame12`/`frame16` are the AIR frames, written to the blocks unrotated — measured per
 *  protocol, NOT assumed from Gallagher. See fsk2a_t55xx_blocks() for the three dumps. */
uint8_t write_awid_to_t55xx(uint8_t *frame12, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count);
uint8_t write_paradox_to_t55xx(uint8_t *frame12, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count);
uint8_t write_pyramid_to_t55xx(uint8_t *frame16, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count);
uint8_t write_noralsy_to_t55xx(uint8_t *frame12, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count);
uint8_t write_securakey_to_t55xx(uint8_t *frame12, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count);
uint8_t write_gallagher_to_t55xx(uint8_t *frame12, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count);
uint8_t write_nexwatch_to_t55xx(uint8_t *frame12, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count);
uint8_t scan_indala224(uint8_t *data);
uint8_t scan_pac(uint8_t *card_id);
uint8_t scan_viking(uint8_t *uid);
uint8_t scan_jablotron(uint8_t *uid);
uint8_t write_em410x_to_t55xx(uint8_t *uid, uint8_t *newkey, uint8_t *old_keys, uint8_t old_key_count);
uint8_t write_em410x_electra_to_t55xx(uint8_t *uid, uint8_t *newkey, uint8_t *old_keys, uint8_t old_key_count);
uint8_t write_hidprox_to_t55xx(uint8_t format, uint32_t fc, uint64_t cn, uint32_t il, uint32_t oem, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count);
uint8_t write_ioprox_to_t55xx(uint8_t *raw_data, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count);
uint8_t write_viking_to_t55xx(uint8_t *uid, uint8_t *newkey, uint8_t *old_keys, uint8_t old_key_count);
uint8_t write_pac_to_t55xx(uint8_t *data, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count);
uint8_t write_jablotron_to_t55xx(uint8_t *uid, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count);
uint8_t write_idteck_to_t55xx(uint8_t *data, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count);
uint8_t write_indala_to_t55xx(uint8_t *raw8, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count);
uint8_t write_indala224_to_t55xx(uint8_t *raw28, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count);
#if defined(PROJECT_CHAMELEON_ULTRA)
uint8_t lf_t55xx_write_block(uint8_t block, uint32_t word, uint32_t passwd, bool use_passwd, bool page1);
#endif
