/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef __ESP_PARTITION_H__
#define __ESP_PARTITION_H__
#ifndef __WINK_HARVESTED_ESP_PARTITION_H__
#define __WINK_HARVESTED_ESP_PARTITION_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_attr.h"
#include "esp_bit_defs.h"
#include "esp_blockdev.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef ESP_PARTITION_SUBTYPE_OTA
#define ESP_PARTITION_SUBTYPE_OTA(i) ((esp_partition_subtype_t)(ESP_PARTITION_SUBTYPE_APP_OTA_MIN + ((i) & 0xf)))
#endif
#ifndef esp_partition_mmap_memory_t
#define esp_partition_mmap_memory_t _Pragma("GCC warning \"'esp_partition_mmap_memory_t' enum is deprecated.\"") esp_partition_mmap_flag_t
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    ESP_PARTITION_MMAP_DATA = 0,
    ESP_PARTITION_MMAP_INST = 1,
    ESP_PARTITION_MMAP_BLOCKS_WRITE = 2,
} esp_partition_mmap_flag_t;
typedef enum {
    ESP_PARTITION_TYPE_APP = 0,
    ESP_PARTITION_TYPE_DATA = 1,
    ESP_PARTITION_TYPE_BOOTLOADER = 2,
    ESP_PARTITION_TYPE_PARTITION_TABLE = 3,
    ESP_PARTITION_TYPE_ANY = 255,
} esp_partition_type_t;
typedef enum {
    ESP_PARTITION_SUBTYPE_BOOTLOADER_PRIMARY = 0,
    ESP_PARTITION_SUBTYPE_BOOTLOADER_OTA = 1,
    ESP_PARTITION_SUBTYPE_BOOTLOADER_RECOVERY = 2,
    ESP_PARTITION_SUBTYPE_PARTITION_TABLE_PRIMARY = 0,
    ESP_PARTITION_SUBTYPE_PARTITION_TABLE_OTA = 1,
    ESP_PARTITION_SUBTYPE_APP_FACTORY = 0,
    ESP_PARTITION_SUBTYPE_APP_OTA_MIN = 16,
    ESP_PARTITION_SUBTYPE_APP_OTA_0 = 16,
    ESP_PARTITION_SUBTYPE_APP_OTA_1 = 17,
    ESP_PARTITION_SUBTYPE_APP_OTA_2 = 18,
    ESP_PARTITION_SUBTYPE_APP_OTA_3 = 19,
    ESP_PARTITION_SUBTYPE_APP_OTA_4 = 20,
    ESP_PARTITION_SUBTYPE_APP_OTA_5 = 21,
    ESP_PARTITION_SUBTYPE_APP_OTA_6 = 22,
    ESP_PARTITION_SUBTYPE_APP_OTA_7 = 23,
    ESP_PARTITION_SUBTYPE_APP_OTA_8 = 24,
    ESP_PARTITION_SUBTYPE_APP_OTA_9 = 25,
    ESP_PARTITION_SUBTYPE_APP_OTA_10 = 26,
    ESP_PARTITION_SUBTYPE_APP_OTA_11 = 27,
    ESP_PARTITION_SUBTYPE_APP_OTA_12 = 28,
    ESP_PARTITION_SUBTYPE_APP_OTA_13 = 29,
    ESP_PARTITION_SUBTYPE_APP_OTA_14 = 30,
    ESP_PARTITION_SUBTYPE_APP_OTA_15 = 31,
    ESP_PARTITION_SUBTYPE_APP_OTA_MAX = 32,
    ESP_PARTITION_SUBTYPE_APP_TEST = 32,
    ESP_PARTITION_SUBTYPE_APP_TEE_MIN = 48,
    ESP_PARTITION_SUBTYPE_APP_TEE_0 = 48,
    ESP_PARTITION_SUBTYPE_APP_TEE_1 = 49,
    ESP_PARTITION_SUBTYPE_APP_TEE_MAX = 49,
    ESP_PARTITION_SUBTYPE_DATA_OTA = 0,
    ESP_PARTITION_SUBTYPE_DATA_PHY = 1,
    ESP_PARTITION_SUBTYPE_DATA_NVS = 2,
    ESP_PARTITION_SUBTYPE_DATA_COREDUMP = 3,
    ESP_PARTITION_SUBTYPE_DATA_NVS_KEYS = 4,
    ESP_PARTITION_SUBTYPE_DATA_EFUSE_EM = 5,
    ESP_PARTITION_SUBTYPE_DATA_UNDEFINED = 6,
    ESP_PARTITION_SUBTYPE_DATA_ESPHTTPD = 128,
    ESP_PARTITION_SUBTYPE_DATA_FAT = 129,
    ESP_PARTITION_SUBTYPE_DATA_SPIFFS = 130,
    ESP_PARTITION_SUBTYPE_DATA_LITTLEFS = 131,
    ESP_PARTITION_SUBTYPE_DATA_TEE_OTA = 144,
    ESP_PARTITION_SUBTYPE_ANY = 255,
} esp_partition_subtype_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct esp_flash_t esp_flash_t;
typedef uint32_t esp_partition_mmap_handle_t;
typedef struct esp_partition_iterator_opaque_ * esp_partition_iterator_t;
typedef struct {
    esp_flash_t* flash_chip;
    esp_partition_type_t type;
    esp_partition_subtype_t subtype;
    uint32_t address;
    uint32_t size;
    uint32_t erase_size;
    char label[17];
    bool encrypted;
    bool readonly;
} esp_partition_t;



#if defined(__WINK_SIM__)
bool esp_partition_check_identity(const esp_partition_t* partition_1, const esp_partition_t* partition_2) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_check_identity out of Core 8 scope.");
#else
bool esp_partition_check_identity(const esp_partition_t* partition_1, const esp_partition_t* partition_2);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_partition_copy(const esp_partition_t* dest_part, uint32_t dest_offset, const esp_partition_t* src_part, uint32_t src_offset, size_t size) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_copy out of Core 8 scope.");
#else
esp_err_t esp_partition_copy(const esp_partition_t* dest_part, uint32_t dest_offset, const esp_partition_t* src_part, uint32_t src_offset, size_t size);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_partition_deregister_external(const esp_partition_t* partition) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_deregister_external out of Core 8 scope.");
#else
esp_err_t esp_partition_deregister_external(const esp_partition_t* partition);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_partition_erase_range(const esp_partition_t* partition,
                                    size_t offset, size_t size) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_erase_range out of Core 8 scope.");
#else
esp_err_t esp_partition_erase_range(const esp_partition_t* partition,
                                    size_t offset, size_t size);
#endif

#if defined(__WINK_SIM__)
esp_partition_iterator_t esp_partition_find(esp_partition_type_t type, esp_partition_subtype_t subtype, const char* label) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_find out of Core 8 scope.");
#else
esp_partition_iterator_t esp_partition_find(esp_partition_type_t type, esp_partition_subtype_t subtype, const char* label);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_partition_find_err(esp_partition_type_t type, esp_partition_subtype_t subtype, const char* label, esp_partition_iterator_t* it) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_find_err out of Core 8 scope.");
#else
esp_err_t esp_partition_find_err(esp_partition_type_t type, esp_partition_subtype_t subtype, const char* label, esp_partition_iterator_t* it);
#endif

#if defined(__WINK_SIM__)
const esp_partition_t* esp_partition_find_first(esp_partition_type_t type, esp_partition_subtype_t subtype, const char* label) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_find_first out of Core 8 scope.");
#else
const esp_partition_t* esp_partition_find_first(esp_partition_type_t type, esp_partition_subtype_t subtype, const char* label);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_partition_find_first_err(esp_partition_type_t type, esp_partition_subtype_t subtype, const char* label, const esp_partition_t** partition) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_find_first_err out of Core 8 scope.");
#else
esp_err_t esp_partition_find_first_err(esp_partition_type_t type, esp_partition_subtype_t subtype, const char* label, const esp_partition_t** partition);
#endif

#if defined(__WINK_SIM__)
const esp_partition_t* esp_partition_get(esp_partition_iterator_t iterator) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_get out of Core 8 scope.");
#else
const esp_partition_t* esp_partition_get(esp_partition_iterator_t iterator);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_partition_get_blockdev(const esp_partition_type_t type, const esp_partition_subtype_t subtype, const char *label, esp_blockdev_handle_t *out_bdl_handle) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_get_blockdev out of Core 8 scope.");
#else
esp_err_t esp_partition_get_blockdev(const esp_partition_type_t type, const esp_partition_subtype_t subtype, const char *label, esp_blockdev_handle_t *out_bdl_handle);
#endif

#if defined(__WINK_SIM__)
uint32_t esp_partition_get_main_flash_sector_size(void) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_get_main_flash_sector_size out of Core 8 scope.");
#else
uint32_t esp_partition_get_main_flash_sector_size(void);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_partition_get_sha256(const esp_partition_t* partition, uint8_t* sha_256) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_get_sha256 out of Core 8 scope.");
#else
esp_err_t esp_partition_get_sha256(const esp_partition_t* partition, uint8_t* sha_256);
#endif

#if defined(__WINK_SIM__)
void esp_partition_iterator_release(esp_partition_iterator_t iterator) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_iterator_release out of Core 8 scope.");
#else
void esp_partition_iterator_release(esp_partition_iterator_t iterator);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_partition_mmap(const esp_partition_t* partition, size_t offset, size_t size,
                             esp_partition_mmap_flag_t flags,
                             const void** out_ptr, esp_partition_mmap_handle_t* out_handle) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_mmap out of Core 8 scope.");
#else
esp_err_t esp_partition_mmap(const esp_partition_t* partition, size_t offset, size_t size,
                             esp_partition_mmap_flag_t flags,
                             const void** out_ptr, esp_partition_mmap_handle_t* out_handle);
#endif

#if defined(__WINK_SIM__)
void esp_partition_munmap(esp_partition_mmap_handle_t handle) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_munmap out of Core 8 scope.");
#else
void esp_partition_munmap(esp_partition_mmap_handle_t handle);
#endif

#if defined(__WINK_SIM__)
esp_partition_iterator_t esp_partition_next(esp_partition_iterator_t iterator) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_next out of Core 8 scope.");
#else
esp_partition_iterator_t esp_partition_next(esp_partition_iterator_t iterator);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_partition_ptr_get_blockdev(const esp_partition_t *partition, esp_blockdev_handle_t *out_bdl_handle) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_ptr_get_blockdev out of Core 8 scope.");
#else
esp_err_t esp_partition_ptr_get_blockdev(const esp_partition_t *partition, esp_blockdev_handle_t *out_bdl_handle);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_partition_read(const esp_partition_t* partition,
                             size_t src_offset, void* dst, size_t size) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_read out of Core 8 scope.");
#else
esp_err_t esp_partition_read(const esp_partition_t* partition,
                             size_t src_offset, void* dst, size_t size);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_partition_read_raw(const esp_partition_t* partition,
                                 size_t src_offset, void* dst, size_t size) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_read_raw out of Core 8 scope.");
#else
esp_err_t esp_partition_read_raw(const esp_partition_t* partition,
                                 size_t src_offset, void* dst, size_t size);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_partition_register_external(esp_flash_t* flash_chip, size_t offset, size_t size,
                                          const char* label, esp_partition_type_t type, esp_partition_subtype_t subtype,
                                          const esp_partition_t** out_partition) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_register_external out of Core 8 scope.");
#else
esp_err_t esp_partition_register_external(esp_flash_t* flash_chip, size_t offset, size_t size,
                                          const char* label, esp_partition_type_t type, esp_partition_subtype_t subtype,
                                          const esp_partition_t** out_partition);
#endif

#if defined(__WINK_SIM__)
void esp_partition_unload_all(void) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_unload_all out of Core 8 scope.");
#else
void esp_partition_unload_all(void);
#endif

#if defined(__WINK_SIM__)
const esp_partition_t* esp_partition_verify(const esp_partition_t* partition) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_verify out of Core 8 scope.");
#else
const esp_partition_t* esp_partition_verify(const esp_partition_t* partition);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_partition_verify_err(const esp_partition_t* partition, const esp_partition_t** out_partition) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_verify_err out of Core 8 scope.");
#else
esp_err_t esp_partition_verify_err(const esp_partition_t* partition, const esp_partition_t** out_partition);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_partition_write(const esp_partition_t* partition,
                              size_t dst_offset, const void* src, size_t size) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_write out of Core 8 scope.");
#else
esp_err_t esp_partition_write(const esp_partition_t* partition,
                              size_t dst_offset, const void* src, size_t size);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_partition_write_raw(const esp_partition_t* partition,
                                  size_t dst_offset, const void* src, size_t size) WINK_SLA_ERROR("Wink SLA Violation: esp_partition_write_raw out of Core 8 scope.");
#else
esp_err_t esp_partition_write_raw(const esp_partition_t* partition,
                                  size_t dst_offset, const void* src, size_t size);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PARTITION_H__ */
#endif /* __ESP_PARTITION_H__ */
