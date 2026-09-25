/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_NVS_FLASH_H
#define WINK_H_GUARD_NVS_FLASH_H
#ifndef __WINK_HARVESTED_NVS_FLASH_H__
#define __WINK_HARVESTED_NVS_FLASH_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"

#include "esp_partition.h"
#include "nvs.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef NVS_KEY_SIZE
#define NVS_KEY_SIZE 32
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
    uint8_t eky[NVS_KEY_SIZE];
    uint8_t tky[NVS_KEY_SIZE];
} nvs_sec_cfg_t;
typedef esp_err_t (*nvs_flash_generate_keys_t) (const void *scheme_data, nvs_sec_cfg_t* cfg);
typedef esp_err_t (*nvs_flash_read_cfg_t) (const void *scheme_data, nvs_sec_cfg_t* cfg);
typedef struct {
    int scheme_id;
    void * scheme_data;
    nvs_flash_generate_keys_t nvs_flash_key_gen;
    nvs_flash_read_cfg_t nvs_flash_read_cfg;
} nvs_sec_scheme_t;

esp_err_t nvs_flash_deinit(void);
esp_err_t nvs_flash_deinit_partition(const char* partition_label);
void nvs_flash_deregister_security_scheme(void);
esp_err_t nvs_flash_erase(void);
esp_err_t nvs_flash_erase_partition(const char *part_name);
esp_err_t nvs_flash_erase_partition_ptr(const esp_partition_t *partition);
esp_err_t nvs_flash_generate_keys(const esp_partition_t* partition, nvs_sec_cfg_t* cfg);
esp_err_t nvs_flash_generate_keys_v2(nvs_sec_scheme_t *scheme_cfg, nvs_sec_cfg_t* cfg);
nvs_sec_scheme_t * nvs_flash_get_default_security_scheme(void);
esp_err_t nvs_flash_init(void);
esp_err_t nvs_flash_init_partition(const char *partition_label);
esp_err_t nvs_flash_init_partition_bdl(const char* partition_label, esp_blockdev_handle_t bdl);
esp_err_t nvs_flash_init_partition_ptr(const esp_partition_t *partition);
esp_err_t nvs_flash_read_security_cfg(const esp_partition_t* partition, nvs_sec_cfg_t* cfg);
esp_err_t nvs_flash_read_security_cfg_v2(nvs_sec_scheme_t *scheme_cfg, nvs_sec_cfg_t* cfg);
esp_err_t nvs_flash_register_security_scheme(nvs_sec_scheme_t *scheme_cfg);
esp_err_t nvs_flash_secure_init(nvs_sec_cfg_t* cfg);
esp_err_t nvs_flash_secure_init_partition(const char *partition_label, nvs_sec_cfg_t* cfg);


#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_NVS_FLASH_H__ */
#endif /* WINK_H_GUARD_NVS_FLASH_H */
