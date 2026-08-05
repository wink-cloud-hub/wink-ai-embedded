// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_storage_esp32.c
 * @brief ESP32 NVS storage implementation.
 */
#include "pal_storage.h"

#include <string.h>
#include "nvs_flash.h"
#include "nvs.h"

#define PAL_STORAGE_NAMESPACE "wink"

void pal_storage_reset(void) {
}

wink_status_t pal_storage_read(const char *key, uint8_t *buf, uint16_t cap, uint16_t *out_len) {
    if (key == NULL || buf == NULL || out_len == NULL) { return WINK_ERR_INVALID_ARG; }

    nvs_handle_t h;
    esp_err_t open = nvs_open(PAL_STORAGE_NAMESPACE, NVS_READONLY, &h);
    if (open == ESP_ERR_NVS_NOT_FOUND || open == ESP_ERR_NVS_NOT_INITIALIZED) {
        return WINK_ERR_EMPTY;
    }
    if (open != ESP_OK) { return WINK_ERR_IO; }

    size_t required = cap;
    esp_err_t get = nvs_get_blob(h, key, buf, &required);
    nvs_close(h);

    if (get == ESP_ERR_NVS_NOT_FOUND) { return WINK_ERR_EMPTY; }
    if (get == ESP_ERR_NVS_INVALID_LENGTH) { return WINK_ERR_INVALID_ARG; }
    if (get != ESP_OK) { return WINK_ERR_IO; }
    if (required > 0xFFFFu) { return WINK_ERR_INVALID_ARG; }

    *out_len = (uint16_t)required;
    return WINK_OK;
}

wink_status_t pal_storage_write(const char *key, const uint8_t *buf, uint16_t len) {
    if (key == NULL) { return WINK_ERR_INVALID_ARG; }
    if (buf == NULL && len > 0u) { return WINK_ERR_INVALID_ARG; }

    nvs_handle_t h;
    esp_err_t open = nvs_open(PAL_STORAGE_NAMESPACE, NVS_READWRITE, &h);
    if (open != ESP_OK) { return WINK_ERR_IO; }

    esp_err_t set = nvs_set_blob(h, key, buf, (size_t)len);
    if (set != ESP_OK) { nvs_close(h); return WINK_ERR_IO; }

    esp_err_t commit = nvs_commit(h);
    nvs_close(h);
    return (commit == ESP_OK) ? WINK_OK : WINK_ERR_IO;
}

wink_status_t pal_storage_erase(const char *key) {
    if (key == NULL) { return WINK_ERR_INVALID_ARG; }

    nvs_handle_t h;
    esp_err_t open = nvs_open(PAL_STORAGE_NAMESPACE, NVS_READWRITE, &h);
    if (open == ESP_ERR_NVS_NOT_FOUND || open == ESP_ERR_NVS_NOT_INITIALIZED) {
        return WINK_OK;
    }
    if (open != ESP_OK) { return WINK_ERR_IO; }

    esp_err_t erase = nvs_erase_key(h, key);
    if (erase == ESP_ERR_NVS_NOT_FOUND) { nvs_close(h); return WINK_OK; }
    if (erase != ESP_OK) { nvs_close(h); return WINK_ERR_IO; }

    esp_err_t commit = nvs_commit(h);
    nvs_close(h);
    return (commit == ESP_OK) ? WINK_OK : WINK_ERR_IO;
}
