/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef ESP_LOG_H_
#define ESP_LOG_H_

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include "pal_log.h"

#if defined(__has_include)
#  if __has_include("sdkconfig.h")
#    include "sdkconfig.h"
#  else
#    include "sdkconfig_base.h"
#  endif
#else
#  include "sdkconfig_base.h"
#endif

#include "esp_log_level.h"
#include "esp_log_color.h"
#include "esp_log_buffer.h"
#include "esp_log_timestamp.h"
#include "esp_log_write.h"
#include "esp_log_format.h"
#include "esp_log_args.h"
#include "esp_log_attr.h"
#include "esp_private/log_attr.h"

#ifdef __cplusplus
extern "C" {
#endif

void esp_log_level_set(const char *tag, esp_log_level_t level);
esp_log_level_t esp_log_level_get(const char *tag);

#define ESP_LOGE(tag, format, ...) pal_log_e(tag, format, ##__VA_ARGS__)
#define ESP_LOGW(tag, format, ...) pal_log_w(tag, format, ##__VA_ARGS__)
#define ESP_LOGI(tag, format, ...) pal_log_i(tag, format, ##__VA_ARGS__)
#define ESP_LOGD(tag, format, ...) pal_log_d(tag, format, ##__VA_ARGS__)
#define ESP_LOGV(tag, format, ...) pal_log_d(tag, format, ##__VA_ARGS__)

#ifndef LOG_FORMAT
#define LOG_FORMAT(letter, format)  LOG_COLOR_ ## letter #letter " (%u) %s: " format LOG_RESET_COLOR "\n"
#endif

#ifdef __cplusplus
}
#endif

#endif /* ESP_LOG_H_ */
