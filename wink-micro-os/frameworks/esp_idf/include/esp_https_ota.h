/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h"
#include "esp_app_desc.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_ERR_HTTPS_OTA_IN_PROGRESS (ESP_ERR_OTA_BASE + 0x10)

typedef struct esp_https_ota_context *esp_https_ota_handle_t;

typedef struct {
    const esp_http_client_config_t *http_config;
    int bulk_flash_erase;
    bool partial_http_download;
    int max_http_request_size;
} esp_https_ota_config_t;

esp_err_t esp_https_ota(const esp_https_ota_config_t *ota_config);
esp_err_t esp_https_ota_begin(const esp_https_ota_config_t *ota_config, esp_https_ota_handle_t *handle);
esp_err_t esp_https_ota_perform(esp_https_ota_handle_t https_ota_handle);
esp_err_t esp_https_ota_finish(esp_https_ota_handle_t https_ota_handle);
esp_err_t esp_https_ota_abort(esp_https_ota_handle_t https_ota_handle);
bool esp_https_ota_is_complete_data_received(esp_https_ota_handle_t https_ota_handle);
esp_err_t esp_https_ota_get_img_desc(esp_https_ota_handle_t https_ota_handle, esp_app_desc_t *new_app_info);
esp_err_t esp_https_ota_get_bootloader_img_desc(esp_https_ota_handle_t https_ota_handle, esp_bootloader_desc_t *new_img_info);
int esp_https_ota_get_image_len_read(esp_https_ota_handle_t https_ota_handle);
int esp_https_ota_get_status_code(esp_https_ota_handle_t https_ota_handle);
int esp_https_ota_get_image_size(esp_https_ota_handle_t https_ota_handle);

#ifdef __cplusplus
}
#endif
