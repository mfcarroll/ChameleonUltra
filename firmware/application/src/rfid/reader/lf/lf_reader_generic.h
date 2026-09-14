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
 * ⭐ SIZED FOR TWO WHOLE FRAMES OF THE LONGEST FORMAT, because one frame is never enough at
 * any SNR: the preamble can start anywhere in the capture, so a one-frame buffer contains a
 * complete frame for only one starting phase in `frame samples`. 28672 bytes = 14336 samples
 * at 16-bit = two 224-bit Indala frames at RF/32.
 *
 * ⚠ IT WAS 8192 BYTES, WHICH WAS TWO 64-BIT FRAMES AND EXACTLY RIGHT UNTIL INDALA224. A
 * 224-bit frame is 7168 samples, so the old buffer held less than one of them — and that made
 * the host tooling blind to the very decode that needed it, since `mfdemod.py` and `ctest`
 * can only see what a sniff returns. The device could hear the tag; nothing could look at it.
 *
 * ⛔ THE NAME SAID SAMPLES AND THE NUMBER WAS BYTES. At 16-bit those differ by two, and the
 * mistake is invisible until a format needs a specific sample count. Renamed.
 *
 * ⚠ This EXCEEDS NETDATA_MAX_DATA_LENGTH many times over, so a full capture cannot be
 * returned in one frame — cmd_processor_lf_sniff() hands it back in chunks of
 * LF_SNIFF_CHUNK_BYTES and the host reassembles. Raising the protocol cap instead faults the
 * device; see the note in netdata.h. 28672 bytes is 8 chunks, and the host loops to 16. */
#define LF_SNIFF_MAX_BYTES  28672

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
/** Samples the LAST capture dropped. Non-zero means the buffer is SPLICED, not merely short —
 *  see the note in lf_reader_generic.c. Valid after raw_read_samples/raw_read_to_buffer. */
uint32_t lf_capture_dropped(void);

bool raw_read_samples(int16_t *samples, size_t count, uint32_t timeout_ms, size_t *outlen,
                      uint16_t settle_ms);

/** ⭐⭐⭐ THE ONE LF CAPTURE BUFFER — 28,672 bytes, shared, and it used to be two.
 *
 * ⛔ This image carried TWO of these: `m_samples` in `lf_indala_data.c` for the sampled readers
 * and `sniff_buf` in `app_cmd.c` for `lf sniff`. Both static, both 0x7000, both resident for the
 * life of the firmware — **57,344 bytes, 22% of the nRF52840's 256 KB**, for two buffers that
 * can never be in use at once because the device dispatches one command at a time (C317).
 *
 * ⭐ It lives HERE rather than in either caller, and that is the point: `lf sniff` is a generic
 * LF feature and `lf_indala_data.c` is a protocol reader, so letting the sniff handler reach
 * into the reader's static would invert the dependency. This file already owns
 * `capture_begin()` and `raw_read_to_buffer()`, so it is where a capture buffer belongs.
 *
 * ⚠ THE CONTRACT, WHICH IS NOW SHARED: whatever captured last owns the contents. A scan
 * between two chunk fetches destroys the earlier capture — already true of command 3060 and of
 * `lf sniff`'s own chunking, and now true ACROSS them as well. */
#define LF_CAPTURE_BUF_BYTES  LF_SNIFF_MAX_BYTES
int16_t *lf_capture_buffer(void);

/** Called once with the FIELD ALREADY UP and settled, immediately before the sample window
 *  opens. ⭐ This exists so a downlink command can be transmitted INTO a live capture — a
 *  T5577 regular-read only answers while the field it was addressed on stays up, so sending
 *  the command and then starting a capture cannot work: `capture_begin()` raises the field
 *  itself and the tag would have been reset in between (C308). */
typedef void (*lf_capture_probe_fn)(void *arg);

/** `raw_read_samples()` with an optional probe fired between the settle and the window.
 *  Passing NULL is exactly `raw_read_samples()`, which is implemented in terms of this. */
bool raw_read_samples_probe(int16_t *samples, size_t count, uint32_t timeout_ms, size_t *outlen,
                            uint16_t settle_ms, lf_capture_probe_fn probe, void *arg);
