/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_CPU_H
#define WINK_H_GUARD_ESP_CPU_H
#ifndef __WINK_HARVESTED_ESP_CPU_H__
#define __WINK_HARVESTED_ESP_CPU_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "esp_attr.h"
#include "esp_err.h"
#include "esp_intr_alloc.h"
#include "sdkconfig.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef ESP_CPU_INTR_DESC_FLAG_RESVD
#define ESP_CPU_INTR_DESC_FLAG_RESVD 0x02
#endif
#ifndef ESP_CPU_INTR_DESC_FLAG_SPECIAL
#define ESP_CPU_INTR_DESC_FLAG_SPECIAL 0x01
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    ESP_CPU_INTR_TYPE_LEVEL = 0,
    ESP_CPU_INTR_TYPE_EDGE = 1,
    ESP_CPU_INTR_TYPE_NA = 2,
} esp_cpu_intr_type_t;
typedef enum {
    ESP_CPU_WATCHPOINT_LOAD = 0,
    ESP_CPU_WATCHPOINT_STORE = 1,
    ESP_CPU_WATCHPOINT_ACCESS = 2,
} esp_cpu_watchpoint_trigger_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef uint32_t esp_cpu_cycle_count_t;
typedef struct {
    int priority;
    esp_cpu_intr_type_t type;
    uint32_t flags;
} esp_cpu_intr_desc_t;
typedef void (*esp_cpu_intr_handler_t)(void *arg);



#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void esp_cpu_branch_prediction_disable(void) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_branch_prediction_disable out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void esp_cpu_branch_prediction_disable(void);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void esp_cpu_branch_prediction_enable(void) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_branch_prediction_enable out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void esp_cpu_branch_prediction_enable(void);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_cpu_clear_breakpoint(int bp_num) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_clear_breakpoint out of Core 8 scope.");
#else
esp_err_t esp_cpu_clear_breakpoint(int bp_num);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_cpu_clear_watchpoint(int wp_num) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_clear_watchpoint out of Core 8 scope.");
#else
esp_err_t esp_cpu_clear_watchpoint(int wp_num);
#endif

#if defined(__WINK_SIM__)
bool esp_cpu_compare_and_set(volatile uint32_t *addr, uint32_t compare_value, uint32_t new_value) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_compare_and_set out of Core 8 scope.");
#else
bool esp_cpu_compare_and_set(volatile uint32_t *addr, uint32_t compare_value, uint32_t new_value);
#endif

#if defined(__WINK_SIM__)
void esp_cpu_configure_region_protection(void) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_configure_region_protection out of Core 8 scope.");
#else
void esp_cpu_configure_region_protection(void);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void esp_cpu_dbgr_break(void) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_dbgr_break out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void esp_cpu_dbgr_break(void);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR bool esp_cpu_dbgr_is_attached(void) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_dbgr_is_attached out of Core 8 scope.");
#else
FORCE_INLINE_ATTR bool esp_cpu_dbgr_is_attached(void);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void esp_cpu_disable_wfe_mode(void) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_disable_wfe_mode out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void esp_cpu_disable_wfe_mode(void);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR intptr_t esp_cpu_get_call_addr(intptr_t return_address) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_get_call_addr out of Core 8 scope.");
#else
FORCE_INLINE_ATTR intptr_t esp_cpu_get_call_addr(intptr_t return_address);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR int esp_cpu_get_core_id(void) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_get_core_id out of Core 8 scope.");
#else
FORCE_INLINE_ATTR int esp_cpu_get_core_id(void);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR int esp_cpu_get_curr_privilege_level(void) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_get_curr_privilege_level out of Core 8 scope.");
#else
FORCE_INLINE_ATTR int esp_cpu_get_curr_privilege_level(void);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR esp_cpu_cycle_count_t esp_cpu_get_cycle_count(void) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_get_cycle_count out of Core 8 scope.");
#else
FORCE_INLINE_ATTR esp_cpu_cycle_count_t esp_cpu_get_cycle_count(void);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void * esp_cpu_get_sp(void) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_get_sp out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void * esp_cpu_get_sp(void);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void * esp_cpu_get_threadptr(void) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_get_threadptr out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void * esp_cpu_get_threadptr(void);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void esp_cpu_intr_disable(uint32_t intr_mask) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_intr_disable out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void esp_cpu_intr_disable(uint32_t intr_mask);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void esp_cpu_intr_edge_ack(int intr_num) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_intr_edge_ack out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void esp_cpu_intr_edge_ack(int intr_num);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void esp_cpu_intr_enable(uint32_t intr_mask) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_intr_enable out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void esp_cpu_intr_enable(uint32_t intr_mask);
#endif

#if defined(__WINK_SIM__)
void esp_cpu_intr_get_desc(int core_id, int intr_num, esp_cpu_intr_desc_t *intr_desc_ret) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_intr_get_desc out of Core 8 scope.");
#else
void esp_cpu_intr_get_desc(int core_id, int intr_num, esp_cpu_intr_desc_t *intr_desc_ret);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR uint32_t esp_cpu_intr_get_enabled_mask(void) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_intr_get_enabled_mask out of Core 8 scope.");
#else
FORCE_INLINE_ATTR uint32_t esp_cpu_intr_get_enabled_mask(void);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void * esp_cpu_intr_get_handler_arg(int intr_num) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_intr_get_handler_arg out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void * esp_cpu_intr_get_handler_arg(int intr_num);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR int esp_cpu_intr_get_priority(int intr_num) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_intr_get_priority out of Core 8 scope.");
#else
FORCE_INLINE_ATTR int esp_cpu_intr_get_priority(int intr_num);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR esp_cpu_intr_type_t esp_cpu_intr_get_type(int intr_num) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_intr_get_type out of Core 8 scope.");
#else
FORCE_INLINE_ATTR esp_cpu_intr_type_t esp_cpu_intr_get_type(int intr_num);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR bool esp_cpu_intr_has_handler(int intr_num) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_intr_has_handler out of Core 8 scope.");
#else
FORCE_INLINE_ATTR bool esp_cpu_intr_has_handler(int intr_num);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void esp_cpu_intr_set_handler(int intr_num, esp_cpu_intr_handler_t handler, void *handler_arg) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_intr_set_handler out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void esp_cpu_intr_set_handler(int intr_num, esp_cpu_intr_handler_t handler, void *handler_arg);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void esp_cpu_intr_set_ivt_addr(const void *ivt_addr) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_intr_set_ivt_addr out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void esp_cpu_intr_set_ivt_addr(const void *ivt_addr);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void esp_cpu_intr_set_mtvt_addr(const void *mtvt_addr) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_intr_set_mtvt_addr out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void esp_cpu_intr_set_mtvt_addr(const void *mtvt_addr);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void esp_cpu_intr_set_priority(int intr_num, int intr_priority) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_intr_set_priority out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void esp_cpu_intr_set_priority(int intr_num, int intr_priority);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void esp_cpu_intr_set_type(int intr_num, esp_cpu_intr_type_t intr_type) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_intr_set_type out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void esp_cpu_intr_set_type(int intr_num, esp_cpu_intr_type_t intr_type);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void esp_cpu_intr_set_xtvt_addr(const void *xtvt_addr) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_intr_set_xtvt_addr out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void esp_cpu_intr_set_xtvt_addr(const void *xtvt_addr);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void * esp_cpu_pc_to_addr(uint32_t pc) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_pc_to_addr out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void * esp_cpu_pc_to_addr(uint32_t pc);
#endif

#if defined(__WINK_SIM__)
void esp_cpu_reset(int core_id) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_reset out of Core 8 scope.");
#else
void esp_cpu_reset(int core_id);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_cpu_set_breakpoint(int bp_num, const void *bp_addr) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_set_breakpoint out of Core 8 scope.");
#else
esp_err_t esp_cpu_set_breakpoint(int bp_num, const void *bp_addr);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void esp_cpu_set_cycle_count(esp_cpu_cycle_count_t cycle_count) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_set_cycle_count out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void esp_cpu_set_cycle_count(esp_cpu_cycle_count_t cycle_count);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void esp_cpu_set_threadptr(void * threadptr) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_set_threadptr out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void esp_cpu_set_threadptr(void * threadptr);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_cpu_set_watchpoint(int wp_num, const void *wp_addr, size_t size, esp_cpu_watchpoint_trigger_t trigger) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_set_watchpoint out of Core 8 scope.");
#else
esp_err_t esp_cpu_set_watchpoint(int wp_num, const void *wp_addr, size_t size, esp_cpu_watchpoint_trigger_t trigger);
#endif

#if defined(__WINK_SIM__)
void esp_cpu_stall(int core_id) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_stall out of Core 8 scope.");
#else
void esp_cpu_stall(int core_id);
#endif

#if defined(__WINK_SIM__)
void esp_cpu_unstall(int core_id) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_unstall out of Core 8 scope.");
#else
void esp_cpu_unstall(int core_id);
#endif

#if defined(__WINK_SIM__)
void esp_cpu_wait_for_intr(void) WINK_SLA_ERROR("Wink SLA Violation: esp_cpu_wait_for_intr out of Core 8 scope.");
#else
void esp_cpu_wait_for_intr(void);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_CPU_H__ */
#endif /* WINK_H_GUARD_ESP_CPU_H */
