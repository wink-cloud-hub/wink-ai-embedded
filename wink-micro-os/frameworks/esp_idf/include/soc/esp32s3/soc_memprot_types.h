/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_SOC_ESP32S3_SOC_MEMPROT_TYPES_H
#define WINK_H_GUARD_SOC_ESP32S3_SOC_MEMPROT_TYPES_H
#ifndef __WINK_HARVESTED_SOC_ESP32S3_SOC_MEMPROT_TYPES_H__
#define __WINK_HARVESTED_SOC_ESP32S3_SOC_MEMPROT_TYPES_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stdint.h>

#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef ESP_MEMPROT_DEFAULT_CONFIG
#define ESP_MEMPROT_DEFAULT_CONFIG() {  .invoke_panic_handler = true,  .lock_feature = true,  .split_addr = NULL,  .mem_type_mask = MEMPROT_TYPE_ALL,  .target_cpu_count = 2,  .target_cpu = {PRO_CPU_NUM, APP_CPU_NUM}  }
#endif
#ifndef ESP_MEMPROT_ZERO_CONFIG
#define ESP_MEMPROT_ZERO_CONFIG() {  .target_cpu_count = 2,  .target_cpu = {PRO_CPU_NUM, APP_CPU_NUM}  }
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    MEMPROT_TYPE_NONE = 0x00000000,
    MEMPROT_TYPE_IRAM0_SRAM = 0x00000001,
    MEMPROT_TYPE_DRAM0_SRAM = 0x00000002,
    MEMPROT_TYPE_IRAM0_RTCFAST = 0x00000004,
    MEMPROT_TYPE_ALL = 0x7FFFFFFF,
    MEMPROT_TYPE_INVALID = 0x80000000,
    MEMPROT_TYPE_IRAM0_ANY = MEMPROT_TYPE_IRAM0_SRAM | MEMPROT_TYPE_IRAM0_RTCFAST,
} esp_mprot_mem_t;
typedef enum {
    MEMPROT_SPLIT_ADDR_NONE = 0x00000000,
    MEMPROT_SPLIT_ADDR_IRAM0_DRAM0 = 0x00000001,
    MEMPROT_SPLIT_ADDR_IRAM0_LINE_0 = 0x00000002,
    MEMPROT_SPLIT_ADDR_IRAM0_LINE_1 = 0x00000004,
    MEMPROT_SPLIT_ADDR_DRAM0_DMA_LINE_0 = 0x00000008,
    MEMPROT_SPLIT_ADDR_DRAM0_DMA_LINE_1 = 0x00000010,
    MEMPROT_SPLIT_ADDR_ALL = 0x7FFFFFFF,
    MEMPROT_SPLIT_ADDR_INVALID = 0x80000000,
    MEMPROT_SPLIT_ADDR_MAIN = MEMPROT_SPLIT_ADDR_IRAM0_DRAM0,
} esp_mprot_split_addr_t;
typedef enum {
    MEMPROT_PMS_AREA_NONE = 0x00000000,
    MEMPROT_PMS_AREA_IRAM0_0 = 0x00000001,
    MEMPROT_PMS_AREA_IRAM0_1 = 0x00000002,
    MEMPROT_PMS_AREA_IRAM0_2 = 0x00000004,
    MEMPROT_PMS_AREA_IRAM0_3 = 0x00000008,
    MEMPROT_PMS_AREA_DRAM0_0 = 0x00000010,
    MEMPROT_PMS_AREA_DRAM0_1 = 0x00000020,
    MEMPROT_PMS_AREA_DRAM0_2 = 0x00000040,
    MEMPROT_PMS_AREA_DRAM0_3 = 0x00000080,
    MEMPROT_PMS_AREA_IRAM0_RTCFAST_LO = 0x00000100,
    MEMPROT_PMS_AREA_IRAM0_RTCFAST_HI = 0x00000200,
    MEMPROT_PMS_AREA_ICACHE_0 = 0x00000400,
    MEMPROT_PMS_AREA_ICACHE_1 = 0x00000800,
    MEMPROT_PMS_AREA_ALL = 0x7FFFFFFF,
    MEMPROT_PMS_AREA_INVALID = 0x80000000,
} esp_mprot_pms_area_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
    bool invoke_panic_handler;
    bool lock_feature;
    void * split_addr;
    uint32_t mem_type_mask;
    size_t target_cpu_count;
    int target_cpu[CONFIG_FREERTOS_NUMBER_OF_CORES];
} esp_memp_config_t;



#if defined(__WINK_SIM__)
const char * esp_mprot_mem_type_to_str(const esp_mprot_mem_t mem_type) WINK_SLA_ERROR("Wink SLA Violation: esp_mprot_mem_type_to_str out of Core 8 scope.");
#else
const char * esp_mprot_mem_type_to_str(const esp_mprot_mem_t mem_type);
#endif

#if defined(__WINK_SIM__)
const char * esp_mprot_pms_area_to_str(const esp_mprot_pms_area_t area_type) WINK_SLA_ERROR("Wink SLA Violation: esp_mprot_pms_area_to_str out of Core 8 scope.");
#else
const char * esp_mprot_pms_area_to_str(const esp_mprot_pms_area_t area_type);
#endif

#if defined(__WINK_SIM__)
const char * esp_mprot_split_addr_to_str(const esp_mprot_split_addr_t line_type) WINK_SLA_ERROR("Wink SLA Violation: esp_mprot_split_addr_to_str out of Core 8 scope.");
#else
const char * esp_mprot_split_addr_to_str(const esp_mprot_split_addr_t line_type);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_SOC_ESP32S3_SOC_MEMPROT_TYPES_H__ */
#endif /* WINK_H_GUARD_SOC_ESP32S3_SOC_MEMPROT_TYPES_H */
