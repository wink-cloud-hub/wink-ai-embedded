// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_deferred.h
 * @brief PAL Deferred-Call Worker (ISR Bottom-Half) Subsystem.
 *
 * Provides a standardized, zero-allocation mechanism to defer execution
 * from ISR or high-rate contexts into dedicated task workers.
 */
#ifndef PAL_DEFERRED_H
#define PAL_DEFERRED_H

#include "wink_status.h"
#include "wink_compiler.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PAL_DEFERRED_HI_QUEUE_CAPACITY 32
#define PAL_DEFERRED_LO_QUEUE_CAPACITY 64

/**
 * @brief Deferred worker priority levels.
 */
typedef enum {
    PAL_DEFERRED_HI = 0,   /**< High-priority worker (pinned to Core 1, fast responses: brake/capture) */
    PAL_DEFERRED_LO = 1,   /**< Low-priority worker (RMT RX, UART, GPIO debounce, background logging) */
    PAL_DEFERRED_PRI_COUNT = 2
} pal_deferred_pri_t;

/**
 * @brief Overflow handling policy when deferred queue is full.
 */
typedef enum {
    PAL_DEFERRED_LOSSY = 0,    /**< Queue full returns WINK_ERR_BUSY, increments drop counter (RMT RX/UART/GPIO) */
    PAL_DEFERRED_CRITICAL = 1  /**< Queue full logs fault and triggers safe fault state, never drops silently (Brake) */
} pal_deferred_policy_t;

/**
 * @brief Callback function type executed in worker task context.
 */
typedef void (*pal_deferred_cb_t)(void *arg);

/**
 * @brief Initialize the deferred-call worker subsystem.
 *
 * Allocates static task stacks and counting semaphores.
 *
 * @param[in] core_id Core affinity for high-priority worker (typically 1 on ESP32, 0xFF for unpinned).
 * @return WINK_OK on success, or error status.
 */
WINK_WARN_UNUSED_RESULT wink_status_t pal_deferred_init(uint8_t core_id);

/**
 * @brief De-initialize the deferred-call worker subsystem.
 */
void pal_deferred_deinit(void);

/**
 * @brief Post a deferred callback from an ISR context (zero-allocation, FromISR-safe).
 *
 * Uses counting semaphore to signal worker task.
 *
 * @param[in] pri High or Low priority worker.
 * @param[in] policy Overflow policy (LOSSY or CRITICAL).
 * @param[in] cb Callback function to execute in task context.
 * @param[in] arg User argument passed to callback.
 * @return WINK_OK on success, WINK_ERR_BUSY if full (LOSSY), or error.
 */
wink_status_t pal_deferred_post_from_isr(pal_deferred_pri_t pri,
                                        pal_deferred_policy_t policy,
                                        pal_deferred_cb_t cb,
                                        void *arg);

/**
 * @brief Post a deferred callback from a task/thread context.
 *
 * @param[in] pri High or Low priority worker.
 * @param[in] policy Overflow policy (LOSSY or CRITICAL).
 * @param[in] cb Callback function to execute in task context.
 * @param[in] arg User argument passed to callback.
 * @return WINK_OK on success, WINK_ERR_BUSY if full, or error.
 */
wink_status_t pal_deferred_post(pal_deferred_pri_t pri,
                               pal_deferred_policy_t policy,
                               pal_deferred_cb_t cb,
                               void *arg);

/**
 * @brief Get high-water mark and dropped counter for diagnostic monitoring.
 *
 * @param[in] pri Queue priority level.
 * @param[out] out_high_water_slots Peak slots used simultaneously.
 * @param[out] out_dropped_count Total dropped callbacks.
 */
void pal_deferred_get_metrics(pal_deferred_pri_t pri,
                             size_t *out_high_water_slots,
                             uint32_t *out_dropped_count);

#ifdef __cplusplus
}
#endif

#endif /* PAL_DEFERRED_H */
