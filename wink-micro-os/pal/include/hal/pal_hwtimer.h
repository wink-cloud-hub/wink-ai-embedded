// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_hwtimer.h
 * @brief ADR-0047 Hardware Timer Subsystem interface for microsecond fast-loop control.
 */

#ifndef PAL_HWTIMER_H
#define PAL_HWTIMER_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"
#include "wink_compiler.h"
#include "pal_osal.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PAL_HWTIMERS_MAX
#define PAL_HWTIMERS_MAX 4
#endif

/**
 * @brief Hardware timer fast-loop ISR callback signature.
 *
 * ADR-0047 / ADR-0017 contract:
 *  - Must be PAL_ISR linked into IRAM;
 *  - Prohibited: SPI Flash access, mutex locks, delay/sleep, pal_log, malloc;
 *  - FreeRTOS APIs must use FromISR suffix.
 */
typedef void (*pal_hwtimer_isr_t)(void *arg);

typedef struct {
    uint8_t            timer_id;      /**< 0..PAL_HWTIMERS_MAX-1; pal_resource arbitrated */
    uint32_t           period_us;     /**< Timer period in microseconds */
    bool               oneshot;       /**< true = single shot, false = periodic auto-reload */
    bool               auto_start;    /**< true = start timer immediately after init */
    pal_os_core_id_t   core_affinity; /**< ADR-0007: Core affinity (Core 1 default for fast loop) */
    uint8_t            isr_priority;  /**< Interrupt priority tier (1..3) */
    bool               uses_fpu;      /**< true = preserve Xtensa FPU context */
    pal_hwtimer_isr_t  callback;      /**< ISR function pointer */
    void              *callback_arg;  /**< User argument passed to callback */
} pal_hwtimer_cfg_t;

/**
 * @brief Initialize a hardware timer channel.
 * @param[in] cfg Configuration struct.
 * @return WINK_OK on success, error status otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_hwtimer_init(const pal_hwtimer_cfg_t *cfg);

/**
 * @brief Start counting and arm the hardware timer interrupt.
 * @param[in] timer_id Hardware timer channel ID.
 * @return WINK_OK on success.
 */
wink_status_t pal_hwtimer_start(uint8_t timer_id);

/**
 * @brief Stop counting and disarm the hardware timer interrupt.
 * @param[in] timer_id Hardware timer channel ID.
 * @return WINK_OK on success.
 */
wink_status_t pal_hwtimer_stop(uint8_t timer_id);

/**
 * @brief Dynamically change timer period.
 * @param[in] timer_id Hardware timer channel ID.
 * @param[in] new_period_us New period in microseconds.
 * @return WINK_OK on success.
 */
wink_status_t pal_hwtimer_change_period(uint8_t timer_id, uint32_t new_period_us);

/**
 * @brief Deinitialize hardware timer channel and release claimed resources.
 * @param[in] timer_id Hardware timer channel ID.
 */
void pal_hwtimer_deinit(uint8_t timer_id);

/**
 * @brief Simulation soft edge trigger (Unit tests and Wasm virtual clock only).
 * @param[in] timer_id Hardware timer channel ID.
 * @return WINK_OK if fired, WINK_ERR_UNSUPPORTED on physical hardware in production.
 */
wink_status_t pal_hwtimer_fire_soft(uint8_t timer_id);

#ifdef __cplusplus
}
#endif

#endif /* PAL_HWTIMER_H */
