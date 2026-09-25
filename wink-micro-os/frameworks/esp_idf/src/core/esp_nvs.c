// SPDX-License-Identifier: LGPL-3.0-only
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

#define NVS_MAX_HANDLES 4
#define NVS_MAX_ENTRIES 16
#define NVS_KEY_LEN 16
#define NVS_VAL_BUF_SIZE 128

_Static_assert(NVS_KEY_LEN >= 16, "NVS_KEY_LEN budget check");
_Static_assert(NVS_MAX_ENTRIES * NVS_VAL_BUF_SIZE <= 4096, "NVS value storage budget check");

typedef struct {
    bool in_use;
    char ns[NVS_KEY_LEN];
} esp_nvs_handle_t;

typedef struct {
    bool valid;
    char ns[NVS_KEY_LEN];
    char key[NVS_KEY_LEN];
    uint8_t data[NVS_VAL_BUF_SIZE];
    size_t len;
} nvs_entry_t;

static esp_nvs_handle_t s_nvs_handles[NVS_MAX_HANDLES];
static nvs_entry_t s_nvs_storage[NVS_MAX_ENTRIES];

esp_err_t nvs_flash_init(void) {
    // 仿真环境下内存 KV 表随进程生命周期初始化，直接返回成功
    return ESP_OK;
}

esp_err_t nvs_flash_deinit(void) {
    for (int i = 0; i < NVS_MAX_HANDLES; i++) {
        s_nvs_handles[i].in_use = false;
    }
    return ESP_OK;
}

esp_err_t nvs_open(const char *name, nvs_open_mode_t open_mode, nvs_handle_t *out_handle) {
    (void)open_mode;
    if (!name || !out_handle) {
        return ESP_ERR_INVALID_ARG;
    }
    for (int i = 0; i < NVS_MAX_HANDLES; i++) {
        if (!s_nvs_handles[i].in_use) {
            s_nvs_handles[i].in_use = true;
            strncpy(s_nvs_handles[i].ns, name, NVS_KEY_LEN - 1);
            s_nvs_handles[i].ns[NVS_KEY_LEN - 1] = '\0';
            *out_handle = (nvs_handle_t)(i + 1);
            return ESP_OK;
        }
    }
    return ESP_ERR_NVS_NOT_ENOUGH_SPACE;
}

void nvs_close(nvs_handle_t handle) {
    uint32_t idx = (uint32_t)handle;
    if (idx >= 1 && idx <= NVS_MAX_HANDLES) {
        s_nvs_handles[idx - 1].in_use = false;
    }
}

esp_err_t nvs_flash_erase(void) {
    memset(s_nvs_storage, 0, sizeof(s_nvs_storage));
    return ESP_OK;
}

esp_err_t nvs_set_blob(nvs_handle_t handle, const char *key, const void *value, size_t length) {
    uint32_t idx = (uint32_t)handle;
    if (idx < 1 || idx > NVS_MAX_HANDLES || !s_nvs_handles[idx - 1].in_use || !key || !value || length > NVS_VAL_BUF_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }
    const char *ns = s_nvs_handles[idx - 1].ns;
    for (int i = 0; i < NVS_MAX_ENTRIES; i++) {
        if (s_nvs_storage[i].valid && strcmp(s_nvs_storage[i].ns, ns) == 0 && strcmp(s_nvs_storage[i].key, key) == 0) {
            memcpy(s_nvs_storage[i].data, value, length);
            s_nvs_storage[i].len = length;
            return ESP_OK;
        }
    }
    for (int i = 0; i < NVS_MAX_ENTRIES; i++) {
        if (!s_nvs_storage[i].valid) {
            s_nvs_storage[i].valid = true;
            strncpy(s_nvs_storage[i].ns, ns, NVS_KEY_LEN - 1);
            s_nvs_storage[i].ns[NVS_KEY_LEN - 1] = '\0';
            strncpy(s_nvs_storage[i].key, key, NVS_KEY_LEN - 1);
            s_nvs_storage[i].key[NVS_KEY_LEN - 1] = '\0';
            memcpy(s_nvs_storage[i].data, value, length);
            s_nvs_storage[i].len = length;
            return ESP_OK;
        }
    }
    return ESP_ERR_NVS_NOT_ENOUGH_SPACE;
}

esp_err_t nvs_get_blob(nvs_handle_t handle, const char *key, void *out_value, size_t *length) {
    uint32_t idx = (uint32_t)handle;
    if (idx < 1 || idx > NVS_MAX_HANDLES || !s_nvs_handles[idx - 1].in_use || !key || !length) {
        return ESP_ERR_INVALID_ARG;
    }
    const char *ns = s_nvs_handles[idx - 1].ns;
    for (int i = 0; i < NVS_MAX_ENTRIES; i++) {
        if (s_nvs_storage[i].valid && strcmp(s_nvs_storage[i].ns, ns) == 0 && strcmp(s_nvs_storage[i].key, key) == 0) {
            if (out_value != NULL) {
                if (*length < s_nvs_storage[i].len) {
                    return ESP_ERR_INVALID_ARG;
                }
                memcpy(out_value, s_nvs_storage[i].data, s_nvs_storage[i].len);
            }
            *length = s_nvs_storage[i].len;
            return ESP_OK;
        }
    }
    return ESP_ERR_NVS_NOT_FOUND;
}

esp_err_t nvs_set_u8(nvs_handle_t h, const char *k, uint8_t v) {
    return nvs_set_blob(h, k, &v, sizeof(v));
}

esp_err_t nvs_get_u8(nvs_handle_t h, const char *k, uint8_t *v) {
    size_t l = sizeof(*v);
    return nvs_get_blob(h, k, v, &l);
}

esp_err_t nvs_set_i8(nvs_handle_t h, const char *k, int8_t v) {
    return nvs_set_blob(h, k, &v, sizeof(v));
}

esp_err_t nvs_get_i8(nvs_handle_t h, const char *k, int8_t *v) {
    size_t l = sizeof(*v);
    return nvs_get_blob(h, k, v, &l);
}

esp_err_t nvs_set_u16(nvs_handle_t h, const char *k, uint16_t v) {
    return nvs_set_blob(h, k, &v, sizeof(v));
}

esp_err_t nvs_get_u16(nvs_handle_t h, const char *k, uint16_t *v) {
    size_t l = sizeof(*v);
    return nvs_get_blob(h, k, v, &l);
}

esp_err_t nvs_set_i16(nvs_handle_t h, const char *k, int16_t v) {
    return nvs_set_blob(h, k, &v, sizeof(v));
}

esp_err_t nvs_get_i16(nvs_handle_t h, const char *k, int16_t *v) {
    size_t l = sizeof(*v);
    return nvs_get_blob(h, k, v, &l);
}

esp_err_t nvs_set_u32(nvs_handle_t h, const char *k, uint32_t v) {
    return nvs_set_blob(h, k, &v, sizeof(v));
}

esp_err_t nvs_get_u32(nvs_handle_t h, const char *k, uint32_t *v) {
    size_t l = sizeof(*v);
    return nvs_get_blob(h, k, v, &l);
}

esp_err_t nvs_set_i32(nvs_handle_t h, const char *k, int32_t v) {
    return nvs_set_blob(h, k, &v, sizeof(v));
}

esp_err_t nvs_get_i32(nvs_handle_t h, const char *k, int32_t *v) {
    size_t l = sizeof(*v);
    return nvs_get_blob(h, k, v, &l);
}

esp_err_t nvs_set_u64(nvs_handle_t h, const char *k, uint64_t v) {
    return nvs_set_blob(h, k, &v, sizeof(v));
}

esp_err_t nvs_get_u64(nvs_handle_t h, const char *k, uint64_t *v) {
    size_t l = sizeof(*v);
    return nvs_get_blob(h, k, v, &l);
}

esp_err_t nvs_set_i64(nvs_handle_t h, const char *k, int64_t v) {
    return nvs_set_blob(h, k, &v, sizeof(v));
}

esp_err_t nvs_get_i64(nvs_handle_t h, const char *k, int64_t *v) {
    size_t l = sizeof(*v);
    return nvs_get_blob(h, k, v, &l);
}

esp_err_t nvs_set_str(nvs_handle_t h, const char *k, const char *v) {
    if (!v) {
        return ESP_ERR_INVALID_ARG;
    }
    return nvs_set_blob(h, k, v, strlen(v) + 1);
}

esp_err_t nvs_get_str(nvs_handle_t h, const char *k, char *v, size_t *l) {
    return nvs_get_blob(h, k, v, l);
}

esp_err_t nvs_commit(nvs_handle_t h) {
    (void)h;
    return ESP_OK;
}
