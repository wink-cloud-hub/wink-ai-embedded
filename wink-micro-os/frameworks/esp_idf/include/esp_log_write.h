/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef ESP_LOG_WRITE_H_
#define ESP_LOG_WRITE_H_

#include <stdarg.h>
#include "esp_log_level.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*vprintf_like_t)(const char *, va_list);

void esp_log_write(esp_log_level_t level, const char *tag, const char *format, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 3, 4)))
#endif
;

void esp_log_writev(esp_log_level_t level, const char *tag, const char *format, va_list args);

vprintf_like_t esp_log_set_vprintf(vprintf_like_t func);

#ifdef __cplusplus
}
#endif

#endif /* ESP_LOG_WRITE_H_ */
