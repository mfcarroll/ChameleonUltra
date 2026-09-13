/* Host shim — `wiegand.c` needs a handful of macros from Nordic's header and these are they,
 * copied verbatim from nrf52_sdk/components/libraries/util/nordic_common.h so the
 * shipping source compiles here unmodified. */
#ifndef SHIM_NORDIC_COMMON_H
#define SHIM_NORDIC_COMMON_H
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define IS_SET(W, B)    (((W) >> (B)) & 1)
#define SET_BIT(W, B)   ((W) |= (1u << (B)))
#define SET_BIT64(W, B) ((W) |= (1uLL << (B)))
#define CLR_BIT(W, B)   ((W) &= ~(1u << (B)))
#endif
