/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_WAKE_STUB_H
#define WINK_H_GUARD_ESP_WAKE_STUB_H
#ifndef __WINK_HARVESTED_ESP_WAKE_STUB_H__
#define __WINK_HARVESTED_ESP_WAKE_STUB_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef ESP_RTC_LOG
#define ESP_RTC_LOG( level, format, ... ) if (LOG_LOCAL_LEVEL >= level) { esp_rom_printf(RTC_STR(format), ##__VA_ARGS__);  esp_wake_stub_uart_tx_wait_idle(0); }
#endif
#ifndef ESP_RTC_LOGD
#define ESP_RTC_LOGD( format, ... ) ESP_RTC_LOG(ESP_LOG_DEBUG, RTC_LOG_FORMAT(D, format), ##__VA_ARGS__)
#endif
#ifndef ESP_RTC_LOGE
#define ESP_RTC_LOGE( format, ... ) ESP_RTC_LOG(ESP_LOG_ERROR, RTC_LOG_FORMAT(E, format), ##__VA_ARGS__)
#endif
#ifndef ESP_RTC_LOGI
#define ESP_RTC_LOGI( format, ... ) ESP_RTC_LOG(ESP_LOG_INFO, RTC_LOG_FORMAT(I, format), ##__VA_ARGS__)
#endif
#ifndef ESP_RTC_LOGV
#define ESP_RTC_LOGV( format, ... ) ESP_RTC_LOG(ESP_LOG_VERBOSE, RTC_LOG_FORMAT(V, format), ##__VA_ARGS__)
#endif
#ifndef ESP_RTC_LOGW
#define ESP_RTC_LOGW( format, ... ) ESP_RTC_LOG(ESP_LOG_WARN, RTC_LOG_FORMAT(W, format), ##__VA_ARGS__)
#endif
#ifndef RTC_LOG_FORMAT
#define RTC_LOG_FORMAT(letter, format) LOG_COLOR_ ## letter format LOG_RESET_COLOR "\n"
#endif
#ifndef RTC_STR
#define RTC_STR(str) (__extension__({static const RTC_RODATA_ATTR char _fmt[] = (str); (const char *)&_fmt;}))
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */

void esp_wake_stub_uart_tx_wait_idle(uint8_t uart_no);


#if defined(__WINK_SIM__)
uint32_t esp_wake_stub_get_wakeup_cause(void) WINK_SLA_ERROR("Wink SLA Violation: esp_wake_stub_get_wakeup_cause out of Core 8 scope.");
#else
uint32_t esp_wake_stub_get_wakeup_cause(void);
#endif

#if defined(__WINK_SIM__)
void esp_wake_stub_set_wakeup_time(uint64_t time_in_us) WINK_SLA_ERROR("Wink SLA Violation: esp_wake_stub_set_wakeup_time out of Core 8 scope.");
#else
void esp_wake_stub_set_wakeup_time(uint64_t time_in_us);
#endif

#if defined(__WINK_SIM__)
void esp_wake_stub_sleep(esp_deep_sleep_wake_stub_fn_t new_stub) WINK_SLA_ERROR("Wink SLA Violation: esp_wake_stub_sleep out of Core 8 scope.");
#else
void esp_wake_stub_sleep(esp_deep_sleep_wake_stub_fn_t new_stub);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_WAKE_STUB_H__ */
#endif /* WINK_H_GUARD_ESP_WAKE_STUB_H */
