// SPDX-License-Identifier: Apache-2.0
#ifndef DAL_LED_H
#define DAL_LED_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LED configuration struct
 */
typedef struct {
    const char *owner;     /**< Instance owner static string */
    uint16_t pin;          /**< Logical GPIO pin */
    bool active_high;      /**< true: active high; false: active low */
} dal_led_config_t;

/**
 * @brief LED handle (POD).
 */
typedef struct {
    dal_led_config_t config; /**< Config copy */
    bool is_on;            /**< Cached current state */
    bool initialized;      /**< Set to true after successful init */
} dal_led_t;

_Static_assert(offsetof(dal_led_t, config) == 0, "config must be the first member");

#if INTPTR_MAX == INT32_MAX   /* ILP32: ESP32 xtensa, wasm32 */
_Static_assert(sizeof(dal_led_config_t) == 8,  "ABI break: config size changed on 32-bit target");
_Static_assert(offsetof(dal_led_t, initialized) == 9,  "ABI break: initialized offset changed on 32-bit");
_Static_assert(sizeof(dal_led_t) == 12, "ABI break: handle size changed on 32-bit target");
#else                         /* LP64 / LLP64: 64-bit Host Simulation */
_Static_assert(sizeof(dal_led_config_t) == 16, "ABI break: config size changed on 64-bit host");
_Static_assert(offsetof(dal_led_t, initialized) == 17, "ABI break: initialized offset changed on 64-bit host");
_Static_assert(sizeof(dal_led_t) == 24, "ABI break: handle size changed on 64-bit host");
#endif

/**
 * @brief Initialize LED driver instance
 *
 * @param[in,out] dev LED instance handle.
 * @param[in] cfg Configuration struct.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_led_init(dal_led_t *dev, const dal_led_config_t *cfg);

/**
 * @brief Turn on LED
 *
 * @param[in,out] dev LED instance handle.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_led_on(dal_led_t *dev);

/**
 * @brief Turn off LED
 *
 * @param[in,out] dev LED instance handle.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_led_off(dal_led_t *dev);

/**
 * @brief Set explicit LED state
 *
 * @param[in,out] dev LED instance handle.
 * @param[in] on Target state (true = on, false = off).
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_led_set(dal_led_t *dev, bool on);

/**
 * @brief Toggle LED state
 *
 * @param[in,out] dev LED instance handle.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_led_toggle(dal_led_t *dev);

/**
 * @brief LED safe-off (turn off output)
 *
 * @param[in,out] dev LED instance handle.
 * @return WINK_OK on success, error status code otherwise.
 */
wink_status_t dal_led_safe_off(dal_led_t *dev);

/**
 * @brief Deinitialize LED driver
 *
 * @param[in,out] dev LED instance handle.
 * @return WINK_OK on success, error status code otherwise.
 */
wink_status_t dal_led_deinit(dal_led_t *dev);

#ifdef __cplusplus
}
#endif

/* Compile-time pruning stubs */
#if !defined(WINK_USE_LED) || !WINK_USE_LED
#define WINK_LED_DISABLED_MSG \
    "LED driver not enabled; add a \"led\" device to wink-app.json " \
    "(or set -DWINK_USE_LED=ON)."
WINK_UNAVAILABLE_MSG(WINK_LED_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_led_init(dal_led_t *dev, const dal_led_config_t *cfg);
WINK_UNAVAILABLE_MSG(WINK_LED_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_led_on(dal_led_t *dev);
WINK_UNAVAILABLE_MSG(WINK_LED_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_led_off(dal_led_t *dev);
WINK_UNAVAILABLE_MSG(WINK_LED_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_led_set(dal_led_t *dev, bool on);
WINK_UNAVAILABLE_MSG(WINK_LED_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_led_toggle(dal_led_t *dev);
WINK_UNAVAILABLE_MSG(WINK_LED_DISABLED_MSG)
wink_status_t dal_led_safe_off(dal_led_t *dev);
WINK_UNAVAILABLE_MSG(WINK_LED_DISABLED_MSG)
wink_status_t dal_led_deinit(dal_led_t *dev);
#endif /* !WINK_USE_LED */

#endif /* DAL_LED_H */
