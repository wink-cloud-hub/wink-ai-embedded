/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_NVS_H
#define WINK_H_GUARD_NVS_H
#ifndef __WINK_HARVESTED_NVS_H__
#define __WINK_HARVESTED_NVS_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_attr.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef ESP_ERR_NVS_BASE
#define ESP_ERR_NVS_BASE 0x1100
#endif
#ifndef ESP_ERR_NVS_CONTENT_DIFFERS
#define ESP_ERR_NVS_CONTENT_DIFFERS (ESP_ERR_NVS_BASE + 0x18)
#endif
#ifndef ESP_ERR_NVS_CORRUPT_KEY_PART
#define ESP_ERR_NVS_CORRUPT_KEY_PART (ESP_ERR_NVS_BASE + 0x17)
#endif
#ifndef ESP_ERR_NVS_ENCR_NOT_SUPPORTED
#define ESP_ERR_NVS_ENCR_NOT_SUPPORTED (ESP_ERR_NVS_BASE + 0x15)
#endif
#ifndef ESP_ERR_NVS_INVALID_HANDLE
#define ESP_ERR_NVS_INVALID_HANDLE (ESP_ERR_NVS_BASE + 0x07)
#endif
#ifndef ESP_ERR_NVS_INVALID_LENGTH
#define ESP_ERR_NVS_INVALID_LENGTH (ESP_ERR_NVS_BASE + 0x0c)
#endif
#ifndef ESP_ERR_NVS_INVALID_NAME
#define ESP_ERR_NVS_INVALID_NAME (ESP_ERR_NVS_BASE + 0x06)
#endif
#ifndef ESP_ERR_NVS_INVALID_STATE
#define ESP_ERR_NVS_INVALID_STATE (ESP_ERR_NVS_BASE + 0x0b)
#endif
#ifndef ESP_ERR_NVS_KEYS_NOT_INITIALIZED
#define ESP_ERR_NVS_KEYS_NOT_INITIALIZED (ESP_ERR_NVS_BASE + 0x16)
#endif
#ifndef ESP_ERR_NVS_KEY_TOO_LONG
#define ESP_ERR_NVS_KEY_TOO_LONG (ESP_ERR_NVS_BASE + 0x09)
#endif
#ifndef ESP_ERR_NVS_NEW_VERSION_FOUND
#define ESP_ERR_NVS_NEW_VERSION_FOUND (ESP_ERR_NVS_BASE + 0x10)
#endif
#ifndef ESP_ERR_NVS_NOT_ENOUGH_SPACE
#define ESP_ERR_NVS_NOT_ENOUGH_SPACE (ESP_ERR_NVS_BASE + 0x05)
#endif
#ifndef ESP_ERR_NVS_NOT_FOUND
#define ESP_ERR_NVS_NOT_FOUND (ESP_ERR_NVS_BASE + 0x02)
#endif
#ifndef ESP_ERR_NVS_NOT_INITIALIZED
#define ESP_ERR_NVS_NOT_INITIALIZED (ESP_ERR_NVS_BASE + 0x01)
#endif
#ifndef ESP_ERR_NVS_NO_FREE_PAGES
#define ESP_ERR_NVS_NO_FREE_PAGES (ESP_ERR_NVS_BASE + 0x0d)
#endif
#ifndef ESP_ERR_NVS_PAGE_FULL
#define ESP_ERR_NVS_PAGE_FULL (ESP_ERR_NVS_BASE + 0x0a)
#endif
#ifndef ESP_ERR_NVS_PART_NOT_FOUND
#define ESP_ERR_NVS_PART_NOT_FOUND (ESP_ERR_NVS_BASE + 0x0f)
#endif
#ifndef ESP_ERR_NVS_READ_ONLY
#define ESP_ERR_NVS_READ_ONLY (ESP_ERR_NVS_BASE + 0x04)
#endif
#ifndef ESP_ERR_NVS_REMOVE_FAILED
#define ESP_ERR_NVS_REMOVE_FAILED (ESP_ERR_NVS_BASE + 0x08)
#endif
#ifndef ESP_ERR_NVS_TYPE_MISMATCH
#define ESP_ERR_NVS_TYPE_MISMATCH (ESP_ERR_NVS_BASE + 0x03)
#endif
#ifndef ESP_ERR_NVS_VALUE_TOO_LONG
#define ESP_ERR_NVS_VALUE_TOO_LONG (ESP_ERR_NVS_BASE + 0x0e)
#endif
#ifndef ESP_ERR_NVS_WRONG_ENCRYPTION
#define ESP_ERR_NVS_WRONG_ENCRYPTION (ESP_ERR_NVS_BASE + 0x19)
#endif
#ifndef ESP_ERR_NVS_XTS_CFG_FAILED
#define ESP_ERR_NVS_XTS_CFG_FAILED (ESP_ERR_NVS_BASE + 0x13)
#endif
#ifndef ESP_ERR_NVS_XTS_CFG_NOT_FOUND
#define ESP_ERR_NVS_XTS_CFG_NOT_FOUND (ESP_ERR_NVS_BASE + 0x14)
#endif
#ifndef ESP_ERR_NVS_XTS_DECR_FAILED
#define ESP_ERR_NVS_XTS_DECR_FAILED (ESP_ERR_NVS_BASE + 0x12)
#endif
#ifndef ESP_ERR_NVS_XTS_ENCR_FAILED
#define ESP_ERR_NVS_XTS_ENCR_FAILED (ESP_ERR_NVS_BASE + 0x11)
#endif
#ifndef NVS_DEFAULT_PART_NAME
#define NVS_DEFAULT_PART_NAME "nvs"
#endif
#ifndef NVS_GUARD_SYSVIEW_MACRO_EXPANSION_POP
#define NVS_GUARD_SYSVIEW_MACRO_EXPANSION_POP() _Pragma("pop_macro(\"U8\")")  _Pragma("pop_macro(\"I8\")")  _Pragma("pop_macro(\"U16\")")  _Pragma("pop_macro(\"I16\")")  _Pragma("pop_macro(\"U32\")")  _Pragma("pop_macro(\"I32\")")  _Pragma("pop_macro(\"U64\")")  _Pragma("pop_macro(\"I64\")")
#endif
#ifndef NVS_GUARD_SYSVIEW_MACRO_EXPANSION_PUSH
#define NVS_GUARD_SYSVIEW_MACRO_EXPANSION_PUSH() _Pragma("push_macro(\"U8\")")  _Pragma("push_macro(\"I8\")")  _Pragma("push_macro(\"U16\")")  _Pragma("push_macro(\"I16\")")  _Pragma("push_macro(\"U32\")")  _Pragma("push_macro(\"I32\")")  _Pragma("push_macro(\"U64\")")  _Pragma("push_macro(\"I64\")")
#endif
#ifndef NVS_KEY_NAME_MAX_SIZE
#define NVS_KEY_NAME_MAX_SIZE 16
#endif
#ifndef NVS_NS_NAME_MAX_SIZE
#define NVS_NS_NAME_MAX_SIZE NVS_KEY_NAME_MAX_SIZE
#endif
#ifndef NVS_PART_NAME_MAX_SIZE
#define NVS_PART_NAME_MAX_SIZE 16
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    NVS_READONLY = 0,
    NVS_READWRITE = 1,
    NVS_READWRITE_PURGE = 2,
} nvs_open_mode_t;
typedef enum {
    NVS_TYPE_U8 = 1,
    NVS_TYPE_I8 = 17,
    NVS_TYPE_U16 = 2,
    NVS_TYPE_I16 = 18,
    NVS_TYPE_U32 = 4,
    NVS_TYPE_I32 = 20,
    NVS_TYPE_U64 = 8,
    NVS_TYPE_I64 = 24,
    NVS_TYPE_FLOAT = 36,
    NVS_TYPE_DOUBLE = 40,
    NVS_TYPE_STR = 33,
    NVS_TYPE_BLOB = 66,
    NVS_TYPE_ANY = 255,
} nvs_type_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef uint32_t nvs_handle_t;
typedef struct {
    char namespace_name[NVS_NS_NAME_MAX_SIZE];
    char key[NVS_KEY_NAME_MAX_SIZE];
    nvs_type_t type;
} nvs_entry_info_t;
typedef struct nvs_opaque_iterator_t * nvs_iterator_t;
typedef struct {
    size_t used_entries;
    size_t free_entries;
    size_t available_entries;
    size_t total_entries;
    size_t namespace_count;
} nvs_stats_t;

void nvs_close(nvs_handle_t handle);
esp_err_t nvs_commit(nvs_handle_t handle);
esp_err_t nvs_entry_find(const char *part_name,
        const char *namespace_name,
        nvs_type_t type,
        nvs_iterator_t *output_iterator);
esp_err_t nvs_entry_find_in_handle(nvs_handle_t handle, nvs_type_t type, nvs_iterator_t *output_iterator);
esp_err_t nvs_entry_info(const nvs_iterator_t iterator, nvs_entry_info_t *out_info);
esp_err_t nvs_entry_next(nvs_iterator_t *iterator);
esp_err_t nvs_erase_all(nvs_handle_t handle);
esp_err_t nvs_erase_key(nvs_handle_t handle, const char* key);
esp_err_t nvs_find_key(nvs_handle_t handle, const char* key, nvs_type_t* out_type);
esp_err_t nvs_get_blob(nvs_handle_t handle, const char* key, void* out_value, size_t* length);
esp_err_t nvs_get_double(nvs_handle_t handle, const char* key, double* out_value);
esp_err_t nvs_get_float(nvs_handle_t handle, const char* key, float* out_value);
esp_err_t nvs_get_i16(nvs_handle_t handle, const char* key, int16_t* out_value);
esp_err_t nvs_get_i32(nvs_handle_t handle, const char* key, int32_t* out_value);
esp_err_t nvs_get_i64(nvs_handle_t handle, const char* key, int64_t* out_value);
esp_err_t nvs_get_i8(nvs_handle_t handle, const char* key, int8_t* out_value);
esp_err_t nvs_get_stats(const char *part_name, nvs_stats_t *nvs_stats);
esp_err_t nvs_get_str(nvs_handle_t handle, const char* key, char* out_value, size_t* length);
esp_err_t nvs_get_u16(nvs_handle_t handle, const char* key, uint16_t* out_value);
esp_err_t nvs_get_u32(nvs_handle_t handle, const char* key, uint32_t* out_value);
esp_err_t nvs_get_u64(nvs_handle_t handle, const char* key, uint64_t* out_value);
esp_err_t nvs_get_u8(nvs_handle_t handle, const char* key, uint8_t* out_value);
esp_err_t nvs_get_used_entry_count(nvs_handle_t handle, size_t* used_entries);
esp_err_t nvs_open(const char* namespace_name, nvs_open_mode_t open_mode, nvs_handle_t *out_handle);
esp_err_t nvs_open_from_partition(const char *part_name, const char* namespace_name, nvs_open_mode_t open_mode, nvs_handle_t *out_handle);
esp_err_t nvs_purge_all(nvs_handle_t handle);
void nvs_release_iterator(nvs_iterator_t iterator);
esp_err_t nvs_set_blob(nvs_handle_t handle, const char* key, const void* value, size_t length);
esp_err_t nvs_set_double(nvs_handle_t handle, const char* key, double value);
esp_err_t nvs_set_float(nvs_handle_t handle, const char* key, float value);
esp_err_t nvs_set_i16(nvs_handle_t handle, const char* key, int16_t value);
esp_err_t nvs_set_i32(nvs_handle_t handle, const char* key, int32_t value);
esp_err_t nvs_set_i64(nvs_handle_t handle, const char* key, int64_t value);
esp_err_t nvs_set_i8(nvs_handle_t handle, const char* key, int8_t value);
esp_err_t nvs_set_str(nvs_handle_t handle, const char* key, const char* value);
esp_err_t nvs_set_u16(nvs_handle_t handle, const char* key, uint16_t value);
esp_err_t nvs_set_u32(nvs_handle_t handle, const char* key, uint32_t value);
esp_err_t nvs_set_u64(nvs_handle_t handle, const char* key, uint64_t value);
esp_err_t nvs_set_u8(nvs_handle_t handle, const char* key, uint8_t value);


#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_NVS_H__ */
#endif /* WINK_H_GUARD_NVS_H */
