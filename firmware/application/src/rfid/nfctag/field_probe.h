#ifndef FIELD_PROBE_H
#define FIELD_PROBE_H

// EXPERIMENT exp/field-probe: describe a reader's field instead of emulating a card.
//
// With the probe on, tag mode senses both fields whatever the active slot holds, and never answers. When an LF field
// appears, the main loop measures it in rounds: the level, the envelope at 100 kHz (the dips are the reader's
// commands, or its field switching off), and the carrier's frequency from its alias on LF_RSSI. When an HF field
// appears, the 14443-A receiver counts the frames the reader sends. Every result goes to a log in the noinit RAM, which
// survives sleep, so the CU can measure on battery and report when it is next connected.

#include <stdbool.h>
#include <stdint.h>

#include "fp_dsp.h"

#define FP_VERSION      (2)
#define FP_LF_RECS      (12)
#define FP_DIPS         (40)
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
};

// fp_lf_rec_t.flags
#define FP_F_MANUAL     (0x01)  // measured by command, not by a field
#define FP_F_ENDED      (0x02)  // the field was gone at the end of the round
#define FP_F_RAILED     (0x04)  // the level capture's p90 is at the ADC's top
#define FP_F_AMBIGUOUS  (0x08)  // the two-rate check found two candidates of similar strength

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
} fp_lf_rec_t;

typedef struct __attribute__((packed)) {
    uint16_t wake;
    uint32_t t_ms;
    uint32_t dur_ms;      // 0xFFFFFFFF while the field lasts
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
void field_probe_process(void); // main loop
bool field_probe_lf_measure_now(fp_lf_rec_t *out);  // one round by command; false if LF sensing is off

void field_probe_hf_field(bool on);  // interrupt context
void field_probe_hf_frame(const uint8_t *data, uint16_t bits, uint32_t rx_status);

// A raw capture of LF_RSSI (or another analog input) with LF_MOD as given, into the LF sniff buffer. Returns samples.
uint32_t field_probe_capture(uint8_t mod, uint16_t rate_khz, uint32_t n, uint8_t ain, uint8_t gain);

#endif
