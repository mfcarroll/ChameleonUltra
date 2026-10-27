#ifndef HFXO_COUNT_H
#define HFXO_COUNT_H

#include <stdbool.h>
#include <stdint.h>

// 64-bit count of 16 MHz HFCLK ticks with HFXO held, for a host to time against a reference clock
// and get the 32 MHz crystal's offset. Uses TIMER1.
typedef struct {
    uint64_t ticks;       // 16 MHz ticks since START
    uint32_t overflows;   // high word, as kept by the wrap interrupt
    uint32_t raw;         // TIMER1 value at the capture
    uint16_t lapses;      // 1 s checks that found HFXO not running (each one is re-requested)
    uint8_t running;      // counter started
    uint8_t hfxo_now;     // HFXO running at this read
} hfxo_count_t;

void hfxo_count_start(void);
void hfxo_count_read(hfxo_count_t *out);
void hfxo_count_stop(void);

#endif
