/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef ESP_VFS_COMMON_H
#define ESP_VFS_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Line ending settings
 */
typedef enum {
    ESP_LINE_ENDINGS_CRLF,
    ESP_LINE_ENDINGS_CR,
    ESP_LINE_ENDINGS_LF,
} esp_line_endings_t;

#ifdef __cplusplus
}
#endif

#endif /* ESP_VFS_COMMON_H */
