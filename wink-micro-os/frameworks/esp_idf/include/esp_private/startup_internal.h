/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_STARTUP_INTERNAL_H
#define WINK_H_GUARD_ESP_PRIVATE_STARTUP_INTERNAL_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_STARTUP_INTERNAL_H__
#define __WINK_HARVESTED_ESP_PRIVATE_STARTUP_INTERNAL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"

#include "esp_attr.h"
#include "esp_bit_defs.h"
#include "esp_err.h"
#include "sdkconfig.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef ESP_SYSTEM_INIT_ALL_CORES
#define ESP_SYSTEM_INIT_ALL_CORES BIT(0)
#endif
#ifndef ESP_SYSTEM_INIT_FN
#define ESP_SYSTEM_INIT_FN(f, stage_, c, priority, ...) static esp_err_t __VA_ARGS__ __esp_sys_init_fn_##f(void);  static _SECTION_ATTR_IMPL_GENERIC("esp_sys_init_fn", priority)  esp_system_init_fn_t esp_sys_init_fn_##f = {  .fn = ( __esp_sys_init_fn_##f),  .cores = (c),  .stage = ESP_SYSTEM_INIT_STAGE_##stage_  };  static esp_err_t __esp_sys_init_fn_##f(void)
#endif
#ifndef ESP_SYSTEM_INIT_STAGE_CORE
#define ESP_SYSTEM_INIT_STAGE_CORE 0
#endif
#ifndef ESP_SYSTEM_INIT_STAGE_SECONDARY
#define ESP_SYSTEM_INIT_STAGE_SECONDARY 1
#endif
#ifndef SYS_STARTUP_FN
#define SYS_STARTUP_FN() ((*g_startup_fn[(esp_cpu_get_core_id())])())
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef void (*sys_startup_fn_t)(void);
typedef struct {
    esp_err_t (*fn)(void);
    uint16_t cores;
    uint16_t stage;
} esp_system_init_fn_t;



#if defined(__WINK_SIM__)
e sp_err_t(*fn)(void) WINK_SLA_ERROR("Wink SLA Violation: sp_err_t out of Core 8 scope.");
#else
e sp_err_t(*fn)(void);
#endif

#if defined(__WINK_SIM__)
void startup_resume_other_cores(void) WINK_SLA_ERROR("Wink SLA Violation: startup_resume_other_cores out of Core 8 scope.");
#else
void startup_resume_other_cores(void);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_STARTUP_INTERNAL_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_STARTUP_INTERNAL_H */
