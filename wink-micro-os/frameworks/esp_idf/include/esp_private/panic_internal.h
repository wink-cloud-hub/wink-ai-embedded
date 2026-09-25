/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_PANIC_INTERNAL_H
#define WINK_H_GUARD_ESP_PRIVATE_PANIC_INTERNAL_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_PANIC_INTERNAL_H__
#define __WINK_HARVESTED_ESP_PRIVATE_PANIC_INTERNAL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stdint.h>

#include "esp_macros.h"
#include "sdkconfig.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef PANIC_INFO_DUMP
#define PANIC_INFO_DUMP(info, dump_fn) {if ((info)->dump_fn) (*(info)->dump_fn)((info->frame));}
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    PANIC_EXCEPTION_DEBUG = 0,
    PANIC_EXCEPTION_IWDT = 1,
    PANIC_EXCEPTION_TWDT = 2,
    PANIC_EXCEPTION_ABORT = 3,
    PANIC_EXCEPTION_FAULT = 4,
} panic_exception_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef void (*panic_info_dump_fn_t)(const void* frame);
typedef struct {
    int core;
    panic_exception_t exception;
    const char* reason;
    const char* description;
    panic_info_dump_fn_t details;
    panic_info_dump_fn_t state;
    const void* addr;
    const void* frame;
    bool pseudo_excause;
} panic_info_t;



#if defined(__WINK_SIM__)
void panic_abort(const char *details) WINK_SLA_ERROR("Wink SLA Violation: panic_abort out of Core 8 scope.");
#else
void panic_abort(const char *details);
#endif

#if defined(__WINK_SIM__)
void panic_arch_fill_info(void *frame, panic_info_t *info) WINK_SLA_ERROR("Wink SLA Violation: panic_arch_fill_info out of Core 8 scope.");
#else
void panic_arch_fill_info(void *frame, panic_info_t *info);
#endif

#if defined(__WINK_SIM__)
void panic_clear_active_interrupts(const void* frame) WINK_SLA_ERROR("Wink SLA Violation: panic_clear_active_interrupts out of Core 8 scope.");
#else
void panic_clear_active_interrupts(const void* frame);
#endif

#if defined(__WINK_SIM__)
uint32_t panic_get_address(const void* frame) WINK_SLA_ERROR("Wink SLA Violation: panic_get_address out of Core 8 scope.");
#else
uint32_t panic_get_address(const void* frame);
#endif

#if defined(__WINK_SIM__)
uint32_t panic_get_cause(const void* frame) WINK_SLA_ERROR("Wink SLA Violation: panic_get_cause out of Core 8 scope.");
#else
uint32_t panic_get_cause(const void* frame);
#endif

#if defined(__WINK_SIM__)
void panic_prepare_frame_from_ctx(void* frame) WINK_SLA_ERROR("Wink SLA Violation: panic_prepare_frame_from_ctx out of Core 8 scope.");
#else
void panic_prepare_frame_from_ctx(void* frame);
#endif

#if defined(__WINK_SIM__)
void panic_print_backtrace(const void *frame, int core) WINK_SLA_ERROR("Wink SLA Violation: panic_print_backtrace out of Core 8 scope.");
#else
void panic_print_backtrace(const void *frame, int core);
#endif

#if defined(__WINK_SIM__)
void panic_print_char(char c) WINK_SLA_ERROR("Wink SLA Violation: panic_print_char out of Core 8 scope.");
#else
void panic_print_char(char c);
#endif

#if defined(__WINK_SIM__)
void panic_print_dec(int d) WINK_SLA_ERROR("Wink SLA Violation: panic_print_dec out of Core 8 scope.");
#else
void panic_print_dec(int d);
#endif

#if defined(__WINK_SIM__)
void panic_print_hex(int h) WINK_SLA_ERROR("Wink SLA Violation: panic_print_hex out of Core 8 scope.");
#else
void panic_print_hex(int h);
#endif

#if defined(__WINK_SIM__)
void panic_print_registers(const void *frame, int core) WINK_SLA_ERROR("Wink SLA Violation: panic_print_registers out of Core 8 scope.");
#else
void panic_print_registers(const void *frame, int core);
#endif

#if defined(__WINK_SIM__)
void panic_print_str(const char *str) WINK_SLA_ERROR("Wink SLA Violation: panic_print_str out of Core 8 scope.");
#else
void panic_print_str(const char *str);
#endif

#if defined(__WINK_SIM__)
void panic_set_address(void *frame, uint32_t addr) WINK_SLA_ERROR("Wink SLA Violation: panic_set_address out of Core 8 scope.");
#else
void panic_set_address(void *frame, uint32_t addr);
#endif

#if defined(__WINK_SIM__)
bool panic_soc_check_pseudo_cause(void *f, panic_info_t *info) WINK_SLA_ERROR("Wink SLA Violation: panic_soc_check_pseudo_cause out of Core 8 scope.");
#else
bool panic_soc_check_pseudo_cause(void *f, panic_info_t *info);
#endif

#if defined(__WINK_SIM__)
void panic_soc_fill_info(void *frame, panic_info_t *info) WINK_SLA_ERROR("Wink SLA Violation: panic_soc_fill_info out of Core 8 scope.");
#else
void panic_soc_fill_info(void *frame, panic_info_t *info);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_PANIC_INTERNAL_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_PANIC_INTERNAL_H */
