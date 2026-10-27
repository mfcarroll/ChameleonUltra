#ifndef LF_EXP_FSK_H
#define LF_EXP_FSK_H

// EXPERIMENT: field test of FSK emulation self-trim (HID Prox, Paradox, ioProx). Each slot has a preset, saved to
// flash, so a tester can compare them at a reader by switching slots, with no computer:
//   0 stock:     LF_MOD drive, no trim (as main)
//   1 drive:     U1 drive (research drvC), no trim
//   2 self-trim: U1 drive, mode 9 (measure the reader, trim to it, phase-anchored restarts, K 26), kept per slot
// plus a flag, LF_EXP_FSK_EXACT_BITS: HID Prox and Paradox bits at exactly RF/50 on average (utils/fsktrim.h),
// instead of 48 carrier cycles for each fc/8 bit (the frame 2 % short).
// Unset slots use LF_EXP_FSK_DEFAULT_PRESET. Command 3120 sets presets and reports what each slot learned.

#include <stdint.h>

#define LF_EXP_FSK_STOCK (0u)
#define LF_EXP_FSK_DRIVE (1u)
#define LF_EXP_FSK_SELFTRIM (2u)
#define LF_EXP_FSK_MODE_MASK (0x03u)
#define LF_EXP_FSK_EXACT_BITS (0x04u)
#define LF_EXP_FSK_DEFAULT_PRESET LF_EXP_FSK_SELFTRIM
#define LF_EXP_FSK_K (26u)  // mode 9's phase, 1/128 carrier cycle (research/lf/caps-fsk-why NOTES-flipk: a board constant)

// The active slot's preset (lf_tag_em.c applies it when an FSK slot's data loads).
uint8_t lf_exp_fsk_preset(uint8_t slot);
// A field was detected while emulating the active slot (LPCOMP handler).
void lf_exp_fsk_note_field(void);
// Command 3120. Request: none (query), or slot u8 (0-7), preset u8 (mode 0-2, | LF_EXP_FSK_EXACT_BITS; 0xFF =
// unset): saved; the caller reloads the slot if it is the active one. Reply: see lf_exp_fsk.c.
uint16_t lf_exp_fsk_command(const uint8_t *req, uint16_t len, uint8_t *out, uint16_t *status);

#endif
