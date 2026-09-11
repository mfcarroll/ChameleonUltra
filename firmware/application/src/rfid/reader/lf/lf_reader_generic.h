#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Capture raw ADC samples from the LF antenna field.
 *
 * The SAADC samples at the PWM period rate (125kHz = 8µs/sample).
 * Each sample is an 8-bit value (14-bit ADC >> 5, clamped to 0xFF).
 * A steady carrier reads ~0x80-0x82; a gap reads noticeably lower.
 *
 * ⚠ 8-BIT MODE THROWS AWAY 5 BITS OF EVERY CONVERSION. The SAADC runs at 14-bit
 * (SAADC_CONFIG_RESOLUTION 3) and the protocol decoders receive that value intact --
 * lf_hidprox_data.c buffers nrf_saadc_value_t and hands decoder.feed() the raw word.
 * Only this debug path truncates, which made weak subcarriers look absent when they
 * were merely sub-LSB: a subcarrier measured at 0.25 LSB here is ~8 counts of the
 * real conversion. Pass raw16 to keep the full value when that distinction matters.
 *
 * @param data        Output buffer for raw samples
 * @param maxlen      Max BYTES to capture (max 4000 for USB frame limit). In raw16
 *                    mode a sample costs 2 bytes, so the capture spans half as long.
 * @param timeout_ms  Stop after this many ms even if buffer not full
 * @param outlen      Actual number of BYTES written
 * @param raw16       false: one byte per sample, 14-bit >> 5, clamped to 0xFF.
 *                    true:  two bytes per sample, big-endian, full 14-bit value.
 * @param settle_ms   Field-on time BEFORE the capture window, 0 = the historical 2ms.
 *                    Everything sampled during settle is discarded, so the capture
 *                    genuinely begins after it — otherwise the samples taken while
 *                    waiting sit at the head of the buffer and get returned first.
 *                    ⚠ A tag is not a signal generator: a T5577 charges off the field
 *                    before it transmits at full amplitude, so too short a settle
 *                    measures a tag that is not yet fully awake. 2ms has never been
 *                    varied, and every LF measurement on this device inherits it.
 * @return            true on success
 */
/** Maximum BYTES a single raw capture can return. Bounded by NETDATA_MAX_DATA_LENGTH.
 *
 * ⭐ 8192 bytes = 4096 samples at 16-bit = exactly two 64-bit Indala frames (2048 samples
 * each at RF/32). That is the point of the number: measured against Proxmark's PSKDemod,
 * demodulation needs >= 3584 samples and fails at 1.0 frames no matter how clean the
 * signal, because the 32-bit preamble can start anywhere in the capture.
 * ⚠ This EXCEEDS NETDATA_MAX_DATA_LENGTH, so a full capture cannot be returned in one
 * frame — cmd_processor_lf_sniff() hands it back in chunks. Raising the protocol cap
 * instead faults the device; see the note in netdata.h. */
#define LF_SNIFF_MAX_SAMPLES  8192

/** Bytes returned per response frame. The capture is sliced into chunks of this size. */
#define LF_SNIFF_CHUNK_BYTES  4000

bool raw_read_to_buffer(uint8_t *data, size_t maxlen, uint32_t timeout_ms, size_t *outlen,
                        bool raw16, uint16_t settle_ms);

/*
 * Capture raw 14-bit conversions into a caller's array, for decoders that work over a
 * whole buffer rather than sample by sample (lf_indala_data.c).
 *
 * Shares capture_begin()/capture_end() with raw_read_to_buffer, so the BLE suspend, the
 * batch-sized ring and the settle-window discard are the same code, not a second copy.
 *
 * ⚠ RETURNS FALSE ON A SHORT CAPTURE rather than reporting what it got. A partial buffer
 * is not a degraded Indala read but a failed one — the demodulator needs two whole frames
 * for one to be guaranteed to land inside the window — so a short capture can only turn
 * into a confident wrong answer.
 *
 * @param samples    output, `count` entries; values are 0..16383.
 * @param count      exact number of samples wanted.
 * @param timeout_ms give up after this long.
 * @param outlen     samples actually written.
 * @param settle_ms  field-on time before the window opens, 0 = the historical 2ms.
 * @return           true only if exactly `count` samples were captured.
 */
bool raw_read_samples(int16_t *samples, size_t count, uint32_t timeout_ms, size_t *outlen,
                      uint16_t settle_ms);
