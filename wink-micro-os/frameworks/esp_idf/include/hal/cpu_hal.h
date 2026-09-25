/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_CPU_HAL_H
#define WINK_H_GUARD_HAL_CPU_HAL_H
#ifndef __WINK_HARVESTED_HAL_CPU_HAL_H__
#define __WINK_HARVESTED_HAL_CPU_HAL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stddef.h>
#include <stdint.h>

#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef cpu_hal_break
#define cpu_hal_break() cpu_ll_break()
#endif
#ifndef cpu_hal_get_core_id
#define cpu_hal_get_core_id() cpu_ll_get_core_id()
#endif
#ifndef cpu_hal_get_cycle_count
#define cpu_hal_get_cycle_count() cpu_ll_get_cycle_count()
#endif
#ifndef cpu_hal_get_sp
#define cpu_hal_get_sp() cpu_ll_get_sp()
#endif
#ifndef cpu_hal_init_hwloop
#define cpu_hal_init_hwloop() cpu_ll_init_hwloop()
#endif
#ifndef cpu_hal_is_debugger_attached
#define cpu_hal_is_debugger_attached() cpu_ll_is_debugger_attached()
#endif
#ifndef cpu_hal_set_cycle_count
#define cpu_hal_set_cycle_count(val) cpu_ll_set_cycle_count(val)
#endif
#ifndef cpu_hal_waiti
#define cpu_hal_waiti() cpu_ll_waiti()
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    WATCHPOINT_TRIGGER_ON_RO = ESP_CPU_WATCHPOINT_LOAD,
    WATCHPOINT_TRIGGER_ON_WO = ESP_CPU_WATCHPOINT_STORE,
    WATCHPOINT_TRIGGER_ON_RW = ESP_CPU_WATCHPOINT_ACCESS,
} watchpoint_trigger_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
void cpu_hal_clear_breakpoint(int id) WINK_SLA_ERROR("Wink SLA Violation: cpu_hal_clear_breakpoint out of Core 8 scope.");
#else
void cpu_hal_clear_breakpoint(int id);
#endif

#if defined(__WINK_SIM__)
void cpu_hal_clear_watchpoint(int id) WINK_SLA_ERROR("Wink SLA Violation: cpu_hal_clear_watchpoint out of Core 8 scope.");
#else
void cpu_hal_clear_watchpoint(int id);
#endif

#if defined(__WINK_SIM__)
void cpu_hal_set_breakpoint(int id, const void *addr) WINK_SLA_ERROR("Wink SLA Violation: cpu_hal_set_breakpoint out of Core 8 scope.");
#else
void cpu_hal_set_breakpoint(int id, const void *addr);
#endif

#if defined(__WINK_SIM__)
void cpu_hal_set_vecbase(const void *base) WINK_SLA_ERROR("Wink SLA Violation: cpu_hal_set_vecbase out of Core 8 scope.");
#else
void cpu_hal_set_vecbase(const void *base);
#endif

#if defined(__WINK_SIM__)
void cpu_hal_set_watchpoint(int id, const void *addr, size_t size, watchpoint_trigger_t trigger) WINK_SLA_ERROR("Wink SLA Violation: cpu_hal_set_watchpoint out of Core 8 scope.");
#else
void cpu_hal_set_watchpoint(int id, const void *addr, size_t size, watchpoint_trigger_t trigger);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_CPU_HAL_H__ */
#endif /* WINK_H_GUARD_HAL_CPU_HAL_H */
