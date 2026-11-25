#pragma once

#include <stdint.h>

#include "lf_ia.h"

// The slot data of the test tag type: the frame it sends, first byte first.
#define LF_IA_TEST_FRAME_SIZE (4u)

// Sets the test module's frame and returns the module, for lf_ia_set_module.
const lf_ia_module_t *lf_ia_test_module_load(const uint8_t *frame);
