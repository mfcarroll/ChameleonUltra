#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Structure for packed wiegand messages
// Always align lowest value (last transmitted) bit to ordinal position 0 (lowest valued bit bottom)
typedef struct {
    uint8_t length;  // number of encoded bits in wiegand message (excluding headers and preamble)
    uint64_t hi;     // bits in x<<64 positions
    uint64_t lo;     // lowest ordinal positions
} wiegand_message_t;

// Structure for unpacked wiegand card, like HID prox
/** How many ALTERNATIVE layout ids a decoded card can carry. Two, because that is what fits
 *  in the HID payload's spare bytes; `matches` still counts them all. */
#define WIEGAND_MAX_OTHER_FORMATS 2

typedef struct {
    uint32_t facility_code;
    uint64_t card_number;
    uint32_t issue_level;
    uint32_t oem;
    uint8_t format;
    /** ⭐ How many formats of this bit length accepted the frame, INCLUDING `format`. 1 means
     *  the read is unambiguous. Anything more means the credential above is one reading of
     *  several, and a caller that prints it as the answer is guessing. */
    uint8_t matches;
    /** ⭐ Whether the winning format VALIDATED anything — parity, a checksum, a spacer — as
     *  opposed to accepting the frame because it was the right length. C300 measured that 12
     *  of the 31 unpackers can never reject, so this is the difference between a credential
     *  that was checked and one that was merely parsed. */
    bool verified;
    /** The first few other layouts that also fit, for naming them. `matches - 1` is exact
     *  even when it exceeds this array. */
    uint8_t others[WIEGAND_MAX_OTHER_FORMATS];
} wiegand_card_t;

typedef struct {
    bool has_parity;
    uint32_t max_fc;   // max facility code
    uint64_t max_cn;   // max cardNumber
    uint32_t max_il;   // max issue_level
    uint32_t max_oem;  // max oem
} card_format_descriptor_t;

typedef enum {
    H10301 = 1,
    IND26,
    IND27,
    INDASC27,
    TECOM27,
    W2804,
    IND29,
    ATSW30,
    ADT31,
    HCP32,
    HPP32,
    KASTLE,
    KANTECH,
    WIE32,
    D10202,
    H10306,
    N10002,
    OPTUS34,
    SMP34,
    BQT34,
    C1K35S,
    C15001,
    S12906,
    SIE36,
    H10320,
    H10302,
    H10304,
    P10004,
    HGEN37,
    MDI37,
    BQT38,
    ISCS,
    PW39,
    P10001,
    CASI40,
    BC40,
    DEFCON32,
    H800002,
    C1K48S,
    AVIG56,
    IR56,
    ACTPHID,
} card_format_t;

// Structure for defined Wiegand card formats available for packing/unpacking
typedef struct {
    card_format_t format;
    uint64_t (*pack)(wiegand_card_t *card);
    wiegand_card_t *(*unpack)(uint64_t hi, uint64_t lo);
    uint32_t bits;  // number of bits in this format
    card_format_descriptor_t fields;
} card_format_table_t;

extern uint64_t pack(wiegand_card_t *card);
extern wiegand_card_t *unpack(uint8_t format_hint, uint8_t length, uint64_t hi, uint64_t lo);

/* ⭐ How many OTHER layouts of the same bit length also accept this frame, naming up to `max`
 * of them in `out`. See the note at the definition: an unpinned read is only the first match,
 * and for 15 of 29 formats that is the wrong one (C284, C285). */
extern wiegand_card_t *wiegand_card_alloc();
