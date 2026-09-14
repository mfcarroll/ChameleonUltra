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
/* Parallel to ALL[], purely so the sweep's output is readable. ⛔ The ENUM is authoritative —
 * these are labels, not identity — and the static assert below is what stops the two drifting. */
static const char *const ALL_NAMES[] = {
    "H10301", "IND26", "IND27", "INDASC27", "TECOM27", "W2804", "IND29", "ATSW30", "ADT31", "HCP32",
    "HPP32", "KASTLE", "KANTECH", "WIE32", "D10202", "H10306", "N10002", "OPTUS34", "SMP34", "BQT34",
    "C1K35S", "C15001", "S12906", "SIE36", "H10320", "H10302", "H10304", "P10004", "HGEN37", "MDI37",
    "BQT38", "ISCS", "PW39", "P10001", "CASI40", "BC40", "DEFCON32", "H800002", "C1K48S", "AVIG56",
    "IR56", "ACTPHID",
};
_Static_assert(sizeof(ALL_NAMES) / sizeof(ALL_NAMES[0]) == ALL_COUNT,
               "ALL_NAMES must stay parallel to ALL");


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


    /* ⭐⭐⭐ THE SAME QUESTION AT EVERY OTHER BIT LENGTH. At 26 bits H10301 accepts every valid
     * ind26 frame, so a non-HID format can never win and a non-HID result means corruption. The
     * message `lf hid prox read` prints for OTHER lengths hedges — "either the tag really carries
     * that layout or the capture was corrupted" — and that hedge has never been checked.
     *
     * ⇒ For each length, pack credentials under each NON-HID format there and ask whether some
     * HID format at that length accepts the result. If one always does, the hedge is wrong there
     * too and the message can be definite. If not, a genuine foreign credential really can win. */
    int verdict_moved = 0;
    {
        /* The formats the table's own `// HID …` comments name. Not my idea of who makes what. */
        static const card_format_t HIDF[] = {
            H10301, ADT31, HCP32, HPP32, D10202, H10306, C1K35S, C15001,
            S12906, SIE36, H10320, H10302, H10304, P10004, HGEN37, ACTPHID,
        };
        printf("\nCan a NON-HID format ever win, by bit length?\n");
        for (uint8_t len = 26; len <= 56; len++) {
            int foreign = 0, covered = 0, uncovered = 0;
            int ex_fmt = 0; uint32_t ex_fc = 0; uint64_t ex_cn = 0;
            for (size_t i = 0; i < ALL_COUNT; i++) {
                int is_hid = 0;
                for (size_t k = 0; k < sizeof(HIDF)/sizeof(HIDF[0]); k++) {
                    if (HIDF[k] == ALL[i]) { is_hid = 1; break; }
                }
                if (is_hid) { continue; }
                for (uint32_t fc = 1; fc < 1024; fc += 101) {
                    for (uint64_t cn = 1; cn < 65536; cn += 6151) {
                        wiegand_card_t c;
                        memset(&c, 0, sizeof(c));
                        c.format = ALL[i]; c.facility_code = fc; c.card_number = cn;
                        uint64_t w = pack(&c);
                        if (w == 0) { continue; }
                        wiegand_card_t *self = unpack((uint8_t)ALL[i], len, 0, w);
                        if (self == NULL) { continue; }   /* not this format's length */
                        free(self);
                        foreign++;
                        /* ⛔ THE QUESTION IS WHO WINS, NOT WHO ACCEPTS. This used to ask
                         * whether ANY HID format accepted the frame, which was the same thing
                         * only because the old walk took the first row in TABLE order and the
                         * HID formats sat first at these lengths. Since C302 the walk prefers a
                         * format that can VALIDATE, so a check-less HID row no longer beats a
                         * validating foreign one — and `lf hid prox read`'s corruption message
                         * branches on exactly this verdict. Asking the real unpinned walk is the
                         * only way the message stays true. */
                        int hid_takes = 0;
                        wiegand_card_t *win = unpack(0, len, 0, w);
                        if (win != NULL) {
                            for (size_t k = 0; k < sizeof(HIDF)/sizeof(HIDF[0]); k++) {
                                if (HIDF[k] == (card_format_t)win->format) { hid_takes = 1; break; }
                            }
                            free(win);
                        }
                        if (hid_takes) { covered++; } else {
                            /* ⭐ Remember one so the branch can be exercised on a real tag
                             * rather than left as a count — `lf hid clone -w <fmt>` writes it.
                             * ⚠ REMEMBERED, not printed here: printing inside the loop put the
                             * example ABOVE the summary line it belongs to, which reads as if it
                             * belonged to the previous length. */
                            if (uncovered == 0) {
                                ex_fmt = (int)ALL[i]; ex_fc = fc; ex_cn = cn;
                            }
                            uncovered++;
                        }
                    }
                }
            }
            if (foreign == 0) { continue; }
            printf("  %2d bits: %5d foreign frames, %5d also taken by an HID format, "
                   "%5d NOT -> %s\n", len, foreign, covered, uncovered,
                   uncovered == 0 ? "non-HID can never win" : "a genuine foreign tag CAN win");
            if (uncovered > 0) {
                printf("           e.g. fmt %d, fc %lu, cn %llu — no HID format takes it\n",
                       ex_fmt, (unsigned long)ex_fc, (unsigned long long)ex_cn);
            }
            /* ⛔ PINNED PER LENGTH, because `lf hid prox read`'s message now BRANCHES on this.
             * The counts themselves depend on the credential grid and are not pinned; the
             * VERDICT is the claim the CLI relies on. */
            /* ⛔ 32 LEFT THIS LIST IN C304. Under the old table-order walk a check-less HID
             * row won every 32-bit frame; under C302's validating-first walk KASTLE wins its
             * own, so 121 of 363 foreign 32-bit frames now surface as foreign. The CLI's
             * corruption message branched on this and was telling operators that correct
             * numbers were fiction. */
            int expect_total_cover = (len == 26 || len == 37);
            if (expect_total_cover != (uncovered == 0)) {
                printf("    ⛔ MOVED: the CLI's per-length wording assumes otherwise\n");
                verdict_moved = 1;
            }
        }
    }


    /* ⭐⭐ `unpack()`'s OWN AMBIGUITY COUNT, CHECKED AGAINST THIS FILE'S INDEPENDENT ONE.
     * C302 folded the enumeration INTO `unpack()` — the walk counts every format that accepted
     * while it is already walking — so there is no longer a second entry point that could drift
     * out of step with the first. This pins the surviving one: for every format at every length,
     * `card->matches` from an UNPINNED unpack must equal this file's own per-format enumeration.
     * Two different walks over the same table, which is the point.
     *
     * ⛔ A table edit that breaks the count now fails `make check` instead of quietly changing
     * what `lf hid prox read` tells an operator. */
    {
        int checked = 0, disagreed = 0;
        for (size_t i = 0; i < ALL_COUNT; i++) {
            for (uint32_t fc = 1; fc < 256; fc += 37) {
                for (uint64_t cn = 1; cn < 4096; cn += 613) {
                    wiegand_card_t c;
                    memset(&c, 0, sizeof(c));
                    c.format = ALL[i]; c.facility_code = fc; c.card_number = cn;
                    uint64_t w = pack(&c);
                    if (w == 0) { continue; }
                    /* Find this format's own bit length by asking which length it unpacks at. */
                    uint8_t len = 0;
                    for (uint8_t L = 26; L <= 56 && len == 0; L++) {
                        wiegand_card_t *t = unpack((uint8_t)ALL[i], L, 0, w);
                        if (t != NULL) { len = L; free(t); }
                    }
                    if (len == 0) { continue; }
                    int mine = matches(len, w, NULL);          /* every format that accepts */
                    wiegand_card_t *u = unpack(0, len, 0, w);  /* UNPINNED: the real read path */
                    int theirs = (u == NULL) ? -1 : (int)u->matches;
                    if (u != NULL) { free(u); }
                    checked++;
                    /* Both count ALL acceptors, so they must agree exactly. */
                    if (theirs != mine) { disagreed++; }
                }
            }
        }
        printf("\nunpack()->matches against this file's own count: %d frames, %d disagree\n",
               checked, disagreed);
        if (disagreed != 0) { verdict_moved = 1; }
    }


    /* ⭐⭐⭐ DOES A FORMAT READ BACK AS ITSELF? — the write side of C284, which measured the read
     * side on 29 real tags at one credential each. `lf hid prox write -f IND26` produces a tag
     * that an unpinned read reports as H10301 (C278, on hardware), and the write command says
     * nothing about it. Before warning, establish WHICH formats have that property over a
     * credential grid rather than the single fc 1 / cn 1 C284 could afford on the bench. */
    {
        int never_self = 0, sometimes_self = 0;
        printf("\nDoes each format read back as ITSELF, unpinned?\n");
        for (size_t i = 0; i < ALL_COUNT; i++) {
            int tried = 0, self = 0; card_format_t stole = 0;
            uint32_t ex_fc = 0; uint64_t ex_cn = 0;
            for (uint32_t fc = 0; fc < 512; fc += 43) {
                for (uint64_t cn = 1; cn < 8192; cn += 811) {
                    wiegand_card_t c;
                    memset(&c, 0, sizeof(c));
                    c.format = ALL[i]; c.facility_code = fc; c.card_number = cn;
                    uint64_t w = pack(&c);
                    if (w == 0) { continue; }
                    uint8_t len = 0;
                    for (uint8_t L = 26; L <= 56 && len == 0; L++) {
                        wiegand_card_t *t = unpack((uint8_t)ALL[i], L, 0, w);
                        if (t != NULL) { len = L; free(t); }
                    }
                    if (len == 0) { continue; }
                    tried++;
                    /* ⛔ THE REAL READ PATH, NOT TABLE ORDER. This used to call this file's own
                     * `matches()` and take the first format in ALL[] order, which was the old
                     * walk's semantics. Since C302 the shipping walk prefers a format that can
                     * VALIDATE, so asking `matches()` would measure an ordering the firmware no
                     * longer uses — a harness quietly grading the wrong algorithm. */
                    wiegand_card_t *win = unpack(0, len, 0, w);
                    card_format_t first = (win == NULL) ? 0 : (card_format_t)win->format;
                    if (win != NULL) { free(win); }
                    if (first == ALL[i]) { self++; }
                    else {
                        /* ⭐ Name the first credential that does NOT read back as itself, so the
                         * partial cases can be exercised on a real tag rather than left as a
                         * ratio — M40: a branch nobody has seen print is untested. */
                        if (stole == 0) { stole = first; ex_fc = fc; ex_cn = cn; }
                    }
                }
            }
            if (tried == 0) { continue; }
            if (self == tried) { continue; }               /* reads back as itself, always */
            printf("  fmt %2d: %4d of %4d read back as itself — the rest report as fmt %d",
                   (int)ALL[i], self, tried, (int)stole);
            if (self > 0) {
                printf("  (e.g. fc %lu cn %llu does not)",
                       (unsigned long)ex_fc, (unsigned long long)ex_cn);
            }
            printf("\n");
            never_self += (self == 0) ? 1 : 0;
            sometimes_self += (self > 0) ? 1 : 0;
        }
        /* ⛔ PINNED — `lf hid prox write`'s warning is built from these two sets (C295). If the
         * table is reordered or a format's checks change, the warning becomes wrong and this
         * says so rather than letting the CLI mislead someone writing a tag.
         * ⭐ 14/3 became 12/4 when C302 taught the walk to prefer a validating format: KASTLE
         * now reads back as itself ALWAYS — it was C284's headline failure — and HGEN37 went
         * from never to 120 of 132. This arm is what caught the CLI lists going stale. */
        printf("  => %d never read back as themselves, %d sometimes\n", never_self, sometimes_self);
        if (never_self != 12 || sometimes_self != 4) {
            printf("  ⛔ MOVED: the write-side warning's format lists are out of date\n");
            verdict_moved = 1;
        }
    }

    /* ⭐⭐⭐ CAN `unpack()` BE FIXED AT ALL? — the arm that decides it, by BEHAVIOUR.
     *
     * C284 measured the relabelling and C285 explained it: 12 of the 31 unpackers have no
     * rejection path, so the first check-less format at a given length swallows everything
     * there. That diagnosis has an obvious-looking repair — implement each format's parity
     * check and the walk stops mislabelling — and the repair is only available IF THE CHECKS
     * EXIST. Reading the source says they do not. Reading the source is also how this project
     * has been wrong before (C269), so this asks the shipping code instead.
     *
     * Feed each format 4096 pseudo-random frames of its own bit length, through its own
     * `unpack()`, and count what it accepts. A format carrying k check bits rejects all but
     * ~2^-k of them; a format carrying none accepts every single one. The bit length is
     * DISCOVERED by sweeping 26..56 rather than hardcoded, so the sweep cannot inherit a
     * wrong constant from the notes.
     *
     * ⭐ THE NULL IS BUILT IN AND IS NOT OPTIONAL. H10301 has two parity bits and must land
     * near 25%; Kastle and H10320 have more and must land lower. If a format known to check
     * came back at 100%, this arm would be measuring its own bug rather than the table, and
     * every conclusion drawn from it would be worthless. */
    int no_check = 0, with_check = 0, unmeasured = 0, h10301_pct = -1, multi_len = 0;
    int bad2 = 0;
    {
        printf("\nselectivity — %% of 4096 random frames each format ACCEPTS at its own length:\n");
        const int N = 4096;
        for (size_t i = 0; i < ALL_COUNT; i++) {
            int hit_len = -1, hit_acc = 0, lens_hit = 0;
            for (int len = 26; len <= 56; len++) {
                uint64_t mask = (1ull << len) - 1ull;
                uint64_t s = 0x9E3779B97F4A7C15ull ^ ((uint64_t)ALL[i] << 32) ^ (uint64_t)len;
                int acc = 0;
                for (int t = 0; t < N; t++) {
                    s ^= s << 13; s ^= s >> 7; s ^= s << 17;   /* xorshift64, fixed seed */
                    wiegand_card_t *c = unpack((uint8_t)ALL[i], (uint8_t)len, 0, s & mask);
                    if (c != NULL) { acc++; free(c); }
                }
                if (acc == 0) { continue; }
                lens_hit++; hit_len = len; hit_acc = acc;
            }
            if (lens_hit == 0) { unmeasured++; continue; }
            if (lens_hit > 1) {
                printf("  ⛔ fmt %2d accepts at %d DIFFERENT lengths\n", (int)ALL[i], lens_hit);
                multi_len++;
            }
            int pct = (hit_acc * 100) / N;
            if (ALL[i] == H10301) { h10301_pct = pct; }
            if (hit_acc == N) {
                no_check++;
                printf("  %-9s (%2d bits): %3d%% — NO CHECK, accepts everything\n",
                       ALL_NAMES[i], hit_len, pct);
            } else {
                with_check++;
            }
        }
        printf("  => %d formats have NO rejection path, %d have one, %d not measurable here\n",
               no_check, with_check, unmeasured);
        /* ⭐ THE NULL, PRINTED RATHER THAN ASSUMED. H10301 carries two parity bits, so it
         * must accept about a quarter of random frames. A number near 100 here would mean
         * the sweep is not reaching the checks and the 100%% rows above prove nothing. */
        printf("  null: H10301 (2 parity bits) accepts %d%% — expected ~25%%\n", h10301_pct);
    }

    /* ⛔ PINNED. If the formats table is reordered, extended or narrowed, these numbers move
     * and this arm says so — which is the whole reason a measurement belongs in the harness
     * rather than in a notebook. */
    /* ⛔ PINNED, AND THIS IS THE ARM THAT CORRECTED C285's ARITHMETIC. C285 published
     * "13 of the 32" while enumerating exactly 12 formats and naming `unpack_nonlinear` as a
     * shared helper in the same breath — the helper was counted as a 32nd unpacker. The table
     * has 31 rows. If these move, either the table changed or this sweep stopped reaching the
     * checks, and both need a person rather than a re-pin. */
    if (no_check != 12 || with_check != 19 || unmeasured != 11) {
        printf("⛔ MOVED: selectivity census (expected 12 / 19 / 11)\n"); bad2 = 1;
    }
    if (h10301_pct < 20 || h10301_pct > 30) {
        printf("⛔ NULL FAILED: H10301 should accept ~25%% of random frames, got %d%%\n", h10301_pct);
        bad2 = 1;
    }
    if (multi_len != 0) { printf("⛔ MOVED: a format accepts at more than one length\n"); bad2 = 1; }

    int bad = bad2;
    if (n != 2) { printf("⛔ MOVED: valid-frame match count\n"); bad = 1; }
    if (first != H10301) { printf("⛔ MOVED: first match is no longer H10301\n"); bad = 1; }
    if (rejected != 13 || stays_h10301 != 0 || becomes_other != 13 || ambiguous != 0) {
        printf("⛔ MOVED: corruption outcome counts\n"); bad = 1;
    }
    if (verdict_moved) { bad = 1; }
    printf("\n%s\n", bad ? "⛔ FAILURES" : "✓ ambiguity counts unchanged");
    return bad;
}
