/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_EXPRESSION_WITH_STACK_H
#define WINK_H_GUARD_ESP_EXPRESSION_WITH_STACK_H
#ifndef __WINK_HARVESTED_ESP_EXPRESSION_WITH_STACK_H__
#define __WINK_HARVESTED_ESP_EXPRESSION_WITH_STACK_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef ESP_EXECUTE_EXPRESSION_WITH_STACK
#define ESP_EXECUTE_EXPRESSION_WITH_STACK(lock, stack, stack_size, expression) esp_execute_shared_stack_function(lock, stack, stack_size, expression)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef void (*shared_stack_function)(void);



#if defined(__WINK_SIM__)
void esp_execute_shared_stack_function(SemaphoreHandle_t lock,
                                       void *stack,
                                       size_t stack_size,
                                       shared_stack_function function) WINK_SLA_ERROR("Wink SLA Violation: esp_execute_shared_stack_function out of Core 8 scope.");
#else
void esp_execute_shared_stack_function(SemaphoreHandle_t lock,
                                       void *stack,
                                       size_t stack_size,
                                       shared_stack_function function);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_EXPRESSION_WITH_STACK_H__ */
#endif /* WINK_H_GUARD_ESP_EXPRESSION_WITH_STACK_H */
