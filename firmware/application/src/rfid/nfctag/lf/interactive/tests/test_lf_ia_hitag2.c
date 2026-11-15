// Host test for the Hitag 2 module: reader commands go in as gap intervals, through the engine's standard decode.

#include <stdio.h>
#include <string.h>

#include "../lf_ia_hitag2.h"

static int m_failed = 0;
static int m_checks = 0;
static uint8_t m_mode = 0;

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

static const uint8_t PAGES[LF_HITAG2_DATA_SIZE] = {
    0x01, 0x02, 0x03, 0x04, 0x0A, 0x0B, 0x0C, 0x0D, 0x02, 0x02, 0x02, 0x02, 0x06, 0x0E, 0x0F, 0x10,
    0x04, 0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x05, 0x06, 0x06, 0x06, 0x06, 0x07, 0x07, 0x07, 0x07,
};

// Sends a command of n bits (MSB first in v) the way the engine would: the reader's 160/240 µs intervals, decoded.
static lf_ia_verdict_t send(const lf_ia_module_t *m, uint64_t v, size_t n, lf_ia_response_t *rsp) {
    uint8_t sent[8] = {0}, bits[16] = {0};
    for (size_t i = 0; i < n; i++) {
        lf_ia_bit_put(sent, i, (v >> (n - 1 - i)) & 1u);
    }
    uint32_t iv[64];
    lf_ia_encode_intervals(sent, n, 160, 240, iv, 64);
    lf_ia_command_t cmd = {.n_gaps = n + 1, .bits = bits, .mode = m_mode};
    cmd.decode_status = lf_ia_decode_intervals(iv, n, &m->decode, bits, 128, &cmd.n_bits, NULL);
    memset(rsp, 0, sizeof(*rsp));
    return m->on_command(m->ctx, &cmd, rsp);
}

// The page a response carries, after checking its framing.
static uint32_t payload(const lf_ia_response_t *rsp) {
    if (rsp->n_bits != 37 || rsp->coding != LF_IA_CODING_MANCHESTER || rsp->bit_fc != 32) {
        return 0xDEADDEADu;
    }
    for (size_t i = 0; i < 5; i++) {
        if (!lf_ia_bit_get(rsp->bits, i)) {
            return 0xDEADDEADu;
        }
    }
    uint32_t p = 0;
    for (size_t i = 5; i < 37; i++) {
        p = p << 1 | lf_ia_bit_get(rsp->bits, i);
    }
    return p;
}

static uint32_t read_cmd(uint32_t n, int inverted) {
    const uint32_t a = inverted ? 1u : 3u, b = inverted ? 2u : 0u;
    return a << 8 | n << 5 | b << 3 | (~n & 7u);
}

int main(void) {
    const lf_ia_module_t *m = lf_ia_hitag2_load(PAGES);
    lf_ia_response_t rsp;
    m->field_on(m->ctx);

    CHECK(send(m, 0x0A0B0C0D, 32, &rsp) == LF_IA_IGNORE, "password before START_AUTH");
    CHECK(send(m, read_cmd(4, 0), 10, &rsp) == LF_IA_IGNORE, "read before authentication");
    CHECK(send(m, 0x18, 5, &rsp) == LF_IA_RESPOND && payload(&rsp) == 0x01020304, "START_AUTH gives the UID");
    CHECK(rsp.delay_fc == 240 && rsp.delay_us == 0, "delay %u fc", rsp.delay_fc);
    CHECK(send(m, 0x1F, 5, &rsp) == LF_IA_IGNORE, "other 5-bit frame");
    m_mode = LF_IA_MODE_ALT_DELAY;
    CHECK(send(m, 0x18, 5, &rsp) == LF_IA_RESPOND && rsp.delay_fc == 292, "alt delay %u fc", rsp.delay_fc);
    m_mode = 0;
    CHECK(send(m, 0x2AB, 11, &rsp) == LF_IA_IGNORE, "an 11-bit frame is noise");
    CHECK(send(m, 0x0A0B0C0D, 32, &rsp) == LF_IA_RESPOND && payload(&rsp) == 0x060E0F10, "password gives page 3");
    for (uint32_t n = 0; n < 8; n++) {
        const uint32_t want = (uint32_t)PAGES[4 * n] << 24 | PAGES[4 * n + 1] << 16 | PAGES[4 * n + 2] << 8 |
                              PAGES[4 * n + 3];
        CHECK(send(m, read_cmd(n, 0), 10, &rsp) == LF_IA_RESPOND && payload(&rsp) == want, "read page %u", n);
        CHECK(send(m, read_cmd(n, 1), 10, &rsp) == LF_IA_RESPOND && payload(&rsp) == ~want, "inverted page %u", n);
    }
    CHECK(send(m, (read_cmd(4, 0) & ~1u), 10, &rsp) == LF_IA_IGNORE, "read with a bad complement");

    m->field_off(m->ctx);
    m->field_on(m->ctx);
    CHECK(send(m, read_cmd(4, 0), 10, &rsp) == LF_IA_IGNORE, "field drop ends the session");
    CHECK(send(m, 0x18, 5, &rsp) == LF_IA_RESPOND, "START_AUTH again");
    CHECK(send(m, 0x0A0B0C0E, 32, &rsp) == LF_IA_IGNORE, "wrong password");
    CHECK(send(m, 0x0A0B0C0D, 32, &rsp) == LF_IA_IGNORE, "the right password after a wrong one needs START_AUTH");
    CHECK(send(m, 0x18, 5, &rsp) == LF_IA_RESPOND, "START_AUTH after the wrong password");
    CHECK(send(m, 0x0A0B0C0D, 32, &rsp) == LF_IA_RESPOND, "then the right password");
    CHECK(send(m, 0x18, 5, &rsp) == LF_IA_RESPOND, "without the mode, START_AUTH after authentication answers");

    // LF_IA_MODE_ALT_PROTOCOL: every second START_AUTH, counted from a fresh field.
    m_mode = LF_IA_MODE_ALT_PROTOCOL;
    m->field_off(m->ctx);
    m->field_on(m->ctx);
    CHECK(send(m, 0x18, 5, &rsp) == LF_IA_RESPOND, "alt: the first START_AUTH answers");
    CHECK(send(m, 0x18, 5, &rsp) == LF_IA_IGNORE, "alt: the second resets, unanswered");
    CHECK(send(m, 0x0A0B0C0D, 32, &rsp) == LF_IA_IGNORE, "alt: the reset tag ignores the password");
    CHECK(send(m, 0x18, 5, &rsp) == LF_IA_RESPOND, "alt: the third answers");
    CHECK(send(m, 0x0A0B0C0D, 32, &rsp) == LF_IA_RESPOND, "alt: then the password");
    CHECK(send(m, 0x18, 5, &rsp) == LF_IA_IGNORE, "alt: START_AUTH after authentication resets");
    CHECK(send(m, read_cmd(4, 0), 10, &rsp) == LF_IA_IGNORE, "alt: and the reads stop");
    CHECK(send(m, 0x18, 5, &rsp) == LF_IA_RESPOND, "alt: the next START_AUTH answers");
    m->field_off(m->ctx);
    m->field_on(m->ctx);
    CHECK(send(m, 0x18, 5, &rsp) == LF_IA_RESPOND, "alt: a field drop starts the count again");
    m_mode = 0;

    printf("%d checks, %d failed\n", m_checks, m_failed);
    return m_failed ? 1 : 0;
}
