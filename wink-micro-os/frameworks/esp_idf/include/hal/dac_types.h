// SPDX-License-Identifier: LGPL-3.0-only
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_assert.h"
#include "esp_bit_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SOC_DAC_SUPPORTED
#define SOC_DAC_SUPPORTED 1
#endif

#ifndef SOC_DAC_CHAN_NUM
#define SOC_DAC_CHAN_NUM 2
#endif

typedef enum {
    DAC_CHAN_0 = 0,
    DAC_CHAN_1 = 1,
    DAC_CHAN_MAX,
} dac_channel_t;

#define IS_VALID_DAC_CHANNEL(channel) ((uint32_t)(channel) < (uint32_t)DAC_CHAN_MAX)

typedef uint32_t dac_channel_mask_t;

#define DAC_CHANNEL_MASK_CH0    (BIT(0))
#define DAC_CHANNEL_MASK_CH1    (BIT(1))
#define DAC_CHANNEL_MASK_ALL    ((1ULL << SOC_DAC_CHAN_NUM) - 1)

#define IS_VALID_DAC_CHANNEL_MASK(mask) (((mask) & ~DAC_CHANNEL_MASK_ALL) == 0)

#define DAC_CHANNEL_MASK_FOREACH(channel, mask) \
    for (uint32_t __dac_mask = (mask), __dac_chan = DAC_CHAN_0; __dac_chan < DAC_CHAN_MAX; __dac_chan++) \
        for (dac_channel_t channel = (dac_channel_t)__dac_chan; __dac_mask & BIT(__dac_chan); __dac_mask &= ~BIT(__dac_chan))

typedef enum {
    DAC_COSINE_ATTEN_DEFAULT    = 0x0,
    DAC_COSINE_ATTEN_DB_0       = 0x0,
    DAC_COSINE_ATTEN_DB_6       = 0x1,
    DAC_COSINE_ATTEN_DB_12      = 0x2,
    DAC_COSINE_ATTEN_DB_18      = 0x3,
} dac_cosine_atten_t;

typedef enum {
    DAC_COSINE_PHASE_0   = 0x02,
    DAC_COSINE_PHASE_180 = 0x03,
} dac_cosine_phase_t;

#ifdef __cplusplus
}
#endif
