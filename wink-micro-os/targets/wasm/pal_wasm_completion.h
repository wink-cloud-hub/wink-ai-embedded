// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_wasm_completion.h
 * @brief Unified asynchronous completion pull-model scheduler for Wasm simulation.
 *
 * Enforces Axis E non-reentrancy rules (no C->JS->C sync callbacks) and models
 * hardware timing latency via the virtual clock.
 */

#ifndef PAL_WASM_COMPLETION_H
#define PAL_WASM_COMPLETION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PAL_WASM_MAX_PENDING_COMPLETIONS
#define PAL_WASM_MAX_PENDING_COMPLETIONS 32
#endif

typedef void (*pal_wasm_completion_cb_t)(void *arg, wink_status_t result);

/**
 * @brief Schedule an asynchronous completion callback after delta_us.
 *
 * @param[in] delta_us Hardware latency modeled in microseconds
 * @param[in] cb Completion callback function
 * @param[in] arg User context passed to callback
 * @return WINK_OK on success, WINK_ERR_NO_RESOURCES if completion queue is full
 */
wink_status_t pal_wasm_schedule_complete_us(uint32_t delta_us,
                                            pal_wasm_completion_cb_t cb,
                                            void *arg);

/**
 * @brief Schedule an asynchronous completion with explicit status result.
 */
wink_status_t pal_wasm_schedule_complete_with_result(uint32_t delta_us,
                                                    pal_wasm_completion_cb_t cb,
                                                    void *arg,
                                                    wink_status_t result);

/**
 * @brief Drain all completed items whose deadline <= virtual clock now.
 *
 * Invoked by pal_wasm_advance_virtual_clock and HEADLESS loop.
 */
void pal_wasm_drain_completions(void);

/**
 * @brief Reset completion queue (testing / initialization).
 */
void pal_wasm_reset_completions(void);

/**
 * @brief Query current number of pending completions.
 */
uint32_t pal_wasm_get_pending_completions_count(void);

#ifdef __cplusplus
}
#endif

#endif /* PAL_WASM_COMPLETION_H */
