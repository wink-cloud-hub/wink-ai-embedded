/**
 * @file pal_storage_esp32.c
 * @brief ESP32 NVS 存储实现（ADR-0008）。
 *
 * namespace "wink"，key 由调用方指定（device tree 用 "dtcfg"）。NVS 按 key 整体覆写
 * blob（原子），torn write 由读侧 CRC 兜底。非易失，跨重启保留。
 * 注：pal_storage_reset 在真机为 no-op（NVS 物理持久，host 测试语义不适用）。
 */
#include "pal_storage.h"

#include <string.h>
#include "nvs_flash.h"
#include "nvs.h"

#define PAL_STORAGE_NAMESPACE "wink"

void pal_storage_reset(void) {
    /* 真机 no-op：NVS 物理持久，host 测试 reset 语义不适用。 */
}

wink_status_t pal_storage_read(const char *key, uint8_t *buf, uint16_t cap, uint16_t *out_len) {
    if (key == NULL || buf == NULL || out_len == NULL) { return WINK_ERR_INVALID_ARG; }

    nvs_handle_t h;
    esp_err_t open = nvs_open(PAL_STORAGE_NAMESPACE, NVS_READONLY, &h);
    if (open == ESP_ERR_NVS_NOT_FOUND || open == ESP_ERR_NVS_NOT_INITIALIZED) {
        return WINK_ERR_EMPTY;                  /* namespace 不存在 → 调用方降级到编译期默认 */
    }
    if (open != ESP_OK) { return WINK_ERR_IO; }

    size_t required = cap;                      /* 输入=buf 容量；输出=实际长度 */
    esp_err_t get = nvs_get_blob(h, key, buf, &required);
    nvs_close(h);

    if (get == ESP_ERR_NVS_NOT_FOUND) { return WINK_ERR_EMPTY; }
    if (get == ESP_ERR_NVS_INVALID_LENGTH) { return WINK_ERR_INVALID_ARG; }  /* buf 过小 */
    if (get != ESP_OK) { return WINK_ERR_IO; }
    if (required > 0xFFFFu) { return WINK_ERR_INVALID_ARG; }                 /* 超 uint16 */

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
        return WINK_OK;                         /* namespace 不存在 = 已擦除，no-op 成功 */
    }
    if (open != ESP_OK) { return WINK_ERR_IO; }

    esp_err_t erase = nvs_erase_key(h, key);
    if (erase == ESP_ERR_NVS_NOT_FOUND) { nvs_close(h); return WINK_OK; }     /* key 不存在 = no-op */
    if (erase != ESP_OK) { nvs_close(h); return WINK_ERR_IO; }

    esp_err_t commit = nvs_commit(h);
    nvs_close(h);
    return (commit == ESP_OK) ? WINK_OK : WINK_ERR_IO;
}
