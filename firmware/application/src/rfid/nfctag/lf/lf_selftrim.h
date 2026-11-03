#ifndef LF_SELFTRIM_CONTROL_H
#define LF_SELFTRIM_CONTROL_H

// PSK1 self-trim: while a PSK1 type emulates, measure the reader's clock against ours
// (utils/selftrim.c) and trim our subcarrier to it (utils/psk1.c), so that the subcarrier phase
// doesn't drift through a read. Enabled by a device setting.
//
// The SAADC capture and the detector run from the main loop (lf_selftrim_process); the trim is
// written into the sequence between bursts, while the PWM is stopped (lf_selftrim_apply). A trim
// is kept across fields, sleep and slot changes, for the slot it was learned on (lf_selftrim.c).

#include <stdbool.h>
#include <stdint.h>

#include "nrf_pwm.h"

typedef struct {
    uint8_t enabled;
    int16_t applied;        // the learned trim, in steps of 3.8 ppm (in the sequence from the next field on)
    int16_t last_ppm10;     // last measurement: reader vs us, ppm x 10
    int16_t last_snr10;     // its strength, dB x 10
    uint8_t last_why;       // selftrim_why_t
    uint16_t measurements;  // since the slot was loaded
} lf_selftrim_status_t;

void lf_selftrim_on_load(void);
void lf_selftrim_on_field(void);
void lf_selftrim_apply(const nrf_pwm_sequence_t *seq, bool psk1);
void lf_selftrim_process(bool emulating, bool psk1);
void lf_selftrim_abort(void);
void lf_selftrim_get_status(lf_selftrim_status_t *s);

#endif
