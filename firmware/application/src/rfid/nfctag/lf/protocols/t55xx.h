#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define T5577_BLOCK_COUNT 8

// t5577 block 0 definitions, thanks proxmark3!
#define T5577_POR_DELAY 0x00000001
#define T5577_ST_TERMINATOR 0x00000008
#define T5577_PWD 0x00000010
#define T5577_MAXBLOCK_SHIFT 5
#define T5577_AOR 0x00000200
#define T5577_PSKCF_RF_2 0
#define T5577_PSKCF_RF_4 0x00000400
#define T5577_PSKCF_RF_8 0x00000800
#define T5577_MODULATION_DIRECT 0
#define T5577_MODULATION_PSK1 0x00001000
#define T5577_MODULATION_PSK2 0x00002000
#define T5577_MODULATION_PSK3 0x00003000
#define T5577_MODULATION_FSK1 0x00004000
#define T5577_MODULATION_FSK2 0x00005000
#define T5577_MODULATION_FSK1a 0x00006000
#define T5577_MODULATION_FSK2a 0x00007000
#define T5577_MODULATION_MANCHESTER 0x00008000
#define T5577_MODULATION_BIPHASE 0x00010000
#define T5577_MODULATION_DIPHASE 0x00018000
#define T5577_X_MODE 0x00020000
#define T5577_BITRATE_RF_8 0
#define T5577_BITRATE_RF_16 0x00040000
#define T5577_BITRATE_RF_32 0x00080000
#define T5577_BITRATE_RF_40 0x000C0000
#define T5577_BITRATE_RF_50 0x00100000
#define T5577_BITRATE_RF_64 0x00140000
#define T5577_BITRATE_RF_100 0x00180000
#define T5577_BITRATE_RF_128 0x001C0000
#define T5577_TESTMODE_DISABLED 0x60000000

#define T5577_OPCODE_RESET 0x00
#define T5577_OPCODE_PAGE0 0x02
#define T5577_OPCODE_PAGE1 0x03
#define T5577_EM410X_64_CONFIG (  \
    T5577_BITRATE_RF_64 |         \
    T5577_MODULATION_MANCHESTER | \
    T5577_PWD |                   \
    (2 << T5577_MAXBLOCK_SHIFT))

#define T5577_EM410X_ELECTRA_CONFIG ( \
    T5577_BITRATE_RF_64 |            \
    T5577_MODULATION_MANCHESTER |    \
    T5577_PWD |                      \
    (4 << T5577_MAXBLOCK_SHIFT))

#define T5577_HIDPROX_CONFIG ( \
    T5577_BITRATE_RF_50 |      \
    T5577_MODULATION_FSK2a |   \
    T5577_PWD |                \
    (3 << T5577_MAXBLOCK_SHIFT))

#define T5577_IOPROX_CONFIG ( \
    T5577_BITRATE_RF_64 |      \
    T5577_MODULATION_FSK2a |   \
    T5577_PWD |                \
    (2 << T5577_MAXBLOCK_SHIFT))

#define T5577_VIKING_CONFIG (     \
    T5577_BITRATE_RF_32 |         \
    T5577_MODULATION_MANCHESTER | \
    T5577_PWD |                   \
    (2 << T5577_MAXBLOCK_SHIFT))

#define T5577_PAC_CONFIG (        \
    T5577_MODULATION_DIRECT |     \
    T5577_BITRATE_RF_32 |         \
    T5577_PWD |                   \
    (4 << T5577_MAXBLOCK_SHIFT))

#define T5577_JABLOTRON_CONFIG (  \
    T5577_MODULATION_DIPHASE |    \
    T5577_BITRATE_RF_64 |         \
    T5577_PWD |                   \
    (2 << T5577_MAXBLOCK_SHIFT))

// Indala: PSK1 at RF/32, subcarrier = carrier/2 (RF_2), 2 data blocks (64-bit frame).
//
// ⭐ 0x00081040, confirmed against two independent sources: it is what Proxmark's
// `lf indala clone` writes (cmdlfindala.c) and what block 0 of a working bench tag reads
// back as. Verified end to end — `lf indala write` reads the credential back off the tag,
// 9 writes out of 9.
//
// ⚠ NOTE THE ABSENCE OF T5577_PWD, which every other config in this file sets. Bit 4 is
// the password-enable bit; setting it makes the tag demand a password for later writes.
// The Indala config proven on this bench does NOT set it, and `lf t55xx detect` reports
// "Password set...... No" for those tags. Whether the others should set it is a separate
// question and is NOT changed here — see NEXT.md.
#define T5577_INDALA_CONFIG (     \
    T5577_BITRATE_RF_32 |         \
    T5577_MODULATION_PSK1 |       \
    T5577_PSKCF_RF_2 |            \
    (2 << T5577_MAXBLOCK_SHIFT))

// Indala 224-bit: PSK2 at RF/32, subcarrier = carrier/2 (RF_2), SEVEN data blocks.
//
// ⛔⛔ PSK2, NOT PSK1 — AND THAT IS NOT A TYPO OF THE 64-BIT CONFIG ABOVE. A 224-bit Indala
// is written by the Proxmark's own `lf indala clone --224` as `000820E0`, and
// `lf t55xx detect` reports Modulation PSK2 against PSK1 for the 64-bit tag written in the
// same session (C99). Our own reader agrees from the other side: the Indala224 format
// decodes the DIFFERENTIAL view and only that one, because the direct view of a PSK2 tag
// is the running XOR of its data and no test inside one capture can tell the two apart
// (C107). ⇒ Writing this as PSK1 would produce a tag nothing on this bench can read.
//
// ⚠ SEVEN data blocks fills page 0 completely — blocks 1-7 — so there is no room for a
// password block, which is why T5577_PWD is absent here as it is for Indala26. Block 7 IS
// the last 32 bits of the frame.
#define T5577_INDALA224_CONFIG (  \
    T5577_BITRATE_RF_32 |         \
    T5577_MODULATION_PSK2 |       \
    T5577_PSKCF_RF_2 |            \
    (7 << T5577_MAXBLOCK_SHIFT))

// Keri: PSK1, subcarrier = carrier/2, 2 data blocks — but expressed in X_MODE with the
// dynamic bit-rate field rather than T5577_BITRATE_RF_32, which is what the Proxmark's
// `lf keri clone` writes and what `lf t55xx detect` reads back as PSK1 / RF/32. ⛔ Kept
// verbatim rather than rewritten into the shape its neighbours use: the value is copied
// from a working clone's own block dump, and the two forms are not known to be equivalent
// on this chip.
//   T5577_TESTMODE_DISABLED | T5577_X_MODE | PSK1 | PSKCF_RF_2 | (0xF << 18) | 2 blocks
#define T5577_KERI_CONFIG (0x603E1040)

// IDTECK: PSK1 at RF/32, subcarrier = carrier/2 (RF_2), 2 data blocks (64-bit frame).
#define T5577_IDTECK_CONFIG (     \
    T5577_BITRATE_RF_32 |         \
    T5577_MODULATION_PSK1 |       \
    T5577_PSKCF_RF_2 |            \
    T5577_PWD |                   \
    (2 << T5577_MAXBLOCK_SHIFT))

#if defined(PROJECT_CHAMELEON_ULTRA)
void t55xx_write_data(uint32_t passwd, uint32_t *blks, uint8_t blk_count);
void t55xx_reset_passwd(uint32_t old_passwd, uint32_t new_passwd);
void t55xx_send_cmd(uint8_t opcode, uint32_t *passwd, uint8_t data_len, uint32_t *data, uint8_t block);
#endif
#ifdef __cplusplus
}
#endif
