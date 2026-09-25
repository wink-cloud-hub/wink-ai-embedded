/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_INTERRUPT_CONTROLLER_HAL_H
#define WINK_H_GUARD_HAL_INTERRUPT_CONTROLLER_HAL_H
#ifndef __WINK_HARVESTED_HAL_INTERRUPT_CONTROLLER_HAL_H__
#define __WINK_HARVESTED_HAL_INTERRUPT_CONTROLLER_HAL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stdint.h>

#include "esp_attr.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    INTDESC_NORMAL = 0,
    INTDESC_RESVD = 1,
    INTDESC_SPECIAL = 2,
} int_desc_flag_t;
typedef enum {
    INTTP_LEVEL = ESP_CPU_INTR_TYPE_LEVEL,
    INTTP_EDGE = ESP_CPU_INTR_TYPE_EDGE,
    INTTP_NA = ESP_CPU_INTR_TYPE_NA,
} int_type_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
    int level;
    int_type_t type;
    int_desc_flag_t cpuflags[SOC_CPU_CORES_NUM];
} int_desc_t;
typedef void (*interrupt_handler_t)(void *arg);



#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR int_desc_flag_t interrupt_controller_hal_desc_flags(int interrupt_number, int cpu_number) WINK_SLA_ERROR("Wink SLA Violation: interrupt_controller_hal_desc_flags out of Core 8 scope.");
#else
FORCE_INLINE_ATTR int_desc_flag_t interrupt_controller_hal_desc_flags(int interrupt_number, int cpu_number);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR int interrupt_controller_hal_desc_level(int interrupt_number) WINK_SLA_ERROR("Wink SLA Violation: interrupt_controller_hal_desc_level out of Core 8 scope.");
#else
FORCE_INLINE_ATTR int interrupt_controller_hal_desc_level(int interrupt_number);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR int_type_t interrupt_controller_hal_desc_type(int interrupt_number) WINK_SLA_ERROR("Wink SLA Violation: interrupt_controller_hal_desc_type out of Core 8 scope.");
#else
FORCE_INLINE_ATTR int_type_t interrupt_controller_hal_desc_type(int interrupt_number);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void interrupt_controller_hal_disable_interrupts(uint32_t mask) WINK_SLA_ERROR("Wink SLA Violation: interrupt_controller_hal_disable_interrupts out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void interrupt_controller_hal_disable_interrupts(uint32_t mask);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void interrupt_controller_hal_edge_int_acknowledge(int intr) WINK_SLA_ERROR("Wink SLA Violation: interrupt_controller_hal_edge_int_acknowledge out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void interrupt_controller_hal_edge_int_acknowledge(int intr);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void interrupt_controller_hal_enable_interrupts(uint32_t mask) WINK_SLA_ERROR("Wink SLA Violation: interrupt_controller_hal_enable_interrupts out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void interrupt_controller_hal_enable_interrupts(uint32_t mask);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR uint32_t interrupt_controller_hal_get_cpu_desc_flags(int interrupt_number, int cpu_number) WINK_SLA_ERROR("Wink SLA Violation: interrupt_controller_hal_get_cpu_desc_flags out of Core 8 scope.");
#else
FORCE_INLINE_ATTR uint32_t interrupt_controller_hal_get_cpu_desc_flags(int interrupt_number, int cpu_number);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void * interrupt_controller_hal_get_int_handler_arg(uint8_t intr) WINK_SLA_ERROR("Wink SLA Violation: interrupt_controller_hal_get_int_handler_arg out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void * interrupt_controller_hal_get_int_handler_arg(uint8_t intr);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR int interrupt_controller_hal_get_level(int interrupt_number) WINK_SLA_ERROR("Wink SLA Violation: interrupt_controller_hal_get_level out of Core 8 scope.");
#else
FORCE_INLINE_ATTR int interrupt_controller_hal_get_level(int interrupt_number);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR int_type_t interrupt_controller_hal_get_type(int interrupt_number) WINK_SLA_ERROR("Wink SLA Violation: interrupt_controller_hal_get_type out of Core 8 scope.");
#else
FORCE_INLINE_ATTR int_type_t interrupt_controller_hal_get_type(int interrupt_number);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR bool interrupt_controller_hal_has_handler(int intr, int cpu) WINK_SLA_ERROR("Wink SLA Violation: interrupt_controller_hal_has_handler out of Core 8 scope.");
#else
FORCE_INLINE_ATTR bool interrupt_controller_hal_has_handler(int intr, int cpu);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR uint32_t interrupt_controller_hal_read_interrupt_mask(void) WINK_SLA_ERROR("Wink SLA Violation: interrupt_controller_hal_read_interrupt_mask out of Core 8 scope.");
#else
FORCE_INLINE_ATTR uint32_t interrupt_controller_hal_read_interrupt_mask(void);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void interrupt_controller_hal_set_int_handler(uint8_t intr, interrupt_handler_t handler, void *arg) WINK_SLA_ERROR("Wink SLA Violation: interrupt_controller_hal_set_int_handler out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void interrupt_controller_hal_set_int_handler(uint8_t intr, interrupt_handler_t handler, void *arg);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void interrupt_controller_hal_set_int_level(int intr, int level) WINK_SLA_ERROR("Wink SLA Violation: interrupt_controller_hal_set_int_level out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void interrupt_controller_hal_set_int_level(int intr, int level);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void interrupt_controller_hal_set_int_type(int intr, int_type_t type) WINK_SLA_ERROR("Wink SLA Violation: interrupt_controller_hal_set_int_type out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void interrupt_controller_hal_set_int_type(int intr, int_type_t type);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_INTERRUPT_CONTROLLER_HAL_H__ */
#endif /* WINK_H_GUARD_HAL_INTERRUPT_CONTROLLER_HAL_H */
