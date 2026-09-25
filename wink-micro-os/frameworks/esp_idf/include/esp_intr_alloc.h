/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_INTR_ALLOC_H
#define WINK_H_GUARD_ESP_INTR_ALLOC_H
#ifndef __WINK_HARVESTED_ESP_INTR_ALLOC_H__
#define __WINK_HARVESTED_ESP_INTR_ALLOC_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "esp_err.h"
#include "esp_intr_types.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef ESP_INTR_DISABLE
#define ESP_INTR_DISABLE(inum) esp_intr_disable_source(inum)
#endif
#ifndef ESP_INTR_ENABLE
#define ESP_INTR_ENABLE(inum) esp_intr_enable_source(inum)
#endif
#ifndef ESP_INTR_FLAG_EDGE
#define ESP_INTR_FLAG_EDGE (1<<9)
#endif
#ifndef ESP_INTR_FLAG_HIGH
#define ESP_INTR_FLAG_HIGH (ESP_INTR_FLAG_LEVEL4|ESP_INTR_FLAG_LEVEL5|ESP_INTR_FLAG_LEVEL6|ESP_INTR_FLAG_NMI)
#endif
#ifndef ESP_INTR_FLAG_INTRDISABLED
#define ESP_INTR_FLAG_INTRDISABLED (1<<11)
#endif
#ifndef ESP_INTR_FLAG_IRAM
#define ESP_INTR_FLAG_IRAM (1<<10)
#endif
#ifndef ESP_INTR_FLAG_LEVEL1
#define ESP_INTR_FLAG_LEVEL1 (1<<1)
#endif
#ifndef ESP_INTR_FLAG_LEVEL2
#define ESP_INTR_FLAG_LEVEL2 (1<<2)
#endif
#ifndef ESP_INTR_FLAG_LEVEL3
#define ESP_INTR_FLAG_LEVEL3 (1<<3)
#endif
#ifndef ESP_INTR_FLAG_LEVEL4
#define ESP_INTR_FLAG_LEVEL4 (1<<4)
#endif
#ifndef ESP_INTR_FLAG_LEVEL5
#define ESP_INTR_FLAG_LEVEL5 (1<<5)
#endif
#ifndef ESP_INTR_FLAG_LEVEL6
#define ESP_INTR_FLAG_LEVEL6 (1<<6)
#endif
#ifndef ESP_INTR_FLAG_LEVELMASK
#define ESP_INTR_FLAG_LEVELMASK (ESP_INTR_FLAG_LEVEL1|ESP_INTR_FLAG_LEVEL2|ESP_INTR_FLAG_LEVEL3|  ESP_INTR_FLAG_LEVEL4|ESP_INTR_FLAG_LEVEL5|ESP_INTR_FLAG_LEVEL6|  ESP_INTR_FLAG_NMI)
#endif
#ifndef ESP_INTR_FLAG_LOWMED
#define ESP_INTR_FLAG_LOWMED (ESP_INTR_FLAG_LEVEL1|ESP_INTR_FLAG_LEVEL2|ESP_INTR_FLAG_LEVEL3)
#endif
#ifndef ESP_INTR_FLAG_NMI
#define ESP_INTR_FLAG_NMI (1<<7)
#endif
#ifndef ESP_INTR_FLAG_SHARED
#define ESP_INTR_FLAG_SHARED (1<<8)
#endif
#ifndef ESP_INTR_FLAG_SHARED_PRIVATE
#define ESP_INTR_FLAG_SHARED_PRIVATE (1<<12)
#endif
#ifndef ETS_INTERNAL_INTR_SOURCE_OFF
#define ETS_INTERNAL_INTR_SOURCE_OFF (-ETS_INTERNAL_PROFILING_INTR_SOURCE)
#endif
#ifndef ETS_INTERNAL_PROFILING_INTR_SOURCE
#define ETS_INTERNAL_PROFILING_INTR_SOURCE -6
#endif
#ifndef ETS_INTERNAL_SW0_INTR_SOURCE
#define ETS_INTERNAL_SW0_INTR_SOURCE -4
#endif
#ifndef ETS_INTERNAL_SW1_INTR_SOURCE
#define ETS_INTERNAL_SW1_INTR_SOURCE -5
#endif
#ifndef ETS_INTERNAL_TIMER0_INTR_SOURCE
#define ETS_INTERNAL_TIMER0_INTR_SOURCE -1
#endif
#ifndef ETS_INTERNAL_TIMER1_INTR_SOURCE
#define ETS_INTERNAL_TIMER1_INTR_SOURCE -2
#endif
#ifndef ETS_INTERNAL_TIMER2_INTR_SOURCE
#define ETS_INTERNAL_TIMER2_INTR_SOURCE -3
#endif
#ifndef ETS_INTERNAL_UNUSED_INTR_SOURCE
#define ETS_INTERNAL_UNUSED_INTR_SOURCE -99
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
int source;              
    int flags;               
    uint32_t intrstatusreg;  
    uint32_t intrstatusmask; 
    intr_handler_t handler;  
    void *arg;               
    struct {
        intr_handle_t handle; 
        const char* name;     
    } bind_by;
} esp_intr_alloc_info_t;



#if defined(__WINK_SIM__)
esp_err_t esp_intr_alloc(int source, int flags, intr_handler_t handler, void *arg, intr_handle_t *ret_handle) WINK_SLA_ERROR("Wink SLA Violation: esp_intr_alloc out of Core 8 scope.");
#else
esp_err_t esp_intr_alloc(int source, int flags, intr_handler_t handler, void *arg, intr_handle_t *ret_handle);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_intr_alloc_bind(int source, int flags, intr_handler_t handler, void *arg, intr_handle_t shared_handle, intr_handle_t *ret_handle) WINK_SLA_ERROR("Wink SLA Violation: esp_intr_alloc_bind out of Core 8 scope.");
#else
esp_err_t esp_intr_alloc_bind(int source, int flags, intr_handler_t handler, void *arg, intr_handle_t shared_handle, intr_handle_t *ret_handle);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_intr_alloc_info(const esp_intr_alloc_info_t *info, intr_handle_t *ret_handle) WINK_SLA_ERROR("Wink SLA Violation: esp_intr_alloc_info out of Core 8 scope.");
#else
esp_err_t esp_intr_alloc_info(const esp_intr_alloc_info_t *info, intr_handle_t *ret_handle);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_intr_alloc_intrstatus(int source, int flags, uint32_t intrstatusreg, uint32_t intrstatusmask, intr_handler_t handler, void *arg, intr_handle_t *ret_handle) WINK_SLA_ERROR("Wink SLA Violation: esp_intr_alloc_intrstatus out of Core 8 scope.");
#else
esp_err_t esp_intr_alloc_intrstatus(int source, int flags, uint32_t intrstatusreg, uint32_t intrstatusmask, intr_handler_t handler, void *arg, intr_handle_t *ret_handle);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_intr_alloc_intrstatus_bind(int source, int flags, uint32_t intrstatusreg, uint32_t intrstatusmask, intr_handler_t handler,
                                         void *arg, intr_handle_t shared_handle, intr_handle_t *ret_handle) WINK_SLA_ERROR("Wink SLA Violation: esp_intr_alloc_intrstatus_bind out of Core 8 scope.");
#else
esp_err_t esp_intr_alloc_intrstatus_bind(int source, int flags, uint32_t intrstatusreg, uint32_t intrstatusmask, intr_handler_t handler,
                                         void *arg, intr_handle_t shared_handle, intr_handle_t *ret_handle);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_intr_disable(intr_handle_t handle) WINK_SLA_ERROR("Wink SLA Violation: esp_intr_disable out of Core 8 scope.");
#else
esp_err_t esp_intr_disable(intr_handle_t handle);
#endif

#if defined(__WINK_SIM__)
void esp_intr_disable_source(int inum) WINK_SLA_ERROR("Wink SLA Violation: esp_intr_disable_source out of Core 8 scope.");
#else
void esp_intr_disable_source(int inum);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_intr_dump(FILE *stream) WINK_SLA_ERROR("Wink SLA Violation: esp_intr_dump out of Core 8 scope.");
#else
esp_err_t esp_intr_dump(FILE *stream);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_intr_enable(intr_handle_t handle) WINK_SLA_ERROR("Wink SLA Violation: esp_intr_enable out of Core 8 scope.");
#else
esp_err_t esp_intr_enable(intr_handle_t handle);
#endif

#if defined(__WINK_SIM__)
void esp_intr_enable_source(int inum) WINK_SLA_ERROR("Wink SLA Violation: esp_intr_enable_source out of Core 8 scope.");
#else
void esp_intr_enable_source(int inum);
#endif

#if defined(__WINK_SIM__)
int esp_intr_flags_to_level(int flags) WINK_SLA_ERROR("Wink SLA Violation: esp_intr_flags_to_level out of Core 8 scope.");
#else
int esp_intr_flags_to_level(int flags);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_intr_free(intr_handle_t handle) WINK_SLA_ERROR("Wink SLA Violation: esp_intr_free out of Core 8 scope.");
#else
esp_err_t esp_intr_free(intr_handle_t handle);
#endif

#if defined(__WINK_SIM__)
int esp_intr_get_cpu(intr_handle_t handle) WINK_SLA_ERROR("Wink SLA Violation: esp_intr_get_cpu out of Core 8 scope.");
#else
int esp_intr_get_cpu(intr_handle_t handle);
#endif

#if defined(__WINK_SIM__)
int esp_intr_get_intno(intr_handle_t handle) WINK_SLA_ERROR("Wink SLA Violation: esp_intr_get_intno out of Core 8 scope.");
#else
int esp_intr_get_intno(intr_handle_t handle);
#endif

#if defined(__WINK_SIM__)
int esp_intr_level_to_flags(int level) WINK_SLA_ERROR("Wink SLA Violation: esp_intr_level_to_flags out of Core 8 scope.");
#else
int esp_intr_level_to_flags(int level);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_intr_mark_shared(int intno, int cpu, bool is_in_iram) WINK_SLA_ERROR("Wink SLA Violation: esp_intr_mark_shared out of Core 8 scope.");
#else
esp_err_t esp_intr_mark_shared(int intno, int cpu, bool is_in_iram);
#endif

#if defined(__WINK_SIM__)
void esp_intr_noniram_disable(void) WINK_SLA_ERROR("Wink SLA Violation: esp_intr_noniram_disable out of Core 8 scope.");
#else
void esp_intr_noniram_disable(void);
#endif

#if defined(__WINK_SIM__)
void esp_intr_noniram_enable(void) WINK_SLA_ERROR("Wink SLA Violation: esp_intr_noniram_enable out of Core 8 scope.");
#else
void esp_intr_noniram_enable(void);
#endif

#if defined(__WINK_SIM__)
bool esp_intr_ptr_in_isr_region(void* ptr) WINK_SLA_ERROR("Wink SLA Violation: esp_intr_ptr_in_isr_region out of Core 8 scope.");
#else
bool esp_intr_ptr_in_isr_region(void* ptr);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_intr_reserve(int intno, int cpu) WINK_SLA_ERROR("Wink SLA Violation: esp_intr_reserve out of Core 8 scope.");
#else
esp_err_t esp_intr_reserve(int intno, int cpu);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_intr_set_in_iram(intr_handle_t handle, bool is_in_iram) WINK_SLA_ERROR("Wink SLA Violation: esp_intr_set_in_iram out of Core 8 scope.");
#else
esp_err_t esp_intr_set_in_iram(intr_handle_t handle, bool is_in_iram);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_INTR_ALLOC_H__ */
#endif /* WINK_H_GUARD_ESP_INTR_ALLOC_H */
