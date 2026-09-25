/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_INTERRUPT_CLIC_LL_H
#define WINK_H_GUARD_HAL_INTERRUPT_CLIC_LL_H
#ifndef __WINK_HARVESTED_HAL_INTERRUPT_CLIC_LL_H__
#define __WINK_HARVESTED_HAL_INTERRUPT_CLIC_LL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>

#include "esp_attr.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef RV_INT_MASK
#define RV_INT_MASK 63
#endif
#ifndef RV_TOTAL_INT_COUNT
#define RV_TOTAL_INT_COUNT 48
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR int interrupt_clic_ll_get_priority(int rv_int_num) WINK_SLA_ERROR("Wink SLA Violation: interrupt_clic_ll_get_priority out of Core 8 scope.");
#else
FORCE_INLINE_ATTR int interrupt_clic_ll_get_priority(int rv_int_num);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR int interrupt_clic_ll_get_type(int rv_int_num) WINK_SLA_ERROR("Wink SLA Violation: interrupt_clic_ll_get_type out of Core 8 scope.");
#else
FORCE_INLINE_ATTR int interrupt_clic_ll_get_type(int rv_int_num);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR bool interrupt_clic_ll_is_vectored(int rv_int_num) WINK_SLA_ERROR("Wink SLA Violation: interrupt_clic_ll_is_vectored out of Core 8 scope.");
#else
FORCE_INLINE_ATTR bool interrupt_clic_ll_is_vectored(int rv_int_num);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void interrupt_clic_ll_route(uint32_t core_id, int intr_src, int intr_num) WINK_SLA_ERROR("Wink SLA Violation: interrupt_clic_ll_route out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void interrupt_clic_ll_route(uint32_t core_id, int intr_src, int intr_num);
#endif

#if defined(__WINK_SIM__)
FORCE_INLINE_ATTR void interrupt_clic_ll_set_vectored(int rv_int_num, bool vectored) WINK_SLA_ERROR("Wink SLA Violation: interrupt_clic_ll_set_vectored out of Core 8 scope.");
#else
FORCE_INLINE_ATTR void interrupt_clic_ll_set_vectored(int rv_int_num, bool vectored);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_INTERRUPT_CLIC_LL_H__ */
#endif /* WINK_H_GUARD_HAL_INTERRUPT_CLIC_LL_H */
