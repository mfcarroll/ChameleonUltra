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

/* Samples lost during the last capture because the ring was full. Zero means the capture
 * is continuous. */
uint32_t raw_read_dropped(void);
