// SPDX-License-Identifier: Apache-2.0
/**
 * @file wink_soft_timer.h
 * @brief Software timer scheduler interface (ADR-0007).
 */
#ifndef WINK_SOFT_TIMER_H
#define WINK_SOFT_TIMER_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Software timer mode */
typedef enum {
    WINK_TIMER_ONESHOT  = 0,  /**< Single shot execution */
    WINK_TIMER_PERIODIC = 1,  /**< Periodic repetition */
} wink_timer_mode_t;

/** @brief Software timer callback prototype */
typedef wink_status_t (*wink_soft_timer_callback_t)(void* arg);

/**
 * @brief Initialize software timer subsystem
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_soft_timer_init(void);

/**
 * @brief Create a new software timer
 *
 * @param[in] callback Callback function pointer.
 * @param[in] arg Context argument pointer.
 * @param[in] mode Timer mode (oneshot or periodic).
 * @param[in] period_ms Timer period in ms.
 * @return Timer handle (>= 0) on success, error status code (< 0) otherwise.
 */
WINK_WARN_UNUSED_RESULT
int32_t wink_soft_timer_create(
    wink_soft_timer_callback_t callback,
    void* arg,
    wink_timer_mode_t mode,
    uint32_t period_ms
);

/**
 * @brief Start software timer
 *
 * @param[in] handle Timer handle.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_soft_timer_start(int32_t handle);

/**
 * @brief Stop software timer
 *
 * @param[in] handle Timer handle.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_soft_timer_stop(int32_t handle);

/**
 * @brief Destroy software timer and release slot
 *
 * @param[in] handle Timer handle.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_soft_timer_destroy(int32_t handle);

/**
 * @brief Dynamically change timer period
 *
 * @param[in] handle Timer handle.
 * @param[in] period_ms New period in ms.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_soft_timer_change_period(int32_t handle, uint32_t period_ms);

/**
 * @brief Dispatch expired software timers
 */
void wink_soft_timer_dispatch(void);

/**
 * @brief Check if currently in LIGHT dispatch context
 * @return True if in LIGHT callback dispatch context.
 */
bool wink_soft_timer_in_light_dispatch(void);

/**
 * @brief Attach diagnostic name to timer slot
 *
 * @param[in] handle Timer handle.
 * @param[in] name Diagnostic name string.
 */
void wink_soft_timer_set_name(int32_t handle, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* WINK_SOFT_TIMER_H */
