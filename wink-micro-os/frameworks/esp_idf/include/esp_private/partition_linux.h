/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_PARTITION_LINUX_H
#define WINK_H_GUARD_ESP_PRIVATE_PARTITION_LINUX_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_PARTITION_LINUX_H__
#define __WINK_HARVESTED_ESP_PRIVATE_PARTITION_LINUX_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef ESP_PARTITION_DEFAULT_EMULATED_FLASH_SIZE
#define ESP_PARTITION_DEFAULT_EMULATED_FLASH_SIZE 0x400000
#endif
#ifndef ESP_PARTITION_EMULATED_SECTOR_SIZE
#define ESP_PARTITION_EMULATED_SECTOR_SIZE 0x1000
#endif
#ifndef ESP_PARTITION_FAIL_AFTER_MODE_BOTH
#define ESP_PARTITION_FAIL_AFTER_MODE_BOTH 0x03
#endif
#ifndef ESP_PARTITION_FAIL_AFTER_MODE_ERASE
#define ESP_PARTITION_FAIL_AFTER_MODE_ERASE 0x01
#endif
#ifndef ESP_PARTITION_FAIL_AFTER_MODE_WRITE
#define ESP_PARTITION_FAIL_AFTER_MODE_WRITE 0x02
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
    char flash_file_name[PATH_MAX];
    size_t flash_file_size;
    char partition_file_name[PATH_MAX];
    bool remove_dump;
} esp_partition_file_mmap_ctrl_t;



#if defined(__WINK_SIM__)
void esp_partition_clear_stats(void) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_clear_stats out of Core 8 scope.");
#else
void esp_partition_clear_stats(void);
#endif

#if defined(__WINK_SIM__)
void esp_partition_fail_after(size_t count, uint8_t mode) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_fail_after out of Core 8 scope.");
#else
void esp_partition_fail_after(size_t count, uint8_t mode);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_partition_file_mmap(const uint8_t **part_desc_addr_start) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_file_mmap out of Core 8 scope.");
#else
esp_err_t esp_partition_file_mmap(const uint8_t **part_desc_addr_start);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_partition_file_munmap(void) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_file_munmap out of Core 8 scope.");
#else
esp_err_t esp_partition_file_munmap(void);
#endif

#if defined(__WINK_SIM__)
size_t esp_partition_get_erase_ops(void) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_get_erase_ops out of Core 8 scope.");
#else
size_t esp_partition_get_erase_ops(void);
#endif

#if defined(__WINK_SIM__)
esp_partition_file_mmap_ctrl_t* esp_partition_get_file_mmap_ctrl_act(void) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_get_file_mmap_ctrl_act out of Core 8 scope.");
#else
esp_partition_file_mmap_ctrl_t* esp_partition_get_file_mmap_ctrl_act(void);
#endif

#if defined(__WINK_SIM__)
esp_partition_file_mmap_ctrl_t* esp_partition_get_file_mmap_ctrl_input(void) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_get_file_mmap_ctrl_input out of Core 8 scope.");
#else
esp_partition_file_mmap_ctrl_t* esp_partition_get_file_mmap_ctrl_input(void);
#endif

#if defined(__WINK_SIM__)
size_t esp_partition_get_read_bytes(void) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_get_read_bytes out of Core 8 scope.");
#else
size_t esp_partition_get_read_bytes(void);
#endif

#if defined(__WINK_SIM__)
size_t esp_partition_get_read_ops(void) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_get_read_ops out of Core 8 scope.");
#else
size_t esp_partition_get_read_ops(void);
#endif

#if defined(__WINK_SIM__)
size_t esp_partition_get_sector_erase_count(size_t sector) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_get_sector_erase_count out of Core 8 scope.");
#else
size_t esp_partition_get_sector_erase_count(size_t sector);
#endif

#if defined(__WINK_SIM__)
size_t esp_partition_get_total_time(void) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_get_total_time out of Core 8 scope.");
#else
size_t esp_partition_get_total_time(void);
#endif

#if defined(__WINK_SIM__)
size_t esp_partition_get_write_bytes(void) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_get_write_bytes out of Core 8 scope.");
#else
size_t esp_partition_get_write_bytes(void);
#endif

#if defined(__WINK_SIM__)
size_t esp_partition_get_write_ops(void) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_get_write_ops out of Core 8 scope.");
#else
size_t esp_partition_get_write_ops(void);
#endif

#if defined(__WINK_SIM__)
const char * esp_partition_subtype_to_str(const uint32_t type, const uint32_t subtype) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_subtype_to_str out of Core 8 scope.");
#else
const char * esp_partition_subtype_to_str(const uint32_t type, const uint32_t subtype);
#endif

#if defined(__WINK_SIM__)
const char * esp_partition_type_to_str(const uint32_t type) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_type_to_str out of Core 8 scope.");
#else
const char * esp_partition_type_to_str(const uint32_t type);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_PARTITION_LINUX_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_PARTITION_LINUX_H */
