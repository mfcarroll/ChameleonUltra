// Host tests for lf_ia_codec. Build and run with `make` in this directory.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../lf_ia_codec.h"
#include "ht2_answer_capture.h"
#include "ht2_cu_answer_capture.h"

static int m_failed = 0;
static int m_checks = 0;

#define CHECK(cond, ...)                                  \
    do {                                                  \
        m_checks++;                                       \
        if (!(cond)) {                                    \
            m_failed++;                                   \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);   \
            printf(__VA_ARGS__);                          \
            printf("\n");                                 \
        }                                                 \
    } while (0)

// A fixed generator, so a failure repeats.
static uint32_t m_rng = 0x12345678u;
static uint32_t rnd(void) {
    m_rng ^= m_rng << 13;
    m_rng ^= m_rng >> 17;
    m_rng ^= m_rng << 5;
    return m_rng;
}

static void bits_from_string(const char *s, uint8_t *bits, size_t *n) {
    *n = 0;
    for (; *s; s++) {
        if (*s == '0' || *s == '1') {
            lf_ia_bit_put(bits, (*n)++, *s == '1');
        }
    }
}

static void bits_to_string(const uint8_t *bits, size_t n, char *out) {
    for (size_t i = 0; i < n; i++) {
        out[i] = lf_ia_bit_get(bits, i) ? '1' : '0';
    }
    out[n] = 0;
}

static void test_decode_bench_capture(void) {
    // The field probe's Hitag 2 start from a PM3 `lf cmdread`, as captured on CU2 on 2026-11-06.
    const uint32_t iv[] = {230, 230, 180, 190, 180};
    const lf_ia_decode_cfg_t cfg = LF_IA_DECODE_CFG_DEFAULT;
    uint8_t bits[4] = {0};
    size_t n;
    uint32_t split;
    char s[64];
    lf_ia_decode_status_t st = lf_ia_decode_intervals(iv, 5, &cfg, bits, 32, &n, &split);
    bits_to_string(bits, n, s);
    CHECK(st == LF_IA_DECODE_OK && n == 5 && strcmp(s, "11000") == 0, "bench start: st=%d n=%zu bits=%s", st, n, s);
    CHECK(split == 205, "bench start split %u", split);
}

static void test_decode_jitter(void) {
    // PM3 `lf cmdread -d 50 -z 116 -o 166`: a 0 is a 166 µs interval and a 1 is 216 µs, read here with up to ±12 µs of
    // capture jitter.
    const lf_ia_decode_cfg_t cfg = LF_IA_DECODE_CFG_DEFAULT;
    for (int trial = 0; trial < 2000; trial++) {
        uint8_t sent[8] = {0}, got[8] = {0};
        const size_t nb = 2 + rnd() % 60;
        bool any0 = false, any1 = false;
        for (size_t i = 0; i < nb; i++) {
            const bool b = rnd() & 1u;
            any0 |= !b;
            any1 |= b;
            lf_ia_bit_put(sent, i, b);
        }
        if (!any0 || !any1) {
            continue;
        }
        uint32_t iv[64];
        CHECK(lf_ia_encode_intervals(sent, nb, 166, 216, iv, 64) == nb, "encode intervals");
        for (size_t i = 0; i < nb; i++) {
            iv[i] = (uint32_t)((int32_t)iv[i] + (int32_t)(rnd() % 25) - 12);
        }
        size_t n;
        lf_ia_decode_status_t st = lf_ia_decode_intervals(iv, nb, &cfg, got, 64, &n, NULL);
        CHECK(st == LF_IA_DECODE_OK && n == nb && memcmp(sent, got, (nb + 7) / 8) == 0, "jitter trial %d", trial);
    }
}

static void test_decode_one_class(void) {
    // 32 zeros, as the earlier password-length test sent: one interval class, so no bits.
    const lf_ia_decode_cfg_t cfg = LF_IA_DECODE_CFG_DEFAULT;
    uint32_t iv[32];
    for (int i = 0; i < 32; i++) {
        iv[i] = 166 + (uint32_t)(rnd() % 21) - 10;
    }
    uint8_t bits[4] = {0};
    size_t n = 99;
    uint32_t mean = 0;
    lf_ia_decode_status_t st = lf_ia_decode_intervals(iv, 32, &cfg, bits, 32, &n, &mean);
    CHECK(st == LF_IA_DECODE_ONE_CLASS && n == 0, "one class: st=%d n=%zu", st, n);
    CHECK(mean > 155 && mean < 177, "one class mean %u", mean);

    // With a fixed split, the same burst reads as 32 zeros, and a burst of long intervals as 32 ones.
    const lf_ia_decode_cfg_t fixed = {.min_ratio_pct = 115, .short_is_one = false, .one_class_split = 200};
    uint32_t split = 0;
    st = lf_ia_decode_intervals(iv, 32, &fixed, bits, 32, &n, &split);
    CHECK(st == LF_IA_DECODE_OK && n == 32 && split == 200, "fixed split, zeros: st=%d n=%zu split=%u", st, n, split);
    CHECK(bits[0] == 0 && bits[1] == 0 && bits[2] == 0 && bits[3] == 0, "fixed split, zeros: %02x%02x%02x%02x",
          bits[0], bits[1], bits[2], bits[3]);
    for (int i = 0; i < 32; i++) {
        iv[i] = 240 + (uint32_t)(rnd() % 21) - 10;
    }
    st = lf_ia_decode_intervals(iv, 32, &fixed, bits, 32, &n, NULL);
    CHECK(st == LF_IA_DECODE_OK && n == 32, "fixed split, ones: st=%d n=%zu", st, n);
    CHECK(bits[0] == 0xFF && bits[1] == 0xFF && bits[2] == 0xFF && bits[3] == 0xFF,
          "fixed split, ones: %02x%02x%02x%02x", bits[0], bits[1], bits[2], bits[3]);

    // A burst with two classes ignores the fixed split and keeps its own midpoint.
    const uint32_t mixed[4] = {150, 250, 150, 250};
    st = lf_ia_decode_intervals(mixed, 4, &fixed, bits, 32, &n, &split);
    CHECK(st == LF_IA_DECODE_OK && split == 200 && (bits[0] & 0xF0) == 0x50, "two classes: split %u", split);
    const uint32_t skew[4] = {170, 270, 170, 270};
    st = lf_ia_decode_intervals(skew, 4, &fixed, bits, 32, &n, &split);
    CHECK(st == LF_IA_DECODE_OK && split == 220, "two classes keep their own split: %u", split);
}

static void test_decode_edges(void) {
    const lf_ia_decode_cfg_t cfg = LF_IA_DECODE_CFG_DEFAULT;
    uint8_t bits[1] = {0};
    size_t n;
    CHECK(lf_ia_decode_intervals(NULL, 0, &cfg, bits, 8, &n, NULL) == LF_IA_DECODE_TOO_FEW, "too few");
    const uint32_t iv[9] = {100, 200, 100, 200, 100, 200, 100, 200, 100};
    CHECK(lf_ia_decode_intervals(iv, 9, &cfg, bits, 8, &n, NULL) == LF_IA_DECODE_OVERFLOW, "overflow");

    const lf_ia_decode_cfg_t inv = {.min_ratio_pct = 115, .short_is_one = true};
    uint8_t b2[2] = {0};
    char s[16];
    lf_ia_decode_intervals(iv, 9, &inv, b2, 16, &n, NULL);
    bits_to_string(b2, n, s);
    CHECK(strcmp(s, "101010101") == 0, "short_is_one: %s", s);
}

static void test_runs_known(void) {
    uint8_t bits[2] = {0};
    size_t nb;
    uint16_t runs[16];

    // Manchester "10" at RF/32: load 16, then no load for 16 + 16 (merged), then load 16.
    bits_from_string("10", bits, &nb);
    size_t n = lf_ia_encode_runs(bits, nb, LF_IA_CODING_MANCHESTER, 32, 0, runs, 16);
    CHECK(n == 3 && runs[0] == LF_IA_RUN(1, 16) && runs[1] == LF_IA_RUN(0, 32) && runs[2] == LF_IA_RUN(1, 16),
          "manchester 10: n=%zu %04x %04x %04x", n, runs[0], runs[1], runs[2]);

    // ASK "1100" at RF/32: load 64, no load 64.
    bits_from_string("1100", bits, &nb);
    n = lf_ia_encode_runs(bits, nb, LF_IA_CODING_ASK, 32, 0, runs, 16);
    CHECK(n == 2 && runs[0] == LF_IA_RUN(1, 64) && runs[1] == LF_IA_RUN(0, 64), "ask 1100: n=%zu", n);

    // Biphase "10" at RF/32, 0 flips mid-bit: load 32 (the 1), then no load 16, load 16 (the 0).
    bits_from_string("10", bits, &nb);
    n = lf_ia_encode_runs(bits, nb, LF_IA_CODING_BIPHASE, 32, 0, runs, 16);
    CHECK(n == 3 && runs[0] == LF_IA_RUN(1, 32) && runs[1] == LF_IA_RUN(0, 16) && runs[2] == LF_IA_RUN(1, 16),
          "biphase 10: n=%zu %04x %04x %04x", n, runs[0], runs[1], runs[2]);

    // Too small an output, and an odd bit length for a two-half coding.
    CHECK(lf_ia_encode_runs(bits, nb, LF_IA_CODING_MANCHESTER, 32, 0, runs, 2) == 0, "max_runs");
    CHECK(lf_ia_encode_runs(bits, nb, LF_IA_CODING_MANCHESTER, 33, 0, runs, 16) == 0, "odd bit_fc");
    CHECK(lf_ia_runs_total_fc(runs, 3) == 64, "total fc");
}

static void test_runs_roundtrip(void) {
    const lf_ia_coding_t codings[] = {LF_IA_CODING_ASK, LF_IA_CODING_MANCHESTER, LF_IA_CODING_BIPHASE};
    const uint32_t flag_sets[] = {0, LF_IA_INVERT, LF_IA_BIPHASE_ONE_FLIPS, LF_IA_INVERT | LF_IA_BIPHASE_ONE_FLIPS};
    const uint16_t rates[] = {16, 32, 64};

    for (size_t c = 0; c < 3; c++) {
        for (size_t f = 0; f < 4; f++) {
            for (size_t r = 0; r < 3; r++) {
                for (int trial = 0; trial < 300; trial++) {
                    uint8_t sent[16] = {0}, got[16] = {0};
                    const size_t nb = 1 + rnd() % 128;
                    for (size_t i = 0; i < nb; i++) {
                        lf_ia_bit_put(sent, i, rnd() & 1u);
                    }
                    // ASK cannot mark a frame's trailing zeros apart from the idle, so it always ends on a 1 here.
                    if (codings[c] == LF_IA_CODING_ASK) {
                        lf_ia_bit_put(sent, nb - 1, !(flag_sets[f] & LF_IA_INVERT));
                    }
                    uint16_t runs[300];
                    size_t n = lf_ia_encode_runs(sent, nb, codings[c], rates[r], flag_sets[f], runs, 300);
                    CHECK(n > 0, "encode c=%zu f=%zu r=%zu", c, f, r);
                    CHECK(lf_ia_runs_total_fc(runs, n) == nb * rates[r], "length c=%zu", c);

                    // Up to a quarter of a half-bit of jitter on each run still rounds to the same half-bits.
                    const int j = rates[r] / 8;
                    for (size_t i = 0; i < n; i++) {
                        const int len = (int)LF_IA_RUN_LEN(runs[i]) + (int)(rnd() % (2 * j + 1)) - j;
                        runs[i] = LF_IA_RUN(LF_IA_RUN_IS_LOAD(runs[i]), (uint16_t)len);
                    }
                    size_t m = lf_ia_decode_runs(runs, n, codings[c], rates[r], flag_sets[f], got, 128);
                    CHECK(m == nb && memcmp(sent, got, (nb + 7) / 8) == 0,
                          "roundtrip c=%zu f=%zu r=%zu nb=%zu m=%zu", c, f, r, nb, m);
                }
            }
        }
    }
}

static void test_runs_violation(void) {
    // A Manchester bit with no mid-bit transition is invalid, so a frame that starts with one decodes to nothing.
    const uint16_t runs[] = {LF_IA_RUN(1, 32), LF_IA_RUN(0, 16)};
    uint8_t got[2];
    CHECK(lf_ia_decode_runs(runs, 2, LF_IA_CODING_MANCHESTER, 32, 0, got, 16) == 0, "manchester violation");
    // A biphase bit with no flip at its start is invalid: the decode keeps the bit before it.
    const uint16_t bp[] = {LF_IA_RUN(1, 64)};
    CHECK(lf_ia_decode_runs(bp, 1, LF_IA_CODING_BIPHASE, 32, 0, got, 16) == 1, "biphase violation");

    // A valid frame followed by junk (here a whole bit of load, as when another burst follows an answer) decodes to
    // the frame.
    uint8_t bits[2] = {0}, back[4] = {0};
    size_t nb;
    bits_from_string("1011001110", bits, &nb);
    uint16_t r[40];
    size_t n = lf_ia_encode_runs(bits, nb, LF_IA_CODING_MANCHESTER, 32, 0, r, 38);
    r[n++] = LF_IA_RUN(1, 32);
    r[n++] = LF_IA_RUN(0, 16);
    char s[33];
    const size_t m = lf_ia_decode_runs(r, n, LF_IA_CODING_MANCHESTER, 32, 0, back, 32);
    bits_to_string(back, m, s);
    CHECK(m == 10 && strcmp(s, "1011001110") == 0, "frame then junk: %zu bits %s", m, s);
    bool clean = true;
    lf_ia_decode_runs_ex(r, n, LF_IA_CODING_MANCHESTER, 32, 0, back, 32, &clean);
    CHECK(!clean, "frame then junk is not a clean end");
    lf_ia_decode_runs_ex(r, n - 2, LF_IA_CODING_MANCHESTER, 32, 0, back, 32, &clean);
    CHECK(clean, "the frame alone is a clean end");
}


// A reader's envelope of a Hitag 2-style answer: idle line, a start-up transient before the window, then a 6-bit
// start-of-frame and a payload, Manchester at RF/32, one sample per carrier period, with noise and edge jitter.
static size_t synth_answer(int16_t *s, size_t n, const uint8_t *bits, size_t nb, int load_low, int noise,
                           int jitter) {
    uint16_t runs[400];
    size_t nr = lf_ia_encode_runs(bits, nb, LF_IA_CODING_MANCHESTER, 32, 0, runs, 400);
    const int idle = 2000, load = load_low ? 1700 : 2300;
    size_t i = 0;
    for (; i < 60; i++) {
        s[i] = (int16_t)(idle + 400 - (int)i * 6);  // the field settling
    }
    for (; i < 205; i++) {
        s[i] = (int16_t)idle;
    }
    for (size_t r = 0; r < nr; r++) {
        int len = (int)LF_IA_RUN_LEN(runs[r]) + (jitter ? (int)(rnd() % (2 * jitter + 1)) - jitter : 0);
        for (int k = 0; k < len && i < n; k++) {
            s[i++] = (int16_t)(LF_IA_RUN_IS_LOAD(runs[r]) ? load : idle);
        }
    }
    for (; i < n; i++) {
        s[i] = (int16_t)idle;
    }
    for (i = 0; i < n; i++) {
        s[i] = (int16_t)(s[i] + (noise ? (int)(rnd() % (2 * noise + 1)) - noise : 0));
    }
    return nr;
}

static void test_envelope(void) {
    static int16_t s[2048];
    for (int trial = 0; trial < 400; trial++) {
        uint8_t sent[8] = {0}, got[16] = {0};
        for (size_t i = 0; i < 6; i++) {
            lf_ia_bit_put(sent, i, 1);
        }
        for (size_t i = 6; i < 38; i++) {
            lf_ia_bit_put(sent, i, rnd() & 1u);
        }
        const int load_low = trial & 1;
        synth_answer(s, 2048, sent, 38, load_low, 40, 2);
        uint16_t runs[200];
        lf_ia_envelope_info_t info;
        size_t nr = lf_ia_envelope_runs(s, 2048, 80, 150, 100, 50, 16, 48, runs, 200, &info);
        size_t nb = lf_ia_decode_runs(runs, nr, LF_IA_CODING_MANCHESTER, 32, 0, got, 128);
        CHECK(nb == 38 && memcmp(sent, got, 5) == 0 && info.first >= 203 && info.first <= 207,
              "envelope trial %d low=%d: nr=%zu nb=%zu first=%zu", trial, load_low, nr, nb, info.first);
    }
    // No answer: noise only.
    synth_answer(s, 2048, (const uint8_t *)"", 0, 1, 40, 0);
    uint16_t runs[200];
    lf_ia_envelope_info_t info;
    CHECK(lf_ia_envelope_runs(s, 2048, 80, 150, 100, 50, 16, 48, runs, 200, &info) == 0, "no answer");
    CHECK(lf_ia_envelope_runs(s, 100, 80, 150, 100, 50, 16, 48, runs, 200, &info) == 0, "window past the end");
}

static void test_envelope_real_answer(void) {
    // Sliced near the low rail, the bench capture decodes to its 38 bits. Sliced midway, the slow fall from each load
    // stretches the runs and it does not.
    uint16_t runs[400];
    lf_ia_envelope_info_t info;
    const size_t n = sizeof(HT2_ANSWER_CAPTURE) / sizeof(HT2_ANSWER_CAPTURE[0]);
    size_t nr = lf_ia_envelope_runs(HT2_ANSWER_CAPTURE, n, 40, 125, 100, 25, 16, 48, runs, 400, &info);
    uint8_t got[8] = {0};
    size_t nb = lf_ia_decode_runs(runs, nr, LF_IA_CODING_MANCHESTER, 32, 0, got, 64);
    char s[65];
    bits_to_string(got, nb, s);
    CHECK(nb == 38 && strcmp(s, "11111100000001000000100000001100000100") == 0, "real answer: %zu bits %s", nb, s);

    nr = lf_ia_envelope_runs(HT2_ANSWER_CAPTURE, n, 40, 125, 100, 50, 16, 48, runs, 400, &info);
    nb = lf_ia_decode_runs(runs, nr, LF_IA_CODING_MANCHESTER, 32, 0, got, 64);
    bits_to_string(got, nb, s);
    CHECK(strcmp(s, "11111100000001000000100000001100000100") != 0, "midway slicing should not decode it: %s", s);
}

static void test_envelope_loaded_tail(void) {
    // The answer ends on a load half and the emulating CU stays loaded after it. That last half-bit is kept.
    uint16_t runs[400];
    lf_ia_envelope_info_t info;
    const size_t n = sizeof(HT2_CU_ANSWER_CAPTURE) / sizeof(HT2_CU_ANSWER_CAPTURE[0]);
    const size_t nr = lf_ia_envelope_runs(HT2_CU_ANSWER_CAPTURE, n, 40, 125, 100, 25, 16, 48, runs, 400, &info);
    uint8_t got[8] = {0};
    const size_t nb = lf_ia_decode_runs(runs, nr, LF_IA_CODING_MANCHESTER, 32, 0, got, 64);
    char s[65];
    bits_to_string(got, nb, s);
    CHECK(nb == 37 && strcmp(s, "1111100000001000000100000001100000100") == 0, "CU answer: %zu bits %s", nb, s);
    CHECK(nr > 0 && LF_IA_RUN_IS_LOAD(runs[nr - 1]) && LF_IA_RUN_LEN(runs[nr - 1]) == 16, "last run %04x",
          nr ? runs[nr - 1] : 0);

    // A load that starts and never ends is no answer: there is nothing before it to end.
    static int16_t flat[600];
    for (size_t i = 0; i < 600; i++) {
        flat[i] = (int16_t)(i < 200 ? 1000 : 5000);
    }
    CHECK(lf_ia_envelope_runs(flat, 600, 40, 125, 100, 25, 16, 48, runs, 400, &info) == 0, "a lone load is no answer");
}

int main(void) {
    test_decode_bench_capture();
    test_envelope_loaded_tail();
    test_decode_jitter();
    test_decode_one_class();
    test_decode_edges();
    test_runs_known();
    test_runs_roundtrip();
    test_runs_violation();
    test_envelope_real_answer();
    test_envelope();
    printf("%d checks, %d failed\n", m_checks, m_failed);
    return m_failed ? 1 : 0;
}
