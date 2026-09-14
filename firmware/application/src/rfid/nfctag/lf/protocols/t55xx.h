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

/* ⛔⛔ T5577_PWD IS NOT SET BY DEFAULT ANY MORE, AND THAT IS A BUG FIX (C322-C325).
 *
 * Every `T5577_*_CONFIG` below used to carry `T5577_PWD`, so **writing any tag silently enabled
 * password protection**. Worse, the key came from two different places: `chameleon_cmd.py`'s
 * module globals (`new_key = 20206666`) for hidprox and em410x, and a function default
 * (`51243648`) for indala, indala224, gproxii and awid. A tag locked by `lf hid prox write`
 * therefore could NOT be opened by `lf gproxii write`, or by the Proxmark, or by anything else —
 * and nothing documented either value. Two tags on this bench were unrecoverable until the
 * password was read off a third.
 *
 * ⇒ Password protection is now OPT-IN and carried by the caller's key: `write_t55xx()` ORs
 * `T5577_PWD` into block 0 only when a non-zero `new_passwd` was actually supplied. A plain
 * write leaves the tag open, which is what every other tool in this space does. */

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
    (2 << T5577_MAXBLOCK_SHIFT))

#define T5577_EM410X_ELECTRA_CONFIG ( \
    T5577_BITRATE_RF_64 |            \
    T5577_MODULATION_MANCHESTER |    \
    (4 << T5577_MAXBLOCK_SHIFT))

#define T5577_HIDPROX_CONFIG ( \
    T5577_BITRATE_RF_50 |      \
    T5577_MODULATION_FSK2a |   \
    (3 << T5577_MAXBLOCK_SHIFT))

#define T5577_IOPROX_CONFIG ( \
    T5577_BITRATE_RF_64 |      \
    T5577_MODULATION_FSK2a |   \
    (2 << T5577_MAXBLOCK_SHIFT))

#define T5577_VIKING_CONFIG (     \
    T5577_BITRATE_RF_32 |         \
    T5577_MODULATION_MANCHESTER | \
    (2 << T5577_MAXBLOCK_SHIFT))

#define T5577_PAC_CONFIG (        \
    T5577_MODULATION_DIRECT |     \
    T5577_BITRATE_RF_32 |         \
    (4 << T5577_MAXBLOCK_SHIFT))

#define T5577_JABLOTRON_CONFIG (  \
    T5577_MODULATION_DIPHASE |    \
    T5577_BITRATE_RF_64 |         \
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

// NexWatch: PSK1 at RF/32, subcarrier = carrier/2 (RF_2), THREE data blocks (96-bit frame).
//
// ⭐ MEASURED, not derived: a Proxmark `lf nexwatch clone --cn 12345678 -m 1 --nc` writes
// block 0 = `00081060` and `lf t55xx detect` reads it back as PSK1 / RF/32 (C164). That is
// Indala26's `00081040` with the block count raised 2 -> 3, which is exactly what a 96-bit
// frame needs and is the whole difference between the two at this layer.
//
// ⚠ T5577_PWD is absent, as it is for Indala26 and Indala224 — the tag the Proxmark writes
// reports "Password set...... No", and matching the reference clone is the point.
#define T5577_NEXWATCH_CONFIG (   \
    T5577_BITRATE_RF_32 |         \
    T5577_MODULATION_PSK1 |       \
    T5577_PSKCF_RF_2 |            \
    (3 << T5577_MAXBLOCK_SHIFT))

// Gallagher: ASK at RF/32, THREE data blocks (96-bit frame), sequence terminator SET.
//
// ⭐ MEASURED from a Proxmark `lf gallagher clone`: block 0 reads back `00088060` and
// `lf t55xx detect` reports **ASK, RF/32, Seq. terminator Yes** (C171). ⛔ The ST bit is what
// distinguishes this from every other config in this file — no PSK protocol here sets it —
// and it is kept because it is what the reference clone writes, not because its effect on
// our own reader has been measured.
//
// ⚠ Written as the literal the clone produces rather than composed from the flags above:
// T5577_MODULATION_ASK and the ST bit are not both expressed by the named constants here, and
// inventing a composition that happens to equal 0x00088060 would assert an equivalence this
// bench has not checked. Keri's config is kept verbatim for the same reason.
#define T5577_GALLAGHER_CONFIG (0x00088060)

// Securakey: ASK at RF/40, three data blocks (96-bit frame), sequence terminator SET.
//
// ⭐ MEASURED from a Proxmark `lf securakey clone`: block 0 `000C8060`, `lf t55xx detect`
// reports ASK / RF/40 / ST Yes (C175). ⛔ RF/40, not Gallagher's RF/32 — the bit rate is not
// a family constant, and the difference from `00088060` is exactly that field.
//
// ✅ THE FULL DUMP, recorded 2026-09-13 because C231 could not cover this writer without it:
//   `lf securakey clone --raw 7FCB400001ADEA5344300000` -> 000C8060 / 7FCB4000 01ADEA53 44300000
// The block form IS the air frame. ⚠ That had to be CHECKED rather than assumed — Keri's is
// three bits out of phase and cost a wrong credential 6 of 6 (C160). `ctest/roundtrip.c` pins it.
#define T5577_SECURAKEY_CONFIG (0x000C8060)

// Noralsy: ASK at RF/32, three data blocks (96-bit frame).
//
// ✅ THE FULL DUMP, recorded 2026-09-13 for the same reason as Securakey's above:
//   `lf noralsy clone --cn 112233 --y 2000` -> 00088068 / BB0214FF 01100022 33070000
// The block form IS the air frame here too, and `ctest/roundtrip.c` pins it.
//
// ⛔⛔ COPIED VERBATIM FROM A CLONE'S BLOCK DUMP, AND `lf t55xx detect` CANNOT VERIFY IT.
// The Proxmark writes `00088068` and then reports "Could not detect modulation automatically"
// on the tag it just created — so unlike every other config in this file, there is no second
// opinion available from `detect`. ⚠ It differs from Gallagher's `00088060` in one bit, and
// what that bit does is NOT established here; do not derive it from the named flags.
#define T5577_NORALSY_CONFIG (0x00088068)

// AWID and Paradox: FSK2a at RF/50, THREE data blocks (96-bit frame).
// Pyramid: the same, with FOUR data blocks (128-bit frame).
//
// ⭐ MEASURED from three Proxmark clones' own block dumps, and each read back by
// `lf t55xx detect` as FSK2a / RF/50: AWID `00107060`, Paradox `00107060`, Pyramid
// `00107080` (C202). They also agree with the Proxmark client's own header, which makes
// two independent sources for each word.
//
// ⚠ Composed from the named flags rather than kept as literals, because unlike Keri's and
// Gallagher's these compose EXACTLY: RF_50 | FSK2a | (n << MAXBLOCK_SHIFT) reproduces all
// three measured words bit for bit. Where a measured word cannot be composed that way it is
// kept verbatim instead — see Keri, Gallagher, Securakey and Noralsy above.
//
// ⚠ Note what is ABSENT: T5577_PWD. These are byte-for-byte T5577_HIDPROX_CONFIG without
// the password-enable bit (and, for Pyramid, one more data block) — the Proxmark's clones
// report "Password set...... No", and matching the reference clone is the point.
#define T5577_AWID_CONFIG (       \
    T5577_BITRATE_RF_50 |         \
    T5577_MODULATION_FSK2a |      \
    (3 << T5577_MAXBLOCK_SHIFT))

#define T5577_PARADOX_CONFIG (    \
    T5577_BITRATE_RF_50 |         \
    T5577_MODULATION_FSK2a |      \
    (3 << T5577_MAXBLOCK_SHIFT))

#define T5577_PYRAMID_CONFIG (    \
    T5577_BITRATE_RF_50 |         \
    T5577_MODULATION_FSK2a |      \
    (4 << T5577_MAXBLOCK_SHIFT))

// ⛔⛔ FDX-A IS **FSK2**, NOT FSK2a — the one bit that separates it from every other FSK
// protocol here, and C171's trap exactly: get it wrong and you write a tag nothing on this
// bench can read. MEASURED off `lf destron clone` (C339): block 0 reads back `00105060`
// against AWID/Paradox/Pyramid/HID's `00107060`, and the Proxmark's own `lf t55xx detect`
// prints `Modulation........ FSK2`. The modulation field (bits 16-12) is 00101 = 5 here and
// 00111 = 7 there.
#define T5577_FDXA_CONFIG (       \
    T5577_BITRATE_RF_50 |         \
    T5577_MODULATION_FSK2 |       \
    (3 << T5577_MAXBLOCK_SHIFT))

// GProxII: BIPHASE at RF/64, three data blocks (96-bit frame).
//
// ⭐ MEASURED from a Proxmark `lf gproxii clone --xor 141 --fmt 26 --fc 123 --cn 1337`: block 0
// reads back `00150060` and `lf t55xx detect` reports **BIPHASE / RF/64 / Inverted No / Seq.
// terminator No** (C204). Its blocks 1-3 are the air frame unrotated, like the FSK three and
// unlike Keri.
//
// ⚠ THE ONLY BIPHASE CONFIG HERE THAT IS NOT DIPHASE. Jablotron above uses
// T5577_MODULATION_DIPHASE (0x00018000); this is T5577_MODULATION_BIPHASE (0x00010000), a
// different field value for a different line coding, and the two are one bit apart. Composed
// from the named flags because it composes exactly: RF_64 | BIPHASE | (3 << MAXBLOCK_SHIFT).
//
// ⚠ T5577_PWD absent, as for every other measured clone here — `detect` reports
// "Password set...... No".
#define T5577_GPROXII_CONFIG (   \
    T5577_BITRATE_RF_64 |        \
    T5577_MODULATION_BIPHASE |   \
    (3 << T5577_MAXBLOCK_SHIFT))

// FDX-B (ISO 11784/11785): DIPHASE at RF/32, four data blocks (128-bit frame).
//
// ⛔⛔ `00098080`, AND THE PROXMARK'S OWN HEADER SAYS SOMETHING ELSE. `cmdlft55xx.h` carries
// `T55X7_FDXB_CONFIG_BLOCK 0x903F0082`, an X-mode value; what `lf fdxb clone` actually writes
// is `00098080`, and `lf t55xx detect` reads that back as **BIPHASEa (CDP) / RF/32 / Inverted
// Yes** (C214). ⇒ The clone's block dump wins over the header, which is the rule C202 set
// after the same disagreement on the FSK three.
//
// ⚠ DIPHASE, NOT BIPHASE — one bit apart, and GProxII next door uses the other one.
// 0x00018000 is T5577_MODULATION_DIPHASE and 0x00010000 is BIPHASE; composing this as BIPHASE
// would write a tag nothing here could read. Jablotron above is the only other DIPHASE config.
#define T5577_FDXB_CONFIG (       \
    T5577_BITRATE_RF_32 |         \
    T5577_MODULATION_DIPHASE |    \
    (4 << T5577_MAXBLOCK_SHIFT))

// IDTECK: PSK1 at RF/32, subcarrier = carrier/2 (RF_2), 2 data blocks (64-bit frame).
#define T5577_IDTECK_CONFIG (     \
    T5577_BITRATE_RF_32 |         \
    T5577_MODULATION_PSK1 |       \
    T5577_PSKCF_RF_2 |            \
    (2 << T5577_MAXBLOCK_SHIFT))

#if defined(PROJECT_CHAMELEON_ULTRA)
void t55xx_write_data(uint32_t passwd, uint32_t *blks, uint8_t blk_count);

/** One pass over `blks` with a single authentication choice; NULL = an open write. */
void t55xx_write_blocks(const uint32_t *passwd, uint32_t *blks, uint8_t blk_count);
void t55xx_reset_passwd(uint32_t old_passwd, uint32_t new_passwd);
/* ⛔ THE THIRD PARAMETER IS A LOCK BIT, NOT A LENGTH. This header called it `data_len` while
 * the implementation in lf_t55xx_data.c has always treated it as `lock_bit`, so a caller who
 * trusted the name and passed 32 would silently get "no lock bit" (any value other than 0 or 1
 * means exactly that). Renamed to match the code 2026-09-15; no behaviour changed. */
void t55xx_send_cmd(uint8_t opcode, uint32_t *passwd, uint8_t lock_bit, uint32_t *data, uint8_t block);
#endif
#ifdef __cplusplus
}
#endif
