// SPDX-License-Identifier: LGPL-3.0-only
/**
 * @file esp_spiffs.h
 * @brief ESP-IDF SPIFFS filesystem facade (WinkMicroOS simulation).
 */
#ifndef _ESP_SPIFFS_H_
#define _ESP_SPIFFS_H_

#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configuration structure for esp_vfs_spiffs_register
 */
typedef struct {
    const char* base_path;          /*!< File path prefix associated with the filesystem. */
    const char* partition_label;    /*!< Optional partition label. */
    size_t max_files;               /*!< Maximum files that could be open at the same time. */
    bool format_if_mount_failed;    /*!< If true, format filesystem if mount fails. */
} esp_vfs_spiffs_conf_t;

/**
 * @brief Register and mount SPIFFS to VFS with given path prefix.
 */
esp_err_t esp_vfs_spiffs_register(const esp_vfs_spiffs_conf_t *conf);

/**
 * @brief Unregister and unmount SPIFFS from VFS.
 */
esp_err_t esp_vfs_spiffs_unregister(const char *partition_label);

/**
 * @brief Check if SPIFFS is mounted.
 */
bool esp_spiffs_mounted(const char *partition_label);

/**
 * @brief Format the SPIFFS partition.
 */
esp_err_t esp_spiffs_format(const char *partition_label);

/**
 * @brief Get partition size information for SPIFFS.
 */
esp_err_t esp_spiffs_info(const char *partition_label, size_t *total_bytes, size_t *used_bytes);

/**
 * @brief Check integrity of SPIFFS filesystem.
 */
esp_err_t esp_spiffs_check(const char *partition_label);

/**
 * @brief Perform garbage collection in SPIFFS partition.
 */
esp_err_t esp_spiffs_gc(const char *partition_label, size_t size_to_gc);

#ifdef __cplusplus
}
#endif

#endif /* _ESP_SPIFFS_H_ */
