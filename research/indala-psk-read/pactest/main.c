/*
 * Host harness for the FIRMWARE PAC decoder — the same pattern as ../ctest.
 *
 * ⭐ THE .c UNDER TEST IS THE ONE THAT SHIPS. pac.c pulls in a handful of Nordic headers for
 * its MODULATOR, which the decode path never touches, so stubs/ supplies just enough for the
 * file to compile unchanged. Copying the decoder instead would test a copy.
 *
 *     make && ./pacdemod ../caps/pac/pac_r0.bin
 *
 * Capture format: `lf sniff --bits 16`, two bytes per sample, big-endian 14-bit.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "protocols/pac.h"

int main(int argc, char **argv) {
    static unsigned char raw[64 * 1024];
    for (int a = 1; a < argc; a++) {
        FILE *f = fopen(argv[a], "rb");
        if (!f) { fprintf(stderr, "cannot open %s\n", argv[a]); return 2; }
        size_t got = fread(raw, 1, sizeof(raw), f);
        fclose(f);
        size_t n = got / 2;

        void *codec = pac.alloc();
        pac.decoder.start(codec, 0);
        int hits = 0;
        size_t first = 0;
        for (size_t k = 0; k < n; k++) {
            uint16_t v = (uint16_t)((raw[2 * k] << 8) | raw[2 * k + 1]);
            if (pac.decoder.feed(codec, v)) {
                if (!hits) first = k;
                hits++;
                if (hits == 1) {
                    char id[32] = {0};
                    memcpy(id, pac.get_data(codec), pac.data_size);
                    printf(" %-28s %6zu samples  CARD %s  (first at %zu)\n",
                           argv[a], n, id, first);
                }
            }
        }
        if (!hits) printf(" %-28s %6zu samples  -\n", argv[a], n);
        pac.free(codec);
    }
    return 0;
}
