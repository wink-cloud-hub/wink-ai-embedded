/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_CRITICAL_SECTION_H
#define WINK_H_GUARD_ESP_PRIVATE_CRITICAL_SECTION_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_CRITICAL_SECTION_H__
#define __WINK_HARVESTED_ESP_PRIVATE_CRITICAL_SECTION_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"

#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef DECLARE_CRIT_SECTION_LOCK_IN_STRUCT
#define DECLARE_CRIT_SECTION_LOCK_IN_STRUCT(lock_name) esp_os_spinlock_t lock_name;
#endif
#ifndef DEFINE_CRIT_SECTION_LOCK
#define DEFINE_CRIT_SECTION_LOCK(lock_name, optional_qualifiers...) optional_qualifiers esp_os_spinlock_t lock_name = SPINLOCK_INITIALIZER
#endif
#ifndef DEFINE_CRIT_SECTION_LOCK_STATIC
#define DEFINE_CRIT_SECTION_LOCK_STATIC(lock_name, optional_qualifiers...) static optional_qualifiers esp_os_spinlock_t lock_name = SPINLOCK_INITIALIZER
#endif
#ifndef INIT_CRIT_SECTION_LOCK_IN_STRUCT
#define INIT_CRIT_SECTION_LOCK_IN_STRUCT(lock_name) .lock_name = portMUX_INITIALIZER_UNLOCKED,
#endif
#ifndef INIT_CRIT_SECTION_LOCK_RUNTIME
#define INIT_CRIT_SECTION_LOCK_RUNTIME(lock_name) spinlock_initialize(lock_name)
#endif
#ifndef OS_SPINLOCK
#define OS_SPINLOCK 1
#endif
#ifndef esp_os_enter_critical
#define esp_os_enter_critical(lock) portENTER_CRITICAL(lock)
#endif
#ifndef esp_os_enter_critical_isr
#define esp_os_enter_critical_isr(lock) portENTER_CRITICAL_ISR(lock)
#endif
#ifndef esp_os_enter_critical_safe
#define esp_os_enter_critical_safe(lock) portENTER_CRITICAL_SAFE(lock)
#endif
#ifndef esp_os_exit_critical
#define esp_os_exit_critical(lock) portEXIT_CRITICAL(lock)
#endif
#ifndef esp_os_exit_critical_isr
#define esp_os_exit_critical_isr(lock) portEXIT_CRITICAL_ISR(lock)
#endif
#ifndef esp_os_exit_critical_safe
#define esp_os_exit_critical_safe(lock) portEXIT_CRITICAL_SAFE(lock)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef spinlock_t esp_os_spinlock_t;



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_CRITICAL_SECTION_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_CRITICAL_SECTION_H */
