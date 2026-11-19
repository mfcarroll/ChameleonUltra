#pragma once

// Taking a Hitag 2 reader's answers apart, with no hardware: the start-of-frame, and agreement between answers.
//
// Hitag 2 password mode has no CRC, so a decode that slips its framing returns a plausible wrong page. Two guards:
// the start-of-frame length is learned once per use, from two clean answers that agree on it, so a later answer with
// junk after it cannot shift its page; and a page counts only when two answers agree.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// ADC counts: an answer must swing at least this far from idle. With no tag, the field rings after the reader's last
// gap, about 2800 counts from the idle level measured before it, and that ringing decodes as 1 bit. The PM3's
// simulated tag swings about 10000, rail to rail.
#define HT2_MIN_DEPTH (5000)

// One use of the reader (a scan or a read), from the field's power-up. The start-of-frame length is the tag's: the
// datasheet gives 5 ones, and some tags (and the PM3's simulation) send 6.
typedef struct {
    uint8_t sof;   // 0 until learned
    uint8_t seen;  // the length the last clean answer gave, 0 for none: one more clean answer must agree with it
} ht2_frame_t;

void ht2_frame_reset(ht2_frame_t *f);

// Takes the 32-bit page out of an answer of n bits. clean says the decode ran to the answer's end rather than stopping
// at junk after it. Until the start-of-frame length is known, only a clean answer of 37 or 38 bits counts, and the
// length is learned from the second one in a row that gives the same length. A clean answer can still carry one bit
// too many: a CU emulating the tag drops its load briefly after the answer, and that decodes as a trailing 0. The
// first answer of the two does not count. After that, an answer counts if it opens with that many ones and has 32
// bits after them; bits past those are ignored. Returns false if the answer does not count.
bool ht2_frame_page(ht2_frame_t *f, const uint8_t *bits, size_t n, bool clean, uint32_t *page);

#define HT2_AGREE_MAX (4u)

// Answers to one question (a page, or the UID), until two agree.
typedef struct {
    uint32_t seen[HT2_AGREE_MAX];
    uint8_t n;
} ht2_agree_t;

void ht2_agree_reset(ht2_agree_t *a);

// Adds an answer. Returns true, with the value in agreed, if it matches an earlier one. The first HT2_AGREE_MAX answers
// are remembered; the caller bounds the tries.
bool ht2_agree_add(ht2_agree_t *a, uint32_t value, uint32_t *agreed);
