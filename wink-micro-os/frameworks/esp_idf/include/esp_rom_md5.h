/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_ROM_MD5_H
#define WINK_H_GUARD_ESP_ROM_MD5_H
#ifndef __WINK_HARVESTED_ESP_ROM_MD5_H__
#define __WINK_HARVESTED_ESP_ROM_MD5_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdint.h>

#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef ESP_ROM_MD5_DIGEST_LEN
#define ESP_ROM_MD5_DIGEST_LEN 16
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
    uint32_t buf[4];
    uint32_t bits[2];
    uint8_t in[64];
} md5_context_t;



#if defined(__WINK_SIM__)
void esp_rom_md5_final(uint8_t *digest, md5_context_t *context) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_md5_final out of Core 8 scope.");
#else
void esp_rom_md5_final(uint8_t *digest, md5_context_t *context);
#endif

#if defined(__WINK_SIM__)
void esp_rom_md5_init(md5_context_t *context) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_md5_init out of Core 8 scope.");
#else
void esp_rom_md5_init(md5_context_t *context);
#endif

#if defined(__WINK_SIM__)
void esp_rom_md5_update(md5_context_t *context, const void *buf, uint32_t len) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_md5_update out of Core 8 scope.");
#else
void esp_rom_md5_update(md5_context_t *context, const void *buf, uint32_t len);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_ROM_MD5_H__ */
#endif /* WINK_H_GUARD_ESP_ROM_MD5_H */
