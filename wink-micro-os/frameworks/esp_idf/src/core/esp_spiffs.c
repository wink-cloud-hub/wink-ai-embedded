// SPDX-License-Identifier: LGPL-3.0-only
/**
 * @file esp_spiffs.c
 * @brief ESP-IDF SPIFFS filesystem simulation facade (ADR-0092 Tier 2).
 */
#include "esp_spiffs.h"
#include "esp_vfs_ram.h"
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

#if defined(__EMSCRIPTEN__) || defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#elif defined(_WIN32)
#include <direct.h>
#define mkdir(p, m) _mkdir(p)
#endif

static bool s_mounted = false;
static char s_base_path[64] = "/spiffs";
static char s_partition_label[32] = {0};

esp_err_t esp_vfs_spiffs_register(const esp_vfs_spiffs_conf_t *conf)
{
    if (conf == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_mounted) {
        return ESP_ERR_INVALID_STATE;
    }

    if (conf->base_path != NULL && conf->base_path[0] != '\0') {
        strncpy(s_base_path, conf->base_path, sizeof(s_base_path) - 1);
        s_base_path[sizeof(s_base_path) - 1] = '\0';
    } else {
        strncpy(s_base_path, "/spiffs", sizeof(s_base_path) - 1);
    }

    if (conf->partition_label != NULL) {
        strncpy(s_partition_label, conf->partition_label, sizeof(s_partition_label) - 1);
        s_partition_label[sizeof(s_partition_label) - 1] = '\0';
    } else {
        s_partition_label[0] = '\0';
    }

    /* Initialize pure RAM sandbox VFS */
    esp_vfs_ram_init();

    /* Ensure mount base directory exists in virtual/host filesystem */
    (void)mkdir(s_base_path, 0755);

    s_mounted = true;
    return ESP_OK;
}

esp_err_t esp_vfs_spiffs_unregister(const char *partition_label)
{
    (void)partition_label;
    if (!s_mounted) {
        return ESP_ERR_INVALID_STATE;
    }
    s_mounted = false;
    return ESP_OK;
}

bool esp_spiffs_mounted(const char *partition_label)
{
    (void)partition_label;
    return s_mounted;
}

esp_err_t esp_spiffs_format(const char *partition_label)
{
    (void)partition_label;
    /* Format memory sandbox and recreate base mount dir */
    esp_vfs_ram_reset();
    (void)mkdir(s_base_path, 0755);
    return ESP_OK;
}

esp_err_t esp_spiffs_info(const char *partition_label, size_t *total_bytes, size_t *used_bytes)
{
    (void)partition_label;
    if (!s_mounted) {
        return ESP_ERR_INVALID_STATE;
    }
    if (total_bytes != NULL) {
        *total_bytes = 896321; /* Standard 1MB partition size with SPIFFS overhead */
    }
    if (used_bytes != NULL) {
        *used_bytes = 0;
    }
    return ESP_OK;
}

esp_err_t esp_spiffs_check(const char *partition_label)
{
    (void)partition_label;
    if (!s_mounted) {
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

esp_err_t esp_spiffs_gc(const char *partition_label, size_t size_to_gc)
{
    (void)partition_label;
    (void)size_to_gc;
    if (!s_mounted) {
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}
