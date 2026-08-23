// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_hwtimer_stub.h
 * @brief Host target testing stub hooks for hardware timer simulation.
 */
#ifndef PAL_HWTIMER_STUB_H
#define PAL_HWTIMER_STUB_H

#include <stdint.h>
#include <stdbool.h>
#include "hal/pal_hwtimer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get total number of callback invocations for a simulated timer.
 * @param[in] timer_id Hardware timer channel ID.
 * @return Total callback count.
 */
uint32_t stub_hwtimer_get_callback_count(uint8_t timer_id);

/**
 * @brief Query if a simulated timer is currently active/running.
 * @param[in] timer_id Hardware timer channel ID.
 * @return true if running.
 */
bool stub_hwtimer_is_running(uint8_t timer_id);

/**
 * @brief Reset stub statistics for a timer channel.
 * @param[in] timer_id Hardware timer channel ID.
 */
void stub_hwtimer_reset(uint8_t timer_id);

#ifdef __cplusplus
}
#endif

#endif /* PAL_HWTIMER_STUB_H */
