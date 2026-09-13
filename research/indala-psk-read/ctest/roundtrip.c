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

    printf("\n%s\n", bad ? "⛔ FAILURES" : "✓ all round trips exact");
    return bad ? 1 : 0;
}
