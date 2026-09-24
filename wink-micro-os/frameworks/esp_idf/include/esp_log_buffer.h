/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef ESP_LOG_BUFFER_H_
#define ESP_LOG_BUFFER_H_

#include <stddef.h>
#include "esp_log_level.h"

#ifdef __cplusplus
extern "C" {
#endif

void esp_log_buffer_hex_internal(const char *tag, const void *buffer, uint16_t buff_len, esp_log_level_t level);
void esp_log_buffer_char_internal(const char *tag, const void *buffer, uint16_t buff_len, esp_log_level_t level);
void esp_log_buffer_hexdump_internal(const char *tag, const void *buffer, uint16_t buff_len, esp_log_level_t level);

#define ESP_LOG_BUFFER_HEX_LEVEL(tag, buffer, buff_len, level) \
    esp_log_buffer_hex_internal(tag, buffer, buff_len, level)

#define ESP_LOG_BUFFER_CHAR_LEVEL(tag, buffer, buff_len, level) \
    esp_log_buffer_char_internal(tag, buffer, buff_len, level)

#define ESP_LOG_BUFFER_HEXDUMP(tag, buffer, buff_len, level) \
    esp_log_buffer_hexdump_internal(tag, buffer, buff_len, level)

#define ESP_LOG_BUFFER_HEX(tag, buffer, buff_len) \
    ESP_LOG_BUFFER_HEX_LEVEL(tag, buffer, buff_len, ESP_LOG_INFO)

#define ESP_LOG_BUFFER_CHAR(tag, buffer, buff_len) \
    ESP_LOG_BUFFER_CHAR_LEVEL(tag, buffer, buff_len, ESP_LOG_INFO)

#ifdef __cplusplus
}
#endif

#endif /* ESP_LOG_BUFFER_H_ */
