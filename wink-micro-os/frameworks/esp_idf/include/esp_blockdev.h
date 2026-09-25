/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_BLOCKDEV_H
#define WINK_H_GUARD_ESP_BLOCKDEV_H
#ifndef __WINK_HARVESTED_ESP_BLOCKDEV_H__
#define __WINK_HARVESTED_ESP_BLOCKDEV_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef ESP_BLOCKDEV_CMD_ERASE_CONTENTS
#define ESP_BLOCKDEV_CMD_ERASE_CONTENTS (ESP_BLOCKDEV_CMD_SYSTEM_BASE + 1)
#endif
#ifndef ESP_BLOCKDEV_CMD_MARK_DELETED
#define ESP_BLOCKDEV_CMD_MARK_DELETED (ESP_BLOCKDEV_CMD_SYSTEM_BASE + 0)
#endif
#ifndef ESP_BLOCKDEV_CMD_SYSTEM_BASE
#define ESP_BLOCKDEV_CMD_SYSTEM_BASE 0x00
#endif
#ifndef ESP_BLOCKDEV_CMD_USER_BASE
#define ESP_BLOCKDEV_CMD_USER_BASE 0x80
#endif
#ifndef ESP_BLOCKDEV_FLAGS_CONFIG_DEFAULT
#define ESP_BLOCKDEV_FLAGS_CONFIG_DEFAULT() {   {   .read_only   = 0,   .encrypted   = 0,   .erase_before_write = 1,   .and_type_write = 1,   .default_val_after_erase = 1,   .reserved = 0                       }  }
#endif
#ifndef ESP_BLOCKDEV_FLAGS_INST_CONFIG_DEFAULT
#define ESP_BLOCKDEV_FLAGS_INST_CONFIG_DEFAULT(flags) {   flags.read_only   = 0;   flags.encrypted   = 0;   flags.erase_before_write = 1;   flags.and_type_write = 1;   flags.default_val_after_erase = 1;   flags.reserved = 0;   }
#endif
#ifndef ESP_BLOCKDEV_HANDLE_INVALID
#define ESP_BLOCKDEV_HANDLE_INVALID NULL
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
    uint64_t start_addr;
    size_t erase_len;
} esp_blockdev_cmd_arg_erase_t;
typedef union esp_blockdev_flags {
struct {
        uint32_t read_only: 1;                      
        uint32_t encrypted: 1;                      
        uint32_t erase_before_write: 1;             
        uint32_t and_type_write: 1;                 
        uint32_t default_val_after_erase: 1;        
        uint32_t reserved: 27;                      
    };
    uint32_t val;
} esp_blockdev_flags_t;
typedef struct {
    uint64_t disk_size;
    size_t read_size;
    size_t write_size;
    size_t erase_size;
    size_t recommended_write_size;
    size_t recommended_read_size;
    size_t recommended_erase_size;
} esp_blockdev_geometry_t;
typedef struct esp_blockdev esp_blockdev_t;
typedef esp_blockdev_t * esp_blockdev_handle_t;
typedef struct {
    esp_err_t (*read)(esp_blockdev_handle_t dev_handle, uint8_t* dst_buf, size_t dst_buf_size, uint64_t src_addr, size_t data_read_len);
    esp_err_t (*write)(esp_blockdev_handle_t dev_handle, const uint8_t* src_buf, uint64_t dst_addr, size_t data_write_len);
    esp_err_t (*erase)(esp_blockdev_handle_t dev_handle, uint64_t start_addr, size_t erase_len);
    esp_err_t (*sync)(esp_blockdev_handle_t dev_handle);
    esp_err_t (*ioctl)(esp_blockdev_handle_t dev_handle, const uint8_t cmd, void* args);
    esp_err_t (*release)(esp_blockdev_handle_t dev_handle);
} esp_blockdev_ops_t;
struct esp_blockdev {
    void* ctx;
    esp_blockdev_flags_t device_flags;
    esp_blockdev_geometry_t geometry;
    const esp_blockdev_ops_t* ops;
};



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_BLOCKDEV_H__ */
#endif /* WINK_H_GUARD_ESP_BLOCKDEV_H */
