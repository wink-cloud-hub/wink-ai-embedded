/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_TIMER_HAL_H
#define WINK_H_GUARD_HAL_TIMER_HAL_H
#ifndef __WINK_HARVESTED_HAL_TIMER_HAL_H__
#define __WINK_HARVESTED_HAL_TIMER_HAL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct timg_dev_t * gptimer_soc_handle_t;
typedef struct {
    gptimer_soc_handle_t dev;
    uint32_t timer_id;
} timer_hal_context_t;



#if defined(__WINK_SIM__)
uint64_t timer_hal_capture_and_get_counter_value(timer_hal_context_t *hal) WINK_SLA_ERROR("Wink SLA Violation: timer_hal_capture_and_get_counter_value out of Core 8 scope.");
#else
uint64_t timer_hal_capture_and_get_counter_value(timer_hal_context_t *hal);
#endif

#if defined(__WINK_SIM__)
void timer_hal_deinit(timer_hal_context_t *hal) WINK_SLA_ERROR("Wink SLA Violation: timer_hal_deinit out of Core 8 scope.");
#else
void timer_hal_deinit(timer_hal_context_t *hal);
#endif

#if defined(__WINK_SIM__)
void timer_hal_init(timer_hal_context_t *hal, uint32_t group_num, uint32_t timer_num) WINK_SLA_ERROR("Wink SLA Violation: timer_hal_init out of Core 8 scope.");
#else
void timer_hal_init(timer_hal_context_t *hal, uint32_t group_num, uint32_t timer_num);
#endif

#if defined(__WINK_SIM__)
void timer_hal_set_counter_value(timer_hal_context_t *hal, uint64_t load_val) WINK_SLA_ERROR("Wink SLA Violation: timer_hal_set_counter_value out of Core 8 scope.");
#else
void timer_hal_set_counter_value(timer_hal_context_t *hal, uint64_t load_val);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_TIMER_HAL_H__ */
#endif /* WINK_H_GUARD_HAL_TIMER_HAL_H */
