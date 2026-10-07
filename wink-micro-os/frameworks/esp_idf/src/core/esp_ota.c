/* SPDX-License-Identifier: LGPL-3.0-only */
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esp_partition.h"
#include "esp_app_desc.h"
#include "esp_bootloader_desc.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char *TAG = "esp_ota";

#define MAX_OTA_SESSIONS 4

typedef struct {
    bool in_use;
    const esp_partition_t *partition;
    size_t image_size;
    size_t written_bytes;
    bool finalized;
} sim_ota_session_t;

static sim_ota_session_t s_ota_sessions[MAX_OTA_SESSIONS];
static const esp_partition_t *s_running_partition = NULL;
static const esp_partition_t *s_boot_partition = NULL;

static void ensure_partitions_init(void) {
    if (s_running_partition == NULL) {
        s_running_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
        if (s_running_partition == NULL) {
            s_running_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
        }
        if (s_running_partition == NULL) {
            s_running_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);
        }
    }
    if (s_boot_partition == NULL) {
        s_boot_partition = s_running_partition;
    }
}

const esp_partition_t* esp_ota_get_running_partition(void) {
    ensure_partitions_init();
    return s_running_partition;
}

const esp_partition_t* esp_ota_get_boot_partition(void) {
    ensure_partitions_init();
    return s_boot_partition ? s_boot_partition : s_running_partition;
}

const esp_partition_t* esp_ota_get_next_update_partition(const esp_partition_t *start_from) {
    ensure_partitions_init();
    const esp_partition_t *curr = start_from ? start_from : s_running_partition;
    if (curr != NULL && curr->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0) {
        const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
        if (p) return p;
    }
    const esp_partition_t *p0 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
    if (p0) return p0;
    return esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
}

esp_err_t esp_ota_begin(const esp_partition_t* partition, size_t image_size, esp_ota_handle_t* out_handle) {
    if (!partition || !out_handle) {
        return ESP_ERR_INVALID_ARG;
    }
    ensure_partitions_init();
    if (partition == s_running_partition) {
        return ESP_ERR_OTA_PARTITION_CONFLICT;
    }
    if (partition->type != ESP_PARTITION_TYPE_APP) {
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = 0; i < MAX_OTA_SESSIONS; i++) {
        if (!s_ota_sessions[i].in_use) {
            s_ota_sessions[i].in_use = true;
            s_ota_sessions[i].partition = partition;
            s_ota_sessions[i].image_size = image_size;
            s_ota_sessions[i].written_bytes = 0;
            s_ota_sessions[i].finalized = false;
            *out_handle = (esp_ota_handle_t)(i + 1);
            ESP_LOGI(TAG, "Commenced OTA update on %s (size %u)", partition->label, (unsigned int)image_size);
            return ESP_OK;
        }
    }
    return ESP_ERR_OTA_ALREADY_IN_PROGRESS;
}

esp_err_t esp_ota_resume(const esp_partition_t *partition, const size_t erase_size, const size_t image_offset, esp_ota_handle_t *out_handle) {
    (void)erase_size;
    esp_err_t err = esp_ota_begin(partition, OTA_SIZE_UNKNOWN, out_handle);
    if (err == ESP_OK && out_handle && *out_handle > 0 && *out_handle <= MAX_OTA_SESSIONS) {
        s_ota_sessions[*out_handle - 1].written_bytes = image_offset;
    }
    return err;
}

esp_err_t esp_ota_write(esp_ota_handle_t handle, const void* data, size_t size) {
    if (handle == 0 || handle > MAX_OTA_SESSIONS || !data) {
        return ESP_ERR_INVALID_ARG;
    }
    sim_ota_session_t *sess = &s_ota_sessions[handle - 1];
    if (!sess->in_use || sess->finalized) {
        return ESP_ERR_INVALID_ARG;
    }
    if (size == 0) {
        return ESP_OK;
    }

    esp_err_t err = esp_partition_write(sess->partition, sess->written_bytes, data, size);
    if (err == ESP_OK) {
        sess->written_bytes += size;
    }
    return err;
}

esp_err_t esp_ota_write_with_offset(esp_ota_handle_t handle, const void *data, size_t size, uint32_t offset) {
    if (handle == 0 || handle > MAX_OTA_SESSIONS || !data) {
        return ESP_ERR_INVALID_ARG;
    }
    sim_ota_session_t *sess = &s_ota_sessions[handle - 1];
    if (!sess->in_use || sess->finalized) {
        return ESP_ERR_INVALID_ARG;
    }
    return esp_partition_write(sess->partition, offset, data, size);
}

esp_err_t esp_ota_end(esp_ota_handle_t handle) {
    if (handle == 0 || handle > MAX_OTA_SESSIONS) {
        return ESP_ERR_INVALID_ARG;
    }
    sim_ota_session_t *sess = &s_ota_sessions[handle - 1];
    if (!sess->in_use) {
        return ESP_ERR_NOT_FOUND;
    }
    sess->finalized = true;
    sess->in_use = false;
    ESP_LOGI(TAG, "OTA update complete, total written %u bytes", (unsigned int)sess->written_bytes);
    return ESP_OK;
}

esp_err_t esp_ota_abort(esp_ota_handle_t handle) {
    if (handle == 0 || handle > MAX_OTA_SESSIONS) {
        return ESP_ERR_INVALID_ARG;
    }
    sim_ota_session_t *sess = &s_ota_sessions[handle - 1];
    if (!sess->in_use) {
        return ESP_ERR_NOT_FOUND;
    }
    sess->in_use = false;
    sess->finalized = false;
    return ESP_OK;
}

esp_err_t esp_ota_set_boot_partition(const esp_partition_t* partition) {
    if (!partition || partition->type != ESP_PARTITION_TYPE_APP) {
        return ESP_ERR_INVALID_ARG;
    }
    ensure_partitions_init();
    s_boot_partition = partition;
    ESP_LOGI(TAG, "Next boot partition set to %s", partition->label);
    return ESP_OK;
}

esp_err_t esp_ota_set_boot_partition_skip_validate(const esp_partition_t* partition) {
    return esp_ota_set_boot_partition(partition);
}

esp_err_t esp_ota_mark_app_valid_cancel_rollback(void) {
    return ESP_OK;
}

esp_err_t esp_ota_mark_app_invalid_rollback(void) {
    return ESP_OK;
}

esp_err_t esp_ota_mark_app_invalid_rollback_and_reboot(void) {
    return ESP_OK;
}

esp_err_t esp_ota_get_partition_description(const esp_partition_t *partition, esp_app_desc_t *app_desc) {
    if (!partition || !app_desc) {
        return ESP_ERR_INVALID_ARG;
    }
    const esp_app_desc_t *desc = esp_app_get_description();
    memcpy(app_desc, desc, sizeof(esp_app_desc_t));
    return ESP_OK;
}

esp_err_t esp_ota_get_bootloader_description(const esp_partition_t *bootloader_partition, esp_bootloader_desc_t *desc) {
    if (!desc) {
        return ESP_ERR_INVALID_ARG;
    }
    const esp_bootloader_desc_t *bdesc = esp_bootloader_get_description();
    memcpy(desc, bdesc, sizeof(esp_bootloader_desc_t));
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * App & Bootloader Description APIs
 * ═══════════════════════════════════════════════════════════════════════════ */

static const esp_app_desc_t s_app_desc = {
    .magic_word = ESP_APP_DESC_MAGIC_WORD,
    .secure_version = 0,
    .version = "1.0.0",
    .project_name = "simple_ota_example",
    .time = "12:00:00",
    .date = "2026-10-07",
    .idf_ver = "v6.1",
    .app_elf_sha256 = { 0xAA, 0xBB, 0xCC, 0xDD },
    .min_efuse_blk_rev_full = 0,
    .max_efuse_blk_rev_full = 0,
    .mmu_page_size = 16,
    .spi_flash_mode = 0,
};

const esp_app_desc_t *esp_app_get_description(void) {
    return &s_app_desc;
}

int esp_app_get_elf_sha256(char* dst, size_t size) {
    if (!dst || size == 0) return 0;
    static const char *dummy_hex = "aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899";
    size_t len = strlen(dummy_hex);
    if (len >= size) len = size - 1;
    memcpy(dst, dummy_hex, len);
    dst[len] = '\0';
    return (int)len;
}

static const esp_bootloader_desc_t s_bootloader_desc = {
    .magic_byte = ESP_BOOTLOADER_DESC_MAGIC_BYTE,
    .secure_version = 0,
    .version = 1,
    .idf_ver = "v6.1",
    .date_time = "2026-10-07 12:00:00",
};

const esp_bootloader_desc_t *esp_bootloader_get_description(void) {
    return &s_bootloader_desc;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * ESP HTTPS OTA Client Implementation
 * ═══════════════════════════════════════════════════════════════════════════ */

struct esp_https_ota_context {
    esp_https_ota_config_t config;
    esp_http_client_handle_t http_client;
    esp_ota_handle_t ota_handle;
    const esp_partition_t *update_partition;
    int status_code;
    int image_size;
    int image_len_read;
    bool is_complete;
};

esp_err_t esp_https_ota_begin(const esp_https_ota_config_t *ota_config, esp_https_ota_handle_t *handle) {
    if (!ota_config || !ota_config->http_config || !handle) {
        return ESP_ERR_INVALID_ARG;
    }

    struct esp_https_ota_context *ctx = (struct esp_https_ota_context*)calloc(1, sizeof(*ctx));
    if (!ctx) {
        return ESP_ERR_NO_MEM;
    }
    ctx->config = *ota_config;

    ctx->http_client = esp_http_client_init(ota_config->http_config);
    if (!ctx->http_client) {
        free(ctx);
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_open(ctx->http_client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(ctx->http_client);
        free(ctx);
        return err;
    }

    int64_t content_len = esp_http_client_fetch_headers(ctx->http_client);
    ctx->status_code = esp_http_client_get_status_code(ctx->http_client);
    if (ctx->status_code != 200) {
        ESP_LOGE(TAG, "HTTP server returned unexpected status code: %d", ctx->status_code);
        esp_http_client_cleanup(ctx->http_client);
        free(ctx);
        return ESP_FAIL;
    }

    ctx->image_size = (int)content_len;
    ctx->update_partition = esp_ota_get_next_update_partition(NULL);
    if (!ctx->update_partition) {
        ESP_LOGE(TAG, "Passive OTA partition not found");
        esp_http_client_cleanup(ctx->http_client);
        free(ctx);
        return ESP_ERR_NOT_FOUND;
    }

    err = esp_ota_begin(ctx->update_partition, content_len > 0 ? (size_t)content_len : OTA_SIZE_UNKNOWN, &ctx->ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
        esp_http_client_cleanup(ctx->http_client);
        free(ctx);
        return err;
    }

    *handle = ctx;
    return ESP_OK;
}

esp_err_t esp_https_ota_perform(esp_https_ota_handle_t https_ota_handle) {
    if (!https_ota_handle) {
        return ESP_ERR_INVALID_ARG;
    }
    struct esp_https_ota_context *ctx = (struct esp_https_ota_context*)https_ota_handle;

    char buf[512];
    int data_read = esp_http_client_read(ctx->http_client, buf, sizeof(buf));
    if (data_read < 0) {
        ESP_LOGE(TAG, "Error: SSL data read error");
        return ESP_FAIL;
    } else if (data_read > 0) {
        esp_err_t err = esp_ota_write(ctx->ota_handle, buf, data_read);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Error: write data to ota partition failed: %s", esp_err_to_name(err));
            return err;
        }
        ctx->image_len_read += data_read;
        if (esp_http_client_is_complete_data_received(ctx->http_client)) {
            ctx->is_complete = true;
            return ESP_OK;
        }
        return ESP_ERR_HTTPS_OTA_IN_PROGRESS;
    } else {
        // data_read == 0, end of stream
        ctx->is_complete = true;
        return ESP_OK;
    }
}

bool esp_https_ota_is_complete_data_received(esp_https_ota_handle_t https_ota_handle) {
    if (!https_ota_handle) return false;
    struct esp_https_ota_context *ctx = (struct esp_https_ota_context*)https_ota_handle;
    return ctx->is_complete;
}

esp_err_t esp_https_ota_finish(esp_https_ota_handle_t https_ota_handle) {
    if (!https_ota_handle) return ESP_ERR_INVALID_ARG;
    struct esp_https_ota_context *ctx = (struct esp_https_ota_context*)https_ota_handle;

    esp_err_t err = esp_ota_end(ctx->ota_handle);
    if (err == ESP_OK) {
        err = esp_ota_set_boot_partition(ctx->update_partition);
    }
    esp_http_client_cleanup(ctx->http_client);
    free(ctx);
    return err;
}

esp_err_t esp_https_ota_abort(esp_https_ota_handle_t https_ota_handle) {
    if (!https_ota_handle) return ESP_ERR_INVALID_ARG;
    struct esp_https_ota_context *ctx = (struct esp_https_ota_context*)https_ota_handle;
    esp_ota_abort(ctx->ota_handle);
    esp_http_client_cleanup(ctx->http_client);
    free(ctx);
    return ESP_OK;
}

esp_err_t esp_https_ota_get_img_desc(esp_https_ota_handle_t https_ota_handle, esp_app_desc_t *new_app_info) {
    if (!new_app_info) return ESP_ERR_INVALID_ARG;
    memcpy(new_app_info, esp_app_get_description(), sizeof(esp_app_desc_t));
    return ESP_OK;
}

esp_err_t esp_https_ota_get_bootloader_img_desc(esp_https_ota_handle_t https_ota_handle, esp_bootloader_desc_t *new_img_info) {
    if (!new_img_info) return ESP_ERR_INVALID_ARG;
    memcpy(new_img_info, esp_bootloader_get_description(), sizeof(esp_bootloader_desc_t));
    return ESP_OK;
}

int esp_https_ota_get_image_len_read(esp_https_ota_handle_t https_ota_handle) {
    if (!https_ota_handle) return -1;
    return ((struct esp_https_ota_context*)https_ota_handle)->image_len_read;
}

int esp_https_ota_get_status_code(esp_https_ota_handle_t https_ota_handle) {
    if (!https_ota_handle) return -1;
    return ((struct esp_https_ota_context*)https_ota_handle)->status_code;
}

int esp_https_ota_get_image_size(esp_https_ota_handle_t https_ota_handle) {
    if (!https_ota_handle) return -1;
    return ((struct esp_https_ota_context*)https_ota_handle)->image_size;
}

esp_err_t esp_https_ota(const esp_https_ota_config_t *ota_config) {
    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(ota_config, &https_ota_handle);
    if (err != ESP_OK) {
        return err;
    }

    while (1) {
        err = esp_https_ota_perform(https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
    }

    if (esp_https_ota_is_complete_data_received(https_ota_handle)) {
        esp_err_t finish_err = esp_https_ota_finish(https_ota_handle);
        if (err == ESP_OK) {
            err = finish_err;
        }
    } else {
        esp_https_ota_abort(https_ota_handle);
        if (err == ESP_OK) {
            err = ESP_FAIL;
        }
    }
    return err;
}
