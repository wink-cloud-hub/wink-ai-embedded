// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_pin_types.h
 * @brief Fundamental pin number type shared across PAL HAL modules.
 *
 * This header contains ONLY the pin type definition and its sentinel.
 * It is intentionally free of any HAL API declarations. Any PAL/DAL
 * module that needs only wink_pin_t should include this file instead
 * of pal_hal.h, to avoid pulling in unrelated GPIO/I2C/PWM API
 * declarations.
 *
 * Existing code that includes pal_hal.h continues to receive wink_pin_t
 * transitively — zero modification required for legacy files.
 *
 * Stage 0 and beyond: new HAL headers that need wink_pin_t MUST include
 * "hal/pal_pin_types.h" and MUST NOT include pal_hal.h solely for this
 * type.
 */
#ifndef PAL_PIN_TYPES_H
#define PAL_PIN_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Unified pin number type.
 *
 * int16_t is chosen so that GPIO_NUM_NC (-1) is not truncated when
 * compared against unsigned pin variables. All PAL/DAL code must use
 * wink_pin_t instead of raw int / uint8_t for GPIO pin numbers.
 *
 * Valid range: [0, SOC_GPIO_PIN_COUNT) for hardware pins.
 * Use WINK_PIN_NC for "not connected / not used".
 */
typedef int16_t wink_pin_t;

/** @brief Sentinel: pin not connected or not configured. */
#define WINK_PIN_NC ((wink_pin_t)(-1))

#ifdef __cplusplus
}
#endif

#endif /* PAL_PIN_TYPES_H */
