#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Capture raw ADC samples from the LF antenna with the reader field on.
 *
 * By default the SAADC samples once per carrier period (125kHz, 8us per sample).
 * lf_125khz_radio_saadc_phase_set() and lf_125khz_radio_saadc_rate_set() change that, and
 * rate_khz must match the rate set, so the settle time skips the right number of samples.
 * An 8-bit sample is the 14-bit conversion >> 5, clamped to 0xFF. A 16-bit sample is the
 * whole conversion, big-endian. A steady carrier reads about 0x80 in 8 bits.
 *
 * @param data        Output buffer for raw samples
 * @param maxlen      Bytes to capture
 * @param timeout_ms  Stop after this many ms even if the buffer is not full
 * @param outlen      Bytes written
 * @param raw16       Two bytes per sample instead of one
 * @param settle_ms   Field-on time before the first kept sample (at least 2ms)
 * @param rate_khz    The sample rate set, or 0 for carrier-locked sampling
 * @return            false if the sample ring could not be allocated
 */
/* The largest capture. The host fetches it in frames of LF_SNIFF_CHUNK_BYTES, because a
 * frame carries at most 4096 bytes. */
#define LF_SNIFF_MAX_BYTES    28672
#define LF_SNIFF_CHUNK_BYTES  4000

bool raw_read_to_buffer(uint8_t *data, size_t maxlen, uint32_t timeout_ms, size_t *outlen,
                        bool raw16, uint16_t settle_ms, uint8_t rate_khz);

/* The read command a probe capture sends before it listens. Each one is a read the
 * Proxmark's `lf search` also sends to find the chip type, with no password or login. */
#define LF_PROBE_NONE          0
#define LF_PROBE_T55XX_TRACE   1  /* T55xx regular read of page 1 block 1 */
#define LF_PROBE_EM4X05_WORD0  2  /* EM4x05 read of word 0 */
#define LF_PROBE_T55XX_CONFIG  3  /* T55xx regular read of page 0 block 0 */
#define LF_PROBE_HITAG2_START  4  /* Hitag 2 start command, START_AUTH (11000) */

/* The field-off time of each gap in the Hitag 2 start command, in us (0 restores the default,
 * 16). The tag sees a longer gap, because the antenna rings on after the field goes off. */
void lf_probe_set_hitag2_gap(uint16_t us);

/* raw_read_to_buffer() with a probe. The field comes up, the tag powers for settle_ms, the
 * probe goes out, and the capture starts once it has. */
bool raw_read_probe_to_buffer(uint8_t *data, size_t maxlen, uint32_t timeout_ms, size_t *outlen,
                              bool raw16, uint16_t settle_ms, uint8_t rate_khz, uint8_t probe);

/* Samples lost during the last capture because the ring was full. Zero means the capture
 * is continuous. */
uint32_t raw_read_dropped(void);

/*
 * Capture `count` full-resolution samples (14-bit, 0-16383), one per carrier
 * period, at the sample phase set with lf_125khz_radio_saadc_phase_set().
 *
 * @return true if all `count` samples arrived within `timeout_ms`.
 *         Check lf_capture_dropped() as well: a capture can be complete but
 *         have lost samples in the middle.
 */
bool raw_read_samples(int16_t *samples, size_t count, uint32_t timeout_ms, size_t *outlen);

/** Samples dropped during the last capture (ring buffer full). */
uint32_t lf_capture_dropped(void);
