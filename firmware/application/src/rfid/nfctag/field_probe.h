#ifndef FIELD_PROBE_H
#define FIELD_PROBE_H

// EXPERIMENT exp/field-probe: describe a reader's field instead of emulating a card.
//
// With the probe on, tag mode senses both fields whatever the active slot holds, and never answers. When an LF field
// appears, the main loop measures it in rounds: the level, the envelope at 100 kHz (the dips are the reader's
// commands, or its field switching off), the carrier's frequency from its alias on LF_RSSI, and the field's on and off
// stretches over 1.8 s. From the envelope it also classifies a Hitag 2 reader's authentication mode by the length of
// the reader's command after the tag's UID reply (32 bits is password mode, 64 bits is crypto mode): a frame-length
// read that classifies the install and recovers no secret. While the field lasts, LPCOMP watches for it dropping, and
// each drop flashes the LEDs red. When an HF field appears, the 14443-A receiver counts the frames the reader sends.
// Every result goes to a log in the noinit RAM, which survives sleep, so the CU can measure on battery and report when
// it is next connected.

#include <stdbool.h>
#include <stdint.h>

#include "fp_dsp.h"

#define FP_VERSION      (5)
#define FP_LF_RECS      (12)
#define FP_DIPS         (40)
#define FP_RUNS         (48)
#define FP_LEVEL_DIPS   (32)
#define FP_LF_FIELDS    (32)
#define FP_HF_FIELDS    (32)

// LF_MOD during a capture: off, on (Q3 holds a constant load), or a tone (fc/8 on our clock).
enum { FP_MOD_LOW = 0, FP_MOD_HIGH = 1, FP_MOD_TONE = 2 };

// What reset the CU in the middle of a run (fp_store_t.crash_kind).
enum { FP_CRASH_NONE = 0, FP_CRASH_WATCHDOG = 1, FP_CRASH_LOCKUP = 2, FP_CRASH_ERROR = 3 };

// How the carrier was placed among the frequencies that share its alias (fp_lf_rec_t.carrier_how).
enum {
    FP_CARRIER_NONE = 0,
    FP_CARRIER_TWO_RATES = 1,  // no envelope line: the tone's alias, placed by a second capture at another rate
    FP_CARRIER_ENVELOPE = 2,   // the 100 kHz envelope's line (100..150 kHz folds to 0..50 kHz one to one)
    FP_CARRIER_ASSUMED = 3,    // no envelope line, and the second rate picked nothing: the nearest to 125 kHz
    FP_CARRIER_GATED = 4,      // the envelope's line over its longest stretch with the field on (the field switched)
};

// fp_lf_rec_t.flags
#define FP_F_MANUAL     (0x01)  // measured by command, not by a field
#define FP_F_ENDED      (0x02)  // the field was gone at the end of the round
#define FP_F_RAILED     (0x04)  // the level capture's p90 is at the ADC's top
#define FP_F_AMBIGUOUS  (0x08)  // the two-rate check found two candidates of similar strength
#define FP_F_SWITCHED   (0x10)  // the field went off during the envelope capture: no carrier from the load tone

// Everything in the log is packed and little-endian; the page reads it with these layouts.
typedef struct __attribute__((packed)) {
    uint8_t mode;        // FP_MOD_*
    uint8_t rate_khz;    // nominal sample rate
    uint16_t n;          // samples
    int16_t min, p10, p50, p90, max;
    uint16_t noise;
    uint32_t line_mhz;   // the strongest line's alias, mHz (0: none looked for or none found)
    int16_t line_snr10;  // its SNR, dB x10
} fp_cap_t;

typedef struct __attribute__((packed)) {
    uint16_t wake;        // the boot it happened in (counted while the probe is on)
    uint16_t field;       // the LF field it belongs to (fp_store_t.lf_fields at the time)
    uint32_t t_ms;        // ms since that boot
    uint8_t round;        // 0 at the start of the field, then each later round
    uint8_t flags;        // FP_F_*
    uint32_t carrier_mhz; // the reader's carrier on our clock, mHz (0: none)
    int16_t carrier_snr10;
    uint8_t carrier_how;  // FP_CARRIER_*
    uint8_t n_dips;       // stored in dips
    uint16_t dips_total;  // found in the envelope capture
    fp_cap_t cap[3];      // level (LF_MOD off, 20 kHz), envelope (100 kHz, 4-sample mean), tone (20 kHz)
    fp_dip_t dips[FP_DIPS];
    uint16_t carrier_n;   // FP_CARRIER_ENVELOPE or _GATED: the envelope samples the line was fitted on (10 us each)
    // The timing capture: LF_MOD off, 8 kHz for 1.79 s, as 1 ms means, the lowest and the highest kept here. A drop in
    // the level that reaches timing_off_at is the field going off. runs times each off where the level crosses half of
    // timing_full. A drop that stays above timing_off_at is a dip, kept in level_dips.
    int16_t timing_low, timing_high;
    uint8_t timing_first_on;  // the first stretch is the field on (1) or off (0); the rest alternate
    uint8_t n_runs;           // stored in runs
    uint16_t runs_total;      // found; 0 with no field
    uint16_t runs[FP_RUNS];   // each stretch, ms; the first and the last are cut by the capture's ends
    // The Hitag 2 reader-side authentication mode, from the envelope (fp_hitag_scan): the length of the reader's
    // command after the tag's UID reply. Reads the frame's length only — it classifies the install, recovers no secret.
    uint8_t auth_mode;        // FP_AUTH_*
    uint8_t auth_start_gaps;  // the START_AUTH burst it was anchored on (0: none seen in the capture)
    uint8_t auth_gaps;        // gaps in that command (about its bit length: ~32 password, ~64 crypto)
    uint16_t auth_span;       // that command's span, first gap to last, in envelope samples (10 us each)
    // Log version 5. A drop starts below timing_full less an eighth and ends back above timing_full less a sixteenth
    // (at least 32 and 16 counts). A dip's start and width are in ms, and its depth is how far below timing_full it
    // went. FP_DIP_OPEN marks a dip cut by the capture's start or end. An off shorter than about 10 ms also lands
    // here, because the level falls too slowly to reach timing_off_at.
    int16_t timing_full;      // the field's full level: the 90th percentile of the 1 ms means with the field on
    int16_t timing_off_at;    // the off level: the larger of FP_FIELD_MIN and a tenth of timing_full
    uint8_t n_level_dips;     // stored in level_dips
    uint16_t level_dips_total;
    fp_dip_t level_dips[FP_LEVEL_DIPS];
} fp_lf_rec_t;

typedef struct __attribute__((packed)) {
    uint16_t wake;
    uint32_t t_ms;
    uint32_t dur_ms;      // 0xFFFFFFFF while the field lasts
    // The times LPCOMP saw LF_RSSI fall below its fixed reference (VDD/16) while LF_MOD was off. The last may be the
    // field's end. A dip can cross that reference as well as an off, so the timing captures say which a drop was.
    uint16_t drops;
} fp_field_t;

typedef struct __attribute__((packed)) {
    uint16_t wake;
    uint32_t t_ms;
    uint32_t dur_ms;      // 0xFFFFFFFF while the field lasts
    uint16_t frames;      // every frame received
    uint16_t reqa, wupa;  // 7-bit REQA (0x26) and WUPA (0x52)
    uint16_t other;       // any other frame
    uint16_t rx_errors;   // frames with a CRC, parity or overrun error
    uint16_t other_bits;  // the last other frame: its length in bits, and its first bytes as received
    uint8_t other_data[4];
} fp_hf_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t version;
    uint8_t enabled;
    uint8_t env_mode;     // LF_MOD during the envelope capture
    uint8_t crashes;      // resets by the watchdog, a lockup or a firmware error since the log began (it survives them)
    uint16_t lf_rec_size; // sizeof(fp_lf_rec_t), for the page's check
    uint16_t wake;
    uint16_t lf_recs;     // written so far; the next goes to lf[lf_recs % FP_LF_RECS]
    uint16_t lf_fields;
    uint16_t hf_fields;
    uint16_t crash_kind;  // the last crash: FP_CRASH_*
    uint32_t crash_err;   // its error code (FP_CRASH_ERROR)
    uint32_t crash_pc;    // where it happened (FP_CRASH_ERROR): the PC, or for an APP_ERROR_CHECK its source line
    uint16_t crash_wake;  // the wake it happened in
    uint16_t boot_reason; // this boot's RESETREAS, bits 0..3 as they are and bits 16..20 at 4..8
    uint16_t usb_dropped; // replies usb_cdc_write gave up on since the log began
    uint16_t usb_err;     // the last of those replies' error code
    fp_lf_rec_t lf[FP_LF_RECS];
    fp_field_t lf_field[FP_LF_FIELDS];
    fp_hf_t hf[FP_HF_FIELDS];
} fp_store_t;

// At boot, in place of the first-power fill of the noinit RAM with 0xFF. After a crash it leaves the log out of the
// fill, so the log survives the crash and records it.
void field_probe_noinit_wipe(uint32_t resetreas);
void field_probe_init(uint32_t resetreas);  // once at boot: checks the log and counts the wake
void field_probe_usb_dropped(uint32_t err);  // usb_cdc_write gave up on a reply
bool field_probe_enabled(void);
void field_probe_set(bool enabled, uint8_t env_mode);
uint8_t field_probe_env_mode(void);
void field_probe_clear(void);
const fp_store_t *field_probe_store(void);

void field_probe_lf_up(void);   // LPCOMP saw a field (interrupt context)
void field_probe_lf_drop(void); // LPCOMP saw the field drop while the probe watched (interrupt context)
void field_probe_lf_seen(bool on); // the drop watch found the field up or down (interrupt context or main loop)
void field_probe_process(void); // main loop
bool field_probe_lf_measure_now(fp_lf_rec_t *out);  // one round by command; false if LF sensing is off

void field_probe_hf_field(bool on);  // interrupt context
void field_probe_hf_frame(const uint8_t *data, uint16_t bits, uint32_t rx_status);

// A raw capture of LF_RSSI (or another analog input) with LF_MOD as given, into the LF sniff buffer. Returns samples.
uint32_t field_probe_capture(uint8_t mod, uint16_t rate_khz, uint32_t n, uint8_t ain, uint8_t gain);

#endif
