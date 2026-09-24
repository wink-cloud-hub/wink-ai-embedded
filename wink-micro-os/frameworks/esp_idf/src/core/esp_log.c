/* SPDX-License-Identifier: LGPL-3.0-only */
#include "esp_log.h"
#include "pal_log.h"
#include "pal_osal.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static esp_log_level_t s_global_log_level = ESP_LOG_INFO;
static vprintf_like_t s_custom_vprintf = NULL;

void esp_log_level_set(const char *tag, esp_log_level_t level) {
    (void)tag;
    s_global_log_level = level;
}

esp_log_level_t esp_log_level_get(const char *tag) {
    (void)tag;
    return s_global_log_level;
}

uint32_t esp_log_timestamp(void) {
    return (uint32_t)pal_os_get_ms();
}

char *esp_log_system_timestamp(void) {
    static char buf[16];
    snprintf(buf, sizeof(buf), "%u", (unsigned)esp_log_timestamp());
    return buf;
}

uint32_t esp_log_early_timestamp(void) {
    return (uint32_t)pal_os_get_ms();
}

vprintf_like_t esp_log_set_vprintf(vprintf_like_t func) {
    vprintf_like_t old = s_custom_vprintf;
    s_custom_vprintf = func;
    return old;
}

void esp_log_writev(esp_log_level_t level, const char *tag, const char *format, va_list args) {
    if (level > s_global_log_level) {
        return;
    }
    if (s_custom_vprintf != NULL) {
        s_custom_vprintf(format, args);
        return;
    }
    pal_log_level_t pal_lvl = PAL_LOG_INFO;
    switch (level) {
        case ESP_LOG_ERROR:   pal_lvl = PAL_LOG_ERROR; break;
        case ESP_LOG_WARN:    pal_lvl = PAL_LOG_WARN;  break;
        case ESP_LOG_INFO:    pal_lvl = PAL_LOG_INFO;  break;
        case ESP_LOG_DEBUG:
        case ESP_LOG_VERBOSE: pal_lvl = PAL_LOG_DEBUG; break;
        default: break;
    }
    if (pal_log_in_isr()) {
        pal_log_isr_write(pal_lvl, tag ? tag : "LOG", format, args);
    } else {
        pal_log_vprintf(pal_lvl, tag ? tag : "LOG", format, args);
    }
}

void esp_log_write(esp_log_level_t level, const char *tag, const char *format, ...) {
    va_list args;
    va_start(args, format);
    esp_log_writev(level, tag, format, args);
    va_end(args);
}

void esp_log_buffer_hex_internal(const char *tag, const void *buffer, uint16_t buff_len, esp_log_level_t level) {
    if (!buffer || buff_len == 0 || level > s_global_log_level) {
        return;
    }
    const uint8_t *ptr = (const uint8_t *)buffer;
    char line[128];
    uint16_t pos = 0;
    for (uint16_t i = 0; i < buff_len; i++) {
        pos += (uint16_t)snprintf(line + pos, sizeof(line) - pos, "%02x ", ptr[i]);
        if ((i + 1) % 16 == 0 || (i + 1) == buff_len) {
            esp_log_write(level, tag, "%s\n", line);
            pos = 0;
        }
    }
}

void esp_log_buffer_char_internal(const char *tag, const void *buffer, uint16_t buff_len, esp_log_level_t level) {
    if (!buffer || buff_len == 0 || level > s_global_log_level) {
        return;
    }
    const char *ptr = (const char *)buffer;
    char line[128];
    uint16_t pos = 0;
    for (uint16_t i = 0; i < buff_len; i++) {
        char c = ptr[i];
        line[pos++] = (c >= 32 && c <= 126) ? c : '.';
        if (pos >= 64 || (i + 1) == buff_len) {
            line[pos] = '\0';
            esp_log_write(level, tag, "%s\n", line);
            pos = 0;
        }
    }
}

void esp_log_buffer_hexdump_internal(const char *tag, const void *buffer, uint16_t buff_len, esp_log_level_t level) {
    esp_log_buffer_hex_internal(tag, buffer, buff_len, level);
}
