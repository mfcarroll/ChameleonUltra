// Hitag 2 password-mode reading. The command timings follow a Hitag 2 reader's (the PM3's armsrc/hitag2.c): a gap of
// 6 carrier periods starts each bit, a 0 is 20 periods gap to gap and a 1 is 30, and a stop gap and 36 periods of
// field end the command. A test tag began its answer about 195 periods after the stop gap, so about 160 after the
// command's end. It answers in Manchester at RF/32 behind 5 or 6 start-of-frame ones.

#include "lf_hitag2_reader.h"

#include <string.h>

#include "app_status.h"
#include "bsp_delay.h"
#include "lf_125khz_radio.h"
#include "lf_hitag2_frame.h"

#define HT2_RESET_MS      (20u)    // field off, so the tag starts from power-up
// The tag takes 313 periods to power up and then waits 232 for START_AUTH, so it must come 2.5 to 4.3 ms after the
// field. 371 periods matches the PM3's reader.
#define HT2_POWER_UP_US   (371u * 8u)
#define HT2_TRIES         (10u)    // START_AUTHs per use: a tag may answer only every second one, and a scan wants three
#define HT2_PAGE_TRIES    (6u)     // READ PAGEs per page, for two answers that agree

// The framing learned in this use (a scan or a read). Each use starts from a field reset, and may meet another tag.
static ht2_frame_t m_frame;

static lf_rdr_link_t m_link = {
    .gap_us = 6u * 8u,
    // Stopping and restarting the drive adds about 14 µs to each bit, measured on the Flipper, so a 0 of 20 periods and
    // a 1 of 30 are asked for that much shorter.
    .bit0_us = 20u * 8u - 14u,
    .bit1_us = 30u * 8u - 14u,
    .eof_gap = 1 | LF_RDR_DAMP_GAPS,
    .stop_us = 36u * 8u,
    .coding = LF_IA_CODING_MANCHESTER,
    .coding_flags = 0,
    .bit_fc = 32,
    .window_us = 1000,
    .capture_n = 2048,
    .min_depth = HT2_MIN_DEPTH,
    // The reader's detector falls back from a load slowly, so slice near the low rail.
    .slice_pct = 25,
};

lf_rdr_link_t *lf_hitag2_reader_link(void) {
    return &m_link;
}

static void bits_put(uint8_t *bits, size_t first, uint32_t v, size_t n) {
    for (size_t i = 0; i < n; i++) {
        lf_ia_bit_put(bits, first + i, (v >> (n - 1u - i)) & 1u);
    }
}

// Sends a command of n bits and returns its 32-bit answer.
static bool exchange(uint32_t cmd, size_t n, uint32_t power_up_us, uint32_t *page) {
    uint8_t tx[4] = {0};
    bits_put(tx, 0, cmd, n);
    lf_rdr_reply_t rx;
    if (!lf_rdr_transact(&m_link, tx, n, power_up_us, &rx)) {
        return false;
    }
    return ht2_frame_page(&m_frame, rx.bits, rx.n_bits, rx.clean, page);
}

// Powers the tag up and sends START_AUTH (11000) until it has answered with its UID `need` times alike. The last
// answered START_AUTH leaves the tag waiting for the password.
static bool start_auth(uint32_t *uid, uint32_t need) {
    lf_rdr_field(false);
    bsp_delay_ms(HT2_RESET_MS);
    ht2_frame_reset(&m_frame);
    ht2_agree_t agree;
    ht2_agree_reset(&agree);
    for (uint32_t i = 0; i < HT2_TRIES; i++) {
        uint32_t v;
        if (!exchange(0x18u, 5, i == 0 ? HT2_POWER_UP_US : 0, &v)) {
            continue;
        }
        if (need < 2 || ht2_agree_add(&agree, v, uid)) {
            *uid = v;
            return true;
        }
    }
    return false;
}

uint8_t lf_hitag2_reader_uid(uint32_t *uid) {
    // Password mode has no CRC: a UID counts once two answers agree.
    const bool ok = start_auth(uid, 2);
    lf_rdr_field(false);
    return ok ? STATUS_LF_TAG_OK : STATUS_LF_TAG_NO_FOUND;
}

uint8_t lf_hitag2_reader_read(uint32_t password, uint32_t pages[8], uint8_t *read_mask) {
    *read_mask = 0;
    memset(pages, 0, 8u * sizeof(uint32_t));
    // One START_AUTH answer is enough here: page 0, read twice alike below, has to match it.
    uint32_t uid;
    if (!start_auth(&uid, 1)) {
        lf_rdr_field(false);
        return STATUS_LF_TAG_NO_FOUND;
    }

    // The tag's answer to the password is page 3. It only shows the password was taken: page 3 is read below. If that
    // answer is lost, the tag may have taken the password anyway, and then it ignores a repeat. So each retry starts
    // again from a field reset and START_AUTH.
    uint32_t page3;
    bool authed = false;
    for (uint32_t i = 0; i < 3 && !authed; i++) {
        if (i > 0 && !start_auth(&uid, 1)) {
            continue;
        }
        authed = exchange(password, 32, 0, &page3);
    }
    if (!authed) {
        pages[0] = uid;
        *read_mask = 1u;
        lf_rdr_field(false);
        return STATUS_MF_ERR_AUTH;
    }

    // READ PAGE is 11 nnn 00 ~nnn. Password mode has no CRC: a page counts once two answers agree.
    for (uint32_t p = 0; p < 8; p++) {
        const uint32_t cmd = 3u << 8 | p << 5 | (~p & 7u);
        ht2_agree_t agree;
        ht2_agree_reset(&agree);
        for (uint32_t i = 0; i < HT2_PAGE_TRIES; i++) {
            uint32_t v;
            if (exchange(cmd, 10, 0, &v) && ht2_agree_add(&agree, v, &pages[p])) {
                *read_mask |= 1u << p;
                break;
            }
        }
    }
    if ((*read_mask & 1u) && pages[0] != uid) {
        *read_mask &= (uint8_t)~1u;  // the UID and page 0 disagree: trust neither
    }
    lf_rdr_field(false);
    return *read_mask == 0xFFu ? STATUS_LF_TAG_OK : STATUS_LF_TAG_NO_FOUND;
}
