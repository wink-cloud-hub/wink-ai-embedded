/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_LOG_BUFFER_H
#define WINK_H_GUARD_ESP_LOG_BUFFER_H
#ifndef __WINK_HARVESTED_ESP_LOG_BUFFER_H__
#define __WINK_HARVESTED_ESP_LOG_BUFFER_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdint.h>

#include "esp_log_level.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef ESP_LOG_BUFFER_CHAR
#define ESP_LOG_BUFFER_CHAR(tag, buffer, buff_len) do { if (ESP_LOG_ENABLED(ESP_LOG_INFO)) {ESP_LOG_BUFFER_CHAR_LEVEL(tag, buffer, buff_len, ESP_LOG_INFO);} } while(0)
#endif
#ifndef ESP_LOG_BUFFER_CHAR_LEVEL
#define ESP_LOG_BUFFER_CHAR_LEVEL(tag, buffer, buff_len, level) do { if (ESP_LOG_ENABLED(level)) {esp_log_buffer_char_internal(tag, buffer, buff_len, level);} } while(0)
#endif
#ifndef ESP_LOG_BUFFER_HEX
#define ESP_LOG_BUFFER_HEX(tag, buffer, buff_len) do { if (ESP_LOG_ENABLED(ESP_LOG_INFO)) {ESP_LOG_BUFFER_HEX_LEVEL(tag, buffer, buff_len, ESP_LOG_INFO);} } while(0)
#endif
#ifndef ESP_LOG_BUFFER_HEXDUMP
#define ESP_LOG_BUFFER_HEXDUMP(tag, buffer, buff_len, level) do { if (ESP_LOG_ENABLED(level)) {esp_log_buffer_hexdump_internal(tag, buffer, buff_len, level);} } while(0)
#endif
#ifndef ESP_LOG_BUFFER_HEX_LEVEL
#define ESP_LOG_BUFFER_HEX_LEVEL(tag, buffer, buff_len, level) do { if (ESP_LOG_ENABLED(level)) {esp_log_buffer_hex_internal(tag, buffer, buff_len, level);} } while(0)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */

void esp_log_buffer_char_internal(const char *tag, const void *buffer, uint16_t buff_len, esp_log_level_t level);
void esp_log_buffer_hex_internal(const char *tag, const void *buffer, uint16_t buff_len, esp_log_level_t level);
void esp_log_buffer_hexdump_internal(const char *tag, const void *buffer, uint16_t buff_len, esp_log_level_t log_level);


#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_LOG_BUFFER_H__ */
#endif /* WINK_H_GUARD_ESP_LOG_BUFFER_H */
