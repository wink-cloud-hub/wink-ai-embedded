// SPDX-License-Identifier: Apache-2.0
#ifndef DAL_DC_MOTOR_H
#define DAL_DC_MOTOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_status.h"
#include "pal_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Brushed DC motor electrical topology (Phase 1: IN/IN only).
 */
typedef enum {
    DAL_DC_MOTOR_VARIANT_IN_IN = 0,        /**< Default - PWM + IN_A + IN_B */
    DAL_DC_MOTOR_VARIANT_PHASE_ENABLE = 1, /**< Reserved */
    DAL_DC_MOTOR_VARIANT_PWM_ON_IN = 2,    /**< Reserved */
} dal_dc_motor_variant_t;

/**
 * @brief Brushed DC motor (H-bridge PWM) configuration.
 *
 * Control semantic: open-loop duty / signed speed (ADR-0048).
 * Default topology: IN/IN (PWM speed + two direction inputs).
 *
 * IN/IN truth table (dir_pin_a = A, dir_pin_b = B):
 * @code
 *   dir_a  dir_b | state
 *     0      0   | coast
 *     1      0   | forward
 *     0      1   | reverse
 *     1      1   | brake (short)
 * @endcode
 */
typedef struct {
    const char *owner;              /**< Resource-claim owner static string */
    uint32_t pwm_freq_hz;           /**< PWM frequency; 0 -> default 20000 Hz */
    dal_dc_motor_variant_t variant; /**< 0 = IN_IN */
    wink_pin_t dir_pin_a;           /**< Direction pin A */
    wink_pin_t dir_pin_b;           /**< Direction pin B (optional; -1 if unused) */
    wink_pin_t enable_pin;          /**< STBY/nSLEEP pin (-1 if unused) */
    uint8_t pwm_channel;            /**< PWM channel for speed */
    bool invert;                    /**< true = swap forward/reverse direction */
} dal_dc_motor_config_t;

/**
 * @brief Brushed DC motor handle (POD).
 */
typedef struct {
    dal_dc_motor_config_t config; /**< Config copy */
    int16_t current_speed_promille; /**< Last set speed in promille [-1000, 1000] */
    bool initialized;             /**< Init succeeded */
} dal_dc_motor_t;

_Static_assert(offsetof(dal_dc_motor_t, config) == 0, "config must be the first member");

#if INTPTR_MAX == INT32_MAX   /* ILP32: ESP32 xtensa, wasm32 */
_Static_assert(sizeof(dal_dc_motor_config_t) == 20, "ABI break: config size changed on 32-bit target");
_Static_assert(offsetof(dal_dc_motor_t, initialized) == 22, "ABI break: initialized offset changed on 32-bit");
_Static_assert(sizeof(dal_dc_motor_t) == 24, "ABI break: handle size changed on 32-bit target");
#else                         /* LP64 / LLP64: 64-bit Host Simulation */
_Static_assert(sizeof(dal_dc_motor_config_t) == 24, "ABI break: config size changed on 64-bit host");
_Static_assert(offsetof(dal_dc_motor_t, initialized) == 26, "ABI break: initialized offset changed on 64-bit host");
_Static_assert(sizeof(dal_dc_motor_t) == 32, "ABI break: handle size changed on 64-bit host");
#endif

/**
 * @brief Initialize brushed DC motor driver.
 *
 * Claims PWM channel + direction GPIO (and optional enable pin) via PAL resource manager.
 *
 * @param[in,out] dev Motor instance handle (caller-owned storage).
 * @param[in] cfg Configuration struct.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_dc_motor_init(dal_dc_motor_t *dev,
                                const dal_dc_motor_config_t *cfg);

/**
 * @brief Set open-loop speed in promille [-1000, 1000].
 *
 * @param[in,out] dev Motor instance handle.
 * @param[in] speed_promille Speed in promille [-1000 (full reverse) ... 1000 (full forward)].
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_dc_motor_set_speed_promille(dal_dc_motor_t *dev, int16_t speed_promille);

/**
 * @brief Read back the current set speed in promille.
 *
 * @param[in] dev Motor instance handle.
 * @param[out] out_speed_promille Pointer to store speed in promille [-1000, 1000].
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_dc_motor_get_speed_promille(const dal_dc_motor_t *dev, int16_t *out_speed_promille);

/**
 * @brief Short-brake: both direction pins HIGH, PWM duty 0.
 *
 * @param[in,out] dev Motor instance handle.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_dc_motor_brake(dal_dc_motor_t *dev);

/**
 * @brief Freewheel / coast: direction pins inactive (LOW), PWM duty 0.
 *
 * @param[in,out] dev Motor instance handle.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_dc_motor_coast(dal_dc_motor_t *dev);

/**
 * @brief Emergency / fault safe-off (ADR-0048): bound to brake (+ enable LOW).
 *
 * @param[in,out] dev Motor instance handle.
 * @return WINK_OK on success, error status code otherwise.
 */
wink_status_t dal_dc_motor_safe_off(dal_dc_motor_t *dev);

/**
 * @brief Deinitialize brushed DC motor driver.
 *
 * ADR-0024 cleanup: safe_off -> stop PWM -> GPIO reset -> release resources -> memset zero.
 *
 * @param[in,out] dev Motor instance handle.
 * @return WINK_OK on success, error status code otherwise.
 */
wink_status_t dal_dc_motor_deinit(dal_dc_motor_t *dev);

#ifdef __cplusplus
}
#endif

/* Compile-time pruning stubs */
#if !defined(WINK_USE_DC_MOTOR) || !WINK_USE_DC_MOTOR
#define WINK_DC_MOTOR_DISABLED_MSG \
    "DC motor driver not enabled; add a \"dc_motor\" device to " \
    "wink-app.json (or set -DWINK_USE_DC_MOTOR=ON)."
WINK_UNAVAILABLE_MSG(WINK_DC_MOTOR_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_dc_motor_init(dal_dc_motor_t *dev,
                                const dal_dc_motor_config_t *cfg);
WINK_UNAVAILABLE_MSG(WINK_DC_MOTOR_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_dc_motor_set_speed_promille(dal_dc_motor_t *dev, int16_t speed_promille);
WINK_UNAVAILABLE_MSG(WINK_DC_MOTOR_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_dc_motor_get_speed_promille(const dal_dc_motor_t *dev, int16_t *out_speed_promille);
WINK_UNAVAILABLE_MSG(WINK_DC_MOTOR_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_dc_motor_brake(dal_dc_motor_t *dev);
WINK_UNAVAILABLE_MSG(WINK_DC_MOTOR_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_dc_motor_coast(dal_dc_motor_t *dev);
WINK_UNAVAILABLE_MSG(WINK_DC_MOTOR_DISABLED_MSG) wink_status_t dal_dc_motor_safe_off(dal_dc_motor_t *dev);
WINK_UNAVAILABLE_MSG(WINK_DC_MOTOR_DISABLED_MSG)
wink_status_t dal_dc_motor_deinit(dal_dc_motor_t *dev);
#endif /* !WINK_USE_DC_MOTOR */

#endif /* DAL_DC_MOTOR_H */
