/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_DEBUG_HELPERS_H
#define WINK_H_GUARD_ESP_DEBUG_HELPERS_H
#ifndef __WINK_HARVESTED_ESP_DEBUG_HELPERS_H__
#define __WINK_HARVESTED_ESP_DEBUG_HELPERS_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>

#include "esp_err.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
    uint32_t pc;
    uint32_t sp;
    uint32_t next_pc;
    const void * exc_frame;
} esp_backtrace_frame_t;



#if defined(__WINK_SIM__)
bool esp_backtrace_get_next_frame(esp_backtrace_frame_t *frame) WINK_SLA_ERROR("Wink SLA Violation: esp_backtrace_get_next_frame out of Core 8 scope.");
#else
bool esp_backtrace_get_next_frame(esp_backtrace_frame_t *frame);
#endif

#if defined(__WINK_SIM__)
extern void esp_backtrace_get_start(uint32_t *pc, uint32_t *sp, uint32_t *next_pc) WINK_SLA_ERROR("Wink SLA Violation: esp_backtrace_get_start out of Core 8 scope.");
#else
extern void esp_backtrace_get_start(uint32_t *pc, uint32_t *sp, uint32_t *next_pc);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_backtrace_print(int depth) WINK_SLA_ERROR("Wink SLA Violation: esp_backtrace_print out of Core 8 scope.");
#else
esp_err_t esp_backtrace_print(int depth);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_backtrace_print_all_tasks(int depth) WINK_SLA_ERROR("Wink SLA Violation: esp_backtrace_print_all_tasks out of Core 8 scope.");
#else
esp_err_t esp_backtrace_print_all_tasks(int depth);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_backtrace_print_from_frame(int depth, const esp_backtrace_frame_t* frame, bool panic) WINK_SLA_ERROR("Wink SLA Violation: esp_backtrace_print_from_frame out of Core 8 scope.");
#else
esp_err_t esp_backtrace_print_from_frame(int depth, const esp_backtrace_frame_t* frame, bool panic);
#endif

#if defined(__WINK_SIM__)
void esp_clear_watchpoint(int no) WINK_SLA_ERROR("Wink SLA Violation: esp_clear_watchpoint out of Core 8 scope.");
#else
void esp_clear_watchpoint(int no);
#endif

#if defined(__WINK_SIM__)
void esp_set_breakpoint_if_jtag(void *fn) WINK_SLA_ERROR("Wink SLA Violation: esp_set_breakpoint_if_jtag out of Core 8 scope.");
#else
void esp_set_breakpoint_if_jtag(void *fn);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_set_watchpoint(int no, void *adr, int size, int flags) WINK_SLA_ERROR("Wink SLA Violation: esp_set_watchpoint out of Core 8 scope.");
#else
esp_err_t esp_set_watchpoint(int no, void *adr, int size, int flags);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_DEBUG_HELPERS_H__ */
#endif /* WINK_H_GUARD_ESP_DEBUG_HELPERS_H */
