/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_SYSTIMER_H
#define WINK_H_GUARD_ESP_PRIVATE_SYSTIMER_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_SYSTIMER_H__
#define __WINK_HARVESTED_ESP_PRIVATE_SYSTIMER_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdint.h>

#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef SYSTIMER_ALARM_ESPTIMER
#define SYSTIMER_ALARM_ESPTIMER 2
#endif
#ifndef SYSTIMER_ALARM_OS_TICK_CORE0
#define SYSTIMER_ALARM_OS_TICK_CORE0 0
#endif
#ifndef SYSTIMER_ALARM_OS_TICK_CORE1
#define SYSTIMER_ALARM_OS_TICK_CORE1 1
#endif
#ifndef SYSTIMER_COUNTER_ESPTIMER
#define SYSTIMER_COUNTER_ESPTIMER 0
#endif
#ifndef SYSTIMER_COUNTER_OS_TICK
#define SYSTIMER_COUNTER_OS_TICK 1
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
uint64_t systimer_ticks_to_us(uint64_t ticks) WINK_SLA_ERROR("Wink SLA Violation: systimer_ticks_to_us out of Core 8 scope.");
#else
uint64_t systimer_ticks_to_us(uint64_t ticks);
#endif

#if defined(__WINK_SIM__)
uint64_t systimer_us_to_ticks(uint64_t us) WINK_SLA_ERROR("Wink SLA Violation: systimer_us_to_ticks out of Core 8 scope.");
#else
uint64_t systimer_us_to_ticks(uint64_t us);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_SYSTIMER_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_SYSTIMER_H */
