/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_NVS_BOOTLOADER_H
#define WINK_H_GUARD_NVS_BOOTLOADER_H
#ifndef __WINK_HARVESTED_NVS_BOOTLOADER_H__
#define __WINK_HARVESTED_NVS_BOOTLOADER_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdint.h>

#include "esp_err.h"
#include "nvs.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
    char* buff_ptr;
    size_t buff_len;
} nvs_bootloader_str_value_placeholder_t;
typedef union {
    uint8_t u8_val;
    int8_t i8_val;
    uint16_t u16_val;
    int16_t i16_val;
    uint32_t u32_val;
    int32_t i32_val;
    uint64_t u64_val;
    int64_t i64_val;
    float float_val;
    double double_val;
    nvs_bootloader_str_value_placeholder_t str_val;
} nvs_bootloader_value_placeholder_t;
typedef struct {
    const char* namespace_name;
    const char* key_name;
    nvs_type_t value_type;
    esp_err_t result_code;
    nvs_bootloader_value_placeholder_t value;
    uint8_t namespace_index;
} nvs_bootloader_read_list_t;

esp_err_t nvs_bootloader_read(const char* partition_name,
                              const size_t read_list_count,
                              nvs_bootloader_read_list_t read_list[]);
esp_err_t nvs_bootloader_read_security_cfg(nvs_sec_scheme_t *scheme_cfg, nvs_sec_cfg_t* cfg);
void nvs_bootloader_secure_deinit(void);
esp_err_t nvs_bootloader_secure_init(const nvs_sec_cfg_t *sec_cfg);


#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_NVS_BOOTLOADER_H__ */
#endif /* WINK_H_GUARD_NVS_BOOTLOADER_H */
