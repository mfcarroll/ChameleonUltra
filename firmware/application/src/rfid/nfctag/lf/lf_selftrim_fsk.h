#ifndef LF_SELFTRIM_FSK_H
#define LF_SELFTRIM_FSK_H

// research exp/fsk-selftrim: self-trim for FSK emulation (ioProx, HID Prox), ported from the PSK1 self-trim
// (feat/lf-psk-selftrim @ 4b333efb, lf_selftrim.c). While FSK emulates, measure the reader's carrier against our
// clock from its alias on LF_RSSI (utils/selftrim.c, 8192 samples at 20 kHz) and trim our subcarrier (hw lftrim)
// so that the U1 contribution stops beating against the reader (research/lf/caps-fsk-why/NOTES-beat, NOTES-trim).
//
// Differences from PSK: the capture's bursts play in a "measure drive" (default 2, U1 + LF_MOD: the alias needs
// LF_MOD switching; mode 1 rails LF_RSSI), all other bursts in the emulate drive (hw lfdrive, default 1); the trim
// aims at a residual offset r (default +25 ppm, reader minus our trimmed clock) rather than an exact match, so the U1
// phase drifts slowly instead of parking; steps are 1e6 / ticks-per-frame ppm (HID 1.66, ioProx ~1.9).
// Off by default. Turning it off leaves the trim as it is (hw lftrim stays manual). RAM only (no retained block).

#include <stdbool.h>
#include <stdint.h>

#define LF_SELFTRIM_FSK_STATUS_LEN (26)

bool lf_selftrim_fsk_busy(void);
void lf_selftrim_fsk_process(void);
// Request (len 0 = query, len 4 = set): enabled u8, residual s16 BE ppm, measure drive u8. Reply: status (BE).
uint16_t lf_selftrim_fsk_command(const uint8_t *req, uint16_t len, uint8_t *out);
// research exp/fsk-phasecont: phase-select status (appended to hw lfcont's reply). BE: valid u8, phase u8 (1/128 cycle,
// the reader's carrier at the capture's centre), count u16, invalid u16, fail u16, snr s16 (dB/10), fp u32 (mHz), ms u16.
#define LF_SELFTRIM_FSK_PHSEL_LEN (16)
uint16_t lf_selftrim_fsk_phsel_status(uint8_t *o);
// The reader's carrier phase (1/128 cycle) at TIMER3 tick cc, from the current field's measurement; 0xFF if none.
uint8_t lf_selftrim_fsk_phase_at(uint32_t cc);
// Mode 2's averaged reader line: fp u32 (mHz), fields averaged u16 (BE), 6 bytes.
uint16_t lf_selftrim_fsk_phavg_status(uint8_t *o);
// Mode 9: locked u8, trim moves u16, rejects u16, last wide-search ppm s16 (x10), wide searches u16, kept u8, kept
// trim s16, restores u16 (BE), 14 bytes.
uint16_t lf_selftrim_fsk_auto_status(uint8_t *o);
// research 3100: the raw capture of the latest first measurement after a restore (off 0xFFFF: its 29-byte summary)
uint16_t lf_selftrim_fsk_rdump(uint16_t off, uint16_t cnt, uint8_t *o);
// Per-slot store for mode 9 (noinit, retained in System OFF): the kept trim for the active slot (IRQ-safe), and
// hw lfcont's mode and K.
bool lf_selftrim_fsk_keep_trim(int16_t *trim);
void lf_selftrim_fsk_keep_cont(uint8_t mode, uint8_t k);
bool lf_selftrim_fsk_kept_cont(uint8_t *mode, uint8_t *k);
// EXPERIMENT: what mode 9 kept for a slot: LF type, trim (steps), averaged reader line (mHz), fields averaged.
bool lf_selftrim_fsk_kept_slot(uint8_t slot, uint16_t *type, int16_t *trim, uint32_t *fp_mhz, uint16_t *n);
void lf_selftrim_fsk_keep_clear(void);

#endif
