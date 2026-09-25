/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_CPU_LL_H
#define WINK_H_GUARD_HAL_CPU_LL_H
#ifndef __WINK_HARVESTED_HAL_CPU_LL_H__
#define __WINK_HARVESTED_HAL_CPU_LL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stdint.h>

#include "esp_attr.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void cpu_ll_break(void) WINK_SLA_ERROR("Wink SLA Violation: cpu_ll_break out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void cpu_ll_break(void);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void cpu_ll_clear_breakpoint(int id) WINK_SLA_ERROR("Wink SLA Violation: cpu_ll_clear_breakpoint out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void cpu_ll_clear_breakpoint(int id);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void cpu_ll_clear_watchpoint(int id) WINK_SLA_ERROR("Wink SLA Violation: cpu_ll_clear_watchpoint out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void cpu_ll_clear_watchpoint(int id);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void cpu_ll_compare_and_set_native(volatile uint32_t *addr, uint32_t compare, uint32_t *set) WINK_SLA_ERROR("Wink SLA Violation: cpu_ll_compare_and_set_native out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void cpu_ll_compare_and_set_native(volatile uint32_t *addr, uint32_t compare, uint32_t *set);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR uint32_t cpu_ll_get_core_id(void) WINK_SLA_ERROR("Wink SLA Violation: cpu_ll_get_core_id out of Core 8 scope.");
#else
FORCE_INLINE_ATTR uint32_t cpu_ll_get_core_id(void);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR uint32_t cpu_ll_get_cycle_count(void) WINK_SLA_ERROR("Wink SLA Violation: cpu_ll_get_cycle_count out of Core 8 scope.");
#else
FORCE_INLINE_ATTR uint32_t cpu_ll_get_cycle_count(void);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void * cpu_ll_get_sp(void) WINK_SLA_ERROR("Wink SLA Violation: cpu_ll_get_sp out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void * cpu_ll_get_sp(void);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void cpu_ll_init_hwloop(void) WINK_SLA_ERROR("Wink SLA Violation: cpu_ll_init_hwloop out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void cpu_ll_init_hwloop(void);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR bool cpu_ll_is_debugger_attached(void) WINK_SLA_ERROR("Wink SLA Violation: cpu_ll_is_debugger_attached out of Core 8 scope.");
#else
FORCE_INLINE_ATTR bool cpu_ll_is_debugger_attached(void);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void * cpu_ll_pc_to_ptr(uint32_t pc) WINK_SLA_ERROR("Wink SLA Violation: cpu_ll_pc_to_ptr out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void * cpu_ll_pc_to_ptr(uint32_t pc);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR uint32_t cpu_ll_ptr_to_pc(const void *addr) WINK_SLA_ERROR("Wink SLA Violation: cpu_ll_ptr_to_pc out of Core 8 scope.");
#else
FORCE_INLINE_ATTR uint32_t cpu_ll_ptr_to_pc(const void *addr);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void cpu_ll_set_breakpoint(int id, uint32_t pc) WINK_SLA_ERROR("Wink SLA Violation: cpu_ll_set_breakpoint out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void cpu_ll_set_breakpoint(int id, uint32_t pc);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void cpu_ll_set_cycle_count(uint32_t val) WINK_SLA_ERROR("Wink SLA Violation: cpu_ll_set_cycle_count out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void cpu_ll_set_cycle_count(uint32_t val);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void cpu_ll_set_vecbase(const void *base) WINK_SLA_ERROR("Wink SLA Violation: cpu_ll_set_vecbase out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void cpu_ll_set_vecbase(const void *base);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void cpu_ll_set_watchpoint(int id, const void *addr, size_t size, bool on_read, bool on_write) WINK_SLA_ERROR("Wink SLA Violation: cpu_ll_set_watchpoint out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void cpu_ll_set_watchpoint(int id, const void *addr, size_t size, bool on_read, bool on_write);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void cpu_ll_waiti(void) WINK_SLA_ERROR("Wink SLA Violation: cpu_ll_waiti out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void cpu_ll_waiti(void);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_CPU_LL_H__ */
#endif /* WINK_H_GUARD_HAL_CPU_LL_H */
