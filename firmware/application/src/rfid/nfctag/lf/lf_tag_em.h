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
#define LF_SECURAKEY_TAG_ID_SIZE 12

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
int lf_tag_securakey_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer);
bool lf_tag_securakey_data_factory(uint8_t slot, tag_specific_type_t tag_type);
bool is_lf_field_exists(void);

/** ⚠ §3 instrumentation: 12 bytes of LF emulation state. ⛔ Remove before upstreaming. */
#define LF_TAG_EM_DEBUG_SIZE 14
void lf_tag_em_debug_get(uint8_t *out);
