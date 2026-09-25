/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef HAL_LEDC_TYPES_H_
#define HAL_LEDC_TYPES_H_

#include <stdint.h>
#include <stdbool.h>
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LEDC_LOW_SPEED_MODE = 0,
    LEDC_HIGH_SPEED_MODE = 1,
    LEDC_SPEED_MODE_MAX = 2,
} ledc_mode_t;

typedef enum {
    LEDC_INTR_DISABLE = 0,
    LEDC_INTR_FADE_END = 1,
    LEDC_INTR_MAX,
} ledc_intr_type_t;

typedef enum {
    LEDC_DUTY_DIR_DECREASE = 0,
    LEDC_DUTY_DIR_INCREASE = 1,
    LEDC_DUTY_DIR_MAX,
} ledc_duty_direction_t;

typedef enum {
    LEDC_AUTO_CLK = 0,
    LEDC_USE_APB_CLK = 1,
    LEDC_USE_RC_FAST_CLK = 2,
    LEDC_USE_REF_TICK = 3,
    LEDC_USE_RTC8M_CLK = 4,
} ledc_clk_cfg_t;

typedef enum {
    LEDC_SLEEP_MODE_NO_ALIVE = 0,
    LEDC_SLEEP_MODE_KEEP_ALIVE = 1,
} ledc_sleep_mode_t;

typedef enum {
    LEDC_TIMER_0 = 0,
    LEDC_TIMER_1,
    LEDC_TIMER_2,
    LEDC_TIMER_3,
    LEDC_TIMER_MAX,
} ledc_timer_t;

typedef enum {
    LEDC_CHANNEL_0 = 0,
    LEDC_CHANNEL_1,
    LEDC_CHANNEL_2,
    LEDC_CHANNEL_3,
    LEDC_CHANNEL_4,
    LEDC_CHANNEL_5,
    LEDC_CHANNEL_6,
    LEDC_CHANNEL_7,
    LEDC_CHANNEL_MAX,
} ledc_channel_t;

typedef enum {
    LEDC_TIMER_1_BIT = 1,
    LEDC_TIMER_2_BIT,
    LEDC_TIMER_3_BIT,
    LEDC_TIMER_4_BIT,
    LEDC_TIMER_5_BIT,
    LEDC_TIMER_6_BIT,
    LEDC_TIMER_7_BIT,
    LEDC_TIMER_8_BIT,
    LEDC_TIMER_9_BIT,
    LEDC_TIMER_10_BIT = 10,
    LEDC_TIMER_11_BIT,
    LEDC_TIMER_12_BIT,
    LEDC_TIMER_13_BIT = 13,
    LEDC_TIMER_14_BIT = 14,
    LEDC_TIMER_15_BIT,
    LEDC_TIMER_16_BIT,
    LEDC_TIMER_17_BIT,
    LEDC_TIMER_18_BIT,
    LEDC_TIMER_19_BIT,
    LEDC_TIMER_20_BIT = 20,
    LEDC_TIMER_BIT_MAX,
} ledc_timer_bit_t;

typedef enum {
    LEDC_FADE_NO_WAIT = 0,
    LEDC_FADE_WAIT_DONE,
    LEDC_FADE_MAX,
} ledc_fade_mode_t;

#ifdef __cplusplus
}
#endif

#endif /* HAL_LEDC_TYPES_H_ */
