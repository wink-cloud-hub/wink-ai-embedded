/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_MEMORY_UTILS_H
#define WINK_H_GUARD_ESP_MEMORY_UTILS_H
#ifndef __WINK_HARVESTED_ESP_MEMORY_UTILS_H__
#define __WINK_HARVESTED_ESP_MEMORY_UTILS_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "esp_attr.h"
#include "sdkconfig.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
static bool esp_dram_match_iram(void) WINK_SLA_ERROR("Wink SLA Violation: esp_dram_match_iram out of Core 8 scope.");
#else
static bool esp_dram_match_iram(void);
#endif

#if defined(__WINK_SIM__)
bool esp_ptr_byte_accessible(const void *p) WINK_SLA_ERROR("Wink SLA Violation: esp_ptr_byte_accessible out of Core 8 scope.");
#else
bool esp_ptr_byte_accessible(const void *p);
#endif

#if defined(__WINK_SIM__)
static void * esp_ptr_diram_dram_to_iram(const void *p) WINK_SLA_ERROR("Wink SLA Violation: esp_ptr_diram_dram_to_iram out of Core 8 scope.");
#else
static void * esp_ptr_diram_dram_to_iram(const void *p);
#endif

#if defined(__WINK_SIM__)
static void * esp_ptr_diram_iram_to_dram(const void *p) WINK_SLA_ERROR("Wink SLA Violation: esp_ptr_diram_iram_to_dram out of Core 8 scope.");
#else
static void * esp_ptr_diram_iram_to_dram(const void *p);
#endif

#if defined(__WINK_SIM__)
static bool esp_ptr_dma_capable(const void *p) WINK_SLA_ERROR("Wink SLA Violation: esp_ptr_dma_capable out of Core 8 scope.");
#else
static bool esp_ptr_dma_capable(const void *p);
#endif

#if defined(__WINK_SIM__)
bool esp_ptr_dma_ext_capable(const void *p) WINK_SLA_ERROR("Wink SLA Violation: esp_ptr_dma_ext_capable out of Core 8 scope.");
#else
bool esp_ptr_dma_ext_capable(const void *p);
#endif

#if defined(__WINK_SIM__)
bool esp_ptr_executable(const void *p) WINK_SLA_ERROR("Wink SLA Violation: esp_ptr_executable out of Core 8 scope.");
#else
bool esp_ptr_executable(const void *p);
#endif

#if defined(__WINK_SIM__)
bool esp_ptr_external_ram(const void *p) WINK_SLA_ERROR("Wink SLA Violation: esp_ptr_external_ram out of Core 8 scope.");
#else
bool esp_ptr_external_ram(const void *p);
#endif

#if defined(__WINK_SIM__)
static bool esp_ptr_in_diram_dram(const void *p) WINK_SLA_ERROR("Wink SLA Violation: esp_ptr_in_diram_dram out of Core 8 scope.");
#else
static bool esp_ptr_in_diram_dram(const void *p);
#endif

#if defined(__WINK_SIM__)
static bool esp_ptr_in_diram_iram(const void *p) WINK_SLA_ERROR("Wink SLA Violation: esp_ptr_in_diram_iram out of Core 8 scope.");
#else
static bool esp_ptr_in_diram_iram(const void *p);
#endif

#if defined(__WINK_SIM__)
static bool esp_ptr_in_dram(const void *p) WINK_SLA_ERROR("Wink SLA Violation: esp_ptr_in_dram out of Core 8 scope.");
#else
static bool esp_ptr_in_dram(const void *p);
#endif

#if defined(__WINK_SIM__)
static bool esp_ptr_in_drom(const void *p) WINK_SLA_ERROR("Wink SLA Violation: esp_ptr_in_drom out of Core 8 scope.");
#else
static bool esp_ptr_in_drom(const void *p);
#endif

#if defined(__WINK_SIM__)
static bool esp_ptr_in_iram(const void *p) WINK_SLA_ERROR("Wink SLA Violation: esp_ptr_in_iram out of Core 8 scope.");
#else
static bool esp_ptr_in_iram(const void *p);
#endif

#if defined(__WINK_SIM__)
static bool esp_ptr_in_rom(const void *p) WINK_SLA_ERROR("Wink SLA Violation: esp_ptr_in_rom out of Core 8 scope.");
#else
static bool esp_ptr_in_rom(const void *p);
#endif

#if defined(__WINK_SIM__)
static bool esp_ptr_in_rtc_dram_fast(const void *p) WINK_SLA_ERROR("Wink SLA Violation: esp_ptr_in_rtc_dram_fast out of Core 8 scope.");
#else
static bool esp_ptr_in_rtc_dram_fast(const void *p);
#endif

#if defined(__WINK_SIM__)
static bool esp_ptr_in_rtc_iram_fast(const void *p) WINK_SLA_ERROR("Wink SLA Violation: esp_ptr_in_rtc_iram_fast out of Core 8 scope.");
#else
static bool esp_ptr_in_rtc_iram_fast(const void *p);
#endif

#if defined(__WINK_SIM__)
static bool esp_ptr_in_rtc_slow(const void *p) WINK_SLA_ERROR("Wink SLA Violation: esp_ptr_in_rtc_slow out of Core 8 scope.");
#else
static bool esp_ptr_in_rtc_slow(const void *p);
#endif

#if defined(__WINK_SIM__)
static bool esp_ptr_in_spm(const void *p) WINK_SLA_ERROR("Wink SLA Violation: esp_ptr_in_spm out of Core 8 scope.");
#else
static bool esp_ptr_in_spm(const void *p);
#endif

#if defined(__WINK_SIM__)
static bool esp_ptr_in_tcm(const void *p) WINK_SLA_ERROR("Wink SLA Violation: esp_ptr_in_tcm out of Core 8 scope.");
#else
static bool esp_ptr_in_tcm(const void *p);
#endif

#if defined(__WINK_SIM__)
static bool esp_ptr_internal(const void *p) WINK_SLA_ERROR("Wink SLA Violation: esp_ptr_internal out of Core 8 scope.");
#else
static bool esp_ptr_internal(const void *p);
#endif

#if defined(__WINK_SIM__)
static void * esp_ptr_rtc_dram_to_iram(const void *p) WINK_SLA_ERROR("Wink SLA Violation: esp_ptr_rtc_dram_to_iram out of Core 8 scope.");
#else
static void * esp_ptr_rtc_dram_to_iram(const void *p);
#endif

#if defined(__WINK_SIM__)
static bool esp_ptr_word_aligned(const void *p) WINK_SLA_ERROR("Wink SLA Violation: esp_ptr_word_aligned out of Core 8 scope.");
#else
static bool esp_ptr_word_aligned(const void *p);
#endif

#if defined(__WINK_SIM__)
static bool esp_rtc_dram_match_rtc_iram(void) WINK_SLA_ERROR("Wink SLA Violation: esp_rtc_dram_match_rtc_iram out of Core 8 scope.");
#else
static bool esp_rtc_dram_match_rtc_iram(void);
#endif

#if defined(__WINK_SIM__)
static bool esp_stack_ptr_in_dram(uint32_t sp) WINK_SLA_ERROR("Wink SLA Violation: esp_stack_ptr_in_dram out of Core 8 scope.");
#else
static bool esp_stack_ptr_in_dram(uint32_t sp);
#endif

#if defined(__WINK_SIM__)
bool esp_stack_ptr_in_extram(uint32_t sp) WINK_SLA_ERROR("Wink SLA Violation: esp_stack_ptr_in_extram out of Core 8 scope.");
#else
bool esp_stack_ptr_in_extram(uint32_t sp);
#endif

#if defined(__WINK_SIM__)
static bool esp_stack_ptr_is_sane(uint32_t sp) WINK_SLA_ERROR("Wink SLA Violation: esp_stack_ptr_is_sane out of Core 8 scope.");
#else
static bool esp_stack_ptr_is_sane(uint32_t sp);
#endif

#if defined(__WINK_SIM__)
bool esp_task_stack_is_sane_cache_disabled(void) WINK_SLA_ERROR("Wink SLA Violation: esp_task_stack_is_sane_cache_disabled out of Core 8 scope.");
#else
bool esp_task_stack_is_sane_cache_disabled(void);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_MEMORY_UTILS_H__ */
#endif /* WINK_H_GUARD_ESP_MEMORY_UTILS_H */
