// SPDX-License-Identifier: Apache-2.0
#ifndef DAL_BUZZER_H
#define DAL_BUZZER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DAL_BUZZER_DEFAULT_FREQ_HZ 2000u
#define DAL_BUZZER_MIN_FREQ_HZ     20u
#define DAL_BUZZER_MAX_FREQ_HZ     8000u

/**
 * @brief Buzzer topology variant enumeration
 */
typedef enum {
    DAL_BUZZER_VARIANT_PASSIVE_PWM = 0, /**< Passive piezo buzzer (PWM frequency driven, default) */
    DAL_BUZZER_VARIANT_ACTIVE_GPIO = 1, /**< Active buzzer (GPIO high/low level switch) */
} dal_buzzer_variant_t;

/**
 * @brief Buzzer configuration POD struct
 */
typedef struct {
    const char          *owner;               /**< Instance owner static string (DAL-S-001) */
    uint32_t             default_freq_hz;     /**< Default frequency in Hz (default 2000Hz) */
    uint16_t             pin;                 /**< ACTIVE_GPIO: required GPIO pin */
    int16_t              enable_pin;          /**< Optional enable pin (-1 sentinel) */
    uint8_t              pwm_channel;         /**< PASSIVE_PWM: PWM channel ID */
    bool                 active_high;         /**< Active high polarity (ignored in PASSIVE_PWM) */
    bool                 enable_active_high;  /**< Enable pin polarity */
    uint8_t              _pad0;               /**< Alignment padding */
    dal_buzzer_variant_t variant;             /**< Topology variant */
} dal_buzzer_config_t;

/**
 * @brief Buzzer instance handle POD struct
 */
typedef struct {
    dal_buzzer_config_t config;               /**< Config copy (offsetof == 0) */
    uint32_t            current_freq_hz;      /**< Current frequency in Hz (0 = quiet) */
    bool                is_on;                /**< Current sound state */
    bool                initialized;          /**< Set to true after successful init */
    uint8_t             _pad0[2];             /**< Alignment padding */
} dal_buzzer_t;

_Static_assert(offsetof(dal_buzzer_t, config) == 0, "config must be the first member");

#if INTPTR_MAX == INT32_MAX   /* ILP32: ESP32 xtensa, wasm32 */
_Static_assert(sizeof(dal_buzzer_config_t) == 20, "ABI break: config size changed on 32-bit target");
_Static_assert(offsetof(dal_buzzer_t, initialized) == 25, "ABI break: initialized offset changed on 32-bit");
_Static_assert(sizeof(dal_buzzer_t) == 28, "ABI break: handle size changed on 32-bit target");
#else                         /* LP64 / LLP64: 64-bit Host Simulation */
_Static_assert(sizeof(dal_buzzer_config_t) == 24, "ABI break: config size changed on 64-bit host");
_Static_assert(offsetof(dal_buzzer_t, initialized) == 29, "ABI break: initialized offset changed on 64-bit host");
_Static_assert(sizeof(dal_buzzer_t) == 32, "ABI break: handle size changed on 64-bit host");
#endif

/**
 * @brief Initialize buzzer driver instance
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_buzzer_init(dal_buzzer_t *dev, const dal_buzzer_config_t *cfg);

/**
 * @brief Deinitialize buzzer driver instance
 */
wink_status_t dal_buzzer_deinit(dal_buzzer_t *dev);

/**
 * @brief Turn on buzzer using default frequency
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_buzzer_on(dal_buzzer_t *dev);

/**
 * @brief Turn off buzzer sound
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_buzzer_off(dal_buzzer_t *dev);

/**
 * @brief Set buzzer state (true = on with default freq, false = off)
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_buzzer_set(dal_buzzer_t *dev, bool on);

/**
 * @brief Toggle buzzer sound state
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_buzzer_toggle(dal_buzzer_t *dev);

/**
 * @brief Play tone at specified frequency in Hz
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_buzzer_play_tone(dal_buzzer_t *dev, uint32_t freq_hz);

/**
 * @brief Stop playing tone
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_buzzer_stop_tone(dal_buzzer_t *dev);

/**
 * @brief Query current sound state
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_buzzer_is_on(const dal_buzzer_t *dev, bool *out_on);

/**
 * @brief Emergency safe-off callback for actuator registry
 */
wink_status_t dal_buzzer_safe_off(dal_buzzer_t *dev);

#ifdef __cplusplus
}
#endif

/* Compile-time pruning stubs */
#if !defined(WINK_USE_BUZZER) || !WINK_USE_BUZZER
#define WINK_BUZZER_DISABLED_MSG \
    "Buzzer driver not enabled; add a \"buzzer\" device to wink-app.json " \
    "(or set -DWINK_USE_BUZZER=ON)."
WINK_UNAVAILABLE_MSG(WINK_BUZZER_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_buzzer_init(dal_buzzer_t *dev, const dal_buzzer_config_t *cfg);
WINK_UNAVAILABLE_MSG(WINK_BUZZER_DISABLED_MSG)
wink_status_t dal_buzzer_deinit(dal_buzzer_t *dev);
WINK_UNAVAILABLE_MSG(WINK_BUZZER_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_buzzer_on(dal_buzzer_t *dev);
WINK_UNAVAILABLE_MSG(WINK_BUZZER_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_buzzer_off(dal_buzzer_t *dev);
WINK_UNAVAILABLE_MSG(WINK_BUZZER_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_buzzer_set(dal_buzzer_t *dev, bool on);
WINK_UNAVAILABLE_MSG(WINK_BUZZER_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_buzzer_toggle(dal_buzzer_t *dev);
WINK_UNAVAILABLE_MSG(WINK_BUZZER_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_buzzer_play_tone(dal_buzzer_t *dev, uint32_t freq_hz);
WINK_UNAVAILABLE_MSG(WINK_BUZZER_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_buzzer_stop_tone(dal_buzzer_t *dev);
WINK_UNAVAILABLE_MSG(WINK_BUZZER_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_buzzer_is_on(const dal_buzzer_t *dev, bool *out_on);
WINK_UNAVAILABLE_MSG(WINK_BUZZER_DISABLED_MSG)
wink_status_t dal_buzzer_safe_off(dal_buzzer_t *dev);
#endif /* !WINK_USE_BUZZER */

#endif /* DAL_BUZZER_H */
