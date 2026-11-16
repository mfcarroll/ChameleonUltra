// Host test for the reader's answer frame and trace records (reader/lf/lf_rdr_frame.c, lf_rdr_trace.c). Host parsers
// read these bytes, so a change to an offset or a width breaks them.

#include <stdio.h>
#include <string.h>

#include "../../../../reader/lf/lf_rdr_frame.h"
#include "../../../../reader/lf/lf_rdr_trace.h"

static int m_failed = 0;
static int m_checks = 0;

#define CHECK(cond, ...)                                \
    do {                                                \
        m_checks++;                                     \
        if (!(cond)) {                                  \
            m_failed++;                                 \
            printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                        \
            printf("\n");                               \
        }                                               \
    } while (0)

static unsigned u16_at(const uint8_t *p) {
    return (unsigned)p[0] | (unsigned)p[1] << 8;
}

static void test_layout(void) {
    lf_rdr_reply_t rx;
    memset(&rx, 0, sizeof(rx));
    rx.n_bits = 0x0123;
    for (size_t i = 0; i < sizeof(rx.bits); i++) {
        rx.bits[i] = (uint8_t)(0xA0 + i);
    }
    rx.env.first = 0x0456;
    rx.env.idle = -2;
    rx.env.load = 0x1234;
    rx.env.threshold = -300;
    const uint16_t runs[3] = {0x0102, 0x0304, 0xFFFE};
    static uint8_t out[LF_RDR_FRAME_MAX];
    memset(out, 0xEE, sizeof(out));

    const size_t len = lf_rdr_frame_pack(&rx, runs, 3, out);
    CHECK(len == 44 + 6, "length %zu", len);
    CHECK(out[0] == 0x23 && out[1] == 0x01, "n_bits bytes %02x %02x", out[0], out[1]);
    CHECK(memcmp(out + 2, rx.bits, 32) == 0, "bits");
    CHECK(u16_at(out + 34) == 0x0456, "first %04x", u16_at(out + 34));
    CHECK(u16_at(out + 36) == 0xFFFE, "idle %04x", u16_at(out + 36));
    CHECK(u16_at(out + 38) == 0x1234, "load %04x", u16_at(out + 38));
    CHECK(u16_at(out + 40) == (uint16_t)-300, "threshold %04x", u16_at(out + 40));
    CHECK(u16_at(out + 42) == 3, "n_runs %u", u16_at(out + 42));
    CHECK(out[44] == 0x02 && out[45] == 0x01, "run 0 bytes %02x %02x", out[44], out[45]);
    CHECK(u16_at(out + 46) == 0x0304 && u16_at(out + 48) == 0xFFFE, "runs 1, 2");
    CHECK(out[50] == 0xEE, "wrote past the frame");
}

// The frame holds 400 runs at most, and still reports the full count.
static void test_run_cap(void) {
    lf_rdr_reply_t rx;
    memset(&rx, 0, sizeof(rx));
    static uint16_t runs[LF_RDR_MAX_RUNS];
    for (size_t i = 0; i < LF_RDR_MAX_RUNS; i++) {
        runs[i] = (uint16_t)(i + 1);
    }
    static uint8_t out[LF_RDR_FRAME_MAX + 2];
    memset(out, 0xEE, sizeof(out));

    const size_t len = lf_rdr_frame_pack(&rx, runs, LF_RDR_MAX_RUNS, out);
    CHECK(len == 44 + 400 * 2, "length %zu", len);
    CHECK(u16_at(out + 42) == LF_RDR_MAX_RUNS, "n_runs %u", u16_at(out + 42));
    CHECK(u16_at(out + 44 + 399 * 2) == 400, "last run %u", u16_at(out + 44 + 399 * 2));
    CHECK(out[LF_RDR_FRAME_MAX] == 0xEE, "wrote past the frame");
}

// No answer: the frame is the header alone, with zero counts.
static void test_empty(void) {
    lf_rdr_reply_t rx;
    memset(&rx, 0, sizeof(rx));
    static uint8_t out[LF_RDR_FRAME_MAX];
    const size_t len = lf_rdr_frame_pack(&rx, NULL, 0, out);
    CHECK(len == 44, "length %zu", len);
    CHECK(u16_at(out) == 0 && u16_at(out + 42) == 0, "counts");
}

static void test_trace_layout(void) {
    lf_rdr_reply_t rx;
    memset(&rx, 0, sizeof(rx));
    rx.n_bits = 38;
    rx.clean = true;
    rx.bits[0] = 0xFF;  // answer bits stay out of the record
    rx.env.first = 0x00E6;
    rx.env.idle = 4138;
    rx.env.load = -5;
    rx.env.threshold = 0x7FFF;
    static uint16_t runs[LF_RDR_MAX_RUNS];
    for (size_t i = 0; i < LF_RDR_MAX_RUNS; i++) {
        runs[i] = (uint16_t)(0x8000u | (i + 1));
    }
    const uint8_t tx[2] = {0xC1, 0xFF};  // READ PAGE 6: 11 110 00 001, then bits past the command
    static uint8_t out[LF_RDR_TRACE_REC_SIZE + 2];
    memset(out, 0xEE, sizeof(out));

    lf_rdr_trace_pack(tx, 10, 70000, &rx, runs, 150, out);
    CHECK(LF_RDR_TRACE_REC_SIZE == 212, "record size %u", (unsigned)LF_RDR_TRACE_REC_SIZE);
    CHECK(out[0] == 10, "n_tx %u", out[0]);
    CHECK(out[1] == 0xC1 && out[2] == 0xC0 && out[3] == 0 && out[4] == 0, "tx %02x %02x %02x %02x", out[1], out[2],
          out[3], out[4]);
    CHECK(u16_at(out + 5) == 0xFFFF, "power-up clamps %04x", u16_at(out + 5));
    CHECK(out[7] == 1, "flags %u", out[7]);
    CHECK(u16_at(out + 8) == 38 && out[8] == 38 && out[9] == 0, "n_bits");
    CHECK(u16_at(out + 10) == 0x00E6, "first %04x", u16_at(out + 10));
    CHECK(u16_at(out + 12) == 4138, "idle %u", u16_at(out + 12));
    CHECK(u16_at(out + 14) == 0xFFFB, "load %04x", u16_at(out + 14));
    CHECK(u16_at(out + 16) == 0x7FFF, "threshold %04x", u16_at(out + 16));
    CHECK(u16_at(out + 18) == 150, "n_runs %u", u16_at(out + 18));
    CHECK(out[20] == 0x01 && out[21] == 0x80, "run 0 bytes %02x %02x", out[20], out[21]);
    CHECK(u16_at(out + 20 + 95 * 2) == 0x8060, "run 95 %04x", u16_at(out + 20 + 95 * 2));
    CHECK(out[LF_RDR_TRACE_REC_SIZE] == 0xEE, "wrote past the record");

    // A long command keeps its first 32 bits. A short answer leaves the run slots past it zero.
    const uint8_t cmd[5] = {0x12, 0x34, 0x56, 0x78, 0xAA};
    rx.clean = false;
    lf_rdr_trace_pack(cmd, 40, 0, &rx, runs, 2, out);
    CHECK(out[0] == 40 && memcmp(out + 1, cmd, 4) == 0, "long command");
    CHECK(out[7] == 0 && u16_at(out + 5) == 0, "flags, power-up");
    CHECK(u16_at(out + 18) == 2 && u16_at(out + 24) == 0 && u16_at(out + 20 + 95 * 2) == 0, "short answer");
}

// The record's power-up field, as an id to tell records apart.
static unsigned rec_id(const uint8_t *rec) {
    return u16_at(rec + 5);
}

static void test_trace_ring(void) {
    lf_rdr_reply_t rx;
    memset(&rx, 0, sizeof(rx));
    const uint8_t tx[1] = {0xC0};
    static uint8_t out[LF_RDR_TRACE_RECORDS * LF_RDR_TRACE_REC_SIZE];
    uint16_t total;

    lf_rdr_trace_clear();
    CHECK(lf_rdr_trace_read(0, out, 14, &total) == 0 && total == 0, "empty");
    for (unsigned i = 0; i < 3; i++) {
        lf_rdr_trace_add(tx, 5, i, &rx, NULL, 0);
    }
    size_t n = lf_rdr_trace_read(1, out, 14, &total);
    CHECK(n == 2 && total == 3 && rec_id(out) == 1 && rec_id(out + LF_RDR_TRACE_REC_SIZE) == 2, "from 1: %zu", n);

    // Past the ring's size, the oldest held is the 7th added, and the reads page through all 64 held.
    lf_rdr_trace_clear();
    for (unsigned i = 0; i < LF_RDR_TRACE_RECORDS + 6; i++) {
        lf_rdr_trace_add(tx, 5, i, &rx, NULL, 0);
    }
    n = lf_rdr_trace_read(0, out, 14, &total);
    CHECK(n == 14 && total == LF_RDR_TRACE_RECORDS + 6 && rec_id(out) == 6, "wrapped: %zu, total %u, first %u", n,
          total, rec_id(out));
    n = lf_rdr_trace_read(56, out, 14, &total);
    CHECK(n == 8 && rec_id(out + 7 * LF_RDR_TRACE_REC_SIZE) == LF_RDR_TRACE_RECORDS + 5, "last page: %zu", n);
    CHECK(lf_rdr_trace_read(LF_RDR_TRACE_RECORDS, out, 14, &total) == 0, "past the end");
}

int main(void) {
    test_layout();
    test_run_cap();
    test_empty();
    test_trace_layout();
    test_trace_ring();
    printf("%d checks, %d failed\n", m_checks, m_failed);
    return m_failed ? 1 : 0;
}
