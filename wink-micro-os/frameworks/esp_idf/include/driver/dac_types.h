// SPDX-License-Identifier: LGPL-3.0-only
#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include "hal/dac_types.h"
#include "esp_bit_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DAC_CHANNEL_MODE_SIMUL,
    DAC_CHANNEL_MODE_ALTER,
} dac_continuous_channel_mode_t;

typedef int dac_continuous_digi_clk_src_t;
typedef int dac_cosine_clk_src_t;

#define DAC_COSINE_CLK_SRC_DEFAULT 0

#ifdef __cplusplus
}
#endif
