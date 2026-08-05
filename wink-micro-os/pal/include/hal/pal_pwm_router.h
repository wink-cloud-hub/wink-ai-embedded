// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_pwm_router.h
 * @brief PAL PWM Hardware Timer Router & Allocation Manager (ADR-0034).
 */

#ifndef PAL_PWM_ROUTER_H
#define PAL_PWM_ROUTER_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PAL_PWM_TIMERS 4

/** @brief Effective LEDC clock source (target-resolved; ADR-0034). */
enum {
    PAL_PWM_EFF_CLK_PLATFORM_AUTO = 0,
    PAL_PWM_EFF_CLK_REF_TICK      = 1,
};

typedef struct {
    uint32_t freq_hz;
    uint8_t  resolution_bits;  /**< Effective resolution in bits (>0) */
    uint8_t  clock_source;     /**< PAL_PWM_EFF_CLK_* */
} pal_pwm_timer_profile_t;

typedef enum {
    PAL_PWM_TIMER_FREE = 0,
    PAL_PWM_TIMER_USED = 1,
} pal_pwm_timer_state_t;

/**
 * @brief Construct default timer profile (13-bit resolution + platform auto clock)
 * @param[in] freq_hz Target PWM frequency in Hz
 * @return Populated timer profile struct
 */
static inline pal_pwm_timer_profile_t pal_pwm_timer_profile_default(uint32_t freq_hz)
{
    pal_pwm_timer_profile_t p;
    p.freq_hz = freq_hz;
    p.resolution_bits = 13u;
    p.clock_source = PAL_PWM_EFF_CLK_PLATFORM_AUTO;
    return p;
}

/**
 * @brief Acquire a PWM hardware timer matching requested profile
 * @param[in] channel PWM channel ID
 * @param[in] profile Pointer to requested timer profile
 * @param[out] out_timer_num Output pointer for allocated timer ID
 * @return WINK_OK on success, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_pwm_router_acquire(uint8_t channel,
                                     const pal_pwm_timer_profile_t *profile,
                                     uint8_t *out_timer_num);

/**
 * @brief Release timer allocated to specified PWM channel
 * @param[in] channel PWM channel ID
 */
void pal_pwm_router_release(uint8_t channel);

/**
 * @brief Check if PWM channel is ready and allocated
 * @param[in] channel PWM channel ID
 * @return true if ready, false otherwise
 */
bool pal_pwm_router_channel_ready(uint8_t channel);

/**
 * @brief Query hardware timer ID allocated to PWM channel
 * @param[in] channel PWM channel ID
 * @return Hardware timer ID
 */
uint8_t pal_pwm_router_channel_timer(uint8_t channel);

/** @brief Reset PWM router state (for test isolation) */
void pal_pwm_router_reset(void);

/**
 * @brief Calculate integer-safe raw duty cycle value from percentage for target resolution bits
 * @param[in] percent Duty cycle percentage [0.0, 100.0]
 * @param[in] bits Target resolution bits
 * @return Raw duty integer value
 */
uint32_t pal_pwm_percent_to_raw(float percent, uint8_t bits);

#ifdef __cplusplus
}
#endif

#endif /* PAL_PWM_ROUTER_H */
