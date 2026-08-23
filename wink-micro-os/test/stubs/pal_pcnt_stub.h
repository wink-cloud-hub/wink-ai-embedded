// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_pcnt_stub.h
 * @brief Host target testing stub hooks for pulse counter simulation and count injection.
 */
#ifndef PAL_PCNT_STUB_H
#define PAL_PCNT_STUB_H

#include <stdint.h>
#include <stddef.h>
#include "hal/pal_pcnt.h"
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Set the current counter value of a PCNT unit handle in host simulation.
 * @param[in] handle Unit handle
 * @param[in] count 64-bit count value
 */
void stub_pcnt_set_count(pal_pcnt_unit_handle_t handle, int64_t count);

/**
 * @brief Add a step/delta to the current counter value of a PCNT unit handle.
 * @param[in] handle Unit handle
 * @param[in] delta Step count (positive or negative)
 */
void stub_pcnt_step(pal_pcnt_unit_handle_t handle, int64_t delta);

/**
 * @brief Force the next operation on a PCNT handle to fail.
 * @param[in] handle Unit handle
 * @param[in] err Error status
 */
void stub_pcnt_force_failure(pal_pcnt_unit_handle_t handle, wink_status_t err);

#ifdef __cplusplus
}
#endif

#endif /* PAL_PCNT_STUB_H */
