// Host test for the reader's answer frame (reader/lf/lf_rdr_frame.c). lfrdr.py parses these bytes, so a change to an
// offset or a width breaks it.

#include <stdio.h>
#include <string.h>

#include "../../../../reader/lf/lf_rdr_frame.h"

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

int main(void) {
    test_layout();
    test_run_cap();
    test_empty();
    printf("%d checks, %d failed\n", m_checks, m_failed);
    return m_failed ? 1 : 0;
}
