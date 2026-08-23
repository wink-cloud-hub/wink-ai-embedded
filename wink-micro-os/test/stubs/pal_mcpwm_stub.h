// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_mcpwm_stub.h
 * @brief Host testing stub hooks for motor control PWM simulation.
 */
#ifndef PAL_MCPWM_STUB_H
#define PAL_MCPWM_STUB_H

#include <stdint.h>
#include <stdbool.h>
#include "hal/pal_mcpwm.h"

#ifdef __cplusplus
extern "C" {
#endif

uint32_t stub_mcpwm_get_duty_ticks(pal_mcpwm_cmp_handle_t cmp);
bool     stub_mcpwm_is_timer_running(pal_mcpwm_timer_handle_t t);
void     stub_mcpwm_trigger_brake(pal_mcpwm_fault_handle_t f);
void     stub_mcpwm_trigger_capture(pal_mcpwm_cap_handle_t cap, uint32_t ts_ns, bool rising);

#ifdef __cplusplus
}
#endif

#endif /* PAL_MCPWM_STUB_H */
