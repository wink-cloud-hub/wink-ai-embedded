/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_XT_WDT_H
#define WINK_H_GUARD_ESP_XT_WDT_H
#ifndef __WINK_HARVESTED_ESP_XT_WDT_H__
#define __WINK_HARVESTED_ESP_XT_WDT_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdint.h>

#include "esp_err.h"
#include "esp_intr_alloc.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
    uint8_t timeout;
    bool auto_backup_clk_enable;
} esp_xt_wdt_config_t;
typedef void (*esp_xt_callback_t)(void *arg);



#if defined(__WINK_SIM__)
esp_err_t esp_xt_wdt_init(const esp_xt_wdt_config_t *cfg) WINK_SLA_ERROR("Wink SLA Violation: esp_xt_wdt_init out of Core 8 scope.");
#else
esp_err_t esp_xt_wdt_init(const esp_xt_wdt_config_t *cfg);
#endif

#if defined(__WINK_SIM__)
void esp_xt_wdt_register_callback(esp_xt_callback_t func, void *arg) WINK_SLA_ERROR("Wink SLA Violation: esp_xt_wdt_register_callback out of Core 8 scope.");
#else
void esp_xt_wdt_register_callback(esp_xt_callback_t func, void *arg);
#endif

#if defined(__WINK_SIM__)
void esp_xt_wdt_restore_clk(void) WINK_SLA_ERROR("Wink SLA Violation: esp_xt_wdt_restore_clk out of Core 8 scope.");
#else
void esp_xt_wdt_restore_clk(void);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_XT_WDT_H__ */
#endif /* WINK_H_GUARD_ESP_XT_WDT_H */
