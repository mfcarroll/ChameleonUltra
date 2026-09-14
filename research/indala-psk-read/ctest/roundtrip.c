/*
 * ⭐⭐ EMITTER -> AIR -> DECODER, both halves the SHIPPING firmware, no hardware.
 *
 * ⛔ WHY THIS EXISTS. psk1.h has said "verify a change here by DECODING it, never by
 * reading it" since the file was written, and until now the only decoder that could do so
 * was a Flipper on a bench. So the emulation encoder shipped three times in one session
 * with no test between the change and the radio, and the second of those three put a
 * stable, confident, WRONG 224-bit credential on the air that Momentum read 6 times out of
 * 6 (C152). A round trip on the host would have caught it in a second.
 *
 * The air model is deliberately the SIMPLEST thing that is still honest:
 *
 *   - one PWM entry is one 16us subcarrier cycle; the reader samples every 8us, so an
 *     entry is exactly TWO samples and the subcarrier sits at fs/2 — which is the whole
 *     premise of the demodulator under test (lf_indala_psk.h).
 *   - the entry's top bit of channel_0 is the subcarrier PHASE, so it selects which of the
 *     two samples is the high one.
 *   - `repeats` holds each entry for repeats+1 PWM periods. ⛔ Honouring it is not a
 *     detail: the encoder stores one entry per BIT and relies on the peripheral to stretch
 *     it, so a model that ignored `repeats` would test a frame 16x too short.
 *   - ⭐ THE BUFFER IS REPLAYED, not extended. That is what the PWM does, and it is
 *     precisely the property C152 turned on: an odd-parity PSK2 frame whose buffer holds
 *     one copy has a phase discontinuity at every wrap that no real tag has.
 *
 * No noise, no drift, no coupling. Those belong to the capture tests over real files; this
 * one answers "does the thing we transmit mean what we think it means", which is a
 * question about arithmetic and has an exact answer.
 */
#include <stdio.h>
#include <string.h>

#include "lf_indala_psk.h"
#include "psk1.h"
#include "lf_ask_manchester.h"
#include "lf_fsk2a.h"
#include "gallagher.h"
#include "securakey.h"
#include "noralsy.h"
#include "awid.h"
#include "gproxii.h"
#include "keri.h"
#include "securakey.h"
#include "nexwatch.h"
#include "lf_ask_biphase.h"
#include "fsk2a_t55xx.h"
#include "t55xx.h"

#define SAMPLES_PER_ENTRY 2      /* 16us subcarrier cycle / 8us sample period */
#define DC                8192   /* mid-scale of the 14-bit SAADC */
#define AMPL              2000   /* comfortably above the straddle gate, far from clipping */

/* Replay the sequence into a sample buffer the way the peripheral would. */
static size_t render(const nrf_pwm_sequence_t *seq, int16_t *out, size_t out_len) {
    const size_t entries = (size_t)seq->length / 4u;
    const size_t holds = (size_t)seq->repeats + 1u;
    size_t n = 0;
    for (size_t i = 0; n < out_len; i++) {
        const nrf_pwm_values_wave_form_t *e = &seq->values.p_wave_form[i % entries];
        int phase = (e->channel_0 & (1u << 15)) ? 1 : 0;
        for (size_t h = 0; h < holds && n < out_len; h++) {
            for (size_t s = 0; s < SAMPLES_PER_ENTRY && n < out_len; s++) {
                int high = ((int)s ^ phase) & 1;
                out[n++] = (int16_t)(DC + (high ? AMPL : -AMPL));
            }
        }
    }
    return n;
}

/* ⭐ THE ASK RENDERER, AND IT IS NOT THE PSK ONE. A PSK entry is one bit held for `repeats`+1
 * periods of an fc/2 subcarrier whose POLARITY is the data; an ASK entry is one bit of
 * `counter_top` CARRIER CYCLES whose first and second halves carry the Manchester transition.
 * Rendering one with the other's model produces a plausible waveform that decodes to nothing,
 * which is the least useful kind of test failure.
 *
 * ⚠ The half-order must match the decoder's: `lf_ask_manchester_decode_fmt` samples at
 * SPB/4 and 3*SPB/4 and calls (high, low) a 1 — so a set polarity bit is FIRST HALF HIGH. */
static size_t render_ask(const nrf_pwm_sequence_t *seq, int16_t *out, size_t out_len) {
    const size_t entries = (size_t)seq->length / 4u;
    size_t n = 0;
    for (size_t i = 0; n < out_len; i++) {
        const nrf_pwm_values_wave_form_t *e = &seq->values.p_wave_form[i % entries];
        const int polarity = (e->channel_0 & (1u << 15)) ? 1 : 0;
        const size_t top = e->counter_top;
        for (size_t s = 0; s < top && n < out_len; s++) {
            const int first_half = (s < top / 2u);
            const int high = polarity ? first_half : !first_half;
            out[n++] = (int16_t)(DC + (high ? AMPL : -AMPL));
        }
    }
    return n;
}

static int hexeq(const uint8_t *got, const char *want, size_t bytes);

/* ⭐⭐ THE BIPHASE RENDERER, and it is the first here that HONOURS THE DUTY. render_ask()
 * hard-codes half the period high because every ASK and FSK entry is a 50% square; a biphase
 * entry holds ONE level for its whole period, expressed as duty 0 or duty == counter_top.
 * Rendering that with render_ask() would turn every held level into a square wave and the
 * round trip would be testing a different emitter than the one that ships. */
static size_t render_level(const nrf_pwm_sequence_t *seq, int16_t *out, size_t out_len) {
    const size_t entries = (size_t)seq->length / 4u;
    size_t n = 0;
    for (size_t i = 0; n < out_len; i++) {
        const nrf_pwm_values_wave_form_t *e = &seq->values.p_wave_form[i % entries];
        const size_t top = e->counter_top;
        const size_t duty = e->channel_0 & 0x7FFFu;
        const int polarity = (e->channel_0 & (1u << 15)) ? 1 : 0;
        for (size_t s = 0; s < top && n < out_len; s++) {
            const int high = (s < duty) ^ polarity;
            out[n++] = (int16_t)(DC + (high ? AMPL : -AMPL));
        }
    }
    return n;
}

/* ⚠ The biphase decoder wants a STEP at each grid point, and a perfectly square rendering
 * gives it one — but its threshold is a fraction of the MEAN boundary step, so a capture in
 * which every boundary steps by the same amount is the easiest case it will ever see. This arm
 * therefore proves the ENCODING, not the decoder's robustness; the capture tests do that. */
static int trial_biphase(const char *name, const char *hex, size_t bits,
                         const protocol *proto, const lf_biphase_format_t *fmt) {
    uint8_t frame[32] = {0};
    for (size_t i = 0; i < bits / 8u; i++) {
        unsigned v; sscanf(hex + 2 * i, "%2x", &v); frame[i] = (uint8_t)v;
    }
    void *codec = proto->alloc();
    const nrf_pwm_sequence_t *seq = proto->modulator(codec, frame);
    if (seq == NULL) {
        printf("  %-28s \u26d4 modulator returned NULL\n", name);
        proto->free(codec);
        return 1;
    }
    const size_t entries = (size_t)seq->length / 4u;
    static int16_t air[LF_SAMPLED_MAX_CAPTURE_SAMPLES];
    size_t n = render_level(seq, air, sizeof(air) / sizeof(air[0]));
    proto->free(codec);

    lf_decode_result_t r;
    int ok = lf_ask_biphase_decode_fmt(air, n, fmt, &r);
    int exact = ok && hexeq(r.id, hex, bits / 8u);
    printf("  %-28s %s  entries %4zu             %s\n", name,
           exact ? "\u2713" : "\u26d4", entries,
           ok ? (exact ? "decode exact" : "DECODED WRONG") : "NO DECODE");
    return exact ? 0 : 1;
}

static int hexeq(const uint8_t *got, const char *want, size_t bytes) {
    char buf[64];
    for (size_t i = 0; i < bytes; i++) sprintf(buf + i * 2, "%02x", got[i]);
    return strcmp(buf, want) == 0;
}

/* `hex` is what goes ON THE WIRE; `want` is what the decoder should hand back. They are the
 * same for every format whose frame starts at a block boundary, and they are NOT for Keri —
 * see the block-form note in lf_tag_em.c (C160). */
static int trial2(const char *name, const char *hex, const char *want, size_t bits,
                  lf_psk1_phase_mode_t mode, const lf_psk1_format_t *fmt);

static int trial(const char *name, const char *hex, size_t bits,
                 lf_psk1_phase_mode_t mode, const lf_psk1_format_t *fmt) {
    return trial2(name, hex, hex, bits, mode, fmt);
}

static int trial2(const char *name, const char *hex, const char *want, size_t bits,
                  lf_psk1_phase_mode_t mode, const lf_psk1_format_t *fmt) {
    uint8_t frame[LF_DECODE_MAX_FRAME_BYTES] = {0};
    size_t bytes = bits / 8;
    for (size_t i = 0; i < bytes; i++) {
        unsigned v;
        sscanf(hex + i * 2, "%2x", &v);
        frame[i] = (uint8_t)v;
    }

    const nrf_pwm_sequence_t *seq = lf_psk1_modulator(frame, bits, mode);
    if (seq == NULL) {
        printf("  %-28s ⛔ modulator refused the frame\n", name);
        return 1;
    }

    /* Parity decides whether the encoder should have doubled the buffer, and the test
     * asserts that directly — a silent reversion would otherwise still pass whenever the
     * decoder got lucky on frame alignment. */
    size_t popcount = 0;
    for (size_t i = 0; i < bits; i++)
        popcount += (frame[i / 8] >> (7 - (i % 8))) & 1u;
    size_t want_entries = (mode == LF_PSK1_PHASE_DIFFERENTIAL && (popcount & 1u)) ? bits * 2 : bits;
    size_t got_entries = (size_t)seq->length / 4u;

    static int16_t samples[LF_SAMPLED_MAX_CAPTURE_SAMPLES];
    size_t want_samples = INDALA_PSK_MIN_SAMPLES(bits) * 2;
    if (want_samples > LF_SAMPLED_MAX_CAPTURE_SAMPLES) want_samples = LF_SAMPLED_MAX_CAPTURE_SAMPLES;
    size_t n = render(seq, samples, want_samples);

    lf_decode_result_t r;
    int ok = lf_psk1_decode_fmt(samples, n, fmt, &r) && hexeq(r.id, want, bytes);
    int shape_ok = (got_entries == want_entries);

    printf("  %-28s %s  entries %4zu (want %4zu) %s  decode %s\n",
           name, (ok && shape_ok) ? "✓" : "⛔",
           got_entries, want_entries, shape_ok ? "ok" : "WRONG",
           ok ? "exact" : "MISMATCH");
    return (ok && shape_ok) ? 0 : 1;
}

/* One ASK protocol through its own emitter and the shipping decoder. */
static int trial_ask(const char *name, const char *hex, size_t bits,
                     const protocol *proto, const lf_ask_format_t *fmt) {
    uint8_t frame[32] = {0};
    for (size_t i = 0; i < bits / 8u; i++) {
        unsigned v; sscanf(hex + 2 * i, "%2x", &v); frame[i] = (uint8_t)v;
    }
    void *codec = proto->alloc();
    const nrf_pwm_sequence_t *seq = proto->modulator(codec, frame);
    if (seq == NULL) {
        printf("  %-28s ⛔ modulator returned NULL\n", name);
        proto->free(codec);
        return 1;
    }
    const size_t entries = (size_t)seq->length / 4u;
    static int16_t air[LF_SAMPLED_MAX_CAPTURE_SAMPLES];
    size_t n = render_ask(seq, air, sizeof(air) / sizeof(air[0]));
    proto->free(codec);

    lf_decode_result_t r;
    int ok = lf_ask_manchester_decode_fmt(air, n, fmt, &r);
    int exact = ok && hexeq(r.id, hex, bits / 8u);
    printf("  %-28s %s  entries %4zu (want %4zu)  %s\n", name,
           exact ? "✓" : "⛔", entries, bits,
           ok ? (exact ? "decode exact" : "DECODED WRONG") : "NO DECODE");
    return exact ? 0 : 1;
}

/* ⭐⭐ THE FSK2a ROUND TRIP, and it reuses render_ask() unchanged — which is worth saying
 * rather than hiding, because it looks like a shortcut and is not. That renderer emits
 * `counter_top` samples per entry, the first half high and the rest low. For an ASK entry
 * that is one Manchester bit; for an FSK entry with counter_top 8 or 10 it is one tone cycle.
 * The same three lines describe both because the peripheral really does treat them the same:
 * one tick is one carrier cycle for every non-PSK1 tag type.
 *
 * ⛔ WHAT THIS ARM IS FOR. The FSK emitter is the first on this branch whose ENTRY COUNT
 * depends on the data — six entries for a 0 and five for a 1 — so the sequence length is
 * computed per call rather than fixed at compile time. An off-by-one there emits a frame that
 * is very slightly the wrong length, which no preamble check would catch and which would show
 * up only as a reader that sometimes works. */
static int trial_fsk(const char *name, const char *hex, size_t bits,
                     const protocol *proto,
                     bool (*decode)(int16_t *, size_t, lf_decode_result_t *)) {
    uint8_t frame[32] = {0};
    for (size_t i = 0; i < bits / 8u; i++) {
        unsigned v; sscanf(hex + 2 * i, "%2x", &v); frame[i] = (uint8_t)v;
    }
    void *codec = proto->alloc();
    const nrf_pwm_sequence_t *seq = proto->modulator(codec, frame);
    if (seq == NULL) {
        printf("  %-28s ⛔ modulator returned NULL\n", name);
        proto->free(codec);
        return 1;
    }
    const size_t entries = (size_t)seq->length / 4u;
    static int16_t air[LF_SAMPLED_MAX_CAPTURE_SAMPLES];
    size_t n = render_ask(seq, air, sizeof(air) / sizeof(air[0]));
    proto->free(codec);

    lf_decode_result_t r;
    int ok = decode(air, n, &r);
    int exact = ok && hexeq(r.id, hex, bits / 8u);
    printf("  %-28s %s  entries %4zu             %s\n", name,
           exact ? "✓" : "⛔", entries,
           ok ? (exact ? "decode exact" : "DECODED WRONG") : "NO DECODE");
    return exact ? 0 : 1;
}

/* ⭐⭐ A TRANSCRIPTION PIN, NOT A ROUND TRIP — said plainly because the arms above are round
 * trips and this one is a different kind of evidence. There is no air and no decoder here: it
 * asserts that the shipping writer turns a frame into the EXACT block words a Proxmark clone
 * of that same credential was dumped holding (C202). It cannot tell you the credential is
 * right; it can only tell you we still transcribe it the way the reference tag does.
 *
 * ⛔ That is the failure it is here to catch. Keri's block form is `(id << 3) | 7`, three bits
 * out of phase with its air frame, and writing the wrong one of the two put a stable, WRONG,
 * confidently-decoded credential on a tag 6 times out of 6 (C160). A rotation introduced into
 * fsk2a_t55xx_blocks() would be invisible to every other test in this file and would need a
 * Proxmark and a tag to notice — which is exactly the sort of check that does not get run.
 *
 * ⚠ These are also the first T5577 writer vectors in the tree. `gallagher_t55xx_writer` and
 * the other five still have no host coverage; adding theirs needs a dump of each reference
 * clone, which is bench work rather than typing. Recorded in NEXT.md.
 */
/* ⭐ The same pin for a writer that is NOT the shared FSK2a transcription — Gallagher's,
 * NexWatch's and above all KERI's, whose block form is `(id << 3) | 7` and therefore three
 * bits out of phase with its air frame. ⛔ Keri is the reason this whole family of arms
 * exists: sending its frame view where its block form belongs gave a stable, confident,
 * WRONG credential 6 times out of 6 (C160), and nothing on the host could see it. */
static int trial_writer(const char *name, const char *hex, size_t frame_bytes,
                        uint8_t (*writer)(uint8_t *, uint32_t *),
                        const uint32_t *want, uint8_t want_n) {
    uint8_t frame[32] = {0};
    for (size_t i = 0; i < frame_bytes; i++) {
        unsigned v; sscanf(hex + 2 * i, "%2x", &v); frame[i] = (uint8_t)v;
    }
    uint32_t blks[8] = {0};
    uint8_t n = writer(frame, blks);
    int bad = (n != want_n);
    for (uint8_t i = 0; i < n && !bad; i++) {
        bad = (blks[i] != want[i]);
    }
    printf("  %-28s %s  blocks %u (want %u)", name, bad ? "\u26d4" : "\u2713", n, want_n);
    if (bad) {
        printf("   got");
        for (uint8_t i = 0; i < n; i++) printf(" %08X", blks[i]);
        printf("   want");
        for (uint8_t i = 0; i < want_n; i++) printf(" %08X", want[i]);
    }
    printf("\n");
    return bad ? 1 : 0;
}

static int trial_t55xx(const char *name, const char *hex, uint8_t words, uint32_t config,
                       const uint32_t *want) {
    uint8_t frame[32] = {0};
    for (size_t i = 0; i < (size_t)words * 4u; i++) {
        unsigned v;
        sscanf(hex + 2 * i, "%2x", &v);
        frame[i] = (uint8_t)v;
    }
    uint32_t blks[8] = {0};
    uint8_t n = fsk2a_t55xx_blocks(frame, words, config, blks);

    int bad = (n != (uint8_t)(words + 1));
    for (uint8_t i = 0; i < n && !bad; i++) {
        bad = (blks[i] != want[i]);
    }
    printf("  %-28s %s  blocks %u (want %u)", name, bad ? "⛔" : "✓", n, words + 1u);
    if (bad) {
        printf("   got");
        for (uint8_t i = 0; i < n; i++) printf(" %08X", blks[i]);
        printf("   want");
        for (uint8_t i = 0; i < (uint8_t)(words + 1); i++) printf(" %08X", want[i]);
    }
    printf("\n");
    return bad ? 1 : 0;
}

/* ⭐⭐ PIN THE EMITTED WAVEFORM'S SHAPE, NOT JUST ITS MEANING. Every other arm in this file
 * asks "does what we emit decode back" — and for AWID the answer is yes while the Flipper
 * reads it 0 of 6, so that question is not sufficient on its own.
 *
 * ⛔ WHAT THIS PINS AND WHY. A real AWID emission was captured on this bench from a Flipper
 * emulating the protocol, decoded byte-exact by our own reader, and measured: the HIGH run is
 * 4 samples on essentially every tone and only the LOW run varies, 4 for RF/8 and 6 for RF/10
 * (C226). Our emitter was emitting 5 and 5 for the long tone — the same PERIOD, which our own
 * decoder cannot tell apart because it only ever looks at the sum, and which no round trip in
 * this file would ever catch. It was corrected to match the measurement.
 *
 * ⇒ Without this arm that correction can revert silently. `counter_top / 2` is the obvious
 * thing to write and it is what was there before. */
static int trial_awid_duty(void) {
    uint8_t frame[12] = {0x01, 0x1D, 0xB2, 0x18, 0x27, 0x1B, 0xD8, 0x11,
                         0x11, 0x11, 0x11, 0x11};
    void *codec = awid.alloc();
    const nrf_pwm_sequence_t *seq = awid.modulator(codec, frame);
    const size_t entries = (size_t)seq->length / 4u;
    int bad = 0;
    size_t shorts = 0, longs = 0;
    unsigned worst = 0;
    for (size_t i = 0; i < entries; i++) {
        const nrf_pwm_values_wave_form_t *e = &seq->values.p_wave_form[i];
        const uint16_t top = e->counter_top;
        const uint16_t duty = e->channel_0 & 0x7FFFu;
        if (top == 8) {
            shorts++;
        } else if (top == 10) {
            longs++;
        } else {
            bad = 1;
        }
        /* The mark is FIXED at 4 regardless of tone — that is the measured reference. */
        if (duty != 4u) {
            bad = 1;
            if (duty > worst) {
                worst = duty;
            }
        }
    }
    awid.free(codec);
    /* ⚠ Report the OFFENDING duty, not the wanted one. A failure line that still reads
     * "mark fixed at 4" describes the test's intention rather than what it found, which
     * is exactly the kind of message that makes a red result easy to skim past. */
    if (bad) {
        printf("  %-28s ⛔  %zu short + %zu long entries, mark reaches %u, want 4\n",
               "AWID duty vs real emission", shorts, longs, worst);
    } else {
        printf("  %-28s ✓  %zu short + %zu long entries, mark fixed at 4\n",
               "AWID duty vs real emission", shorts, longs);
    }
    return bad;
}


/* ⭐⭐⭐ THE ERROR-DETECTION SWEEP — how each frame gate behaves when the frame is WRONG.
 *
 * ⛔ WHY THIS EXISTS. C251 measured a real reader handing an operator a confident credential
 * of a protocol the tag was not: a burst flipped a few bits, HID's parity correctly rejected
 * the frame, and `unpack()` walked on to a format whose checks were weaker and accepted it.
 * Every arm above asks "does a CORRECT frame survive the round trip". None of them asks the
 * question that defect is about: what does the decoder do with a frame that is wrong?
 *
 * Each arm below flips exactly one frame bit, re-emits, re-decodes, and sorts the outcome:
 *
 *   rejected   the gate caught it — no frame returned
 *   silent     accepted, and the id came back IDENTICAL to the truth. Not a pass: it means
 *              that bit does not reach the returned id at all (padding, or a preamble bit the
 *              decoder re-derives), so the flip was never testable in the first place
 *   WRONG      accepted, id differs — a confident wrong credential, which is the C251 shape
 *
 * ⚠ WHAT A "WRONG" COUNT IS AND IS NOT. A high count is not automatically our bug. A protocol
 * carrying no integrity field cannot reject anything, and that is the protocol's property, not
 * the decoder's. The number is actionable only where the format HAS a checksum, CRC or parity
 * and we are not enforcing it — which is exactly what this table makes visible.
 *
 * ⚠ AND THIS BYPASSES THE CAPTURE-LEVEL DEFENCE. The device readers require two agreeing
 * stacks before they report anything, which would catch an error that does not repeat. These
 * arms feed one perfect rendering of a corrupted frame, so they characterise the FRAME GATE
 * alone. Both layers are real; only one of them is measured here. */
typedef enum { FAM_PSK1, FAM_ASK, FAM_FSK, FAM_BIPHASE } family_t;

typedef struct {
    family_t fam;
    const protocol *proto;
    lf_psk1_phase_mode_t mode;
    const lf_psk1_format_t *pfmt;
    const lf_ask_format_t *afmt;
    const lf_biphase_format_t *bfmt;
    bool (*fdec)(int16_t *, size_t, lf_decode_result_t *);
} arm_t;

/* Emit `frame`, render it the way its family renders, decode. 1 if the decoder returned. */
static int run_once(const arm_t *a, uint8_t *frame, size_t bits, lf_decode_result_t *r) {
    static int16_t air[LF_SAMPLED_MAX_CAPTURE_SAMPLES];
    const size_t cap = sizeof(air) / sizeof(air[0]);
    size_t n = 0;

    if (a->fam == FAM_PSK1) {
        const nrf_pwm_sequence_t *seq = lf_psk1_modulator(frame, bits, a->mode);
        if (seq == NULL) return 0;
        size_t want = INDALA_PSK_MIN_SAMPLES(bits) * 2;
        if (want > cap) want = cap;
        n = render(seq, air, want);
        return lf_psk1_decode_fmt(air, n, a->pfmt, r) ? 1 : 0;
    }

    void *codec = a->proto->alloc();
    const nrf_pwm_sequence_t *seq = a->proto->modulator(codec, frame);
    if (seq == NULL) { a->proto->free(codec); return 0; }
    n = (a->fam == FAM_BIPHASE) ? render_level(seq, air, cap) : render_ask(seq, air, cap);
    a->proto->free(codec);

    if (a->fam == FAM_ASK) return lf_ask_manchester_decode_fmt(air, n, a->afmt, r) ? 1 : 0;
    if (a->fam == FAM_BIPHASE) return lf_ask_biphase_decode_fmt(air, n, a->bfmt, r) ? 1 : 0;
    return a->fdec(air, n, r) ? 1 : 0;
}

/* ⛔ THE COUNTS ARE PINNED, not just printed. A gate that quietly weakens — an `accept` hook
 * dropped, a preamble shortened — changes these numbers and nothing else about the harness
 * would notice, because every round trip above would still be exact. */
static int sweep(const char *name, const char *hex, const char *want, size_t bits,
                 const arm_t *a, size_t exp_rejected, size_t exp_silent, size_t exp_wrong) {
    uint8_t truth[LF_DECODE_MAX_FRAME_BYTES] = {0};
    const size_t bytes = bits / 8u;
    for (size_t i = 0; i < bytes; i++) {
        unsigned v; sscanf(hex + 2 * i, "%2x", &v); truth[i] = (uint8_t)v;
    }

    size_t rejected = 0, silent = 0, wrong = 0;
    for (size_t b = 0; b < bits; b++) {
        uint8_t f[LF_DECODE_MAX_FRAME_BYTES];
        memcpy(f, truth, sizeof(f));
        f[b / 8u] ^= (uint8_t)(1u << (7u - (b % 8u)));

        lf_decode_result_t r;
        if (!run_once(a, f, bits, &r)) { rejected++; continue; }
        if (hexeq(r.id, want, bytes)) silent++; else wrong++;
    }
    int ok = (rejected == exp_rejected && silent == exp_silent && wrong == exp_wrong);
    printf("  %-28s %4zu bits   rejected %4zu   silent %4zu   WRONG %4zu   (%3zu%% caught) %s\n",
           name, bits, rejected, silent, wrong,
           bits ? (rejected * 100u) / bits : 0u,
           ok ? "" : "\u26d4 MOVED");
    return ok ? 0 : 1;
}

int main(void) {
    int bad = 0;
    puts("emitter -> air -> decoder, both halves the shipping firmware\n");

    /* The bench credential, PSK1: the frame `lf indala read` decodes 110/160 on device. */
    bad += trial("Indala26  PSK1", "a0000000e6bd0e92", 64,
                 LF_PSK1_PHASE_DIRECT, &LF_PSK1_FORMAT_INDALA64);

    /* IDTECK shares the buffer and the encoder; only the preamble differs. */
    bad += trial("IDTECK    PSK1", "4944544b55667788", 64,
                 LF_PSK1_PHASE_DIRECT, &LF_PSK1_FORMAT_IDTECK);

    /* ⭐ Keri shares the encoder whole — same mode, same buffer, only the preamble differs.
     * This is the ONLY verification its emulate arm has: the bench lost coupling before it
     * could be read off the air (C159), and the grid says so. */
    /* ⛔ KERI EMITS THE BLOCK FORM AND DECODES TO THE FRAME VIEW — the same 64-bit cycle
     * three bits apart. This asymmetry IS the test: emulating the frame view instead gave
     * Momentum a stable wrong credential 6 of 6 (C160), and nothing on the host would have
     * caught it, because our own decoder is rotation-insensitive and accepts either. */
    bad += trial2("Keri      PSK1 block form", "00000004000181cf", "e000000080003039", 64,
                  LF_PSK1_PHASE_DIRECT, &LF_PSK1_FORMAT_KERI);

    /* ⭐ NexWatch, the fourth protocol through the shared encoder and the first at 96 bits.
     * ⛔ NO ROTATION, and the contrast with the Keri arm directly above is the point: these
     * are the exact bytes a Proxmark clone of card 12345678 leaves in T5577 blocks 1-3, and
     * they are ALSO the air frame, because NexWatch's preamble starts at the block boundary
     * where Keri's sits three bits before one. Both arms are `trial`/`trial2` against a real
     * clone's own dump, so neither protocol's alignment rests on an assumption (C164, C166). */
    bad += trial("NexWatch  PSK1 96-bit", "5600000000436455121e6000", 96,
                 LF_PSK1_PHASE_DIRECT, &LF_PSK1_FORMAT_NEXWATCH);

    /* ⭐ A SECOND NEXWATCH FRAME, AND IT IS NOT PADDING. The first is even-parity by luck of
     * the credential; this is our own written card 87654321 / mode 2 / Quadrakey, whose
     * frame differs in the mode nibble, the parity nibble and the checksum byte. An encoder
     * that mangled any of those three would still pass the arm above, because the preamble
     * and the scrambled id would survive. */
    bad += trial("NexWatch  PSK1 2nd credential", "560000000012776a2f207b00", 96,
                 LF_PSK1_PHASE_DIRECT, &LF_PSK1_FORMAT_NEXWATCH);

    /* ⭐ C152's frame. Its bits XOR to 1, so the encoder MUST emit two copies. */
    bad += trial("Indala224 PSK2 odd parity",
                 "80000001b23523a6c2e31eba3cbee4afb3c6ad1fcf649393928c14e5", 224,
                 LF_PSK1_PHASE_DIFFERENTIAL, &LF_PSK1_FORMAT_INDALA224);

    /* The even-parity control: same format, one copy, and it must still decode. Without
     * this arm "always double" would pass every test above. */
    bad += trial("Indala224 PSK2 even parity",
                 "80000001b23523a6c2e31eba3cbee4afb3c6ad1fcf649393928c14e4", 224,
                 LF_PSK1_PHASE_DIFFERENTIAL, &LF_PSK1_FORMAT_INDALA224);


    /* ⭐⭐ THE ASK FAMILY, WHICH HAD NO ROUND TRIP AT ALL UNTIL NOW. Every PSK protocol has had
     * one since C152 shipped three wrong encodings in a single session; the four ASK protocols
     * went to the radio with nothing between them and it. These compile the SHIPPING emitters
     * — `gallagher.c`, `securakey.c`, `noralsy.c` — and feed them to the SHIPPING decoder.
     * ⚠ Securakey is the arm that matters most structurally: it is RF/40 where the others are
     * RF/32, so it is the one that would catch a bit rate hard-coded back into the emitter. */
    bad += trial_ask("Gallagher ASK RF/32", "7feaa31e76d86c6d868cc249", 96,
                     &gallagher, &LF_ASK_FORMAT_GALLAGHER);
    bad += trial_ask("Securakey ASK RF/40", "7fcb400001adea5344300000", 96,
                     &securakey, &LF_ASK_FORMAT_SECURAKEY);
    bad += trial_ask("Noralsy   ASK RF/32", "bb0214ff0112402233670000", 96,
                     &noralsy, &LF_ASK_FORMAT_NORALSY);


    /* ⭐⭐ THE FIRST FSK2a EMITTER — emitter to air to decoder, both halves the shipping
     * firmware, on the credential the Proxmark wrote and this bench read back byte-exactly
     * (C201, C203). ⚠ The frame is all-zeros-heavy, which is the WORST case for the entry
     * count: a 0 costs six entries and a 1 costs five. */
    bad += trial_fsk("AWID     FSK2a RF/8-10", "011db218271bd81111111111", 96,
                     &awid, awid_fsk_decode);
    bad += trial_awid_duty();

    /* ⭐⭐ THE FIRST BIPHASE EMITTER, on the credential the Proxmark wrote and our own reader
     * read back 12 of 12 exact (C213). ⚠ Its entry count is FIXED at two per bit where AWID's
     * varies with the data — biphase spends the same time on both symbols. */
    bad += trial_biphase("GProxII  biphase RF/64", "f84602a46119d4a114211046", 96,
                         &gproxii, &LF_BIPHASE_FORMAT_GPROXII);

    /* ⭐⭐ THE FSK2a T5577 WRITER, pinned to three real Proxmark clones' own block dumps.
     * ⚠ AWID and Paradox share a config word exactly; only Pyramid differs, and only in the
     * block-count field. All three block forms equal their air frames — measured per protocol,
     * because Keri's does not (C160, C202). */
    static const uint32_t want_awid[]    = {0x00107060, 0x011D8171, 0x1DD11811, 0x11111111};
    static const uint32_t want_paradox[] = {0x00107060, 0x0F555556, 0x95596A6A, 0x9999A59A};
    static const uint32_t want_pyramid[] = {0x00107080, 0x00010101, 0x01010101, 0x0101016E,
                                            0xB35E5DA4};
    bad += trial_t55xx("AWID     -> T5577 blocks", "011d81711dd1181111111111", 3,
                       T5577_AWID_CONFIG, want_awid);
    bad += trial_t55xx("Paradox  -> T5577 blocks", "0f55555695596a6a9999a59a", 3,
                       T5577_PARADOX_CONFIG, want_paradox);
    bad += trial_t55xx("Pyramid  -> T5577 blocks", "00010101010101010101016eb35e5da4", 4,
                       T5577_PYRAMID_CONFIG, want_pyramid);

    /* ⭐⭐ THE OTHER WRITERS, pinned from the reference dumps already recorded in the tree —
     * C202 flagged that six of them had no host coverage at all, and five of those six had
     * their reference blocks written down and unused. ⛔ KERI IS THE ONE THAT MATTERS: its
     * block form is `(id << 3) | 7`, three bits out of phase with the air frame it is given,
     * and that rotation is the exact bug that shipped a confident wrong credential 6 of 6. */
    static const uint32_t want_keri[]    = {T5577_KERI_CONFIG, 0x00000004, 0x000181CF};
    static const uint32_t want_gal[]     = {T5577_GALLAGHER_CONFIG, 0x7FEAA31E, 0x76D86C6D,
                                            0x868CC249};
    static const uint32_t want_nw[]      = {T5577_NEXWATCH_CONFIG, 0x56000000, 0x00436455,
                                            0x121E6000};
    static const uint32_t want_gproxii[] = {T5577_GPROXII_CONFIG, 0xF84602A4, 0x6119D4A1,
                                            0x14211046};
    static const uint32_t want_fdxb[]    = {T5577_FDXB_CONFIG, 0x0031BD39, 0x740201F8,
                                            0x804039B5, 0x18040201};
    bad += trial_writer("Keri ROTATED -> blocks", "e000000080003039", 8,
                        keri_t55xx_writer, want_keri, 3);
    bad += trial_writer("Gallagher -> T5577 blocks", "7feaa31e76d86c6d868cc249", 12,
                        gallagher_t55xx_writer, want_gal, 4);
    bad += trial_writer("NexWatch -> T5577 blocks", "5600000000436455121e6000", 12,
                        nexwatch_t55xx_writer, want_nw, 4);
    bad += trial_t55xx("GProxII  -> T5577 blocks", "f84602a46119d4a114211046", 3,
                       T5577_GPROXII_CONFIG, want_gproxii);
    bad += trial_t55xx("FDX-B    -> T5577 blocks", "0031bd39740201f8804039b518040201", 4,
                       T5577_FDXB_CONFIG, want_fdxb);

    /* ⭐⭐ FDX-A, measured off `lf destron clone --uid 0F1E2D3C4B` (C339) — and it is the ONE
     * that does not share AWID's config: **FSK2, not FSK2a**, `00105060` against `00107060`,
     * one bit in the modulation field. C171's trap, and the reason every config here is read
     * off a reference clone rather than inherited from a sibling.
     *
     * ⛔ THE INPUT IS THE COMPLEMENT OF WHAT `lf fdxa read` PRINTS, and that is the protocol,
     * not a quirk of this test: an FSK2 tag stores the inverse of what our FSK2a-path reader
     * reports. `write_fdxa_to_t55xx()` applies the complement and hands the result to the same
     * `fsk2a_t55xx_blocks()` as everything else, so what is pinned here is what that function
     * is actually given. The complement itself is asserted just below. */
    static const uint32_t want_fdxa[] = {0x00105060, 0xAAE26A55, 0x69566659, 0x655A5A65};
    bad += trial_t55xx("FDX-A    -> T5577 blocks", "aae26a5569566659655a5a65", 3,
                       T5577_FDXA_CONFIG, want_fdxa);

    /* ⚠ The reader's raw and the stored blocks, from the SAME tag, must be bitwise inverse.
     * Measured on two Proxmark clones (C339). Pinned here so a change to either side that
     * breaks the relationship fails on the host instead of on the bench. */
    {
        static const char reader_raw[] = "551d95aa96a999a69aa5a59a";
        static const char stored[]     = "aae26a5569566659655a5a65";
        int inv_bad = 0;
        for (size_t i = 0; i < sizeof(reader_raw) - 1; i += 2) {
            unsigned a = 0, b = 0;
            sscanf(reader_raw + i, "%2x", &a);
            sscanf(stored + i, "%2x", &b);
            if (((a ^ b) & 0xFF) != 0xFF) { inv_bad++; }
        }
        printf("  %-34s %s\n", "FDX-A reader raw ~= blocks",
               inv_bad == 0 ? "ok" : "MISMATCH");
        bad += inv_bad;
    }

    /* ✅ THE LAST TWO, and their reference dumps were MEASURED for this rather than found: C231
     * had to leave Securakey and Noralsy uncovered because nobody had ever written their block
     * forms down, and inventing the expected blocks from our own writer's code would have
     * tested nothing. Two Proxmark clones later they are here. ⚠ Both turn out to be straight
     * transcriptions, which is what makes the arms cheap — and is exactly the thing that had
     * to be checked rather than assumed, because Keri's is not. */
    static const uint32_t want_sk[]  = {T5577_SECURAKEY_CONFIG, 0x7FCB4000, 0x01ADEA53,
                                        0x44300000};
    static const uint32_t want_nor[] = {T5577_NORALSY_CONFIG, 0xBB0214FF, 0x01100022,
                                        0x33070000};
    bad += trial_writer("Securakey -> T5577 blocks", "7fcb400001adea5344300000", 12,
                        securakey_t55xx_writer, want_sk, 4);
    bad += trial_writer("Noralsy  -> T5577 blocks", "bb0214ff0110002233070000", 12,
                        noralsy_t55xx_writer, want_nor, 4);


    /* ⭐⭐⭐ THE ERROR-DETECTION SWEEP. One flipped bit per row of the frame, every bit, every
     * protocol that has an emitter here. See the block comment above `sweep()` for what the
     * three columns mean and, more importantly, what they do NOT mean. */
    puts("\nONE FLIPPED FRAME BIT — what each gate does with a frame that is wrong\n");
    {
        const arm_t psk_ind    = {FAM_PSK1, NULL, LF_PSK1_PHASE_DIRECT, &LF_PSK1_FORMAT_INDALA64, NULL, NULL, NULL};
        const arm_t psk_idteck = {FAM_PSK1, NULL, LF_PSK1_PHASE_DIRECT, &LF_PSK1_FORMAT_IDTECK,   NULL, NULL, NULL};
        const arm_t psk_keri   = {FAM_PSK1, NULL, LF_PSK1_PHASE_DIRECT, &LF_PSK1_FORMAT_KERI,     NULL, NULL, NULL};
        const arm_t psk_nw     = {FAM_PSK1, NULL, LF_PSK1_PHASE_DIRECT, &LF_PSK1_FORMAT_NEXWATCH, NULL, NULL, NULL};
        const arm_t psk_224    = {FAM_PSK1, NULL, LF_PSK1_PHASE_DIFFERENTIAL, &LF_PSK1_FORMAT_INDALA224, NULL, NULL, NULL};
        const arm_t ask_gal    = {FAM_ASK, &gallagher, 0, NULL, &LF_ASK_FORMAT_GALLAGHER, NULL, NULL};
        const arm_t ask_sk     = {FAM_ASK, &securakey, 0, NULL, &LF_ASK_FORMAT_SECURAKEY, NULL, NULL};
        const arm_t ask_nor    = {FAM_ASK, &noralsy,   0, NULL, &LF_ASK_FORMAT_NORALSY,   NULL, NULL};
        const arm_t fsk_awid   = {FAM_FSK, &awid,      0, NULL, NULL, NULL, awid_fsk_decode};
        const arm_t bi_gpii    = {FAM_BIPHASE, &gproxii, 0, NULL, NULL, &LF_BIPHASE_FORMAT_GPROXII, NULL};

        bad += sweep("Indala26  PSK1",  "a0000000e6bd0e92", "a0000000e6bd0e92", 64, &psk_ind, 35, 0, 29);
        bad += sweep("IDTECK    PSK1",  "4944544b55667788", "4944544b55667788", 64, &psk_idteck, 32, 0, 32);
        /* ⛔ Keri emits the block form and decodes to the frame view — the sweep flips bits in
         * what goes ON THE WIRE and compares against what comes BACK, same as its round trip. */
        bad += sweep("Keri      PSK1",  "00000004000181cf", "e000000080003039", 64, &psk_keri, 33, 0, 31);
        bad += sweep("NexWatch  PSK1",  "5600000000436455121e6000", "5600000000436455121e6000", 96, &psk_nw, 80, 0, 16);
        bad += sweep("Indala224 PSK2",  "80000001b23523a6c2e31eba3cbee4afb3c6ad1fcf649393928c14e4",
                                 "80000001b23523a6c2e31eba3cbee4afb3c6ad1fcf649393928c14e4", 224, &psk_224, 28, 0, 196);
        bad += sweep("Gallagher ASK",   "7feaa31e76d86c6d868cc249", "7feaa31e76d86c6d868cc249", 96, &ask_gal, 88, 0, 8);
        bad += sweep("Securakey ASK",   "7fcb400001adea5344300000", "7fcb400001adea5344300000", 96, &ask_sk, 28, 0, 68);
        bad += sweep("Noralsy   ASK",   "bb0214ff0112402233670000", "bb0214ff0112402233670000", 96, &ask_nor, 80, 0, 16);
        bad += sweep("AWID      FSK2a", "011db218271bd81111111111", "011db218271bd81111111111", 96, &fsk_awid, 96, 0, 0);
        bad += sweep("GProxII   biphase", "f84602a46119d4a114211046", "f84602a46119d4a114211046", 96, &bi_gpii, 64, 0, 32);
    }

    printf("\n%s\n", bad ? "⛔ FAILURES" : "✓ all round trips exact");
    return bad ? 1 : 0;
}
