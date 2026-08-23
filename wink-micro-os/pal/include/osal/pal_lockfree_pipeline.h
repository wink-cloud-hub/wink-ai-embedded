// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_lockfree_pipeline.h
 * @brief Lock-free SPSC double-buffered fast/slow control loop communication pipeline.
 *
 * Implements acquire/release lock-free data transfer between FreeRTOS slow tasks
 * and microsecond IRAM fast ISRs with zero locking overhead and guaranteed torn-read immunity.
 */

#ifndef PAL_LOCKFREE_PIPELINE_H
#define PAL_LOCKFREE_PIPELINE_H

#include <stdint.h>
#include <stdbool.h>
#include "osal/pal_atomic.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int16_t q15_t;
typedef int32_t q31_t;

/**
 * @brief Standard FOC slow-to-fast loop command payload.
 */
typedef struct {
    q15_t    target_speed_q15;
    q15_t    target_angle_q15;
    uint32_t seq_id;
} foc_slow_to_fast_cmd_t;

/**
 * @brief Standard FOC fast-to-slow loop status payload.
 */
typedef struct {
    q15_t    actual_current_q15;
    q15_t    actual_velocity_q15;
    uint16_t fault_flags;
    uint32_t seq_id;
} foc_fast_to_slow_status_t;

/**
 * @brief Lock-free bidirectional double-buffered pipeline.
 */
typedef struct {
    volatile foc_slow_to_fast_cmd_t    cmd_slot[2];
    volatile uint8_t                   cmd_idx;
    volatile foc_fast_to_slow_status_t stat_slot[2];
    volatile uint8_t                   stat_idx;
} foc_pipeline_t;

#define FOC_PIPELINE_INITIALIZER { .cmd_idx = 0, .stat_idx = 0 }

/**
 * @brief Initialize a lock-free pipeline instance.
 */
static inline void foc_pipeline_init(foc_pipeline_t *p) {
    if (p == NULL) return;
    p->cmd_idx = 0;
    p->stat_idx = 0;
    p->cmd_slot[0] = (foc_slow_to_fast_cmd_t){0};
    p->cmd_slot[1] = (foc_slow_to_fast_cmd_t){0};
    p->stat_slot[0] = (foc_fast_to_slow_status_t){0};
    p->stat_slot[1] = (foc_fast_to_slow_status_t){0};
}

/**
 * @brief Publish new command from slow task (Task context, single writer).
 */
static inline void foc_publish_cmd(foc_pipeline_t *p, const foc_slow_to_fast_cmd_t *cmd) {
    uint8_t w = 1u - (uint8_t)PAL_ATOMIC_LOAD(&p->cmd_idx, PAL_ACQ);
    p->cmd_slot[w] = *cmd;
    PAL_ATOMIC_STORE(&p->cmd_idx, w, PAL_REL);
}

/**
 * @brief Consume latest command in fast ISR (ISR context, single reader).
 */
static inline foc_slow_to_fast_cmd_t foc_consume_cmd(const foc_pipeline_t *p) {
    uint8_t i = (uint8_t)PAL_ATOMIC_LOAD(&p->cmd_idx, PAL_ACQ);
    return p->cmd_slot[i];
}

/**
 * @brief Publish new status from fast ISR (ISR context, single writer).
 */
static inline void foc_publish_status(foc_pipeline_t *p, const foc_fast_to_slow_status_t *st) {
    uint8_t w = 1u - (uint8_t)PAL_ATOMIC_LOAD(&p->stat_idx, PAL_ACQ);
    p->stat_slot[w] = *st;
    PAL_ATOMIC_STORE(&p->stat_idx, w, PAL_REL);
}

/**
 * @brief Consume latest status in slow task (Task context, single reader).
 */
static inline foc_fast_to_slow_status_t foc_consume_status(const foc_pipeline_t *p) {
    uint8_t i = (uint8_t)PAL_ATOMIC_LOAD(&p->stat_idx, PAL_ACQ);
    return p->stat_slot[i];
}

#ifdef __cplusplus
}
#endif

#endif /* PAL_LOCKFREE_PIPELINE_H */
