// SPDX-License-Identifier: Apache-2.0
#ifndef DAL_RC_SERVO_H
#define DAL_RC_SERVO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief RC Servo PWM clock requirement (DAL level) */
typedef uint8_t dal_rc_servo_clock_requirement_t;

enum {
    DAL_RC_SERVO_CLOCK_AUTO            = 0,
    DAL_RC_SERVO_CLOCK_STABLE_REQUIRED = 1,
};

/**
 * @brief RC Servo configuration struct (input for dal_rc_servo_init)
 */
typedef struct {
    const char                    *owner;              /**< Instance owner static string */
    uint16_t                       min_pulse_us;       /**< Minimum pulse width in µs (typically 500) */
    uint16_t                       max_pulse_us;       /**< Maximum pulse width in µs (typically 2500) */
    uint16_t                       max_angle_ddeg;     /**< Maximum stroke angle in 0.1° (0 = default 1800 = 180.0°) */
    uint8_t                        pwm_channel;        /**< PWM channel ID [0, PAL_PWM_CHANNELS) */
    uint8_t                        resolution_bits;    /**< 0 = AUTO -> target default 13-bit */
    dal_rc_servo_clock_requirement_t clock_requirement; /**< 0 = AUTO */
} dal_rc_servo_config_t;

/**
 * @brief RC Servo instance handle (POD).
 */
typedef struct {
    dal_rc_servo_config_t config;          /**< Config copy */
    uint16_t              current_angle_ddeg; /**< State: Current angle (0.1°, clamped) */
    bool                  initialized;     /**< State: Set to true after successful init */
} dal_rc_servo_t;

_Static_assert(offsetof(dal_rc_servo_t, config) == 0, "config must be the first member");

#if INTPTR_MAX == INT32_MAX   /* ILP32: ESP32 xtensa, wasm32 */
_Static_assert(sizeof(dal_rc_servo_config_t) == 16, "ABI break: config size changed on 32-bit target");
_Static_assert(offsetof(dal_rc_servo_t, initialized) == 18, "ABI break: initialized offset changed on 32-bit");
_Static_assert(sizeof(dal_rc_servo_t) == 20, "ABI break: handle size changed on 32-bit target");
#else                         /* LP64 / LLP64: 64-bit Host Simulation */
_Static_assert(sizeof(dal_rc_servo_config_t) == 24, "ABI break: config size changed on 64-bit host");
_Static_assert(offsetof(dal_rc_servo_t, initialized) == 26, "ABI break: initialized offset changed on 64-bit host");
_Static_assert(sizeof(dal_rc_servo_t) == 32, "ABI break: handle size changed on 64-bit host");
#endif

/**
 * @brief Initialize RC servo: validate config, claim PWM channel, program 50Hz, zero duty.
 *
 * @param[in,out] dev Servo instance handle.
 * @param[in] cfg Configuration struct.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_rc_servo_init(dal_rc_servo_t *dev, const dal_rc_servo_config_t *cfg);

/**
 * @brief Set servo target angle in 0.1° (ddeg).
 *
 * @param[in,out] dev Servo instance handle.
 * @param[in] angle_ddeg Target angle in 0.1° (e.g., 900 = 90.0°, 1800 = 180.0°).
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_rc_servo_set_angle(dal_rc_servo_t *dev, uint16_t angle_ddeg);

/**
 * @brief Servo safe-off (duty=0 -> limp).
 *
 * @param[in,out] dev Servo instance handle.
 * @return WINK_OK on success, error status code otherwise.
 */
wink_status_t dal_rc_servo_safe_off(dal_rc_servo_t *dev);

/**
 * @brief Apply Flash parameter overrides before initialization (ADR-0008).
 *
 * @param[in,out] dev Servo instance handle pointer.
 * @param[in] params Binary parameter buffer.
 * @param[in] len Parameter length in bytes.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_rc_servo_apply_override(void *dev, const uint8_t *params, uint16_t len);

/**
 * @brief Deinitialize RC servo driver.
 *
 * Safe-off -> stop PWM -> reset GPIO -> release resources -> memset zero.
 *
 * @param[in,out] dev Servo instance handle.
 * @return WINK_OK on success, error status code otherwise.
 */
wink_status_t dal_rc_servo_deinit(dal_rc_servo_t *dev);

#ifdef __cplusplus
}
#endif

/* Compile-time pruning stubs */
#if !defined(WINK_USE_RC_SERVO) || !WINK_USE_RC_SERVO
#define WINK_RC_SERVO_DISABLED_MSG \
    "RC servo driver not enabled; add a \"rc_servo\" device to wink-app.json " \
    "(or set -DWINK_USE_RC_SERVO=ON)."
WINK_UNAVAILABLE_MSG(WINK_RC_SERVO_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_rc_servo_init(dal_rc_servo_t *dev, const dal_rc_servo_config_t *cfg);
WINK_UNAVAILABLE_MSG(WINK_RC_SERVO_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_rc_servo_set_angle(dal_rc_servo_t *dev, uint16_t angle_ddeg);
WINK_UNAVAILABLE_MSG(WINK_RC_SERVO_DISABLED_MSG)
wink_status_t dal_rc_servo_safe_off(dal_rc_servo_t *dev);
WINK_UNAVAILABLE_MSG(WINK_RC_SERVO_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_rc_servo_apply_override(void *dev, const uint8_t *params, uint16_t len);
WINK_UNAVAILABLE_MSG(WINK_RC_SERVO_DISABLED_MSG)
wink_status_t dal_rc_servo_deinit(dal_rc_servo_t *dev);
#endif /* !WINK_USE_RC_SERVO */

#endif /* DAL_RC_SERVO_H */
