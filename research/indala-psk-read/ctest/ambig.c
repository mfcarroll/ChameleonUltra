/* ⭐⭐⭐ HOW AMBIGUOUS IS A WIEGAND FRAME? — the measurement the upstream conversation needs.
 *
 * C251 found a corrupted H10301 frame coming back as a confident `Indala 26-bit`, because
 * `unpack()` walks every format of the same bit length and returns the FIRST that accepts.
 * C276 shipped a warning and established the walk has exactly one caller. What neither
 * answered is the question that decides whether the walk should be narrowed at all:
 *
 *   ⇒ does a VALID H10301 frame match only H10301, or do others accept it too?
 *
 * If valid frames are uniquely matched, the first-match ordering never matters in normal use
 * and only bites on corruption — which the warning now covers, and narrowing is cheap. If
 * valid frames routinely match several formats, the ordering is load-bearing on every read
 * and narrowing would change what working readers report.
 *
 * ⛔ This compiles the SHIPPING `wiegand.c` — not a transcription — and asks it one format at
 * a time through its own `unpack()`, so the answer is the firmware's own, not a model of it. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wiegand.h"

/* Every format in the table, so the sweep cannot miss one by being written from memory. */
static const card_format_t ALL[] = {
    H10301, IND26, IND27, INDASC27, TECOM27, W2804, IND29, ATSW30, ADT31, HCP32,
    HPP32, KASTLE, KANTECH, WIE32, D10202, H10306, N10002, OPTUS34, SMP34, BQT34,
    C1K35S, C15001, S12906, SIE36, H10320, H10302, H10304, P10004, HGEN37, MDI37,
    BQT38, ISCS, PW39, P10001, CASI40, BC40, DEFCON32, H800002, C1K48S, AVIG56,
    IR56, ACTPHID,
};
#define ALL_COUNT (sizeof(ALL) / sizeof(ALL[0]))

/* Which formats of this bit length accept the frame? Returns the count and names the first. */
static int matches(uint8_t length, uint64_t lo, card_format_t *first) {
    int n = 0;
    for (size_t i = 0; i < ALL_COUNT; i++) {
        wiegand_card_t *c = unpack((uint8_t)ALL[i], length, 0, lo);
        if (c != NULL) {
            if (n == 0 && first != NULL) { *first = ALL[i]; }
            n++;
            free(c);
        }
    }
    return n;
}

int main(void) {
    wiegand_card_t card;
    memset(&card, 0, sizeof(card));
    card.format = H10301;
    card.facility_code = 123;
    card.card_number = 4567;
    uint64_t lo = pack(&card);

    card_format_t first = 0;
    int n = matches(26, lo, &first);
    printf("valid H10301 FC 123 / CN 4567 -> lo %010llx\n", (unsigned long long)lo);
    printf("  formats of length 26 that ACCEPT it: %d   (first in table order: %d)\n", n, (int)first);
    printf("  they are:");
    for (size_t i = 0; i < ALL_COUNT; i++) {
        wiegand_card_t *c = unpack((uint8_t)ALL[i], 26, 0, lo);
        if (c != NULL) {
            printf("  fmt %d -> FC %lu card %llu", (int)ALL[i],
                   (unsigned long)c->facility_code, (unsigned long long)c->card_number);
            free(c);
        }
    }
    printf("\n\n");

    /* Every single-bit corruption of that frame: how many stay decodable, and as what? */
    int stays_h10301 = 0, becomes_other = 0, rejected = 0, ambiguous = 0;
    for (int b = 0; b < 26; b++) {
        uint64_t bad = lo ^ (1ULL << b);
        card_format_t f = 0;
        int m = matches(26, bad, &f);
        if (m == 0) { rejected++; continue; }
        if (m > 1) { ambiguous++; }
        if (f == H10301) { stays_h10301++; } else { becomes_other++; }
    }
    printf("26 single-bit corruptions of that frame:\n");
    printf("  rejected by every format      %2d\n", rejected);
    printf("  still first-matched H10301    %2d\n", stays_h10301);
    printf("  first-matched a DIFFERENT fmt %2d\n", becomes_other);
    printf("  accepted by MORE than one fmt %2d\n", ambiguous);


    /* ⭐⭐ CAN A NON-HID FORMAT EVER WIN AT 26 BITS? Three real ind26 credentials written to a
     * tag all read back as H10301, which suggests H10301 accepts everything ind26 does. If that
     * holds, the "no HID layout fits" branch in `lf hid prox read` is unreachable at 26 bits and
     * saying so is better than shipping a message nobody can see. */
    {
        int ind26_total = 0, h10301_also = 0;
        for (uint32_t fc = 0; fc < 4096; fc += 37) {
            for (uint32_t cn = 0; cn < 4096; cn += 53) {
                wiegand_card_t c;
                memset(&c, 0, sizeof(c));
                c.format = IND26; c.facility_code = fc; c.card_number = cn;
                uint64_t w = pack(&c);
                if (w == 0) { continue; }
                wiegand_card_t *as_ind = unpack(IND26, 26, 0, w);
                if (as_ind == NULL) { continue; }
                free(as_ind);
                ind26_total++;
                wiegand_card_t *as_hid = unpack(H10301, 26, 0, w);
                if (as_hid != NULL) { h10301_also++; free(as_hid); }
            }
        }
        printf("\nind26 frames that H10301 ALSO accepts: %d of %d\n", h10301_also, ind26_total);
        if (h10301_also == ind26_total) {
            printf("  ⇒ H10301 accepts EVERY ind26 frame, so at 26 bits a non-HID format can\n"
                   "    never win the walk — H10301 is first in the table.\n");
        }
    }

    /* ⛔ PINNED. If the formats table is reordered, extended or narrowed, these numbers move
     * and this arm says so — which is the whole reason a measurement belongs in the harness
     * rather than in a notebook. */
    int bad = 0;
    if (n != 2) { printf("⛔ MOVED: valid-frame match count\n"); bad = 1; }
    if (first != H10301) { printf("⛔ MOVED: first match is no longer H10301\n"); bad = 1; }
    if (rejected != 13 || stays_h10301 != 0 || becomes_other != 13 || ambiguous != 0) {
        printf("⛔ MOVED: corruption outcome counts\n"); bad = 1;
    }
    printf("\n%s\n", bad ? "⛔ FAILURES" : "✓ ambiguity counts unchanged");
    return bad;
}
