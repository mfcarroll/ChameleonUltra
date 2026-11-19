// The engine's bench test module. It answers every command with one fixed frame, so a reader can check capture,
// turnaround and modulation with no protocol involved. Not part of the framework.

#include "lf_ia_test_module.h"

#include <string.h>

#define TEST_EOF_US     (400u)   // 50 carrier periods: longer than any bit of `lf cmdread -z 116 -o 166 -d 50`
#define TEST_DELAY_US   (1632u)  // 204 carrier periods, a Hitag 2 tag's wait before it answers
#define TEST_BIT_FC     (32u)    // RF/32

static uint8_t m_frame[LF_IA_TEST_FRAME_SIZE];

static lf_ia_verdict_t on_command(void *ctx, const lf_ia_command_t *cmd, lf_ia_response_t *rsp) {
    (void)ctx;
    (void)cmd;
    memcpy(rsp->bits, m_frame, sizeof(m_frame));
    rsp->n_bits = 8u * sizeof(m_frame);
    rsp->coding = LF_IA_CODING_MANCHESTER;
    rsp->coding_flags = 0;
    rsp->bit_fc = TEST_BIT_FC;
    rsp->delay_us = TEST_DELAY_US;
    return LF_IA_RESPOND;
}

static const lf_ia_module_t m_test_module = {
    .name = "test",
    .eof_us = TEST_EOF_US,
    .want_decode = true,
    .decode = LF_IA_DECODE_CFG_DEFAULT,
    .field_on = NULL,
    .on_command = on_command,
    .field_off = NULL,
    .ctx = NULL,
};

const lf_ia_module_t *lf_ia_test_module_load(const uint8_t *frame) {
    memcpy(m_frame, frame, sizeof(m_frame));
    return &m_test_module;
}
