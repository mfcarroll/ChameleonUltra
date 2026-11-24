// Host test of the Hitag 2 reader's reset probe, against a simulated tag on a fake reader link. The tag answers
// START_AUTH with its UID behind 5 start-of-frame ones, and is either every-second (it ignores a START_AUTH while it
// waits for the password, and goes idle) or answers every one. A field gap of at least reset_us powers it down.

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app_status.h"
#include "lf_hitag2_reader.h"

static const uint32_t UID = 0x01020304u;  // the bench placeholder, made up

static struct {
    bool every_second;
    uint32_t reset_us;
    bool waiting;      // answered START_AUTH, waiting for the password
    bool field_on;
    unsigned lose_every;  // drop every nth answer to the probe's START_AUTH, 0 for none
    bool probing;
    bool absent;          // no tag in the field
    unsigned n_answers;
} tag;

void bsp_delay_ms(uint16_t nms) { (void)nms; }
void bsp_delay_us(uint32_t nus) { (void)nus; }
void bsp_wdt_feed(void) {}

void lf_rdr_field(bool on) {
    tag.field_on = on;
    if (!on) {
        tag.waiting = false;
    }
}

static bool tag_command(const uint8_t *tx, size_t n_tx, lf_rdr_reply_t *rx) {
    memset(rx, 0, sizeof(*rx));
    if (tag.absent || n_tx != 5 || (tx[0] >> 3) != 0x18u) {
        return true;  // only START_AUTH is simulated
    }
    if (tag.every_second && tag.waiting) {
        tag.waiting = false;
        return true;
    }
    tag.waiting = true;
    if (tag.probing && tag.lose_every && ++tag.n_answers % tag.lose_every == 0) {
        return true;
    }
    for (size_t i = 0; i < 5; i++) {
        lf_ia_bit_put(rx->bits, i, 1);
    }
    for (size_t i = 0; i < 32; i++) {
        lf_ia_bit_put(rx->bits, 5 + i, (UID >> (31 - i)) & 1u);
    }
    rx->n_bits = 37;
    rx->clean = true;
    return true;
}

bool lf_rdr_transact(const lf_rdr_link_t *link, const uint8_t *tx, size_t n_tx, uint32_t power_up_us,
                     lf_rdr_reply_t *rx) {
    (void)link;
    if (power_up_us) {
        tag.field_on = true;
    }
    assert(tag.field_on);
    return tag_command(tx, n_tx, rx);
}

bool lf_rdr_transact_reset(const lf_rdr_link_t *link, const uint8_t *tx, size_t n_tx, uint32_t off_us, uint32_t on_us,
                           lf_rdr_reply_t *rx) {
    (void)link;
    (void)on_us;
    assert(tag.field_on);
    if (off_us >= tag.reset_us) {
        tag.waiting = false;
    }
    tag.probing = true;
    const bool ok = tag_command(tx, n_tx, rx);
    tag.probing = false;
    return ok;
}

static const uint16_t GAPS[] = {0, 100, 290, 400, 1000, 20000};
#define N (sizeof(GAPS) / sizeof(GAPS[0]))

static void probe(bool every_second, uint32_t reset_us, unsigned lose_every, uint8_t *started, uint8_t *answered) {
    memset(&tag, 0, sizeof(tag));
    tag.every_second = every_second;
    tag.reset_us = reset_us;
    tag.lose_every = lose_every;
    assert(lf_hitag2_reader_reset_probe(GAPS, N, 3000, 4, started, answered) == STATUS_LF_TAG_OK);
    assert(!tag.field_on);  // the probe leaves the field off
}

int main(void) {
    uint8_t started[N], answered[N];

    // Every-second, reset by a gap of 300 µs or more: silent after 0 and 100, answers from 400 up. 290 is just short.
    probe(true, 300, 0, started, answered);
    for (size_t i = 0; i < N; i++) {
        assert(started[i] == 4);
        assert(answered[i] == (GAPS[i] >= 300 ? 4 : 0));
    }

    // Every-second, kept through any gap the probe makes: silent throughout.
    probe(true, 50000, 0, started, answered);
    for (size_t i = 0; i < N; i++) {
        assert(started[i] == 4 && answered[i] == 0);
    }

    // A tag that answers every START_AUTH answers after every gap, including none.
    probe(false, 300, 0, started, answered);
    for (size_t i = 0; i < N; i++) {
        assert(started[i] == 4 && answered[i] == 4);
    }

    // Lost answers: every third answer to the probe's START_AUTH is lost, so the long gaps get 8 of their 12.
    probe(true, 300, 3, started, answered);
    unsigned long_gap = 0;
    for (size_t i = 0; i < N; i++) {
        assert(started[i] == 4);
        assert(answered[i] <= started[i]);
        if (GAPS[i] < 300) {
            assert(answered[i] == 0);
        } else {
            long_gap += answered[i];
        }
    }
    assert(long_gap == 8);

    // No tag: nothing starts, and the probe says so.
    memset(&tag, 0, sizeof(tag));
    tag.absent = true;
    assert(lf_hitag2_reader_reset_probe(GAPS, N, 3000, 2, started, answered) == STATUS_LF_TAG_NO_FOUND);
    for (size_t i = 0; i < N; i++) {
        assert(started[i] == 0 && answered[i] == 0);
    }

    puts("test_lf_hitag2_reader: ok");
    return 0;
}
