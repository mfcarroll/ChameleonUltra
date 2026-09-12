/*
 * Host harness for the FIRMWARE Indala decoder.
 *
 * ⭐ THIS IS THE INDEPENDENT CHECK, and it is the point of the whole file. mfdemod.py and
 * lf_indala_psk.c share no code: one is numpy floating point with an FFT brick-wall
 * filter, the other is C integer arithmetic with a 3-tap notch folded into the boxcar.
 * Running both over the same committed captures is the only thing that can catch a port
 * that is self-consistently wrong — which is exactly how this project lost a fortnight to
 * a decoder whose own unit test encoded with the same bug it decoded with.
 *
 *     make && ./cdemod ../caps/phasebits/tag_p*.bin
 *     make check        # both decoders over all 320 captures, compared
 *
 * Capture format: `lf sniff --bits 16` — two bytes per sample, big-endian, 14-bit value,
 * so the high byte never exceeds 0x3F.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lf_indala_psk.h"

static const char TRUTH[] = "a0000000e6bd0e92";

int main(int argc, char **argv) {
    int quiet = 0, hits = 0, decoded = 0, files = 0;
    static int16_t buf[LF_PSK1_MAX_CAPTURE_SAMPLES];

    int mode224 = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-q")) {
            quiet = 1;
            continue;
        }
        if (!strcmp(argv[i], "--224")) {
            mode224 = 1;
            continue;
        }
        FILE *f = fopen(argv[i], "rb");
        if (!f) {
            fprintf(stderr, "cannot open %s\n", argv[i]);
            return 2;
        }
        static unsigned char raw[LF_PSK1_MAX_CAPTURE_SAMPLES * 2];
        size_t got = fread(raw, 1, sizeof(raw), f);
        fclose(f);
        files++;

        size_t n = got / 2;
        int ok16 = (got >= 2 && (got % 2) == 0);
        for (size_t k = 0; k < n && ok16; k++) {
            if (raw[2 * k] > 0x3F) {
                ok16 = 0;
            }
        }
        if (!ok16) {
            if (!quiet) printf(" %-40s not a 16-bit capture\n", argv[i]);
            continue;
        }
        for (size_t k = 0; k < n; k++) {
            buf[k] = (int16_t)(((unsigned)raw[2 * k] << 8) | raw[2 * k + 1]);
        }

        /* ⭐ BOTH FORMATS, SAME CAPTURE. The question C90 raises is whether an IDTECK frame
         * that the Indala preamble falsely matches can be RECOGNISED as IDTECK from the same
         * samples — if so, decoding IDTECK is the rejection test. Needs its own copy of the
         * buffer because the decoder works in place. */
        static int16_t buf2[LF_PSK1_MAX_CAPTURE_SAMPLES];
        memcpy(buf2, buf, n * sizeof(buf[0]));
        indala_psk_result_t ri;
        int idteck = lf_psk1_decode_fmt(buf2, n, &LF_PSK1_FORMAT_IDTECK, &ri);
        char ihex[17] = "-";
        if (idteck) {
            for (int k = 0; k < 8; k++) sprintf(ihex + 2 * k, "%02x", ri.id[k]);
        }

        if (mode224) {
            indala_psk_result_t r2;
            if (!indala224_psk1_decode(buf, n, &r2)) {
                printf(" %-44s %5zu samples  -            energy %7ld\n",
                       argv[i], n, (long)r2.energy);
                continue;
            }
            decoded++;
            printf(" %-44s %5zu samples  ", argv[i], n);
            for (int k = 0; k < 28; k++) printf("%02x", r2.id[k]);
            printf("  off %2u pos %3u %s\n", r2.offset, r2.bit_pos,
                   r2.inverted ? "inv" : "");
            continue;
        }

        indala_psk_result_t r;
        if (!indala_psk1_decode(buf, n, &r)) {
            if (!quiet) printf(" %-40s %5zu samples  -   IDTECK %s\n", argv[i], n, ihex);
            continue;
        }
        if (0) {
            /* ⭐ energy is valid here and nowhere else is it visible — this line is the
             * calibration for INDALA_PSK_ENERGY_PRESENT. */
            if (!quiet) printf(" %-40s %5zu samples  -%50s energy %7ld\n",
                               argv[i], n, "", (long)r.energy);
            continue;
        }
        decoded++;
        char hex[17];
        for (int k = 0; k < 8; k++) {
            sprintf(hex + 2 * k, "%02x", r.id[k]);
        }
        int match = !strcmp(hex, TRUTH);
        hits += match;
        if (!quiet) {
            printf(" %-40s %5zu samples  %s%s%s  off %2u pos %3u %s amp %7ld  "
                   "Fmt 26 FC %u Card %u parity %u%u %s\n",
                   argv[i], n, match ? "*** " : "", hex, match ? " ***" : "",
                   r.offset, r.bit_pos, r.inverted ? "inv" : "   ", (long)r.amp,
                   r.fc, r.csn, (r.parity >> 1) & 1, r.parity & 1,
                   r.wiegand26_ok ? "parity-ok" : "parity-BAD");
            printf(" %-40s %*senergy %7ld   IDTECK %s\n", "", 57, "", (long)r.energy, ihex);
        }
    }
    fprintf(stderr, "%d files, %d produced a frame, %d matched %s\n",
            files, decoded, hits, TRUTH);
    return 0;
}
