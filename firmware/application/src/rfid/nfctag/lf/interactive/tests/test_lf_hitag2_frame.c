// Host test for the Hitag 2 reader's framing and agreement (reader/lf/lf_hitag2_frame.c).

#include <stdio.h>
#include <string.h>

#include "../../../../reader/lf/lf_hitag2_frame.h"
#include "../lf_ia_codec.h"
#include "ht2_answer_capture.h"
#include "ht2_no_tag_capture.h"

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

// An answer: sof ones, the page, then junk bits (as given, first first).
static size_t answer(uint8_t *bits, size_t sof, uint32_t page, const char *junk) {
    memset(bits, 0, 8);
    size_t n = 0;
    for (size_t i = 0; i < sof; i++) {
        lf_ia_bit_put(bits, n++, true);
    }
    for (int i = 31; i >= 0; i--) {
        lf_ia_bit_put(bits, n++, (page >> i) & 1u);
    }
    for (; *junk; junk++) {
        lf_ia_bit_put(bits, n++, *junk == '1');
    }
    return n;
}

// The captures through lf_rdr_ia's envelope step with the Hitag 2 link: 40 periods skipped, a 1000 µs window, sliced
// 25 % from the low rail, 32 periods a bit. Returns the bits decoded.
static size_t envelope_bits(const int16_t *s, size_t n, int32_t min_depth, size_t *n_runs) {
    uint16_t runs[400];
    uint8_t bits[32];
    lf_ia_envelope_info_t info;
    bool clean;
    *n_runs = lf_ia_envelope_runs(s, n, 40, 125, min_depth, 25, 16, 48, runs, 400, &info);
    return lf_ia_decode_runs_ex(runs, *n_runs, LF_IA_CODING_MANCHESTER, 32, 0, bits, 256, &clean);
}

static void test_min_depth(void) {
    const size_t n_ans = sizeof(HT2_ANSWER_CAPTURE) / sizeof(HT2_ANSWER_CAPTURE[0]);
    const size_t n_none = sizeof(HT2_NO_TAG_CAPTURE) / sizeof(HT2_NO_TAG_CAPTURE[0]);
    size_t nr;
    // At min_depth 40 the field's ringing passes for a load. Its second swing never reaches mid-level, so the slicer
    // ends the answer there, short of a bit. HT2_MIN_DEPTH rejects it outright.
    size_t nb = envelope_bits(HT2_NO_TAG_CAPTURE, n_none, 40, &nr);
    CHECK(nr > 0 && nb == 0, "no tag at min_depth 40: %zu bits, %zu runs", nb, nr);
    nb = envelope_bits(HT2_NO_TAG_CAPTURE, n_none, HT2_MIN_DEPTH, &nr);
    CHECK(nb == 0 && nr == 0, "no tag at HT2_MIN_DEPTH: %zu bits, %zu runs", nb, nr);
    nb = envelope_bits(HT2_ANSWER_CAPTURE, n_ans, HT2_MIN_DEPTH, &nr);
    CHECK(nb == 38, "answer at HT2_MIN_DEPTH: %zu bits", nb);
}

int main(void) {
    test_min_depth();
    ht2_frame_t f;
    uint8_t b[8];
    uint32_t page = 0;
    size_t n;

    // A clean answer with 6 start-of-frame ones sets the length, also for a page that starts with ones.
    ht2_frame_reset(&f);
    n = answer(b, 6, 0xF0123456u, "");
    CHECK(ht2_frame_page(&f, b, n, true, &page) && page == 0xF0123456u && f.sof == 6, "learn 6: %08x", page);
    // Then an answer with junk after it keeps its framing: the junk is ignored.
    n = answer(b, 6, 0xE1000001u, "11");
    CHECK(ht2_frame_page(&f, b, n, false, &page) && page == 0xE1000001u, "junk after, learned: %08x", page);
    // An answer short of the learned length does not count.
    n = answer(b, 5, 0x01020304u, "");
    CHECK(!ht2_frame_page(&f, b, n, true, &page), "short of the learned length");

    // Before the length is known, an answer with junk after it does not count. This was the slip: 6 ones, a page
    // starting with 1, and one junk bit made 39 bits, read as 7 ones and a shifted page.
    ht2_frame_reset(&f);
    n = answer(b, 6, 0x80000001u, "1");
    CHECK(!ht2_frame_page(&f, b, n, false, &page) && f.sof == 0, "junk before learning");
    // Nor does a clean answer of a length no tag sends (here one that lost its first start-of-frame bit).
    n = answer(b, 4, 0x01020304u, "");
    CHECK(!ht2_frame_page(&f, b, n, true, &page) && f.sof == 0, "36 bits does not set the length");
    // The datasheet's 5 ones.
    n = answer(b, 5, 0x0A0B0C0Du, "");
    CHECK(ht2_frame_page(&f, b, n, true, &page) && page == 0x0A0B0C0Du && f.sof == 5, "learn 5: %08x", page);
    // A zero where a start-of-frame one belongs does not count.
    n = answer(b, 5, 0x0A0B0C0Du, "");
    lf_ia_bit_put(b, 2, false);
    CHECK(!ht2_frame_page(&f, b, n, true, &page), "broken start-of-frame");

    // Agreement: a page counts once two answers match.
    ht2_agree_t a;
    uint32_t v = 0;
    ht2_agree_reset(&a);
    CHECK(!ht2_agree_add(&a, 0x11111111u, &v), "first answer alone");
    CHECK(!ht2_agree_add(&a, 0x22222222u, &v), "a different second answer");
    CHECK(ht2_agree_add(&a, 0x11111111u, &v) && v == 0x11111111u, "a third matching the first");
    ht2_agree_reset(&a);
    for (uint32_t i = 0; i < 6; i++) {
        CHECK(!ht2_agree_add(&a, i, &v), "all different %u", i);
    }

    printf("%d checks, %d failed\n", m_checks, m_failed);
    return m_failed ? 1 : 0;
}
