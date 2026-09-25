/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_LOG_H
#define WINK_H_GUARD_ESP_LOG_H
#ifndef __WINK_HARVESTED_ESP_LOG_H__
#define __WINK_HARVESTED_ESP_LOG_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>

#include "esp_log_args.h"
#include "esp_log_attr.h"
#include "esp_log_buffer.h"
#include "esp_log_color.h"
#include "esp_log_config.h"
#include "esp_log_format.h"
#include "esp_log_level.h"
#include "esp_log_timestamp.h"
#include "esp_log_write.h"
#include "esp_rom_sys.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef ESP_DRAM_LOGD
#define ESP_DRAM_LOGD(tag, format, ...) do { ESP_DRAM_LOG_IMPL(tag, format, ESP_LOG_DEBUG, D, ##__VA_ARGS__); } while(0)
#endif
#ifndef ESP_DRAM_LOGE
#define ESP_DRAM_LOGE(tag, format, ...) do { ESP_DRAM_LOG_IMPL(tag, format, ESP_LOG_ERROR, E, ##__VA_ARGS__); } while(0)
#endif
#ifndef ESP_DRAM_LOGI
#define ESP_DRAM_LOGI(tag, format, ...) do { ESP_DRAM_LOG_IMPL(tag, format, ESP_LOG_INFO, I, ##__VA_ARGS__); } while(0)
#endif
#ifndef ESP_DRAM_LOGV
#define ESP_DRAM_LOGV(tag, format, ...) do { ESP_DRAM_LOG_IMPL(tag, format, ESP_LOG_VERBOSE, V, ##__VA_ARGS__); } while(0)
#endif
#ifndef ESP_DRAM_LOGW
#define ESP_DRAM_LOGW(tag, format, ...) do { ESP_DRAM_LOG_IMPL(tag, format, ESP_LOG_WARN, W, ##__VA_ARGS__); } while(0)
#endif
#ifndef ESP_DRAM_LOG_IMPL
#define ESP_DRAM_LOG_IMPL(tag, format, configs, log_tag_letter, ...) do {  if (ESP_LOG_VERSION == 1 || (ESP_LOG_VERSION == 2 && !ESP_LOG_API_CONSTRAINED_ENV_SAFE)) {  if (_ESP_LOG_EARLY_ENABLED(configs)) { esp_rom_printf(_ESP_LOG_DRAM_LOG_FORMAT(log_tag_letter, format), tag, ##__VA_ARGS__); }  } else {  if (ESP_LOG_ENABLED(configs)) { esp_log(ESP_LOG_CONFIG_INIT((configs) | ESP_LOG_CONFIGS_DEFAULT | ESP_LOG_CONFIG_CONSTRAINED_ENV | ESP_LOG_CONFIG_DIS_COLOR | ESP_LOG_CONFIG_DIS_TIMESTAMP), tag, ESP_LOG_ATTR_DRAM_STR(format) ESP_LOG_ARGS(__VA_ARGS__)); }  } } while(0)
#endif
#ifndef ESP_EARLY_LOGD
#define ESP_EARLY_LOGD(tag, format, ...) do { ESP_LOG_EARLY_IMPL(tag, format, ESP_LOG_DEBUG, D, ##__VA_ARGS__); } while(0)
#endif
#ifndef ESP_EARLY_LOGE
#define ESP_EARLY_LOGE(tag, format, ...) do { ESP_LOG_EARLY_IMPL(tag, format, ESP_LOG_ERROR, E, ##__VA_ARGS__); } while(0)
#endif
#ifndef ESP_EARLY_LOGI
#define ESP_EARLY_LOGI(tag, format, ...) do { ESP_LOG_EARLY_IMPL(tag, format, ESP_LOG_INFO, I, ##__VA_ARGS__); } while(0)
#endif
#ifndef ESP_EARLY_LOGV
#define ESP_EARLY_LOGV(tag, format, ...) do { ESP_LOG_EARLY_IMPL(tag, format, ESP_LOG_VERBOSE, V, ##__VA_ARGS__); } while(0)
#endif
#ifndef ESP_EARLY_LOGW
#define ESP_EARLY_LOGW(tag, format, ...) do { ESP_LOG_EARLY_IMPL(tag, format, ESP_LOG_WARN, W, ##__VA_ARGS__); } while(0)
#endif
#ifndef ESP_LOGD
#define ESP_LOGD(tag, format, ...) do { ESP_LOG_LEVEL_LOCAL(ESP_LOG_DEBUG, tag, format, ##__VA_ARGS__); } while(0)
#endif
#ifndef ESP_LOGE
#define ESP_LOGE(tag, format, ...) do { ESP_LOG_LEVEL_LOCAL(ESP_LOG_ERROR, tag, format, ##__VA_ARGS__); } while(0)
#endif
#ifndef ESP_LOGI
#define ESP_LOGI(tag, format, ...) do { ESP_LOG_LEVEL_LOCAL(ESP_LOG_INFO, tag, format, ##__VA_ARGS__); } while(0)
#endif
#ifndef ESP_LOGV
#define ESP_LOGV(tag, format, ...) do { ESP_LOG_LEVEL_LOCAL(ESP_LOG_VERBOSE, tag, format, ##__VA_ARGS__); } while(0)
#endif
#ifndef ESP_LOGW
#define ESP_LOGW(tag, format, ...) do { ESP_LOG_LEVEL_LOCAL(ESP_LOG_WARN, tag, format, ##__VA_ARGS__); } while(0)
#endif
#ifndef ESP_LOG_EARLY_IMPL
#define ESP_LOG_EARLY_IMPL(tag, format, configs, log_tag_letter, ...) do {  if (ESP_LOG_VERSION == 1 || (ESP_LOG_VERSION == 2 && !ESP_LOG_API_CONSTRAINED_ENV_SAFE)) {  if (_ESP_LOG_EARLY_ENABLED(configs)) { esp_rom_printf(LOG_FORMAT(log_tag_letter, format), esp_log_timestamp(), tag, ##__VA_ARGS__); }  } else {  if (ESP_LOG_ENABLED(configs)) { esp_log(ESP_LOG_CONFIG_INIT((configs) | ESP_LOG_CONFIGS_DEFAULT | ESP_LOG_CONFIG_CONSTRAINED_ENV), tag, ESP_LOG_ATTR_STR(format) ESP_LOG_ARGS(__VA_ARGS__)); }  } } while(0)
#endif
#ifndef ESP_LOG_LEVEL
#define ESP_LOG_LEVEL(configs, tag, format, ...) do {  if (ESP_LOG_GET_LEVEL(configs)==ESP_LOG_ERROR)        { esp_log(ESP_LOG_CONFIG_INIT(ESP_LOG_ERROR),   tag, LOG_FORMAT(E, format), esp_log_timestamp(), tag, ##__VA_ARGS__); }  else if (ESP_LOG_GET_LEVEL(configs)==ESP_LOG_WARN)    { esp_log(ESP_LOG_CONFIG_INIT(ESP_LOG_WARN),    tag, LOG_FORMAT(W, format), esp_log_timestamp(), tag, ##__VA_ARGS__); }  else if (ESP_LOG_GET_LEVEL(configs)==ESP_LOG_DEBUG)   { esp_log(ESP_LOG_CONFIG_INIT(ESP_LOG_DEBUG),   tag, LOG_FORMAT(D, format), esp_log_timestamp(), tag, ##__VA_ARGS__); }  else if (ESP_LOG_GET_LEVEL(configs)==ESP_LOG_VERBOSE) { esp_log(ESP_LOG_CONFIG_INIT(ESP_LOG_VERBOSE), tag, LOG_FORMAT(V, format), esp_log_timestamp(), tag, ##__VA_ARGS__); }  else                                                  { esp_log(ESP_LOG_CONFIG_INIT(ESP_LOG_INFO),    tag, LOG_FORMAT(I, format), esp_log_timestamp(), tag, ##__VA_ARGS__); }  } while(0)
#endif
#ifndef ESP_LOG_LEVEL_LOCAL
#define ESP_LOG_LEVEL_LOCAL(configs, tag, format, ...) do { if (_ESP_LOG_ENABLED(configs)) { ESP_LOG_LEVEL(configs, tag, format, ##__VA_ARGS__); } } while(0)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */

void esp_log(esp_log_config_t config, const char* tag, const char* format, ...);
void esp_log_va(esp_log_config_t config, const char *tag, const char *format, va_list args);


#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_LOG_H__ */
#endif /* WINK_H_GUARD_ESP_LOG_H */
