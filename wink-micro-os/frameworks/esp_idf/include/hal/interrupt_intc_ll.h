/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_INTERRUPT_INTC_LL_H
#define WINK_H_GUARD_HAL_INTERRUPT_INTC_LL_H
#ifndef __WINK_HARVESTED_HAL_INTERRUPT_INTC_LL_H__
#define __WINK_HARVESTED_HAL_INTERRUPT_INTC_LL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"

#include "esp_attr.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR int interrupt_intc_ll_get_priority(int rv_int_num) WINK_SLA_ERROR("Wink SLA Violation: interrupt_intc_ll_get_priority out of Core 8 scope.");
#else
FORCE_INLINE_ATTR int interrupt_intc_ll_get_priority(int rv_int_num);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR int interrupt_intc_ll_get_type(int rv_int_num) WINK_SLA_ERROR("Wink SLA Violation: interrupt_intc_ll_get_type out of Core 8 scope.");
#else
FORCE_INLINE_ATTR int interrupt_intc_ll_get_type(int rv_int_num);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR uint32_t interrupt_intc_ll_get_unmask(void) WINK_SLA_ERROR("Wink SLA Violation: interrupt_intc_ll_get_unmask out of Core 8 scope.");
#else
FORCE_INLINE_ATTR uint32_t interrupt_intc_ll_get_unmask(void);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void interrupt_intc_ll_route(int intr_src, int intr_num) WINK_SLA_ERROR("Wink SLA Violation: interrupt_intc_ll_route out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void interrupt_intc_ll_route(int intr_src, int intr_num);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_INTERRUPT_INTC_LL_H__ */
#endif /* WINK_H_GUARD_HAL_INTERRUPT_INTC_LL_H */
