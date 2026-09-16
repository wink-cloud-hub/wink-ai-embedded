// SPDX-License-Identifier: LGPL-3.0-only
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
#include <stddef.h>
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
    p->cmd_slot[0].target_speed_q15 = 0;
    p->cmd_slot[0].target_angle_q15 = 0;
    p->cmd_slot[0].seq_id = 0;
    p->cmd_slot[1].target_speed_q15 = 0;
    p->cmd_slot[1].target_angle_q15 = 0;
    p->cmd_slot[1].seq_id = 0;
    p->stat_slot[0].actual_current_q15 = 0;
    p->stat_slot[0].actual_velocity_q15 = 0;
    p->stat_slot[0].fault_flags = 0;
    p->stat_slot[0].seq_id = 0;
    p->stat_slot[1].actual_current_q15 = 0;
    p->stat_slot[1].actual_velocity_q15 = 0;
    p->stat_slot[1].fault_flags = 0;
    p->stat_slot[1].seq_id = 0;
}

/**
 * @brief Publish new command from slow task (Task context, single writer).
 */
static inline void foc_publish_cmd(foc_pipeline_t *p, const foc_slow_to_fast_cmd_t *cmd) {
    uint8_t w = 1u - (uint8_t)PAL_ATOMIC_LOAD(&p->cmd_idx, PAL_ACQ);
    p->cmd_slot[w].target_speed_q15 = cmd->target_speed_q15;
    p->cmd_slot[w].target_angle_q15 = cmd->target_angle_q15;
    p->cmd_slot[w].seq_id = cmd->seq_id;
    PAL_ATOMIC_STORE(&p->cmd_idx, w, PAL_REL);
}

/**
 * @brief Consume latest command in fast ISR (ISR context, single reader).
 */
static inline foc_slow_to_fast_cmd_t foc_consume_cmd(const foc_pipeline_t *p) {
    uint8_t i = (uint8_t)PAL_ATOMIC_LOAD(&p->cmd_idx, PAL_ACQ);
    foc_slow_to_fast_cmd_t res;
    res.target_speed_q15 = p->cmd_slot[i].target_speed_q15;
    res.target_angle_q15 = p->cmd_slot[i].target_angle_q15;
    res.seq_id = p->cmd_slot[i].seq_id;
    return res;
}

/**
 * @brief Publish new status from fast ISR (ISR context, single writer).
 */
static inline void foc_publish_status(foc_pipeline_t *p, const foc_fast_to_slow_status_t *st) {
    uint8_t w = 1u - (uint8_t)PAL_ATOMIC_LOAD(&p->stat_idx, PAL_ACQ);
    p->stat_slot[w].actual_current_q15 = st->actual_current_q15;
    p->stat_slot[w].actual_velocity_q15 = st->actual_velocity_q15;
    p->stat_slot[w].fault_flags = st->fault_flags;
    p->stat_slot[w].seq_id = st->seq_id;
    PAL_ATOMIC_STORE(&p->stat_idx, w, PAL_REL);
}

/**
 * @brief Consume latest status in slow task (Task context, single reader).
 */
static inline foc_fast_to_slow_status_t foc_consume_status(const foc_pipeline_t *p) {
    uint8_t i = (uint8_t)PAL_ATOMIC_LOAD(&p->stat_idx, PAL_ACQ);
    foc_fast_to_slow_status_t res;
    res.actual_current_q15 = p->stat_slot[i].actual_current_q15;
    res.actual_velocity_q15 = p->stat_slot[i].actual_velocity_q15;
    res.fault_flags = p->stat_slot[i].fault_flags;
    res.seq_id = p->stat_slot[i].seq_id;
    return res;
}

#ifdef __cplusplus
}
#endif

#endif /* PAL_LOCKFREE_PIPELINE_H */
