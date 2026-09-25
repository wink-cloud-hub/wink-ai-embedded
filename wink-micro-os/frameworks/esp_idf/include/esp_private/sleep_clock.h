/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_SLEEP_CLOCK_H
#define WINK_H_GUARD_ESP_PRIVATE_SLEEP_CLOCK_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_SLEEP_CLOCK_H__
#define __WINK_HARVESTED_ESP_PRIVATE_SLEEP_CLOCK_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_attr.h"
#include "esp_check.h"
#include "esp_err.h"
#include "sdkconfig.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
bool clock_domain_pd_allowed(void) WINK_SLA_ERROR("Wink SLA Violation: clock_domain_pd_allowed out of Core 8 scope.");
#else
bool clock_domain_pd_allowed(void);
#endif

#if defined(__WINK_SIM__)
esp_err_t sleep_clock_modem_retention_init(void *arg) WINK_SLA_ERROR("Wink SLA Violation: sleep_clock_modem_retention_init out of Core 8 scope.");
#else
esp_err_t sleep_clock_modem_retention_init(void *arg);
#endif

#if defined(__WINK_SIM__)
esp_err_t sleep_clock_system_retention_init(void *arg) WINK_SLA_ERROR("Wink SLA Violation: sleep_clock_system_retention_init out of Core 8 scope.");
#else
esp_err_t sleep_clock_system_retention_init(void *arg);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_SLEEP_CLOCK_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_SLEEP_CLOCK_H */
