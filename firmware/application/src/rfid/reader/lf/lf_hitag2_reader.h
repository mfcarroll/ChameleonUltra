#pragma once

// A Hitag 2 reader in password mode, on the interactive reader engine (lf_rdr_ia.h).

#include <stdint.h>

#include "lf_rdr_ia.h"

// The link the reader uses, which the bench may tune (gap width above all, against the antenna's ringing).
lf_rdr_link_t *lf_hitag2_reader_link(void);

// Powers the tag up and reads its UID (page 0). Returns STATUS_LF_TAG_OK or STATUS_LF_TAG_NO_FOUND.
uint8_t lf_hitag2_reader_uid(uint32_t *uid);

// Reads all eight pages with the password: the UID, the password, then page 3's answer, then READ PAGE for each
// page. read_mask has a bit set for each page read. Returns STATUS_LF_TAG_OK if every page was read,
// STATUS_MF_ERR_AUTH if the tag gave its UID but did not answer the password, else STATUS_LF_TAG_NO_FOUND.
uint8_t lf_hitag2_reader_read(uint32_t password, uint32_t pages[8], uint8_t *read_mask);
