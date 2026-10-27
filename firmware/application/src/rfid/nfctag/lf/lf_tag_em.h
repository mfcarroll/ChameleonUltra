#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rfid_main.h"
#include "tag_emulation.h"

#define LF_EM410X_TAG_ID_SIZE 5
#define LF_EM410X_ELECTRA_TAG_ID_SIZE 13
#define LF_IOPROX_TAG_ID_SIZE 16
#define LF_PARADOX_TAG_ID_SIZE 6
#define LF_HIDPROX_TAG_ID_SIZE 13
#define LF_VIKING_TAG_ID_SIZE 4
#define LF_PAC_TAG_ID_SIZE 8
#define LF_JABLOTRON_TAG_ID_SIZE 5
#define LF_IDTECK_TAG_ID_SIZE 8

void lf_tag_125khz_sense_switch(bool enable);
int lf_tag_data_loadcb(tag_specific_type_t type, tag_data_buffer_t *buffer);
int lf_tag_em410x_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer);
bool lf_tag_em410x_data_factory(uint8_t slot, tag_specific_type_t tag_type);
int lf_tag_hidprox_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer);
bool lf_tag_hidprox_data_factory(uint8_t slot, tag_specific_type_t tag_type);
int lf_tag_ioprox_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer);
bool lf_tag_ioprox_data_factory(uint8_t slot, tag_specific_type_t tag_type);
int lf_tag_paradox_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer);
bool lf_tag_paradox_data_factory(uint8_t slot, tag_specific_type_t tag_type);
int lf_tag_viking_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer);
bool lf_tag_viking_data_factory(uint8_t slot, tag_specific_type_t tag_type);
int lf_tag_pac_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer);
bool lf_tag_pac_data_factory(uint8_t slot, tag_specific_type_t tag_type);
int lf_tag_jablotron_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer);
bool lf_tag_jablotron_data_factory(uint8_t slot, tag_specific_type_t tag_type);
int lf_tag_idteck_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer);
bool lf_tag_idteck_data_factory(uint8_t slot, tag_specific_type_t tag_type);
bool is_lf_field_exists(void);

// research: hw lfstate snapshot; returns the length written (at most LF_TAG_EM_STATE_MAX)
#define LF_TAG_EM_STATE_MAX 256
uint16_t lf_tag_em_state_get(uint8_t *out);
// research: hw lfsettle; ms 0 queries, else sets (capped at 50); returns the value in force
uint16_t lf_tag_em_settle_set(uint16_t ms);
// research: hw lfstall, watchdog test hook; false unless sensing with a waveform loaded
bool lf_tag_em_stall(void);
// research exp/fsk-drvtrim: hw lfdrive (3093). mode > LF_FSK_DRIVE_MAX (utils/fsktrim.h) queries; applies at
// the next burst start. Returns the requested mode, *active the mode PWM0 is set up for.
uint8_t lf_tag_em_drive_set(uint8_t mode, uint8_t *active);
// research exp/fsk-selftrim: hw lfphase (3096), per-burst start delay in us (0 = off).
uint8_t lf_tag_em_phase_set(uint8_t set, uint8_t us);
// research exp/fsk-phasecont: hw lfcont (3097). Reply: on u8, offset u8, flags u8 (bit0 grid running, bit1 PPI
// allocated), arms u16, first u16, late u16, fail u16, gap_cycles u32 (BE).
// v2 (phase select): on is the mode (0 off, 1 continuous, 2 continuous + phase select; offset is then the phase K),
// flags bit2 anchor valid, then anchored u16.
// v3 (placed phase): mode 3 = measure as 2, restarts on the trimmed grid; mode 4 = 2 with the measured period (2
// takes the anchored period from the trimmed grid). After the phase-select status: the last
// field's last burst index u16, first burst after the measurement index u16 (0xFFFF none), how u8 (1 grid, 2 anchored),
// start ticks u32 of the first / post-measurement / last burst, then the measured reader phase u8 (1/128, 0xFF none)
// at each of those three starts.
// v4: + the averaged reader line fp u32 (mHz), fields u16 (mode 2's restart period; 5 = grid, 4 = per field).
// v5: + the measure detune u8 (ppm).
// v6: + mode 9: locked u8, trim moves u16, rejects u16, last wide-search ppm s16 (x10), wide searches u16.
// v7: + kept u8, kept trim s16, restores u16 (the per-slot store).
#define LF_TAG_EM_CONT_LEN (17 + 16 + 20 + 6 + 1 + 25)  // + LF_SELFTRIM_FSK_PHSEL_LEN + placed phase + average
uint16_t lf_tag_em_cont_set(bool set, uint8_t mode, uint8_t offset, uint8_t detune_ppm, uint8_t *o);
bool lf_tag_em_cont_phsel(uint8_t *k);
bool lf_tag_em_cont_saadc_start(uint32_t *t0);
uint8_t lf_tag_em_cont_period(void);
bool lf_tag_em_cont_auto(uint16_t *gen);
void lf_tag_em_cont_anchor_set(uint64_t anchor_q32, uint64_t cycle_q32);
// research exp/fsk-drvtrim: hw lftrim (3094), ioProx / HID Prox subcarrier trim in 16 MHz ticks per frame
// (+ = shorter frame, faster subcarrier). Applies at the next burst start; RAM only, 0 at boot.
int16_t lf_tag_em_trim_set(bool set, int16_t trim, int16_t *applied, uint32_t *frame_ticks);

// research exp/fsk-selftrim: what lf_selftrim_fsk.c needs from the emulator.
typedef struct {
    bool emulating;           // a field was detected and bursts are playing
    bool fsk;                 // the loaded type is ioProx or HID Prox, with a sequence
    uint32_t frame_ticks;     // 16 MHz ticks per frame, as last prepared (0 before the first prepare)
    int16_t trim_req, trim_act;
    uint8_t drive_act;
    uint16_t det;             // field detections so far (wraps)
    uint16_t loads;           // FSK sequence loads so far
    uint16_t measure_bursts;  // bursts started in the measure drive
} lf_tag_em_fsk_info_t;
int16_t *lf_tag_em_alias_buffer(size_t *cap_samples);
void lf_tag_em_fsk_info(lf_tag_em_fsk_info_t *i);
void lf_tag_em_fsk_measure(bool on, uint8_t drive);
bool lf_tag_em_fsk_measure_playing(void);
