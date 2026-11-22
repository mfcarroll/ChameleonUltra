// Host tests for the exchange log (lf_ia_trace.c) on a simulated flash area. The simulated flash only clears bits on a
// write, erases a page at a time, and runs one operation at a time, completed when the test says.

#define LF_IA_HOST_TEST
#include "../lf_ia_trace.c"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_FLASH (192u * 1024u)

static uint8_t g_flash[MAX_FLASH];
static uint32_t g_size = MAX_FLASH;
static bool g_auto = true;  // complete each operation at once
static struct {
    bool on;
    bool erase;
    uint32_t offset, len;
    uint8_t data[PIECE_SLEEP];
} g_op;
static int g_erases, g_writes;
static uint32_t g_cut = UINT32_MAX;  // the next write stops after this many bytes

static void sim_complete(void) {
    if (!g_op.on) {
        return;
    }
    g_op.on = false;
    if (g_op.erase) {
        memset(g_flash + g_op.offset, 0xFF, LF_IA_FLASH_PAGE);
        g_erases++;
    } else {
        const uint32_t n = g_op.len < g_cut ? g_op.len : g_cut;
        for (uint32_t i = 0; i < n; i++) {
            g_flash[g_op.offset + i] &= g_op.data[i];
        }
        g_cut = UINT32_MAX;
        g_writes++;
    }
    lf_ia_trace_flash_done(true);
}

const uint8_t *lf_ia_flash_base(void) { return g_flash; }
uint32_t lf_ia_flash_size(void) { return g_size; }

bool lf_ia_flash_write(uint32_t offset, const void *src, uint32_t len) {
    assert(offset % 4 == 0 && len % 4 == 0 && len <= PIECE_SLEEP && offset + len <= g_size);
    if (g_op.on) {
        return false;
    }
    g_op.on = true;
    g_op.erase = false;
    g_op.offset = offset;
    g_op.len = len;
    memcpy(g_op.data, src, len);
    if (g_auto) {
        sim_complete();
    }
    return true;
}

bool lf_ia_flash_erase_page(uint32_t offset) {
    assert(offset % LF_IA_FLASH_PAGE == 0 && offset < g_size);
    if (g_op.on) {
        return false;
    }
    g_op.on = true;
    g_op.erase = true;
    g_op.offset = offset;
    if (g_auto) {
        sim_complete();
    }
    return true;
}

bool lf_ia_flash_busy(void) { return g_op.on; }
void lf_ia_flash_wait(void) { sim_complete(); }

// ---- A parser, as a client would write it ----------------------------------------------------------------------

typedef struct {
    int headers, commands, offs, totals, bad, firmwares;
    int first_type;
    uint16_t boot[2048];
    uint16_t session[2048];  // per header
    uint8_t slot[2048];
    uint8_t outcome[2048];   // per field-off
    int with_gaps;
    int gaps_failed;         // commands with a failed decode that carry gaps
    uint32_t last_totals_session;
} parsed_t;

static void parse(const uint8_t *s, uint32_t n, parsed_t *p) {
    memset(p, 0, sizeof(*p));
    uint32_t pos = 0;
    bool bad_run = false;
    while (pos < n) {
        const uint8_t *e = s + pos;
        const uint32_t len = 4u * e[2];
        if (!(pos + 8 <= n && e[0] == ENTRY_MAGIC && e[2] >= 2 && pos + len <= n && e[len - 2] == e[2] &&
              get16(e + len - 4) == crc16(e, len - 4))) {
            p->bad += !bad_run;
            bad_run = true;
            pos += 4;
            continue;
        }
        bad_run = false;
        const uint8_t *q = e + 4;
        if (!p->first_type) {
            p->first_type = e[1];
        }
        switch (e[1]) {
            case E_HEADER:
                p->boot[p->headers] = get16(q);
                p->session[p->headers] = get16(q + 2);
                p->slot[p->headers] = q[8];
                p->headers++;
                break;
            case E_COMMAND:
                p->commands++;
                if (q[2] & CF_GAPS) {
                    p->with_gaps++;
                    if ((q[3] >> 4) != LF_IA_DECODE_OK) {
                        p->gaps_failed++;
                    }
                }
                break;
            case E_FIELD_OFF:
                p->outcome[p->offs++] = q[17];
                break;
            case E_FIRMWARE:
                assert(strcmp((const char *)q, GIT_VERSION) == 0);
                p->firmwares++;
                break;
            case E_TOTALS:
                p->totals++;
                p->last_totals_session = get32(q + 4);
                break;
        }
        pos += len;
    }
}

static uint8_t g_stream[MAX_FLASH + RAM_BYTES];

static uint32_t read_all(void) {
    uint32_t end = 0, n = 0;
    for (;;) {
        const size_t got = lf_ia_trace_read(n, g_stream + n, 1000, &end);
        if (got == 0) {
            break;
        }
        n += (uint32_t)got;
    }
    assert(n == end);
    return n;
}

// ---- Driving the log as the engine does --------------------------------------------------------------------------

static uint32_t g_t = 1000;

static void command(uint16_t n_bits, bool answered, uint8_t decode) {
    lf_ia_trace_rec_t *r = lf_ia_trace_begin(LF_IA_TR_COMMAND);
    g_t += 5000;
    r->t_us = g_t;
    r->t_detect_us = g_t + 400;
    r->n_gaps = (uint16_t)(n_bits + 1);
    r->n_bits = decode == LF_IA_DECODE_OK ? n_bits : 0;
    r->decode_status = decode;
    r->level = 1234;
    for (uint16_t i = 0; i < r->n_gaps && i < LF_IA_TRACE_GAPS; i++) {
        r->gaps[i].start_us = (uint16_t)(i * 200);
        r->gaps[i].width_us = 50;
    }
    r->max_gap_us = 50;
    memset(r->bits, 0xA5, sizeof(r->bits));
    r->fc_measured_dhz = 1250000;
    if (answered) {
        r->verdict = 1;
        r->t_ready_us = g_t + 600;
        r->t_due_us = g_t + 1568;
        r->n_tx_bits = 37;
        memset(r->tx_bits, 0x5A, sizeof(r->tx_bits));
    }
    lf_ia_trace_commit();
    if (answered) {
        r = lf_ia_trace_last();
        r->t_tx_us = g_t + 1570;
        r->t_tx_end_us = g_t + 1570 + 9472;
        r->t_field_seen_us = r->t_tx_end_us - 20;
    }
    lf_ia_trace_flush();
}

enum { NONE, FAILURE, SUCCESS };

static void present(uint8_t slot, int kind) {
    lf_ia_trace_session_start(g_t, slot, 1, 10);
    if (kind != NONE) {
        command(5, true, LF_IA_DECODE_OK);
        command(32, kind == SUCCESS, LF_IA_DECODE_OK);
        command(5, true, LF_IA_DECODE_OK);
        command(32, kind == SUCCESS, LF_IA_DECODE_OK);
        command(3, false, LF_IA_DECODE_TOO_FEW);
        if (kind == SUCCESS) {
            command(10, true, LF_IA_DECODE_OK);
            command(10, true, LF_IA_DECODE_OK);
        }
    }
    g_t += 3000;
    lf_ia_trace_rec_t *off = lf_ia_trace_session_end(g_t, 1200);
    off->fc_measured_dhz = 1250000;
    lf_ia_trace_flush();
}

static void process_until_idle(void) {
    for (int i = 0; i < 100000; i++) {
        lf_ia_trace_process(true);
        if (!g_op.on && m_store.state != LF_IA_LOG_ERASING && !m_saving) {
            break;
        }
    }
}

// A power-on reset: retained RAM wiped to 0xFF, the boot's statics zeroed.
static void power_on(void) {
    memset(&m_store, 0xFF, sizeof(m_store));
    m_boot_counted = false;
    m_rec_committed = false;
    memset(&m_pres, 0, sizeof(m_pres));
    m_op = OP_NONE;
    m_saving = false;
    g_op.on = false;
    lf_ia_trace_init();
}

static void fresh(uint32_t size) {
    g_size = size;
    memset(g_flash, 0xFF, sizeof(g_flash));
    g_auto = true;
    g_erases = g_writes = 0;
    power_on();
    process_until_idle();
    assert(m_store.state == LF_IA_LOG_READY);
}

// ---- Tests ---------------------------------------------------------------------------------------------------------

static void test_crc(void) {
    assert(crc16((const uint8_t *)"123456789", 9) == 0x29B1);  // CRC-16/CCITT-FALSE check value
}

static void test_blank_flash_starts_ready(void) {
    g_size = MAX_FLASH;
    memset(g_flash, 0xFF, sizeof(g_flash));
    power_on();
    assert(m_store.state == LF_IA_LOG_ERASING);
    lf_ia_trace_process(false);
    assert(m_store.state == LF_IA_LOG_ERASING && g_writes == 0);  // nothing while a field is present
    process_until_idle();
    assert(m_store.state == LF_IA_LOG_READY && g_erases == 0 && start_valid());
}

static void test_saves_in_order_while_quiet(void) {
    fresh(MAX_FLASH);
    for (int i = 0; i < 4; i++) {
        present((uint8_t)(1 + i % 8), i % 2 ? SUCCESS : FAILURE);
    }
    lf_ia_trace_process(true);
    assert(m_store.saved == 0);  // under half the buffer: no save yet
    uint32_t n = read_all();
    parsed_t p;
    parse(g_stream, n, &p);
    assert(p.headers == 4 && p.offs == 4 && p.bad == 0);

    // Fill past half; with a field present nothing is written.
    while (m_store.head - m_store.saved < SAVE_AT) {
        present(3, FAILURE);
    }
    const int writes = g_writes;
    lf_ia_trace_process(false);
    assert(g_writes == writes);
    // Quiet: one piece per call, each at most PIECE_QUIET.
    lf_ia_trace_process(true);
    assert(g_writes == writes + 1 && m_store.saved <= PIECE_QUIET);
    // A field interrupts the save, and the save goes on once quiet again.
    lf_ia_trace_process(false);
    assert(g_writes == writes + 1);
    process_until_idle();
    assert(m_store.head == m_store.saved);

    n = read_all();
    parse(g_stream, n, &p);
    assert(p.bad == 0 && p.totals == 1 && p.headers == p.offs);
    for (int i = 1; i < p.headers; i++) {
        assert(p.session[i] == p.session[i - 1] + 1);
    }
    // Per session: the first START_AUTH, the first password, the first read and every failed decode keep gaps.
    int successes = 0;
    for (int i = 0; i < p.offs; i++) {
        successes += p.outcome[i] == 2;
    }
    assert(p.gaps_failed == p.headers);
    assert(p.with_gaps == 3 * p.headers + successes);
}

static void test_survives_power_on(void) {
    fresh(MAX_FLASH);
    for (int i = 0; i < 40; i++) {
        present((uint8_t)(1 + i % 8), SUCCESS);
        process_until_idle();
    }
    lf_ia_trace_save_all();
    assert(m_store.head == m_store.saved && m_store.dropped == 0);
    const uint16_t boot = m_store.boot;
    const uint32_t sessions = m_store.stats.sessions;
    power_on();
    assert(m_store.flags & LF_IA_LOGI_RECOVERED);
    assert(m_store.state == LF_IA_LOG_READY && m_store.stats.sessions == sessions);
    present(2, FAILURE);
    assert(m_store.boot == boot + 1);
    lf_ia_trace_save_all();
    parsed_t p;
    parse(g_stream, read_all(), &p);
    assert(p.headers == 41 && p.bad == 0);
    assert(p.firmwares == 2 && p.first_type == E_FIRMWARE);  // one at the start, one at the power-on
    assert(p.session[40] == p.session[39] + 1 && p.boot[40] == p.boot[39] + 1);
}

static void test_other_format_erased_gently(void) {
    fresh(MAX_FLASH);
    for (int i = 0; i < 100; i++) {
        present(1, SUCCESS);
    }
    lf_ia_trace_save_all();
    g_flash[4] = 2;  // the start block now names format 2
    g_auto = false;
    g_erases = 0;
    power_on();
    assert(m_store.state == LF_IA_LOG_ERASING);
    present(4, FAILURE);  // logged to RAM meanwhile
    lf_ia_trace_process(true);
    assert(g_op.on && g_op.erase && g_op.offset == 0);
    lf_ia_trace_process(true);  // one operation at a time
    assert(g_erases == 0);
    sim_complete();
    lf_ia_trace_process(false);  // a field: no next page
    assert(!g_op.on);
    int calls = 0;
    while (m_store.state == LF_IA_LOG_ERASING) {
        lf_ia_trace_process(true);
        sim_complete();
        calls++;
    }
    assert(g_erases >= 2 && calls >= g_erases);  // a page per call, untouched pages skipped
    assert(m_store.saved == 0 && start_valid());
    g_auto = true;
    lf_ia_trace_save_all();
    parsed_t p;
    parse(g_stream, read_all(), &p);
    assert(p.headers == 1 && p.slot[0] == 4 && p.bad == 0);
}

static void test_filling_keeps_first_success_and_failure(void) {
    fresh(24u * 1024u);
    for (int i = 0; i < 400; i++) {
        present((uint8_t)(1 + i % 2), i % 3 == 0 ? SUCCESS : i % 3 == 1 ? FAILURE : NONE);
        process_until_idle();
    }
    assert(m_store.flags & LF_IA_LOGI_FILLING);
    assert(m_store.not_kept > 0);
    // Slots 3 and 4 have nothing kept yet: their first success and failure still go in.
    const uint32_t before = m_store.not_kept;
    present(3, SUCCESS);
    present(3, SUCCESS);
    present(4, FAILURE);
    assert(m_store.not_kept == before + 1);
    assert((m_store.slot_done >> 4) & 1 && (m_store.slot_done >> 7) & 1);
    lf_ia_trace_save_all();
    assert(m_store.state == LF_IA_LOG_READY || m_store.state == LF_IA_LOG_FULL);
    assert(m_store.saved <= capacity());
    parsed_t p;
    parse(g_stream, read_all(), &p);
    assert(p.bad == 0);
}

static void test_cut_write_is_skipped(void) {
    fresh(MAX_FLASH);
    for (int i = 0; i < 3; i++) {
        present(1, SUCCESS);
    }
    g_cut = 40;  // the next piece stops after 40 bytes
    lf_ia_trace_save_all();
    // The write's check fails: the log stops rather than write over a damaged stream.
    assert(m_store.state == LF_IA_LOG_BROKEN);
    // After a power-on, the recovery finds the stream's end, counts the damaged entry, and saving carries on.
    power_on();
    assert(m_store.state == LF_IA_LOG_READY && m_store.recover_bad == 1);
    present(5, FAILURE);
    lf_ia_trace_save_all();
    parsed_t p;
    parse(g_stream, read_all(), &p);
    assert(p.bad == 1 && p.slot[p.headers - 1] == 5);
}

static void test_clear_ignores_operation_in_flight(void) {
    fresh(MAX_FLASH);
    while (m_store.head - m_store.saved < SAVE_AT) {
        present(6, SUCCESS);
    }
    g_auto = false;
    lf_ia_trace_process(true);
    assert(g_op.on && !g_op.erase);
    lf_ia_trace_clear();
    assert(m_store.saved == 0 && m_store.boot == 1 && m_store.head == 16);  // the firmware entry only
    sim_complete();
    while (m_store.state == LF_IA_LOG_ERASING) {
        lf_ia_trace_process(true);
        sim_complete();
    }
    lf_ia_trace_process(true);
    assert(m_store.state == LF_IA_LOG_READY && m_store.saved == 0);
    g_auto = true;
    present(7, FAILURE);
    lf_ia_trace_save_all();
    parsed_t p;
    parse(g_stream, read_all(), &p);
    assert(p.headers == 1 && p.session[0] == 1 && p.boot[0] == 1 && p.bad == 0);
    assert(p.firmwares == 1 && p.first_type == E_FIRMWARE);
}

static void test_ram_full_drops(void) {
    fresh(MAX_FLASH);
    for (int i = 0; i < 200; i++) {
        present(8, SUCCESS);  // never quiet: nothing saved
    }
    assert(m_store.dropped > 0 && m_store.head - m_store.saved <= RAM_BYTES);
    lf_ia_trace_save_all();
    parsed_t p;
    parse(g_stream, read_all(), &p);
    assert(p.bad == 0);
}

int main(void) {
    test_crc();
    test_blank_flash_starts_ready();
    test_saves_in_order_while_quiet();
    test_survives_power_on();
    test_other_format_erased_gently();
    test_filling_keeps_first_success_and_failure();
    test_cut_write_is_skipped();
    test_clear_ignores_operation_in_flight();
    test_ram_full_drops();
    printf("test_lf_ia_trace: all passed\n");
    return 0;
}
