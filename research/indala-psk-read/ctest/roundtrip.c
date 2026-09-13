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
    uint8_t frame[LF_PSK1_MAX_FRAME_BYTES] = {0};
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

    static int16_t samples[LF_PSK1_MAX_CAPTURE_SAMPLES];
    size_t want_samples = INDALA_PSK_MIN_SAMPLES(bits) * 2;
    if (want_samples > LF_PSK1_MAX_CAPTURE_SAMPLES) want_samples = LF_PSK1_MAX_CAPTURE_SAMPLES;
    size_t n = render(seq, samples, want_samples);

    indala_psk_result_t r;
    int ok = lf_psk1_decode_fmt(samples, n, fmt, &r) && hexeq(r.id, want, bytes);
    int shape_ok = (got_entries == want_entries);

    printf("  %-28s %s  entries %4zu (want %4zu) %s  decode %s\n",
           name, (ok && shape_ok) ? "✓" : "⛔",
           got_entries, want_entries, shape_ok ? "ok" : "WRONG",
           ok ? "exact" : "MISMATCH");
    return (ok && shape_ok) ? 0 : 1;
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

    printf("\n%s\n", bad ? "⛔ FAILURES" : "✓ all round trips exact");
    return bad ? 1 : 0;
}
