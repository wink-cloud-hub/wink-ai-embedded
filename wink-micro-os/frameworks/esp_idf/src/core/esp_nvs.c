// SPDX-License-Identifier: LGPL-3.0-only
#include "nvs_flash.h"
#include "esp_sim_handle.h"
#include "nvs.h"
#include "esp_fault.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#if defined(_WIN32)
#  include <direct.h>
#  include <io.h>
#  include <sys/stat.h>
#  define nvs_mkdir(dir) _mkdir(dir)
#  define nvs_unlink(path) _unlink(path)
#else
#  include <sys/stat.h>
#  include <unistd.h>
#  define nvs_mkdir(dir) mkdir(dir, 0755)
#  define nvs_unlink(path) unlink(path)
#endif

#define NVS_MAX_HANDLES 4
_Static_assert(NVS_MAX_HANDLES <= 64, "NVS_MAX_HANDLES must not exceed 64 (handle encoding limit)");

#ifndef NVS_MAX_ENTRIES
#  ifdef CONFIG_NVS_MAX_ENTRIES
#    define NVS_MAX_ENTRIES CONFIG_NVS_MAX_ENTRIES
#  else
#    define NVS_MAX_ENTRIES 16
#  endif
#endif

#define NVS_KEY_LEN 16
#define NVS_VAL_BUF_SIZE 128

_Static_assert(NVS_KEY_LEN >= 16, "NVS_KEY_LEN budget check");
_Static_assert(NVS_MAX_ENTRIES * NVS_VAL_BUF_SIZE <= 16384, "NVS value storage budget check");

#define ESP_SIM_NVS_MAGIC 0x4E565331u /* "NVS1" */
#define ESP_SIM_NVS_VERSION 2

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;         /* 0x4E565331 ("NVS1") */
    uint16_t version;       /* 1 or 2 */
    uint16_t entry_count;   /* Number of valid entries */
    uint32_t crc32;         /* CRC32 of valid entries data */
    uint32_t reserved;      /* Alignment */
} esp_sim_nvs_header_t;

typedef struct {
    char     ns[NVS_KEY_LEN];
    char     key[NVS_KEY_LEN];
    uint32_t len;
    uint8_t  data[NVS_VAL_BUF_SIZE];
} esp_sim_nvs_record_v1_t;

typedef struct {
    char     ns[NVS_KEY_LEN];
    char     key[NVS_KEY_LEN];
    uint32_t len;
    uint32_t type;
    uint8_t  data[NVS_VAL_BUF_SIZE];
} esp_sim_nvs_record_t;
#pragma pack(pop)

typedef struct {
    bool in_use;
    uint32_t token;
    char ns[NVS_KEY_LEN];
} esp_nvs_handle_t;

typedef struct {
    bool valid;
    char ns[NVS_KEY_LEN];
    char key[NVS_KEY_LEN];
    uint8_t data[NVS_VAL_BUF_SIZE];
    size_t len;
    nvs_type_t type;
} nvs_entry_t;

static esp_nvs_handle_t s_nvs_handles[NVS_MAX_HANDLES];
static nvs_entry_t s_nvs_storage[NVS_MAX_ENTRIES];

static esp_nvs_handle_t *resolve_nvs_handle(nvs_handle_t handle) {
    uint32_t slot;
    if (!esp_sim_handle_decode((void *)(uintptr_t)handle,
                               ESP_SIM_HANDLE_NVS, NVS_MAX_HANDLES, &slot)) {
        return NULL;
    }
    esp_nvs_handle_t *record = &s_nvs_handles[slot];
    return record->in_use && record->token == handle ? record : NULL;
}

#if defined(__EMSCRIPTEN__)
__attribute__((weak)) int wink_wasm_nvs_save(const void *buf, size_t size);
__attribute__((weak)) int wink_wasm_nvs_load(void *buf, size_t max_size, size_t *out_size);
#endif

static uint32_t esp_sim_nvs_crc32_update(uint32_t crc, const uint8_t *data, size_t length) {
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (-(int)(crc & 1)));
        }
    }
    return crc;
}

static bool esp_sim_nvs_path_is_directory(const char *path) {
#if defined(_WIN32)
    struct _stat st;
    return _stat(path, &st) == 0 && (st.st_mode & _S_IFDIR) != 0;
#else
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

static esp_err_t esp_sim_nvs_get_sandbox_paths(char *out_final, size_t final_sz, char *out_tmp, size_t tmp_sz) {
    const char *dir = getenv("WINK_SIM_SANDBOX_DIR");
    if (!dir || dir[0] == '\0') {
        dir = ".sim_sandbox";
    }
    if (nvs_mkdir(dir) != 0 && (errno != EEXIST || !esp_sim_nvs_path_is_directory(dir))) {
        return ESP_FAIL;
    }
    int final_len = snprintf(out_final, final_sz, "%s/nvs_storage.bin", dir);
    int tmp_len = snprintf(out_tmp, tmp_sz, "%s/nvs_storage.bin.tmp", dir);
    if (final_len < 0 || (size_t)final_len >= final_sz ||
        tmp_len < 0 || (size_t)tmp_len >= tmp_sz) {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

esp_err_t nvs_flash_init(void) {
    for (int i = 0; i < NVS_MAX_HANDLES; i++) {
        s_nvs_handles[i].in_use = false;
        s_nvs_handles[i].token = 0;
        s_nvs_handles[i].ns[0] = '\0';
    }
    memset(s_nvs_storage, 0, sizeof(s_nvs_storage));

    char final_path[256], tmp_path[256];
    esp_err_t path_err = esp_sim_nvs_get_sandbox_paths(final_path, sizeof(final_path), tmp_path, sizeof(tmp_path));
    if (path_err != ESP_OK) {
        return path_err;
    }
    FILE *f = fopen(final_path, "rb");
    if (!f && errno != ENOENT) {
        return ESP_FAIL;
    }
    if (f) {
        esp_sim_nvs_header_t hdr;
        if (fread(&hdr, sizeof(hdr), 1, f) == 1) {
            if (hdr.magic == ESP_SIM_NVS_MAGIC && (hdr.version == 1 || hdr.version == ESP_SIM_NVS_VERSION) && hdr.entry_count <= NVS_MAX_ENTRIES) {
                uint32_t crc = 0xFFFFFFFFu;
                bool ok = true;
                if (hdr.version == 1) {
                    esp_sim_nvs_record_v1_t rec_v1;
                    for (uint16_t i = 0; i < hdr.entry_count; i++) {
                        if (fread(&rec_v1, sizeof(rec_v1), 1, f) != 1) {
                            ok = false;
                            break;
                        }
                        crc = esp_sim_nvs_crc32_update(crc, (const uint8_t *)&rec_v1, sizeof(rec_v1));
                        s_nvs_storage[i].valid = true;
                        strncpy(s_nvs_storage[i].ns, rec_v1.ns, NVS_KEY_LEN - 1);
                        s_nvs_storage[i].ns[NVS_KEY_LEN - 1] = '\0';
                        strncpy(s_nvs_storage[i].key, rec_v1.key, NVS_KEY_LEN - 1);
                        s_nvs_storage[i].key[NVS_KEY_LEN - 1] = '\0';
                        s_nvs_storage[i].len = rec_v1.len;
                        s_nvs_storage[i].type = NVS_TYPE_BLOB;
                        size_t copy_len = rec_v1.len > NVS_VAL_BUF_SIZE ? NVS_VAL_BUF_SIZE : rec_v1.len;
                        memcpy(s_nvs_storage[i].data, rec_v1.data, copy_len);
                    }
                } else {
                    esp_sim_nvs_record_t rec;
                    for (uint16_t i = 0; i < hdr.entry_count; i++) {
                        if (fread(&rec, sizeof(rec), 1, f) != 1) {
                            ok = false;
                            break;
                        }
                        crc = esp_sim_nvs_crc32_update(crc, (const uint8_t *)&rec, sizeof(rec));
                        s_nvs_storage[i].valid = true;
                        strncpy(s_nvs_storage[i].ns, rec.ns, NVS_KEY_LEN - 1);
                        s_nvs_storage[i].ns[NVS_KEY_LEN - 1] = '\0';
                        strncpy(s_nvs_storage[i].key, rec.key, NVS_KEY_LEN - 1);
                        s_nvs_storage[i].key[NVS_KEY_LEN - 1] = '\0';
                        s_nvs_storage[i].len = rec.len;
                        s_nvs_storage[i].type = (nvs_type_t)rec.type;
                        size_t copy_len = rec.len > NVS_VAL_BUF_SIZE ? NVS_VAL_BUF_SIZE : rec.len;
                        memcpy(s_nvs_storage[i].data, rec.data, copy_len);
                    }
                }
                if (!ok || (~crc != hdr.crc32)) {
                    memset(s_nvs_storage, 0, sizeof(s_nvs_storage));
                }
            }
        }
        fclose(f);
    }

    return ESP_OK;
}

esp_err_t nvs_flash_deinit(void) {
    for (int i = 0; i < NVS_MAX_HANDLES; i++) {
        s_nvs_handles[i].in_use = false;
        s_nvs_handles[i].token = 0;
        s_nvs_handles[i].ns[0] = '\0';
    }
    /* Discard uncommitted RAM modifications; committed flash remains in sandbox storage */
    memset(s_nvs_storage, 0, sizeof(s_nvs_storage));
    return ESP_OK;
}

void esp_sim_nvs_reset_memory(void) {
    memset(s_nvs_storage, 0, sizeof(s_nvs_storage));
    for (int i = 0; i < NVS_MAX_HANDLES; i++) {
        s_nvs_handles[i].in_use = false;
        s_nvs_handles[i].token = 0;
        s_nvs_handles[i].ns[0] = '\0';
    }
}

esp_err_t nvs_open(const char *name, nvs_open_mode_t open_mode, nvs_handle_t *out_handle) {
    (void)open_mode;
    if (!name || !out_handle) {
        return ESP_ERR_INVALID_ARG;
    }
    for (int i = 0; i < NVS_MAX_HANDLES; i++) {
        if (!s_nvs_handles[i].in_use) {
            uint32_t token = esp_sim_handle_issue(ESP_SIM_HANDLE_NVS, (uint32_t)i);
            if (token == 0) return ESP_ERR_NVS_NOT_ENOUGH_SPACE;
            s_nvs_handles[i].in_use = true;
            s_nvs_handles[i].token = token;
            strncpy(s_nvs_handles[i].ns, name, NVS_KEY_LEN - 1);
            s_nvs_handles[i].ns[NVS_KEY_LEN - 1] = '\0';
            *out_handle = (nvs_handle_t)token;
            return ESP_OK;
        }
    }
    return ESP_ERR_NVS_NOT_ENOUGH_SPACE;
}

esp_err_t nvs_open_from_partition(const char *part_name, const char *namespace_name, nvs_open_mode_t open_mode, nvs_handle_t *out_handle) {
    (void)part_name;
    return nvs_open(namespace_name, open_mode, out_handle);
}

void nvs_close(nvs_handle_t handle) {
    esp_nvs_handle_t *record = resolve_nvs_handle(handle);
    if (record) record->in_use = false;
}

esp_err_t nvs_flash_erase(void) {
    memset(s_nvs_storage, 0, sizeof(s_nvs_storage));
    char final_path[256], tmp_path[256];
    esp_err_t path_err = esp_sim_nvs_get_sandbox_paths(final_path, sizeof(final_path), tmp_path, sizeof(tmp_path));
    if (path_err != ESP_OK) {
        return path_err;
    }
    if ((nvs_unlink(final_path) != 0 && errno != ENOENT) ||
        (nvs_unlink(tmp_path) != 0 && errno != ENOENT)) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t nvs_flash_erase_partition(const char *part_name) {
    (void)part_name;
    return nvs_flash_erase();
}

esp_err_t nvs_erase_key(nvs_handle_t handle, const char *key) {
    esp_nvs_handle_t *record = resolve_nvs_handle(handle);
    if (!record || !key) {
        return ESP_ERR_INVALID_ARG;
    }
    const char *ns = record->ns;
    for (int i = 0; i < NVS_MAX_ENTRIES; i++) {
        if (s_nvs_storage[i].valid && strcmp(s_nvs_storage[i].ns, ns) == 0 && strcmp(s_nvs_storage[i].key, key) == 0) {
            s_nvs_storage[i].valid = false;
            memset(s_nvs_storage[i].data, 0, sizeof(s_nvs_storage[i].data));
            s_nvs_storage[i].len = 0;
            return ESP_OK;
        }
    }
    return ESP_ERR_NVS_NOT_FOUND;
}

esp_err_t nvs_erase_all(nvs_handle_t handle) {
    esp_nvs_handle_t *record = resolve_nvs_handle(handle);
    if (!record) {
        return ESP_ERR_INVALID_ARG;
    }
    const char *ns = record->ns;
    for (int i = 0; i < NVS_MAX_ENTRIES; i++) {
        if (s_nvs_storage[i].valid && strcmp(s_nvs_storage[i].ns, ns) == 0) {
            s_nvs_storage[i].valid = false;
            memset(s_nvs_storage[i].data, 0, sizeof(s_nvs_storage[i].data));
            s_nvs_storage[i].len = 0;
        }
    }
    return ESP_OK;
}

esp_err_t nvs_get_used_entry_count(nvs_handle_t handle, size_t *used_entries) {
    esp_nvs_handle_t *record = resolve_nvs_handle(handle);
    if (!record || !used_entries) {
        return ESP_ERR_INVALID_ARG;
    }
    const char *ns = record->ns;
    size_t count = 0;
    for (int i = 0; i < NVS_MAX_ENTRIES; i++) {
        if (s_nvs_storage[i].valid && strcmp(s_nvs_storage[i].ns, ns) == 0) {
            count++;
        }
    }
    *used_entries = count;
    return ESP_OK;
}

static esp_err_t nvs_set_typed_blob(nvs_handle_t handle, const char *key, const void *value, size_t length, nvs_type_t type) {
    if (sim_esp_fault_is_active(ESP_FAULT_DOMAIN_NVS, ESP_FAULT_NVS_PARTITION_FULL)) {
        return ESP_ERR_NVS_NOT_ENOUGH_SPACE;
    }
    esp_nvs_handle_t *record = resolve_nvs_handle(handle);
    if (!record || !key || !value || length > NVS_VAL_BUF_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }
    const char *ns = record->ns;
    for (int i = 0; i < NVS_MAX_ENTRIES; i++) {
        if (s_nvs_storage[i].valid && strcmp(s_nvs_storage[i].ns, ns) == 0 && strcmp(s_nvs_storage[i].key, key) == 0) {
            memcpy(s_nvs_storage[i].data, value, length);
            s_nvs_storage[i].len = length;
            s_nvs_storage[i].type = type;
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
            s_nvs_storage[i].type = type;
            return ESP_OK;
        }
    }
    return ESP_ERR_NVS_NOT_ENOUGH_SPACE;
}

esp_err_t nvs_set_blob(nvs_handle_t handle, const char *key, const void *value, size_t length) {
    return nvs_set_typed_blob(handle, key, value, length, NVS_TYPE_BLOB);
}

esp_err_t nvs_get_blob(nvs_handle_t handle, const char *key, void *out_value, size_t *length) {
    if (sim_esp_fault_is_active(ESP_FAULT_DOMAIN_NVS, ESP_FAULT_NVS_READ_CORRUPT)) {
        return ESP_ERR_NVS_CORRUPT_KEY_PART;
    }
    esp_nvs_handle_t *record = resolve_nvs_handle(handle);
    if (!record || !key || !length) {
        return ESP_ERR_INVALID_ARG;
    }
    const char *ns = record->ns;
    for (int i = 0; i < NVS_MAX_ENTRIES; i++) {
        if (s_nvs_storage[i].valid && strcmp(s_nvs_storage[i].ns, ns) == 0 && strcmp(s_nvs_storage[i].key, key) == 0) {
            if (out_value != NULL) {
                if (*length < s_nvs_storage[i].len) {
                    *length = s_nvs_storage[i].len;
                    return ESP_ERR_NVS_INVALID_LENGTH;
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
    return nvs_set_typed_blob(h, k, &v, sizeof(v), NVS_TYPE_U8);
}

esp_err_t nvs_get_u8(nvs_handle_t h, const char *k, uint8_t *v) {
    size_t l = sizeof(*v);
    return nvs_get_blob(h, k, v, &l);
}

esp_err_t nvs_set_i8(nvs_handle_t h, const char *k, int8_t v) {
    return nvs_set_typed_blob(h, k, &v, sizeof(v), NVS_TYPE_I8);
}

esp_err_t nvs_get_i8(nvs_handle_t h, const char *k, int8_t *v) {
    size_t l = sizeof(*v);
    return nvs_get_blob(h, k, v, &l);
}

esp_err_t nvs_set_u16(nvs_handle_t h, const char *k, uint16_t v) {
    return nvs_set_typed_blob(h, k, &v, sizeof(v), NVS_TYPE_U16);
}

esp_err_t nvs_get_u16(nvs_handle_t h, const char *k, uint16_t *v) {
    size_t l = sizeof(*v);
    return nvs_get_blob(h, k, v, &l);
}

esp_err_t nvs_set_i16(nvs_handle_t h, const char *k, int16_t v) {
    return nvs_set_typed_blob(h, k, &v, sizeof(v), NVS_TYPE_I16);
}

esp_err_t nvs_get_i16(nvs_handle_t h, const char *k, int16_t *v) {
    size_t l = sizeof(*v);
    return nvs_get_blob(h, k, v, &l);
}

esp_err_t nvs_set_u32(nvs_handle_t h, const char *k, uint32_t v) {
    return nvs_set_typed_blob(h, k, &v, sizeof(v), NVS_TYPE_U32);
}

esp_err_t nvs_get_u32(nvs_handle_t h, const char *k, uint32_t *v) {
    size_t l = sizeof(*v);
    return nvs_get_blob(h, k, v, &l);
}

esp_err_t nvs_set_i32(nvs_handle_t h, const char *k, int32_t v) {
    return nvs_set_typed_blob(h, k, &v, sizeof(v), NVS_TYPE_I32);
}

esp_err_t nvs_get_i32(nvs_handle_t h, const char *k, int32_t *v) {
    size_t l = sizeof(*v);
    return nvs_get_blob(h, k, v, &l);
}

esp_err_t nvs_set_u64(nvs_handle_t h, const char *k, uint64_t v) {
    return nvs_set_typed_blob(h, k, &v, sizeof(v), NVS_TYPE_U64);
}

esp_err_t nvs_get_u64(nvs_handle_t h, const char *k, uint64_t *v) {
    size_t l = sizeof(*v);
    return nvs_get_blob(h, k, v, &l);
}

esp_err_t nvs_set_i64(nvs_handle_t h, const char *k, int64_t v) {
    return nvs_set_typed_blob(h, k, &v, sizeof(v), NVS_TYPE_I64);
}

esp_err_t nvs_get_i64(nvs_handle_t h, const char *k, int64_t *v) {
    size_t l = sizeof(*v);
    return nvs_get_blob(h, k, v, &l);
}

esp_err_t nvs_set_str(nvs_handle_t h, const char *k, const char *v) {
    if (!v) {
        return ESP_ERR_INVALID_ARG;
    }
    return nvs_set_typed_blob(h, k, v, strlen(v) + 1, NVS_TYPE_STR);
}

esp_err_t nvs_get_str(nvs_handle_t h, const char *k, char *v, size_t *l) {
    return nvs_get_blob(h, k, v, l);
}

esp_err_t nvs_commit(nvs_handle_t h) {
    if (!resolve_nvs_handle(h)) return ESP_ERR_INVALID_ARG;

    uint16_t count = 0;
    uint32_t crc = 0xFFFFFFFFu;
    esp_sim_nvs_record_t rec;

    for (int i = 0; i < NVS_MAX_ENTRIES; i++) {
        if (s_nvs_storage[i].valid) {
            memset(&rec, 0, sizeof(rec));
            strncpy(rec.ns, s_nvs_storage[i].ns, NVS_KEY_LEN - 1);
            strncpy(rec.key, s_nvs_storage[i].key, NVS_KEY_LEN - 1);
            rec.len = (uint32_t)s_nvs_storage[i].len;
            rec.type = (uint32_t)s_nvs_storage[i].type;
            size_t copy_len = s_nvs_storage[i].len > NVS_VAL_BUF_SIZE ? NVS_VAL_BUF_SIZE : s_nvs_storage[i].len;
            memcpy(rec.data, s_nvs_storage[i].data, copy_len);
            crc = esp_sim_nvs_crc32_update(crc, (const uint8_t *)&rec, sizeof(rec));
            count++;
        }
    }

    esp_sim_nvs_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = ESP_SIM_NVS_MAGIC;
    hdr.version = ESP_SIM_NVS_VERSION;
    hdr.entry_count = count;
    hdr.crc32 = ~crc;

    char final_path[256], tmp_path[256];
    esp_err_t path_err = esp_sim_nvs_get_sandbox_paths(final_path, sizeof(final_path), tmp_path, sizeof(tmp_path));
    if (path_err != ESP_OK) {
        return path_err;
    }
    FILE *f = fopen(tmp_path, "wb");
    if (!f) {
        return ESP_FAIL;
    }
    if (fwrite(&hdr, sizeof(hdr), 1, f) != 1) {
        fclose(f);
        (void)nvs_unlink(tmp_path);
        return ESP_FAIL;
    }
    for (int i = 0; i < NVS_MAX_ENTRIES; i++) {
        if (s_nvs_storage[i].valid) {
            memset(&rec, 0, sizeof(rec));
            strncpy(rec.ns, s_nvs_storage[i].ns, NVS_KEY_LEN - 1);
            strncpy(rec.key, s_nvs_storage[i].key, NVS_KEY_LEN - 1);
            rec.len = (uint32_t)s_nvs_storage[i].len;
            rec.type = (uint32_t)s_nvs_storage[i].type;
            size_t copy_len = s_nvs_storage[i].len > NVS_VAL_BUF_SIZE ? NVS_VAL_BUF_SIZE : s_nvs_storage[i].len;
            memcpy(rec.data, s_nvs_storage[i].data, copy_len);
            if (fwrite(&rec, sizeof(rec), 1, f) != 1) {
                fclose(f);
                (void)nvs_unlink(tmp_path);
                return ESP_FAIL;
            }
        }
    }
    int flush_failed = fflush(f) != 0;
    int close_failed = fclose(f) != 0;
    if (flush_failed || close_failed) {
        (void)nvs_unlink(tmp_path);
        return ESP_FAIL;
    }

    /* Atomic overwrite on Windows requires removing destination before rename */
    (void)nvs_unlink(final_path);
    if (rename(tmp_path, final_path) != 0) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

struct nvs_opaque_iterator_t {
    char namespace_name[NVS_KEY_LEN];
    nvs_type_t type;
    size_t current_index;
};

static bool nvs_iterator_match(size_t index, const char *namespace_name, nvs_type_t type) {
    if (index >= NVS_MAX_ENTRIES || !s_nvs_storage[index].valid) {
        return false;
    }
    if (namespace_name != NULL && namespace_name[0] != '\0') {
        if (strcmp(s_nvs_storage[index].ns, namespace_name) != 0) {
            return false;
        }
    }
    if (type != NVS_TYPE_ANY) {
        if (s_nvs_storage[index].type != type) {
            return false;
        }
    }
    return true;
}

esp_err_t nvs_entry_find(const char *part_name,
                         const char *namespace_name,
                         nvs_type_t type,
                         nvs_iterator_t *output_iterator) {
    (void)part_name;
    if (!output_iterator) {
        return ESP_ERR_INVALID_ARG;
    }
    *output_iterator = NULL;

    for (size_t i = 0; i < NVS_MAX_ENTRIES; i++) {
        if (nvs_iterator_match(i, namespace_name, type)) {
            struct nvs_opaque_iterator_t *it = (struct nvs_opaque_iterator_t *)malloc(sizeof(struct nvs_opaque_iterator_t));
            if (!it) {
                return ESP_ERR_NO_MEM;
            }
            if (namespace_name) {
                strncpy(it->namespace_name, namespace_name, NVS_KEY_LEN - 1);
                it->namespace_name[NVS_KEY_LEN - 1] = '\0';
            } else {
                it->namespace_name[0] = '\0';
            }
            it->type = type;
            it->current_index = i;
            *output_iterator = it;
            return ESP_OK;
        }
    }
    return ESP_ERR_NVS_NOT_FOUND;
}

esp_err_t nvs_entry_find_in_handle(nvs_handle_t handle, nvs_type_t type, nvs_iterator_t *output_iterator) {
    esp_nvs_handle_t *record = resolve_nvs_handle(handle);
    if (!record || !output_iterator) {
        return ESP_ERR_INVALID_ARG;
    }
    return nvs_entry_find(NULL, record->ns, type, output_iterator);
}

esp_err_t nvs_entry_info(const nvs_iterator_t iterator, nvs_entry_info_t *out_info) {
    if (!iterator || !out_info) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t idx = iterator->current_index;
    if (idx >= NVS_MAX_ENTRIES || !s_nvs_storage[idx].valid) {
        return ESP_ERR_INVALID_STATE;
    }
    strncpy(out_info->namespace_name, s_nvs_storage[idx].ns, sizeof(out_info->namespace_name) - 1);
    out_info->namespace_name[sizeof(out_info->namespace_name) - 1] = '\0';
    strncpy(out_info->key, s_nvs_storage[idx].key, sizeof(out_info->key) - 1);
    out_info->key[sizeof(out_info->key) - 1] = '\0';
    out_info->type = s_nvs_storage[idx].type;
    return ESP_OK;
}

esp_err_t nvs_entry_next(nvs_iterator_t *iterator) {
    if (!iterator || !*iterator) {
        return ESP_ERR_INVALID_ARG;
    }
    struct nvs_opaque_iterator_t *it = *iterator;
    for (size_t i = it->current_index + 1; i < NVS_MAX_ENTRIES; i++) {
        if (nvs_iterator_match(i, it->namespace_name, it->type)) {
            it->current_index = i;
            return ESP_OK;
        }
    }
    free(it);
    *iterator = NULL;
    return ESP_ERR_NVS_NOT_FOUND;
}

void nvs_release_iterator(nvs_iterator_t iterator) {
    if (iterator) {
        free(iterator);
    }
}

esp_err_t nvs_find_key(nvs_handle_t handle, const char *key, nvs_type_t *out_type) {
    esp_nvs_handle_t *record = resolve_nvs_handle(handle);
    if (!record || !key || !out_type) {
        return ESP_ERR_INVALID_ARG;
    }
    const char *ns = record->ns;
    for (int i = 0; i < NVS_MAX_ENTRIES; i++) {
        if (s_nvs_storage[i].valid && strcmp(s_nvs_storage[i].ns, ns) == 0 && strcmp(s_nvs_storage[i].key, key) == 0) {
            *out_type = s_nvs_storage[i].type;
            return ESP_OK;
        }
    }
    return ESP_ERR_NVS_NOT_FOUND;
}
