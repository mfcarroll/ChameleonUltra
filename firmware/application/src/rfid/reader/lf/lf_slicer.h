#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * The shared slicer: turn a carrier-locked sample buffer into levels.
 *
 * ⭐⭐ ONE SLICER, USED BY EVERY DECODER THAT WORKS ON LEVELS — the ASK/Manchester family and
 * the FSK2a family both. It is here rather than duplicated because duplicating exactly this
 * has already cost a day: `framedrift.py` sliced against a 512-sample TRAILING average where
 * the firmware used 256-sample BLOCK MEANS, and the two disagreed on which captures contained
 * a frame at all. An analysis tool that reasons about a decoder must share its front end; so
 * must two decoders that claim to see the same signal.
 *
 * ⛔ THE REFERENCE IS BLOCK MEANS, NOT A MOVING AVERAGE. A trailing average lags the signal by
 * half its width, which a drifting capture turns into a slicing bias that varies along the
 * buffer. Block means do not lag. 256 samples is 8 bit periods at RF/32 — far shorter than
 * any drift worth correcting and far longer than any bit.
 */

#define LF_SLICER_BLOCK_SHIFT 8
#define LF_SLICER_BLOCK       (1u << LF_SLICER_BLOCK_SHIFT)

/** Build the per-block DC reference for `samples[0..n)`. Call once per capture. */
void lf_slicer_build_dc(const int16_t *samples, size_t n);

/** The level at one sample, against the DC built by the last `lf_slicer_build_dc`.
 *  `lp` 1 is raw; 3 averages the sample with its two neighbours. */
bool lf_slicer_level(const int16_t *samples, size_t n, size_t i, uint8_t lp);
