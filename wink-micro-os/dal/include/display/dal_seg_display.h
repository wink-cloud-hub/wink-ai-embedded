// SPDX-License-Identifier: Apache-2.0
#ifndef DAL_SEG_DISPLAY_H
#define DAL_SEG_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 7-Segment Display physical variants (SSOT with wink-plugin-peripherals)
 */
typedef uint8_t dal_seg_display_variant_t;
enum {
    DAL_SEG_DISPLAY_VARIANT_DIRECT_GPIO_8D = 0, /**< 8-digit multiplexed direct GPIO */
    DAL_SEG_DISPLAY_VARIANT_DIRECT_GPIO_4D = 1, /**< 4-digit multiplexed direct GPIO */
    DAL_SEG_DISPLAY_VARIANT_DIRECT_GPIO_2D = 2, /**< 2-digit multiplexed direct GPIO */
    DAL_SEG_DISPLAY_VARIANT_DIRECT_GPIO_1D = 3, /**< 1-digit direct GPIO */
};
#define DAL_SEG_DISPLAY_VARIANT_COUNT 4

/**
 * @brief 7-Segment Display pin configuration
 */
typedef struct {
    uint16_t seg_pins[8];    /**< Segment pins: A, B, C, D, E, F, G, DP */
    uint16_t com_pins[8];    /**< Digit enable pins: COM0..COM7 (unused set to 0xFFFF) */
    uint8_t num_digits;      /**< 1..8 digits */
    bool common_anode;       /**< true: common anode; false: common cathode */
    bool seg_active_high;    /**< true: active high segments */
    bool dig_active_low;     /**< true: active low digit select */
} dal_seg_display_pin_config_t;

/**
 * @brief 7-Segment Display instance configuration
 */
typedef struct {
    const char *owner;
    dal_seg_display_variant_t variant;
    dal_seg_display_pin_config_t pins;
} dal_seg_display_config_t;

/**
 * @brief 7-Segment Display driver handle
 */
typedef struct {
    dal_seg_display_config_t config;
    uint8_t display_buffer[8]; /**< Raw segment bitmasks for up to 8 digits */
    uint8_t current_digit;     /**< Current scan index */
    bool initialized;
} dal_seg_display_t;

/**
 * @brief Initialize 7-segment display driver
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_seg_display_init(dal_seg_display_t *dev, const dal_seg_display_config_t *cfg);

/**
 * @brief Write raw segment mask to a specific digit
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_seg_display_write_mask(dal_seg_display_t *dev, uint8_t digit_idx, uint8_t seg_mask);

/**
 * @brief Step dynamic scanning for multiplexed display (call periodically from timer)
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_seg_display_scan_step(dal_seg_display_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* DAL_SEG_DISPLAY_H */
