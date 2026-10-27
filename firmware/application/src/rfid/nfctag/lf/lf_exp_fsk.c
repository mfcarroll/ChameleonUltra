#include "lf_exp_fsk.h"

#include <string.h>

#include "app_status.h"
#include "fds_util.h"
#include "lf_selftrim_fsk.h"
#include "lf_tag_em.h"
#include "nrf.h"
#include "tag_emulation.h"

#define EXP_SLOTS (8)
#define EXP_FDS_FILE_ID (0x1F00)  // EXPERIMENT: outside the ranges in fds_ids.h
#define EXP_FDS_RECORD_KEY (0x1)
#define EXP_REPLY_VERSION (1u)

static uint8_t m_presets[EXP_SLOTS];  // 0xFF = unset (default preset)
static bool m_presets_loaded = false;

// Fields detected per slot since a cold power-on or a clear: kept through sleep like mode 9's store, so a tester
// can see afterwards whether the reader's field was seen at all.
#define EXP_FIELDS_MAGIC (0x45584631u)
static __attribute__((section(".noinit_expfsk"))) struct {
    uint32_t magic;
    uint16_t fields[EXP_SLOTS];
} m_fields;

static void presets_load(void) {
    uint16_t len = sizeof(m_presets);
    if (!fds_read_sync(EXP_FDS_FILE_ID, EXP_FDS_RECORD_KEY, &len, m_presets) || len != sizeof(m_presets)) {
        memset(m_presets, 0xFF, sizeof(m_presets));
    }
    m_presets_loaded = true;
}

uint8_t lf_exp_fsk_preset(uint8_t slot) {
    if (!m_presets_loaded) {
        if (__get_IPSR() != 0) {
            return LF_EXP_FSK_DEFAULT_PRESET;  // flash reads only from thread mode (the load callback runs first)
        }
        presets_load();
    }
    if (slot >= EXP_SLOTS || m_presets[slot] > LF_EXP_FSK_SELFTRIM) {
        return LF_EXP_FSK_DEFAULT_PRESET;
    }
    return m_presets[slot];
}

void lf_exp_fsk_note_field(void) {
    const uint8_t slot = tag_emulation_get_slot();
    if (m_fields.magic != EXP_FIELDS_MAGIC) {
        memset(&m_fields, 0, sizeof(m_fields));
        m_fields.magic = EXP_FIELDS_MAGIC;
    }
    if (slot < EXP_SLOTS && m_fields.fields[slot] < 0xFFFF) {
        m_fields.fields[slot]++;
    }
}

static uint16_t put_u16(uint8_t *o, uint16_t n, uint16_t v) {
    o[n++] = (uint8_t)(v >> 8);
    o[n++] = (uint8_t)v;
    return n;
}

static uint16_t put_u32(uint8_t *o, uint16_t n, uint32_t v) {
    n = put_u16(o, n, (uint16_t)(v >> 16));
    return put_u16(o, n, (uint16_t)v);
}

// Reply (big-endian):
//   version u8 (1), active slot u8 (0-7), default preset u8, K u8,
//   live: drive requested u8, drive active u8, trim requested s16, trim applied s16, frame ticks u32,
//         mode 9 status length u8 + lf_selftrim_fsk_auto_status (locked u8, moves u16, rejects u16,
//         last wide search ppm x10 s16, wide searches u16, kept u8, kept trim s16, restores u16, ...),
//   then per slot (8 x 18 bytes): stored preset u8 (0xFF unset), preset in use u8, slot's LF type u16,
//         fields seen u16, kept valid u8, kept LF type u16, kept trim s16 (steps), kept reader line u32 (mHz;
//         reader vs our crystal in ppm = (mHz / 1000 - 5000) / 0.125), fields averaged u16, pad u8.
uint16_t lf_exp_fsk_command(const uint8_t *req, uint16_t len, uint8_t *o, uint16_t *status) {
    *status = STATUS_SUCCESS;
    lf_exp_fsk_preset(0);  // the presets loaded (thread mode)
    if (len == 1 && req[0] == 0xC1) {  // clear: fields seen and mode 9's kept readers
        memset(&m_fields, 0, sizeof(m_fields));
        m_fields.magic = EXP_FIELDS_MAGIC;
        lf_selftrim_fsk_keep_clear();
    } else if (len == 2) {
        if (req[0] >= EXP_SLOTS || (req[1] > LF_EXP_FSK_SELFTRIM && req[1] != 0xFF)) {
            *status = STATUS_PAR_ERR;
            return 0;
        }
        m_presets[req[0]] = req[1];
        if (!fds_write_sync(EXP_FDS_FILE_ID, EXP_FDS_RECORD_KEY, sizeof(m_presets), m_presets)) {
            *status = STATUS_FLASH_WRITE_FAIL;
            return 0;
        }
    } else if (len != 0) {
        *status = STATUS_PAR_ERR;
        return 0;
    }

    uint16_t n = 0;
    o[n++] = EXP_REPLY_VERSION;
    o[n++] = tag_emulation_get_slot();
    o[n++] = LF_EXP_FSK_DEFAULT_PRESET;
    o[n++] = LF_EXP_FSK_K;
    uint8_t drive_act;
    o[n++] = lf_tag_em_drive_set(0xFF, &drive_act);
    o[n++] = drive_act;
    int16_t applied;
    uint32_t frame_ticks;
    n = put_u16(o, n, (uint16_t)lf_tag_em_trim_set(false, 0, &applied, &frame_ticks));
    n = put_u16(o, n, (uint16_t)applied);
    n = put_u32(o, n, frame_ticks);
    const uint16_t at = n++;
    o[at] = (uint8_t)lf_selftrim_fsk_auto_status(&o[n]);
    n += o[at];
    const bool fields_ok = m_fields.magic == EXP_FIELDS_MAGIC;
    for (uint8_t s = 0; s < EXP_SLOTS; s++) {
        tag_slot_specific_type_t types;
        tag_emulation_get_specific_types_by_slot(s, &types);
        o[n++] = m_presets[s];
        o[n++] = lf_exp_fsk_preset(s);
        n = put_u16(o, n, (uint16_t)types.tag_lf);
        n = put_u16(o, n, fields_ok ? m_fields.fields[s] : 0);
        uint16_t kt = 0, kn = 0;
        int16_t ktrim = 0;
        uint32_t kfp = 0;
        o[n++] = lf_selftrim_fsk_kept_slot(s, &kt, &ktrim, &kfp, &kn);
        n = put_u16(o, n, kt);
        n = put_u16(o, n, (uint16_t)ktrim);
        n = put_u32(o, n, kfp);
        n = put_u16(o, n, kn);
        o[n++] = 0;
    }
    return n;
}
