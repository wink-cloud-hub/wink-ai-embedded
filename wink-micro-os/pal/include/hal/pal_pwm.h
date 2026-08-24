// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_pwm.h
 * @brief PAL PWM Interface Subsystem with Basis Points, Dynamic Pin Routing, and Zero Soft-FP (ADR-0066).
 */

#ifndef PAL_PWM_H
#define PAL_PWM_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"
#include "hal/pal_pin_types.h"
#include "hal/pal_target_caps.h"
#include "wink_compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PAL_PWM_CLOCK_AUTO            = 0,
    PAL_PWM_CLOCK_STABLE_REQUIRED = 1,
} pal_pwm_clock_requirement_t;

typedef struct {
    uint32_t                    struct_size;       /**< Size of struct for forward ABI compatibility */
    wink_pin_t                  pin;               /**< Target physical GPIO pin (WINK_PIN_NC = use channel default) */
    uint32_t                    freq_hz;           /**< PWM base frequency in Hz */
    uint8_t                     resolution_bits;   /**< 0 = AUTO -> target optimal default */
    pal_pwm_clock_requirement_t clock_requirement; /**< Clock source stability requirement */
} pal_pwm_config_t;

/**
 * @brief Query physical GPIO mapped to specified PWM channel
 * @param[in] channel PWM channel ID [0, PAL_PWM_CHANNEL_MAX)
 * @param[out] out_pin Output pointer for mapped GPIO pin
 * @return WINK_OK on success, WINK_ERR_INVALID_ARG on out of bounds, WINK_ERR_UNSUPPORTED if target lacks routing
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_pwm_channel_pin(uint8_t channel, wink_pin_t *out_pin);

/**
 * @brief Legacy basic init wrapper (default pin + auto clock)
 * @param[in] channel PWM channel ID [0, PAL_PWM_CHANNEL_MAX)
 * @param[in] frequency_hz PWM frequency in Hz
 * @return WINK_OK on success, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_pwm_init(uint8_t channel, uint32_t frequency_hz);

/**
 * @brief Extended init: dynamic pin routing + freq + resolution + clock requirement
 * @param[in] channel PWM channel ID [0, PAL_PWM_CHANNEL_MAX)
 * @param[in] cfg Extended PWM configuration struct
 * @return WINK_OK on success, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_pwm_init_ex(uint8_t channel, const pal_pwm_config_t *cfg);

/**
 * @brief Set PWM duty cycle in Basis Points (0..10000 = 0.00%..100.00%)
 * @note ISR-Safe. Guarantees 0 soft-float library overhead and zero 32-bit overflow.
 * @param[in] channel PWM channel ID
 * @param[in] basis_points Duty cycle in basis points [0, 10000]
 * @return WINK_OK on success, WINK_ERR_INVALID_ARG on out of range (>10000)
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_pwm_set_duty_bp(uint8_t channel, uint16_t basis_points);

#ifndef PAL_PWM_HIDE_FLOAT_API
/**
 * @brief Legacy floating-point duty cycle setter (0.0f..1.0f)
 * @deprecated Use pal_pwm_set_duty_bp instead to eliminate soft-fp code bloat (ADR-0066).
 * @param[in] channel PWM channel ID
 * @param[in] duty Duty cycle in range [0.0f, 1.0f]
 * @return WINK_OK on success, error status code otherwise
 */
WINK_DEPRECATED_MSG("Use pal_pwm_set_duty_bp instead to eliminate soft-fp library overhead")
WINK_WARN_UNUSED_RESULT
wink_status_t pal_pwm_set_duty(uint8_t channel, float duty);
#endif /* PAL_PWM_HIDE_FLOAT_API */

/**
 * @brief Dynamically adjust PWM frequency on active channel
 * @param[in] channel PWM channel ID
 * @param[in] freq_hz New frequency in Hz
 * @return WINK_OK on success, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_pwm_set_freq(uint8_t channel, uint32_t freq_hz);

/**
 * @brief Deinitialize specified PWM channel and release claimed pin & channel resources
 * @param[in] channel PWM channel ID
 * @return WINK_OK on success, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_pwm_deinit(uint8_t channel);

#ifdef __cplusplus
}
#endif

#endif /* PAL_PWM_H */
