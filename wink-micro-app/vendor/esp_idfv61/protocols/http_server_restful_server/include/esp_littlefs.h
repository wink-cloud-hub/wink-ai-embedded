/* SPDX-License-Identifier: CC0-1.0 */
#pragma once
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *base_path;
    const char *partition_label;
    bool format_if_mount_failed;
} esp_vfs_littlefs_conf_t;

static inline esp_err_t esp_vfs_littlefs_register(const esp_vfs_littlefs_conf_t *conf) {
    (void)conf;
    return ESP_OK;
}

static inline esp_err_t esp_littlefs_info(const char *partition_label, size_t *total_bytes, size_t *used_bytes) {
    (void)partition_label;
    if (total_bytes) *total_bytes = 0;
    if (used_bytes) *used_bytes = 0;
    return ESP_OK;
}

static inline esp_err_t esp_littlefs_format(const char *partition_label) {
    (void)partition_label;
    return ESP_OK;
}

#ifdef __cplusplus
}
#endif
