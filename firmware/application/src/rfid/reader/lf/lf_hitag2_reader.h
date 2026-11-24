#pragma once

// A Hitag 2 reader in password mode, on the interactive reader engine (lf_rdr_ia.h).

#include <stdint.h>

#include "lf_rdr_ia.h"

// The link the reader uses, which the bench may tune (gap width above all, against the antenna's ringing).
lf_rdr_link_t *lf_hitag2_reader_link(void);

// Powers the tag up and reads its UID (page 0), counting it once two START_AUTH answers agree. Returns
// STATUS_LF_TAG_OK or STATUS_LF_TAG_NO_FOUND.
uint8_t lf_hitag2_reader_uid(uint32_t *uid);

// Reads all eight pages with the password: START_AUTH for the UID, the password, then READ PAGE for each page until
// two answers agree. Page 0 also has to match the UID. read_mask has a bit set for each page read. Returns
// STATUS_LF_TAG_OK if every page was read, STATUS_MF_ERR_AUTH if the tag gave its UID but did not answer the password
// (then only page 0, the UID, is set), else STATUS_LF_TAG_NO_FOUND.
uint8_t lf_hitag2_reader_read(uint32_t password, uint32_t pages[8], uint8_t *read_mask);

// Whether a field gap resets the tag. A Hitag 2 tag waiting for its password ignores another START_AUTH and goes back
// to idle (the datasheet, rev 2.1, 4.2.2), and a tag that loses power starts idle and answers. For each gap, rounds
// times: power the tag up and get its UID (start_auth, which leaves it waiting for the password), then switch the field
// off for the gap, on for wait_us, and send START_AUTH. answered[i] counts the answers that carried the same UID,
// out of started[i] tries that got a UID first. A gap of 0 leaves the field on: the tag's own every-second behaviour.
// Returns STATUS_LF_TAG_OK if any try got a UID, else STATUS_LF_TAG_NO_FOUND.
uint8_t lf_hitag2_reader_reset_probe(const uint16_t *gaps_us, size_t n_gaps, uint32_t wait_us, uint8_t rounds,
                                     uint8_t *started, uint8_t *answered);
