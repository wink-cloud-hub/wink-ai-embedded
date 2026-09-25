/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_LEDC_TYPES_H
#define WINK_H_GUARD_HAL_LEDC_TYPES_H
#ifndef __WINK_HARVESTED_HAL_LEDC_TYPES_H__
#define __WINK_HARVESTED_HAL_LEDC_TYPES_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stdint.h>

#include "soc/clk_tree_defs.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    LEDC_HIGH_SPEED_MODE = 0,
    LEDC_LOW_SPEED_MODE = 1,
    LEDC_SPEED_MODE_MAX = 2,
} ledc_mode_t;
typedef enum {
    LEDC_INTR_DISABLE = 0,
    LEDC_INTR_FADE_END = 1,
    LEDC_INTR_MAX = 2,
} ledc_intr_type_t;
typedef enum {
    LEDC_DUTY_DIR_DECREASE = 0,
    LEDC_DUTY_DIR_INCREASE = 1,
    LEDC_DUTY_DIR_MAX = 2,
} ledc_duty_direction_t;
typedef enum {
    LEDC_SLOW_CLK_RC_FAST = 8,
    LEDC_SLOW_CLK_APB = 4,
} ledc_slow_clk_sel_t;
typedef enum {
    LEDC_REF_TICK = 11,
    LEDC_APB_CLK = 4,
    LEDC_SCLK = 4,
} ledc_clk_src_t;
typedef enum {
    LEDC_TIMER_0 = 0,
    LEDC_TIMER_1 = 1,
    LEDC_TIMER_2 = 2,
    LEDC_TIMER_3 = 3,
    LEDC_TIMER_MAX = 4,
} ledc_timer_t;
typedef enum {
    LEDC_CHANNEL_0 = 0,
    LEDC_CHANNEL_1 = 1,
    LEDC_CHANNEL_2 = 2,
    LEDC_CHANNEL_3 = 3,
    LEDC_CHANNEL_4 = 4,
    LEDC_CHANNEL_5 = 5,
    LEDC_CHANNEL_6 = 6,
    LEDC_CHANNEL_7 = 7,
    LEDC_CHANNEL_MAX = 8,
} ledc_channel_t;
typedef enum {
    LEDC_TIMER_1_BIT = 1,
    LEDC_TIMER_2_BIT = 2,
    LEDC_TIMER_3_BIT = 3,
    LEDC_TIMER_4_BIT = 4,
    LEDC_TIMER_5_BIT = 5,
    LEDC_TIMER_6_BIT = 6,
    LEDC_TIMER_7_BIT = 7,
    LEDC_TIMER_8_BIT = 8,
    LEDC_TIMER_9_BIT = 9,
    LEDC_TIMER_10_BIT = 10,
    LEDC_TIMER_11_BIT = 11,
    LEDC_TIMER_12_BIT = 12,
    LEDC_TIMER_13_BIT = 13,
    LEDC_TIMER_14_BIT = 14,
    LEDC_TIMER_15_BIT = 15,
    LEDC_TIMER_16_BIT = 16,
    LEDC_TIMER_17_BIT = 17,
    LEDC_TIMER_18_BIT = 18,
    LEDC_TIMER_19_BIT = 19,
    LEDC_TIMER_20_BIT = 20,
    LEDC_TIMER_BIT_MAX = 21,
} ledc_timer_bit_t;
typedef enum {
    LEDC_FADE_NO_WAIT = 0,
    LEDC_FADE_WAIT_DONE = 1,
    LEDC_FADE_MAX = 2,
} ledc_fade_mode_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef soc_periph_ledc_clk_src_legacy_t ledc_clk_cfg_t;



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_LEDC_TYPES_H__ */
#endif /* WINK_H_GUARD_HAL_LEDC_TYPES_H */
