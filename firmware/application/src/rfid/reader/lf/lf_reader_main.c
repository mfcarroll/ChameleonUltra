#include "lf_reader_main.h"

#include <stdbool.h>
#include "bsp_delay.h"
#include "bsp_time.h"
#include "hex_utils.h"
#include "lf_125khz_radio.h"
#include "lf_reader_data.h"
#include "protocols/em410x.h"
#include "protocols/ioprox.h"
#include "lf_indala_data.h"
#include "lf_indala_psk.h"
#include "protocols/hidprox.h"
#include "protocols/idteck.h"
#include "protocols/indala.h"
#include "protocols/t55xx.h"
#include "protocols/jablotron.h"
#include "protocols/keri.h"
#include "protocols/nexwatch.h"
#include "protocols/gallagher.h"
#include "protocols/securakey.h"
#include "protocols/noralsy.h"
#include "protocols/fsk2a_t55xx.h"
#include "lf_ask_biphase.h"
#include "lf_fsk2a.h"
#include "protocols/pac.h"
#include "protocols/viking.h"

#define NRF_LOG_MODULE_NAME lf_main
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
NRF_LOG_MODULE_REGISTER();

// The default card search is available N Millisecond timeout
static uint32_t g_timeout_readem_ms = 500;

/**
 * Search EM410X tag
 */
uint8_t scan_em410x(uint8_t *uid) {
    if (em410x_read(uid, g_timeout_readem_ms)) {
        return STATUS_LF_TAG_OK;
    }
    return STATUS_LF_TAG_NO_FOUND;
}

/**
 * Search HID Prox tag
 */
uint8_t scan_hidprox(uint8_t *data, uint8_t format_hint) {
    if (hidprox_read(data, format_hint, g_timeout_readem_ms)) {
        return STATUS_LF_TAG_OK;
    }
    return STATUS_LF_TAG_NO_FOUND;
}

/**
 * @brief Search Indala tag (PSK1, RF/32, fc/2 subcarrier)
 *
 * ⚠ UNLIKE EVERY OTHER LF SCAN HERE, this one is not a streaming edge decoder — it takes
 * whole 4096-sample SAADC captures and demodulates them. It therefore needs a bigger time
 * budget than the 500ms default: a capture is ~35ms and it insists on two that agree, so
 * 500ms buys ~14 attempts against a measured 95th percentile of 5.
 *
 * @param data INDALA_READ_DATA_SIZE bytes; see lf_indala_data.h for the layout
 * @return STATUS_LF_TAG_OK on success
 */
/**
 * ⭐ SHARED BY EVERY PSK1 READER, WHICH IS THE WHOLE POINT OF ITS BEING HERE rather than
 * inside the Indala scan. IDTECK is the same physical layer — 64-bit PSK1 at RF/32 on an
 * fc/2 subcarrier — so a reader for it inherits the same inability to demodulate a source
 * whose subcarrier is not locked to our carrier, and must report it the same way.
 *
 * ⛔ The status names the MEASUREMENT, not the diagnosis. "fc/2 energy present, no frame
 * recovered" is what the device observed. "It is an emulator" is the likely cause and
 * belongs in the host's message, where a detuned or damaged real tag can be named beside it.
 */
static inline uint8_t lf_psk1_failure_status(int32_t energy) {
    return (energy >= INDALA_PSK_ENERGY_PRESENT) ? STATUS_LF_SIGNAL_NOT_DECODED
                                                 : STATUS_LF_TAG_NO_FOUND;
}

uint8_t scan_indala(uint8_t *data) {
    /* ⚠ NOT g_timeout_readem_ms. 500ms was right when a read was 2-4 captures; stacking
     * spends up to 8 per sample phase to buy sqrt(N) of signal, and the tags that NEED
     * stacking are exactly the ones that will use the whole budget. A tag that reads in
     * 100ms still returns in 100ms — this only changes how long a hard one is given
     * before being called absent. */
    int32_t energy = 0;
    if (indala_read(data, INDALA_READ_TIMEOUT_MS, &energy)) {
        return STATUS_LF_TAG_OK;
    }
    return lf_psk1_failure_status(energy);
}

/**
 * @brief Search Indala 224-bit tag
 *
 * ⚠ Slower than the 64-bit scans by construction: a 224-bit frame needs a 14336-sample
 * capture, 114ms against 33ms, so the same budget buys about a quarter of the attempts.
 *
 * @param data INDALA224_READ_DATA_SIZE bytes; see lf_indala_data.h for the layout
 * @return STATUS_LF_TAG_OK on success
 */
uint8_t scan_indala224(uint8_t *data) {
    int32_t energy = 0;
    if (indala224_read(data, INDALA_READ_TIMEOUT_MS, &energy)) {
        return STATUS_LF_TAG_OK;
    }
    return lf_psk1_failure_status(energy);
}

/**
 * @brief Search IDTECK tag
 *
 * Same budget and same failure reporting as Indala — it is the same demodulation against a
 * different preamble, so a marginal read costs the same captures.
 *
 * @param data IDTECK_READ_DATA_SIZE bytes; see lf_indala_data.h for the layout
 * @return STATUS_LF_TAG_OK on success
 */
uint8_t scan_idteck(uint8_t *data) {
    int32_t energy = 0;
    if (idteck_read(data, INDALA_READ_TIMEOUT_MS, &energy)) {
        return STATUS_LF_TAG_OK;
    }
    return lf_psk1_failure_status(energy);
}

/* ⚠ Same status convention as the other PSK1 readers: `lf_psk1_failure_status` turns
 * "energy present but nothing decoded" into 0x43 rather than "not found", which is the
 * signal to reposition (C89). */
uint8_t scan_keri(uint8_t *data) {
    int32_t energy = 0;
    if (keri_read(data, INDALA_READ_TIMEOUT_MS, &energy)) {
        return STATUS_LF_TAG_OK;
    }
    return lf_psk1_failure_status(energy);
}

/**
 * @brief Search NexWatch tag
 * @param output NEXWATCH_READ_DATA_SIZE bytes: the 12-byte frame, the descrambled card
 *               number, the inferred magic byte, the mode, phase and offset
 * @return STATUS_LF_TAG_OK on success
 */
uint8_t scan_gallagher(uint8_t *data) {
    int32_t energy = 0;
    if (gallagher_read(data, INDALA_READ_TIMEOUT_MS, &energy)) {
        return STATUS_LF_TAG_OK;
    }
    /* ⚠ NOT `lf_psk1_failure_status` — that maps the PSK decoder's `energy`, which is a bit
     * integrator amplitude, against INDALA_PSK_ENERGY_PRESENT. The ASK decoder reports a
     * Manchester-violation percentage instead, a different quantity on a different scale, and
     * passing it to that mapping would produce a confident and meaningless hint. */
    return STATUS_LF_TAG_NO_FOUND;
}

uint8_t scan_fdxa(uint8_t *data) {
    int32_t energy = 0;
    if (fdxa_read(data, INDALA_READ_TIMEOUT_MS, &energy)) {
        return STATUS_LF_TAG_OK;
    }
    return STATUS_LF_TAG_NO_FOUND;
}

uint8_t scan_paradox(uint8_t *data) {
    int32_t energy = 0;
    if (paradox_read(data, INDALA_READ_TIMEOUT_MS, &energy)) {
        return STATUS_LF_TAG_OK;
    }
    return STATUS_LF_TAG_NO_FOUND;
}

uint8_t scan_pyramid(uint8_t *data) {
    int32_t energy = 0;
    if (pyramid_read(data, INDALA_READ_TIMEOUT_MS, &energy)) {
        return STATUS_LF_TAG_OK;
    }
    return STATUS_LF_TAG_NO_FOUND;
}

uint8_t scan_fdxb(uint8_t *data) {
    int32_t energy = 0;
    if (fdxb_read(data, INDALA_READ_TIMEOUT_MS, &energy)) {
        return STATUS_LF_TAG_OK;
    }
    return STATUS_LF_TAG_NO_FOUND;
}

uint8_t scan_gproxii(uint8_t *data) {
    int32_t energy = 0;
    return scan_gproxii_energy(data, &energy);
}

/* ⚠ The energy-reporting variant — see the note on cmd_processor_gproxii_scan. */
uint8_t scan_gproxii_energy(uint8_t *data, int32_t *energy_out) {
    int32_t energy = 0;
    /* ⚠ SIX SECONDS, NOT THE SHARED THREE — and that is a measured cost, not a shrug. See
     * GPROXII_READ_TIMEOUT_MS for the arithmetic: a 114ms capture and eight tries across
     * several sample phases does not fit in three. */
    bool got = gproxii_read(data, GPROXII_READ_TIMEOUT_MS, &energy);
    if (energy_out != NULL) {
        *energy_out = energy;
    }
    if (got) {
        return STATUS_LF_TAG_OK;
    }
    /* ⚠ NOT `lf_psk1_failure_status`, for the same reason scan_gallagher gives — and more
     * strongly here. The biphase decoder's `energy` is the mean bit-boundary STEP in ADC
     * counts, which is neither the PSK integrator amplitude that mapping expects nor the
     * violation percentage the ASK decoder reports. Three decoders, three different
     * quantities; only the PSK one has a calibrated threshold. */
    if (energy_out != NULL) {
        *energy_out = energy;
    }
    return STATUS_LF_TAG_NO_FOUND;
}

uint8_t scan_awid(uint8_t *data) {
    int32_t energy = 0;
    if (awid_read(data, INDALA_READ_TIMEOUT_MS, &energy)) {
        return STATUS_LF_TAG_OK;
    }
    return STATUS_LF_TAG_NO_FOUND;
}

uint8_t scan_instafob(uint8_t *data) {
    int32_t energy = 0;
    if (instafob_read(data, INDALA_READ_TIMEOUT_MS, &energy)) {
        return STATUS_LF_TAG_OK;
    }
    return STATUS_LF_TAG_NO_FOUND;
}

uint8_t scan_noralsy(uint8_t *data) {
    int32_t energy = 0;
    if (noralsy_read(data, INDALA_READ_TIMEOUT_MS, &energy)) {
        return STATUS_LF_TAG_OK;
    }
    return STATUS_LF_TAG_NO_FOUND;
}

uint8_t scan_securakey(uint8_t *data) {
    int32_t energy = 0;
    if (securakey_read(data, INDALA_READ_TIMEOUT_MS, &energy)) {
        return STATUS_LF_TAG_OK;
    }
    /* ⚠ Not `lf_psk1_failure_status` — see the note in scan_gallagher. */
    return STATUS_LF_TAG_NO_FOUND;
}

uint8_t scan_nexwatch(uint8_t *data) {
    int32_t energy = 0;
    if (nexwatch_read(data, INDALA_READ_TIMEOUT_MS, &energy)) {
        return STATUS_LF_TAG_OK;
    }
    return lf_psk1_failure_status(energy);
}

/**
 * @brief Search ioProx tag
 * @param output 16 bytes ioprox_codec_t->data layout: version, facility code, card number, raw8
 * @return STATUS_LF_TAG_OK on success
 */
uint8_t scan_ioprox(uint8_t *data, uint8_t format_hint) {
    if (ioprox_read(data, format_hint, g_timeout_readem_ms)) {
        return STATUS_LF_TAG_OK;
    }
    return STATUS_LF_TAG_NO_FOUND;
}

/**
 * @brief Decode raw8 data to structured ioProx format
 * @param raw8 Input 8 bytes
 * @param output 16 bytes ioprox_codec_t->data layout: version, facility code, card number, raw8
 * @return STATUS_SUCCESS on success
 */
uint8_t decode_ioprox_raw(uint8_t *raw8, uint8_t *output) {
    if (ioprox_decode_raw_to_data(raw8, output)) {
        return STATUS_SUCCESS;
    }
    return STATUS_CMD_ERR;
}

/**
 * @brief Encode ioProx parameters to structured ioProx format
 * @param ver Version byte
 * @param fc Facility code byte
 * @param cn Card number (16-bit)
 * @param out 16 bytes ioprox_codec_t->data layout: version, facility code, card number, raw8
 * @return STATUS_SUCCESS on success
 */
uint8_t encode_ioprox_params(uint8_t ver, uint8_t fc, uint16_t cn, uint8_t *out) {
    if (ioprox_encode_params_to_data(ver, fc, cn, out)) {
        return STATUS_SUCCESS;
    }
    return STATUS_CMD_ERR;
}

/**
 * Search PAC/Stanley tag
 */
uint8_t scan_pac(uint8_t *card_id) {
    if (pac_read(card_id, g_timeout_readem_ms)) {
        return STATUS_LF_TAG_OK;
    }
    return STATUS_LF_TAG_NO_FOUND;
}

/**
 * Search Viking tag
 */
uint8_t scan_viking(uint8_t *uid) {
    if (viking_read(uid, g_timeout_readem_ms)) {
        return STATUS_LF_TAG_OK;
    }
    return STATUS_LF_TAG_NO_FOUND;
}

/**
 * Search Jablotron tag
 */
uint8_t scan_jablotron(uint8_t *uid) {
    if (jablotron_read(uid, g_timeout_readem_ms)) {
        return STATUS_LF_TAG_OK;
    }
    return STATUS_LF_TAG_NO_FOUND;
}

/**
 * Try reset t55XX tag passwords by enumerating old passwords.
 */
static void try_reset_t55xx_passwd(uint32_t new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count) {
    for (uint8_t i = 0; i < old_passwd_count; i++) {
        uint32_t old_passwd = bytes_to_num(old_passwds + i * 4, 4);
        t55xx_reset_passwd(old_passwd, new_passwd);
    }
    t55xx_reset_passwd(new_passwd, new_passwd);
}

/**
 * Write card data to t55xx
 */
static uint8_t write_t55xx(uint32_t *blks, uint8_t blk_count, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count) {
    uint32_t passwd = bytes_to_num(new_passwd, 4);

    start_lf_125khz_radio();
    bsp_delay_ms(1);  // Delays for a while after starting the field

    try_reset_t55xx_passwd(passwd, old_passwds, old_passwd_count);
    t55xx_write_data(passwd, blks, blk_count);

    stop_lf_125khz_radio();

    // writing results should be verified by upper computer
    return STATUS_LF_TAG_OK;
}

/**
 * Write em410x card data to t55xx
 */
uint8_t write_em410x_to_t55xx(uint8_t *uid, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count) {
    uint32_t blks[7] = {0x00};
    uint8_t blk_count = em410x_t55xx_writer(uid, blks);
    if (blk_count == 0) {
        return STATUS_PAR_ERR;
    }
    return write_t55xx(blks, blk_count, new_passwd, old_passwds, old_passwd_count);
}

uint8_t write_em410x_electra_to_t55xx(uint8_t *uid, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count) {
    uint32_t blks[7] = {0x00};
    uint8_t blk_count = em410x_electra_t55xx_writer(uid, blks);
    if (blk_count == 0) {
        return STATUS_PAR_ERR;
    }
    return write_t55xx(blks, blk_count, new_passwd, old_passwds, old_passwd_count);
}

/**
 * Write hidprox card data to t55xx
 */
uint8_t write_hidprox_to_t55xx(uint8_t format, uint32_t fc, uint64_t cn, uint32_t il, uint32_t oem, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count) {
    wiegand_card_t card = {
        .format = format,
        .card_number = cn,
        .facility_code = fc,
        .issue_level = il,
        .oem = oem,
    };
    uint32_t blks[7] = {0x00};
    uint8_t blk_count = hidprox_t55xx_writer(&card, blks);
    if (blk_count == 0) {
        return STATUS_PAR_ERR;
    }
    return write_t55xx(blks, blk_count, new_passwd, old_passwds, old_passwd_count);
}

/**
 * Write ioprox card data to t55xx
 */
uint8_t write_ioprox_to_t55xx(uint8_t *card_data, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count) {
    // Prepare T5577 block array: index 0 = config word, 1-2 = data blocks
    uint32_t blks[3] = {0x00};

    uint8_t blk_count = ioprox_t55xx_writer(card_data, blks);

    if (blk_count == 0) {
        return STATUS_PAR_ERR;
    }

    return write_t55xx(blks, blk_count, new_passwd, old_passwds, old_passwd_count);
}

/**
 * Write viking card data to t55xx
 */
uint8_t write_viking_to_t55xx(uint8_t *uid, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count) {
    uint32_t blks[7] = {0x00};
    uint8_t blk_count = viking_t55xx_writer(uid, blks);
    if (blk_count == 0) {
        return STATUS_PAR_ERR;
    }
    return write_t55xx(blks, blk_count, new_passwd, old_passwds, old_passwd_count);
}

uint8_t write_pac_to_t55xx(uint8_t *data, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count) {
    uint32_t blks[7] = {0x00};
    uint8_t blk_count = pac_t55xx_writer(data, blks);
    if (blk_count == 0) return STATUS_PAR_ERR;
    return write_t55xx(blks, blk_count, new_passwd, old_passwds, old_passwd_count);
}

/**
 * Write jablotron card data to t55xx
 */
uint8_t write_jablotron_to_t55xx(uint8_t *uid, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count) {
    uint32_t blks[7] = {0x00};
    uint8_t blk_count = jablotron_t55xx_writer(uid, blks);
    if (blk_count == 0) {
        return STATUS_PAR_ERR;
    }
    return write_t55xx(blks, blk_count, new_passwd, old_passwds, old_passwd_count);
}

/**
 * Write IDTECK card data to t55xx (PSK1 RF/32, 64-bit frame).
 */
uint8_t write_idteck_to_t55xx(uint8_t *data, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count) {
    uint32_t blks[7] = {0x00};
    uint8_t blk_count = idteck_t55xx_writer(data, blks);
    if (blk_count == 0) return STATUS_PAR_ERR;
    return write_t55xx(blks, blk_count, new_passwd, old_passwds, old_passwd_count);
}

/**
 * @brief Write a raw 64-bit Indala frame to a T55xx tag (PSK1, RF/32, 2 data blocks).
 *
 * ⭐ THIS GOES THROUGH write_t55xx() AND THAT IS THE ENTIRE POINT. The obvious alternative
 * is lf_t55xx_write_block() three times, and it DOES NOT RELIABLY WORK: measured on a real
 * tag, one of three such writes landed. The raw single-block path starts the field, waits
 * 1ms, sends the block once, resets, and stops the field — per block — so every block is
 * written to a tag charging from cold, once, with no second attempt. write_t55xx() holds
 * the field on across all blocks and t55xx_write_data() sends each one TWICE (password
 * write then open write), which is why every protocol writer on this device uses it.
 *
 * ⚠ A T5577 SENDS NO ACKNOWLEDGEMENT, so this returns STATUS_LF_TAG_OK regardless, exactly
 * as every other writer here does. It reports what was transmitted, not what landed. Read
 * the tag back before believing it.
 *
 * @param raw8 8 bytes, the 64-bit frame big-endian
 * @return STATUS_LF_TAG_OK
 */
uint8_t write_indala_to_t55xx(uint8_t *raw8, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count) {
    /* ⭐ The block layout and the config word live in indala.c with every other protocol's,
     * rather than as a literal here. They were a literal `0x00081040` until Indala224 needed
     * a second config word and the choice was one copy or two. */
    uint32_t blks[7] = {0x00};
    uint8_t blk_count = indala_t55xx_writer(raw8, blks);
    if (blk_count == 0) {
        return STATUS_PAR_ERR;
    }
    return write_t55xx(blks, blk_count, new_passwd, old_passwds, old_passwd_count);
}

/**
 * @brief Write a raw 224-bit Indala frame to a T55xx tag (PSK2, RF/32, 7 data blocks).
 *
 * ⛔ EIGHT BLOCKS, NOT SEVEN — `blks` here is one longer than every other writer in this
 * file. Block 0 is the config and blocks 1-7 the frame, which is the whole of page 0. The
 * `uint32_t blks[7]` those writers declare is sized for a config plus six data words and
 * would overflow by one here.
 *
 * ⚠ A 224-bit frame leaves no password block, so this tag cannot be password-protected —
 * block 7 is the last 32 bits of the credential. `write_t55xx()` still runs its password
 * reset first, and the data write that follows overwrites whatever that left in block 7.
 *
 * ⚠ A T5577 SENDS NO ACKNOWLEDGEMENT — returns STATUS_LF_TAG_OK regardless, exactly as
 * every other writer here does. Read the tag back before believing it.
 *
 * @param raw28 28 bytes, the 224-bit frame big-endian
 * @return STATUS_LF_TAG_OK
 */
uint8_t write_indala224_to_t55xx(uint8_t *raw28, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count) {
    uint32_t blks[8] = {0x00};
    uint8_t blk_count = indala224_t55xx_writer(raw28, blks);
    if (blk_count == 0) {
        return STATUS_PAR_ERR;
    }
    return write_t55xx(blks, blk_count, new_passwd, old_passwds, old_passwd_count);
}

/**
 * @brief Write a raw 64-bit Keri frame to a T55xx tag (PSK1, RF/32, 2 data blocks).
 *
 * ⛔ `frame8` is the AIR frame — E0000000 followed by the internal id — not the block
 * contents. keri_t55xx_writer() rotates it into the form a T5577 clocks out. See the long
 * note there for why the two differ.
 *
 * ⚠ A T5577 SENDS NO ACKNOWLEDGEMENT — returns STATUS_LF_TAG_OK regardless. Read it back.
 */
uint8_t write_keri_to_t55xx(uint8_t *frame8, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count) {
    uint32_t blks[7] = {0x00};
    uint8_t blk_count = keri_t55xx_writer(frame8, blks);
    if (blk_count == 0) {
        return STATUS_PAR_ERR;
    }
    return write_t55xx(blks, blk_count, new_passwd, old_passwds, old_passwd_count);
}

uint8_t write_noralsy_to_t55xx(uint8_t *frame12, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count) {
    uint32_t blks[4] = {0x00};
    uint8_t blk_count = noralsy_t55xx_writer(frame12, blks);
    if (blk_count == 0) {
        return STATUS_PAR_ERR;
    }
    return write_t55xx(blks, blk_count, new_passwd, old_passwds, old_passwd_count);
}

uint8_t write_securakey_to_t55xx(uint8_t *frame12, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count) {
    uint32_t blks[4] = {0x00};
    uint8_t blk_count = securakey_t55xx_writer(frame12, blks);
    if (blk_count == 0) {
        return STATUS_PAR_ERR;
    }
    return write_t55xx(blks, blk_count, new_passwd, old_passwds, old_passwd_count);
}

uint8_t write_gallagher_to_t55xx(uint8_t *frame12, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count) {
    uint32_t blks[4] = {0x00};
    uint8_t blk_count = gallagher_t55xx_writer(frame12, blks);
    if (blk_count == 0) {
        return STATUS_PAR_ERR;
    }
    return write_t55xx(blks, blk_count, new_passwd, old_passwds, old_passwd_count);
}

uint8_t write_nexwatch_to_t55xx(uint8_t *frame12, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count) {
    uint32_t blks[4] = {0x00};
    uint8_t blk_count = nexwatch_t55xx_writer(frame12, blks);
    if (blk_count == 0) {
        return STATUS_PAR_ERR;
    }
    return write_t55xx(blks, blk_count, new_passwd, old_passwds, old_passwd_count);
}

/**
 * @brief Write a raw 96-bit AWID frame to a T55xx tag (FSK2a, RF/50, 3 data blocks).
 *
 * ⚠ A T5577 SENDS NO ACKNOWLEDGEMENT — returns STATUS_LF_TAG_OK regardless. Read it back.
 */
uint8_t write_awid_to_t55xx(uint8_t *frame12, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count) {
    uint32_t blks[4] = {0x00};
    uint8_t blk_count = fsk2a_t55xx_blocks(frame12, 3, T5577_AWID_CONFIG, blks);
    return write_t55xx(blks, blk_count, new_passwd, old_passwds, old_passwd_count);
}

/**
 * @brief Write a raw 96-bit Paradox frame to a T55xx tag (FSK2a, RF/50, 3 data blocks).
 *
 * ⚠ Identical to AWID's at this layer — the two share a config word exactly (C202). They
 * differ in preamble and payload layout, which is the decoder's business, not the writer's.
 */
uint8_t write_paradox_to_t55xx(uint8_t *frame12, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count) {
    uint32_t blks[4] = {0x00};
    uint8_t blk_count = fsk2a_t55xx_blocks(frame12, 3, T5577_PARADOX_CONFIG, blks);
    return write_t55xx(blks, blk_count, new_passwd, old_passwds, old_passwd_count);
}

/**
 * @brief Write a raw 128-bit Pyramid frame to a T55xx tag (FSK2a, RF/50, 4 data blocks).
 *
 * ⚠ FOUR data blocks, not three — Pyramid's frame is 128 bits, and the block-count field is
 * the only thing its config word does not share with AWID's and Paradox's.
 */
uint8_t write_pyramid_to_t55xx(uint8_t *frame16, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count) {
    uint32_t blks[5] = {0x00};
    uint8_t blk_count = fsk2a_t55xx_blocks(frame16, 4, T5577_PYRAMID_CONFIG, blks);
    return write_t55xx(blks, blk_count, new_passwd, old_passwds, old_passwd_count);
}

/**
 * @brief Write a raw 128-bit FDX-B frame to a T55xx tag (DIPHASE, RF/32, 4 data blocks).
 *
 * ⚠ The block form is the air frame unrotated, measured from the reference clone's dump
 * (C214). Shares fsk2a_t55xx_blocks() because the transcription is identical — the function is
 * named for where it came from, not for the only family allowed to use it.
 *
 * ⚠ A T5577 SENDS NO ACKNOWLEDGEMENT — returns STATUS_LF_TAG_OK regardless. Read it back.
 */
uint8_t write_fdxb_to_t55xx(uint8_t *frame16, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count) {
    uint32_t blks[5] = {0x00};
    uint8_t blk_count = fsk2a_t55xx_blocks(frame16, 4, T5577_FDXB_CONFIG, blks);
    return write_t55xx(blks, blk_count, new_passwd, old_passwds, old_passwd_count);
}

/**
 * @brief Write a raw 96-bit GProxII frame to a T55xx tag (BIPHASE, RF/64, 3 data blocks).
 *
 * ⚠ The block form is the air frame unrotated — measured from the reference clone's own dump
 * (C204), the same question Keri answers differently (C160). Shares fsk2a_t55xx_blocks()
 * because the transcription is identical; only the config word differs, and the function is
 * named for where it came from rather than for the only family allowed to use it.
 *
 * ⚠ A T5577 SENDS NO ACKNOWLEDGEMENT — returns STATUS_LF_TAG_OK regardless. Read it back.
 */
uint8_t write_gproxii_to_t55xx(uint8_t *frame12, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count) {
    uint32_t blks[4] = {0x00};
    uint8_t blk_count = fsk2a_t55xx_blocks(frame12, 3, T5577_GPROXII_CONFIG, blks);
    return write_t55xx(blks, blk_count, new_passwd, old_passwds, old_passwd_count);
}

/* ⛔ THERE IS DELIBERATELY NO write_fdxa_to_t55xx, and that is a refusal rather than a
 * deferral. The Proxmark has no FDX-A clone — `lf fdx` is FDX-B, a different protocol — so a
 * tag we wrote could only be read back by our own reader, which is the self-certification that
 * put a wrong `idteck.c` upstream and that this project exists downstream of (C185). InstaFob
 * is withheld for the same reason. ⇒ Ship it when something here can read it, not before. */

/**
 * Set the LF card scanning timeout value (in milliseconds).
 */
void set_scan_tag_timeout(uint32_t ms) { g_timeout_readem_ms = ms; }

#if defined(PROJECT_CHAMELEON_ULTRA)
/**
 * Write a single raw 32-bit word to a T55xx block.
 *
 * Unlike write_em410x_to_t55xx() and friends, this writes the exact word
 * supplied with no protocol encoding — useful for custom configuration
 * words, recovery of locked tags, or scripted programming.
 *
 * Only available on Chameleon Ultra (Lite has no LF writer hardware).
 *
 * @param block      Block number (0-7 for page 0, 0-3 for page 1)
 * @param word       32-bit data word to write
 * @param passwd     Password for password-protected write (ignored when use_passwd is false)
 * @param use_passwd true = password-protected write, false = open write
 * @param page1      true = target page 1, false = page 0
 * @return           STATUS_LF_TAG_OK always (T55xx gives no ACK; verify by reading back)
 */
uint8_t lf_t55xx_write_block(uint8_t block, uint32_t word, uint32_t passwd, bool use_passwd, bool page1) {
    uint8_t opcode = page1 ? T5577_OPCODE_PAGE1 : T5577_OPCODE_PAGE0;
    uint32_t *pwd_ptr = use_passwd ? &passwd : NULL;

    start_lf_125khz_radio();
    bsp_delay_ms(1);  // Delay for a while after starting the field

    t55xx_send_cmd(opcode, pwd_ptr, 0, &word, block);
    t55xx_send_cmd(T5577_OPCODE_RESET, NULL, 0, NULL, 0);

    stop_lf_125khz_radio();
    return STATUS_LF_TAG_OK;
}
#endif
