#include "lf_ia_trace.h"

#include <string.h>

#include "lf_ia_codec.h"
#include "lf_ia_trace_flash.h"

#ifdef LF_IA_HOST_TEST
#define LOCK()   {
#define UNLOCK() }
#define RETAINED
#else
#include "app_util_platform.h"
#include "bsp_wdt.h"
#define LOCK()   CRITICAL_REGION_ENTER()
#define UNLOCK() CRITICAL_REGION_EXIT()
// The retained noinit region (noinit.ld) survives System OFF and the reset that USB wake causes, so the RAM buffer
// does too. check_wakeup_src wipes it to 0xFF at the first power-up, which makes the store's header invalid.
#define RETAINED __attribute__((section(".noinit_lfia")))
#endif

// ---- The flash format -------------------------------------------------------------------------------------------
//
// The area starts with a 16-byte start block: START_MAGIC, LF_IA_LOG_FORMAT (u16), 0 (u16), the stream's capacity
// (u32), 0 (u32). The stream of entries follows it, and the rest of the area is erased (all ones).
//
// An entry is a whole number of words: a header word (ENTRY_MAGIC, its type, its length in words, 0), the payload
// padded with zeros, and a trailer word (the CRC-16/CCITT of everything before the trailer, the length again, 0). The
// trailer's zero byte means an entry never ends in an erased word, so the stream's end is the area's last word that
// is not all ones.
//
// Payloads, little-endian:
// - E_HEADER, at each session's start: boot (u16), session (u16), field-on time (u32), slot 1 to 8 (u8), mode (u8),
//   0 (u16).
// - E_COMMAND: session (u16), flags (u8, CF_), verdict | decode status << 4 (u8), the end of the last gap (u32), from
//   there to detect, ready and due (u16 each), the start error against due (i16), the answer's length (u16), from the
//   last gap's end to the last time the field was seen (u16, 0xFFFF: never), gaps (u8), bits (u8), sent bits (u16),
//   split (u16), level (u16), noise (u16), the widest gap (u16), runs (u16), then the decoded bits and the sent bits
//   (as many bytes as they need, up to 16 each), then with CF_GAPS the gaps (start and width, u16 each, up to 48).
// - E_FIELD_OFF: session (u16), flags (u8, the record's), measurement status (u8), the time the field went (u32),
//   level (u16), SNR (i16, 0.1 dB), measured carrier (u32, 0.1 Hz, 0: none), measurement time (u8, 32 µs units),
//   outcome (u8, OUTCOME_), commands (u16).
// - E_TOTALS, when a save starts and at sleep: boot (u16), slot_done (u16), session (u32), dropped (u32), not kept
//   (u32), 0 (u32), then lf_ia_stats_t.
// - E_FIRMWARE, after a clear and at each start-up that lost the retained RAM (a flash or a power-on reset): the
//   firmware's git version, NUL-padded. The entries after it, until the next E_FIRMWARE, come from that firmware.
#define LF_IA_LOG_FORMAT (1u)
#define START_MAGIC      (0x4C41494Cu)  // "LIAL"
#define START_BYTES      (16u)
#define ENTRY_MAGIC      (0xA0u + LF_IA_LOG_FORMAT)
#define ENTRY_MAX        (4u + 34u + 16u + 16u + 4u * LF_IA_TRACE_GAPS + 4u)

enum { E_HEADER = 1, E_FIELD_OFF = 2, E_COMMAND = 3, E_TOTALS = 4, E_FIRMWARE = 5 };

#ifndef GIT_VERSION
#define GIT_VERSION "unknown"
#endif

#define CF_LATE          (1u << 0)
#define CF_GAPS_DROPPED  (1u << 1)
#define CF_ENCODE_FAILED (1u << 2)
#define CF_MEASURED      (1u << 3)  // the carrier was measured when the engine answered
#define CF_GAPS          (1u << 4)  // the gaps follow the bits
#define CF_TX            (1u << 5)  // the modulator started the answer

enum { OUTCOME_NONE = 0, OUTCOME_FAILURE = 1, OUTCOME_SUCCESS = 2 };

// ---- The RAM buffer and saving ------------------------------------------------------------------------------------

#define RAM_BYTES   (12288u)
#define SAVE_AT     (RAM_BYTES / 2u)  // a save starts once the buffer holds this much
// Bytes per write while quiet. The CPU halts for each word, so a reader's field that comes on meanwhile waits for the
// piece to finish: 16 words, about 0.7 ms.
#define PIECE_QUIET (64u)
#define PIECE_SLEEP (1024u)
// If the flash left after the RAM buffer's contents is less than this, the log is filling: it keeps a presentation
// only if it is its slot's first kept success or first kept failure.
#define FILL_MARGIN (RAM_BYTES)
#define MAX_FAILS   (8u)

// The store's check doesn't compare RAM_BYTES, so a change to it needs a new version.
#define STORE_MAGIC   (0x33414C46u)  // "FLA3"
#define STORE_VERSION (1u)

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint8_t state;
    uint8_t flags;
    uint16_t boot;
    lf_ia_stats_t stats;  // stats.sessions numbers the sessions
    uint16_t slot_done;
    uint16_t recover_bad;
    uint32_t dropped;
    uint32_t not_kept;
    uint32_t erase_next;  // the next page to check, while erasing
    // The stream: bytes [0, saved) are in flash, [saved, head) in ram, at their offset modulo RAM_BYTES.
    uint32_t saved;
    uint32_t head;
    uint8_t ram[RAM_BYTES];
} store_t;

static RETAINED store_t m_store;

static bool m_boot_counted;  // zeroed at boot, unlike the store

// The record the engine fills, and whether it is committed and not yet encoded.
static lf_ia_trace_rec_t m_rec;
static bool m_rec_committed;

// The session in progress, zeroed at boot: a reset during a session loses its context, not its entries.
static struct {
    bool on;
    uint32_t start;  // the stream offset of its header
    uint16_t accept_bits;
    uint8_t slot;    // 1 to 8
    uint16_t commands;
    bool success;
    uint8_t lens[8];  // the decoded lengths whose first command kept its gaps
    uint8_t n_lens;
} m_pres;

enum { OP_NONE, OP_WRITE, OP_ERASE, OP_START };
static uint8_t m_op;
static uint32_t m_op_len;
static uint32_t m_op_gen;
static volatile bool m_op_done;
static volatile bool m_op_ok;
static uint32_t m_gen;   // a clear changes it, so an operation started before the clear is ignored
static uint8_t m_fails;  // operations in a row that failed
static bool m_saving;    // a save is in progress: it goes on while quiet until the buffer is empty
static uint32_t m_wbuf[PIECE_SLEEP / 4u];

static const uint16_t m_crc_nibble[16] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
};

static uint16_t crc16(const uint8_t *p, size_t n) {
    uint16_t c = 0xFFFF;
    for (size_t i = 0; i < n; i++) {
        c = (uint16_t)(c << 4) ^ m_crc_nibble[((c >> 12) ^ (p[i] >> 4)) & 0xFu];
        c = (uint16_t)(c << 4) ^ m_crc_nibble[((c >> 12) ^ p[i]) & 0xFu];
    }
    return c;
}

static uint32_t capacity(void) {
    return lf_ia_flash_size() - START_BYTES;
}

static bool store_valid(void) {
    return m_store.magic == STORE_MAGIC && m_store.version == STORE_VERSION && m_store.size == sizeof(store_t);
}

static void store_reset(void) {
    memset(&m_store, 0, offsetof(store_t, ram));
    m_store.magic = STORE_MAGIC;
    m_store.version = STORE_VERSION;
    m_store.size = sizeof(store_t);
    m_store.state = lf_ia_flash_base() ? LF_IA_LOG_ERASING : LF_IA_LOG_BROKEN;
}

// Callers that can race the engine's interrupt writes hold the lock around this.
static void store_ensure(void) {
    if (!store_valid()) {
        store_reset();
    }
    if (!m_boot_counted) {
        m_boot_counted = true;
        m_store.boot++;
    }
}

static void put16(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put32(uint8_t *p, uint32_t v) {
    put16(p, v);
    put16(p + 2, v >> 16);
}

static uint16_t get16(const uint8_t *p) {
    return (uint16_t)(p[0] | p[1] << 8);
}

static uint32_t get32(const uint8_t *p) {
    return get16(p) | (uint32_t)get16(p + 2) << 16;
}

static uint16_t sat16(uint32_t v) {
    return (uint16_t)(v > UINT16_MAX ? UINT16_MAX : v);
}

// Frames payload as an entry and appends it to the RAM buffer, or counts it as dropped if it doesn't fit.
static void append(uint8_t type, const uint8_t *payload, size_t len) {
    uint8_t e[ENTRY_MAX];
    const size_t words = 2u + (len + 3u) / 4u;
    const size_t n = 4u * words;
    e[0] = ENTRY_MAGIC;
    e[1] = type;
    e[2] = (uint8_t)words;
    e[3] = 0;
    memcpy(e + 4, payload, len);
    memset(e + 4 + len, 0, n - 8u - len);
    put16(e + n - 4u, crc16(e, n - 4u));
    e[n - 2u] = (uint8_t)words;
    e[n - 1u] = 0;
    LOCK();
    store_ensure();
    if (RAM_BYTES - (m_store.head - m_store.saved) < n) {
        m_store.dropped++;
    } else {
        for (size_t i = 0; i < n; i++) {
            m_store.ram[(m_store.head + i) % RAM_BYTES] = e[i];
        }
        m_store.head += (uint32_t)n;
    }
    UNLOCK();
}

static void append_totals(void) {
    uint8_t p[20 + sizeof(lf_ia_stats_t)];
    LOCK();
    store_ensure();
    put16(p, m_store.boot);
    put16(p + 2, m_store.slot_done);
    put32(p + 4, m_store.stats.sessions);
    put32(p + 8, m_store.dropped);
    put32(p + 12, m_store.not_kept);
    put32(p + 16, 0);
    memcpy(p + 20, &m_store.stats, sizeof(lf_ia_stats_t));
    UNLOCK();
    append(E_TOTALS, p, sizeof(p));
}

static void append_firmware(void) {
    uint8_t p[32] = {0};
    const size_t n = strlen(GIT_VERSION) < sizeof(p) ? strlen(GIT_VERSION) : sizeof(p);
    memcpy(p, GIT_VERSION, n);
    append(E_FIRMWARE, p, (n + 4u) & ~3u);  // at least one NUL
}

// The gaps go in for a command that failed to decode, and for the first decoded command of each length in the
// session: they show the reader's pulse timing.
static bool keep_gaps(const lf_ia_trace_rec_t *r) {
    if (r->decode_status != LF_IA_DECODE_OK) {
        return true;
    }
    for (uint8_t i = 0; i < m_pres.n_lens; i++) {
        if (m_pres.lens[i] == r->n_bits) {
            return false;
        }
    }
    if (m_pres.n_lens < sizeof(m_pres.lens)) {
        m_pres.lens[m_pres.n_lens++] = (uint8_t)r->n_bits;
    }
    return true;
}

static void encode_command(const lf_ia_trace_rec_t *r) {
    uint8_t p[ENTRY_MAX];
    const bool answered = r->verdict == 1u && r->t_ready_us != 0;
    uint8_t flags = 0;
    flags |= (r->flags & LF_IA_TRF_LATE) ? CF_LATE : 0u;
    flags |= (r->flags & LF_IA_TRF_GAPS_DROPPED) ? CF_GAPS_DROPPED : 0u;
    flags |= (r->flags & LF_IA_TRF_ENCODE_FAILED) ? CF_ENCODE_FAILED : 0u;
    flags |= r->fc_measured_dhz ? CF_MEASURED : 0u;
    flags |= r->t_tx_us ? CF_TX : 0u;
    const bool gaps = keep_gaps(r);
    flags |= gaps ? CF_GAPS : 0u;

    memset(p, 0, 34);
    put16(p, m_store.stats.sessions);
    p[2] = flags;
    p[3] = (uint8_t)((r->verdict & 0xFu) | (r->decode_status & 0xFu) << 4);
    put32(p + 4, r->t_us);
    put16(p + 8, sat16(r->t_detect_us - r->t_us));
    if (answered) {
        put16(p + 10, sat16(r->t_ready_us - r->t_us));
        put16(p + 12, sat16(r->t_due_us - r->t_us));
        if (r->t_tx_us) {
            int32_t err = (int32_t)(r->t_tx_us - r->t_due_us);
            err = err > INT16_MAX ? INT16_MAX : err < INT16_MIN ? INT16_MIN : err;
            put16(p + 14, (uint16_t)(int16_t)err);
            put16(p + 16, sat16(r->t_tx_end_us - r->t_tx_us));
        }
    }
    const uint16_t seen = sat16(r->t_field_seen_us - r->t_us);
    put16(p + 18, r->t_field_seen_us == 0 ? UINT16_MAX : seen < UINT16_MAX ? seen : UINT16_MAX - 1u);
    p[20] = (uint8_t)(r->n_gaps > UINT8_MAX ? UINT8_MAX : r->n_gaps);
    p[21] = (uint8_t)(r->n_bits > UINT8_MAX ? UINT8_MAX : r->n_bits);
    put16(p + 22, r->n_tx_bits);
    put16(p + 24, sat16(r->split));
    put16(p + 26, r->level);
    put16(p + 28, r->n_noise);
    put16(p + 30, r->max_gap_us);
    put16(p + 32, r->n_runs);
    size_t n = 34;
    const size_t nb = ((r->n_bits < 128u ? r->n_bits : 128u) + 7u) / 8u;
    memcpy(p + n, r->bits, nb);
    n += nb;
    const size_t nt = ((r->n_tx_bits < 128u ? r->n_tx_bits : 128u) + 7u) / 8u;
    memcpy(p + n, r->tx_bits, nt);
    n += nt;
    if (gaps) {
        const size_t ng = r->n_gaps < LF_IA_TRACE_GAPS ? r->n_gaps : LF_IA_TRACE_GAPS;
        for (size_t i = 0; i < ng; i++, n += 4) {
            put16(p + n, r->gaps[i].start_us);
            put16(p + n + 2, r->gaps[i].width_us);
        }
    }
    append(E_COMMAND, p, n);

    m_pres.commands++;
    if (answered && m_pres.accept_bits && r->n_bits == m_pres.accept_bits) {
        m_pres.success = true;
    }
}

// Logs the session's end, and if the log is filling, drops the whole session unless the rule keeps it.
static void encode_field_off(const lf_ia_trace_rec_t *r) {
    const uint8_t outcome = m_pres.commands == 0 ? OUTCOME_NONE : m_pres.success ? OUTCOME_SUCCESS : OUTCOME_FAILURE;
    uint8_t p[20];
    put16(p, m_store.stats.sessions);
    p[2] = r->flags;
    p[3] = r->meas_status;
    put32(p + 4, r->t_us);
    put16(p + 8, r->level);
    put16(p + 10, (uint16_t)r->meas_snr_db10);
    put32(p + 12, r->fc_measured_dhz);
    p[16] = r->meas_us32;
    p[17] = outcome;
    put16(p + 18, m_pres.commands);
    append(E_FIELD_OFF, p, sizeof(p));
    if (!m_pres.on) {
        return;
    }
    m_pres.on = false;

    const uint16_t bit = m_pres.slot >= 1 && m_pres.slot <= 8
                             ? (uint16_t)(1u << (2u * (m_pres.slot - 1u) + (outcome == OUTCOME_SUCCESS ? 0u : 1u)))
                             : 0u;
    LOCK();
    // Filling stays on until a clear. The test counts the RAM buffer's contents, which shrink when a session is
    // dropped, so without the flag the next session could pass it again.
    if (m_store.state != LF_IA_LOG_ERASING &&
        capacity() - m_store.saved < (m_store.head - m_store.saved) + FILL_MARGIN) {
        m_store.flags |= LF_IA_LOGI_FILLING;
    }
    const bool filling = m_store.flags & LF_IA_LOGI_FILLING;
    if (filling && (outcome == OUTCOME_NONE || (m_store.slot_done & bit))) {
        m_store.head = m_pres.start;
        m_store.not_kept++;
    } else if (outcome != OUTCOME_NONE) {
        m_store.slot_done |= bit;
    }
    UNLOCK();
}

void lf_ia_trace_flush(void) {
    if (!m_rec_committed) {
        return;
    }
    m_rec_committed = false;
    if (m_rec.type == LF_IA_TR_COMMAND) {
        encode_command(&m_rec);
    } else {
        encode_field_off(&m_rec);
    }
}

void lf_ia_trace_session_start(uint32_t t_us, uint8_t slot, uint8_t mode, uint16_t accept_bits) {
    lf_ia_trace_flush();
    uint8_t p[12];
    LOCK();
    store_ensure();
    m_store.stats.sessions++;
    m_pres.start = m_store.head;
    put16(p, m_store.boot);
    put16(p + 2, m_store.stats.sessions);
    UNLOCK();
    m_pres.on = true;
    m_pres.accept_bits = accept_bits;
    m_pres.slot = slot;
    m_pres.commands = 0;
    m_pres.success = false;
    m_pres.n_lens = 0;
    put32(p + 4, t_us);
    p[8] = slot;
    p[9] = mode;
    put16(p + 10, 0);
    append(E_HEADER, p, sizeof(p));
}

lf_ia_trace_rec_t *lf_ia_trace_session_end(uint32_t t_us, uint16_t level) {
    lf_ia_trace_rec_t *r = lf_ia_trace_begin(LF_IA_TR_FIELD_OFF);
    r->t_us = t_us;
    r->level = level;
    lf_ia_trace_commit();
    return r;
}

lf_ia_trace_rec_t *lf_ia_trace_begin(uint8_t type) {
    lf_ia_trace_flush();
    memset(&m_rec, 0, sizeof(m_rec));
    m_rec.type = type;
    return &m_rec;
}

void lf_ia_trace_commit(void) {
    m_rec_committed = true;
}

lf_ia_trace_rec_t *lf_ia_trace_last(void) {
    return m_rec_committed ? &m_rec : NULL;
}

lf_ia_stats_t *lf_ia_trace_stats(void) {
    store_ensure();
    return &m_store.stats;
}

// ---- Flash --------------------------------------------------------------------------------------------------------

static bool page_erased(uint32_t offset) {
    const uint32_t *w = (const uint32_t *)(lf_ia_flash_base() + offset);
    for (uint32_t i = 0; i < LF_IA_FLASH_PAGE / 4u; i++) {
        if (w[i] != UINT32_MAX) {
            return false;
        }
    }
    return true;
}

static bool start_valid(void) {
    const uint8_t *s = lf_ia_flash_base();
    return get32(s) == START_MAGIC && get16(s + 4) == LF_IA_LOG_FORMAT && get32(s + 8) == capacity();
}

static void fail(void) {
    if (++m_fails >= MAX_FAILS) {
        m_store.state = LF_IA_LOG_BROKEN;
    }
}

// Applies the result of the operation that completed.
static void op_finish(void) {
    const uint8_t op = m_op;
    m_op = OP_NONE;
    m_op_done = false;
    if (m_op_gen != m_gen) {
        return;
    }
    if (!m_op_ok) {
        fail();
        return;
    }
    if (op == OP_WRITE) {
        const uint8_t *dst = lf_ia_flash_base() + START_BYTES + m_store.saved;
        if (memcmp(dst, m_wbuf, m_op_len) != 0) {
            m_store.state = LF_IA_LOG_BROKEN;
            return;
        }
        m_fails = 0;
        LOCK();
        m_store.saved += m_op_len;
        UNLOCK();
    } else if (op == OP_START) {
        m_fails = 0;
        m_store.state = start_valid() ? LF_IA_LOG_READY : LF_IA_LOG_BROKEN;
    } else if (op == OP_ERASE) {
        m_fails = 0;  // the next step checks the page again
    }
}

static bool op_start(uint8_t op, uint32_t offset, uint32_t len) {
    m_op_done = false;
    m_op = op;
    m_op_len = len;
    m_op_gen = m_gen;
    const bool started = op == OP_ERASE ? lf_ia_flash_erase_page(offset) : lf_ia_flash_write(offset, m_wbuf, len);
    if (!started) {
        m_op = OP_NONE;
    }
    return started;
}

// Checks the pages from erase_next, erases the first that holds data, and writes the start block once all are erased.
static void erase_step(void) {
    while (m_store.erase_next < lf_ia_flash_size()) {
        if (!page_erased(m_store.erase_next)) {
            op_start(OP_ERASE, m_store.erase_next, 0);
            return;
        }
        m_store.erase_next += LF_IA_FLASH_PAGE;
    }
    memset(m_wbuf, 0, START_BYTES);
    uint8_t *b = (uint8_t *)m_wbuf;
    put32(b, START_MAGIC);
    put16(b + 4, LF_IA_LOG_FORMAT);
    put32(b + 8, capacity());
    op_start(OP_START, 0, START_BYTES);
}

// Writes the next piece of the RAM buffer. Returns whether a write started.
static bool save_step(uint32_t piece, bool force) {
    uint32_t saved, pending;
    LOCK();
    saved = m_store.saved;
    pending = m_store.head - saved;
    UNLOCK();
    if (!m_saving && !force && pending < SAVE_AT) {
        return false;
    }
    if (!m_saving && pending > 0) {
        m_saving = true;
        if (!force && !(m_store.flags & LF_IA_LOGI_FILLING)) {
            append_totals();
        }
        LOCK();
        pending = m_store.head - saved;
        UNLOCK();
    }
    if (pending == 0) {
        m_saving = false;
        return false;
    }
    uint32_t n = pending < piece ? pending : piece;
    if (n > capacity() - saved) {
        n = capacity() - saved;
    }
    if (n == 0) {
        m_store.state = LF_IA_LOG_FULL;
        m_saving = false;
        return false;
    }
    uint8_t *b = (uint8_t *)m_wbuf;
    LOCK();
    for (uint32_t i = 0; i < n; i++) {
        b[i] = m_store.ram[(saved + i) % RAM_BYTES];
    }
    UNLOCK();
    return op_start(OP_WRITE, START_BYTES + saved, n);
}

void lf_ia_trace_flash_done(bool ok) {
    m_op_ok = ok;
    m_op_done = true;
}

void lf_ia_trace_process(bool quiet) {
    store_ensure();
    if (m_op != OP_NONE) {
        if (!m_op_done) {
            return;
        }
        op_finish();
    }
    if (!quiet || lf_ia_flash_busy()) {
        return;
    }
    if (m_store.state == LF_IA_LOG_ERASING) {
        erase_step();
    } else if (m_store.state == LF_IA_LOG_READY) {
        save_step(PIECE_QUIET, false);
    }
}

// Waits for the operation in progress, if any, and applies its result. Returns false if it never completed.
static bool op_wait(void) {
    for (uint32_t i = 0; m_op != OP_NONE && !m_op_done && i < 100000u; i++) {
        lf_ia_flash_wait();
    }
    if (m_op != OP_NONE && m_op_done) {
        op_finish();
    }
    return m_op == OP_NONE;
}

void lf_ia_trace_save_all(void) {
    lf_ia_trace_flush();
    store_ensure();
    if (!op_wait() || m_store.state != LF_IA_LOG_READY) {
        return;
    }
    if (!(m_store.flags & LF_IA_LOGI_FILLING)) {
        append_totals();
    }
    m_saving = false;
    for (uint32_t guard = 0; guard < 4u * RAM_BYTES / PIECE_SLEEP; guard++) {
        if (!save_step(PIECE_SLEEP, true) || !op_wait() || m_store.state != LF_IA_LOG_READY) {
            break;
        }
#ifndef LF_IA_HOST_TEST
        bsp_wdt_feed();
#endif
    }
}

// ---- Start-up, clear and read-out ---------------------------------------------------------------------------------

// Takes the write position, the counters and the totals from flash, after the retained RAM lost them. The counters
// come from the last header and the last totals entry. The totals miss what happened after the last save that wrote
// one.
static void recover(void) {
    const uint8_t *s = lf_ia_flash_base() + START_BYTES;
    uint32_t end = capacity();
    while (end >= 4u && get32(s + end - 4u) == UINT32_MAX) {
        end -= 4u;
    }
    uint32_t boot = 0, session = 0, pos = 0;
    bool bad_run = false;
    while (pos < end) {
        const uint8_t *e = s + pos;
        const uint32_t n = 4u * e[2];
        const bool ok = pos + 8u <= end && e[0] == ENTRY_MAGIC && e[2] >= 2u && pos + n <= end &&
                        e[n - 2u] == e[2] && get16(e + n - 4u) == crc16(e, n - 4u);
        if (!ok) {
            if (!bad_run) {
                m_store.recover_bad++;
            }
            bad_run = true;
            pos += 4u;
            continue;
        }
        bad_run = false;
        const uint8_t *p = e + 4;
        if (e[1] == E_HEADER) {
            boot = get16(p);
            // A header carries the session's low 16 bits, and sessions only count up.
            uint32_t s = (session & ~0xFFFFu) | get16(p + 2);
            session = s < session ? s + 0x10000u : s;
        } else if (e[1] == E_TOTALS) {
            boot = get16(p) > boot ? get16(p) : boot;
            m_store.slot_done = get16(p + 2);
            session = get32(p + 4) > session ? get32(p + 4) : session;
            m_store.dropped = get32(p + 8);
            m_store.not_kept = get32(p + 12);
            memcpy(&m_store.stats, p + 20, sizeof(lf_ia_stats_t));
        }
        pos += n;
    }
    m_store.boot = (uint16_t)boot;
    m_store.stats.sessions = session;
    m_store.saved = end;
    m_store.head = end;
    m_store.flags |= LF_IA_LOGI_RECOVERED;
    m_store.state = LF_IA_LOG_READY;
}

void lf_ia_trace_init(void) {
    if (store_valid()) {
        return;
    }
    store_reset();
    if (lf_ia_flash_base() && start_valid()) {
        recover();
    }
    append_firmware();
}

void lf_ia_trace_clear(void) {
    LOCK();
    store_reset();
    m_boot_counted = false;
    store_ensure();  // this boot counts as boot 1
    m_rec_committed = false;
    m_pres.on = false;
    m_saving = false;
    m_fails = 0;
    m_gen++;
    UNLOCK();
    append_firmware();
}

void lf_ia_trace_info(lf_ia_trace_info_t *out) {
    memset(out, 0, sizeof(*out));
    LOCK();
    store_ensure();
    out->format = LF_IA_LOG_FORMAT;
    out->state = m_store.state;
    out->flags = m_store.flags;
    out->erase_left = m_store.state == LF_IA_LOG_ERASING
                          ? (uint16_t)((lf_ia_flash_size() - m_store.erase_next) / LF_IA_FLASH_PAGE) : 0u;
    out->boot = m_store.boot;
    out->session = m_store.stats.sessions;
    out->capacity = lf_ia_flash_base() ? capacity() : 0u;
    out->saved = m_store.saved;
    out->pending = m_store.head - m_store.saved;
    out->ram_size = RAM_BYTES;
    out->dropped = m_store.dropped;
    out->not_kept = m_store.not_kept;
    out->slot_done = m_store.slot_done;
    out->recover_bad = m_store.recover_bad;
    out->stats = m_store.stats;
    UNLOCK();
}

size_t lf_ia_trace_read(uint32_t offset, uint8_t *out, size_t max, uint32_t *end_out) {
    size_t n = 0;
    uint32_t end = 0;
    store_ensure();
    while (n < max) {
        const uint32_t pos = offset + (uint32_t)n;
        uint32_t saved;
        size_t take = 0;
        LOCK();
        saved = m_store.saved;
        end = m_store.head;
        if (pos >= saved && pos < end) {
            // At most 256 bytes with interrupts off.
            take = end - pos < max - n ? end - pos : max - n;
            take = take < 256u ? take : 256u;
            for (size_t i = 0; i < take; i++) {
                out[n + i] = m_store.ram[(pos + i) % RAM_BYTES];
            }
        }
        UNLOCK();
        if (pos < saved) {
            take = saved - pos < max - n ? saved - pos : max - n;
            memcpy(out + n, lf_ia_flash_base() + START_BYTES + pos, take);
        }
        if (take == 0) {
            break;
        }
        n += take;
    }
    *end_out = end;
    return n;
}
