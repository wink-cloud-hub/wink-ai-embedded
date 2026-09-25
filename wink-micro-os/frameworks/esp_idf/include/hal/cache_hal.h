/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_CACHE_HAL_H
#define WINK_H_GUARD_HAL_CACHE_HAL_H
#ifndef __WINK_HARVESTED_HAL_CACHE_HAL_H__
#define __WINK_HARVESTED_HAL_CACHE_HAL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stdint.h>

#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
    uint8_t core_nums;
    uint32_t l2_cache_size;
    uint32_t l2_cache_line_size;
} cache_hal_config_t;



#if defined(__WINK_SIM__)
void cache_hal_disable(uint32_t cache_level, cache_type_t type) WINK_SLA_ERROR("Wink SLA Violation: cache_hal_disable out of Core 8 scope.");
#else
void cache_hal_disable(uint32_t cache_level, cache_type_t type);
#endif

#if defined(__WINK_SIM__)
void cache_hal_enable(uint32_t cache_level, cache_type_t type) WINK_SLA_ERROR("Wink SLA Violation: cache_hal_enable out of Core 8 scope.");
#else
void cache_hal_enable(uint32_t cache_level, cache_type_t type);
#endif

#if defined(__WINK_SIM__)
uint32_t cache_hal_get_cache_line_size(uint32_t cache_level, cache_type_t type) WINK_SLA_ERROR("Wink SLA Violation: cache_hal_get_cache_line_size out of Core 8 scope.");
#else
uint32_t cache_hal_get_cache_line_size(uint32_t cache_level, cache_type_t type);
#endif

#if defined(__WINK_SIM__)
void cache_hal_init(const cache_hal_config_t *config) WINK_SLA_ERROR("Wink SLA Violation: cache_hal_init out of Core 8 scope.");
#else
void cache_hal_init(const cache_hal_config_t *config);
#endif

#if defined(__WINK_SIM__)
bool cache_hal_invalidate_addr(uint32_t vaddr, uint32_t size) WINK_SLA_ERROR("Wink SLA Violation: cache_hal_invalidate_addr out of Core 8 scope.");
#else
bool cache_hal_invalidate_addr(uint32_t vaddr, uint32_t size);
#endif

#if defined(__WINK_SIM__)
bool cache_hal_is_cache_enabled(uint32_t cache_level, cache_type_t type) WINK_SLA_ERROR("Wink SLA Violation: cache_hal_is_cache_enabled out of Core 8 scope.");
#else
bool cache_hal_is_cache_enabled(uint32_t cache_level, cache_type_t type);
#endif

#if defined(__WINK_SIM__)
void cache_hal_preload(uint32_t cache_level, cache_type_t type, uint32_t vaddr, uint32_t size, cache_preload_order_t order) WINK_SLA_ERROR("Wink SLA Violation: cache_hal_preload out of Core 8 scope.");
#else
void cache_hal_preload(uint32_t cache_level, cache_type_t type, uint32_t vaddr, uint32_t size, cache_preload_order_t order);
#endif

#if defined(__WINK_SIM__)
void cache_hal_preload_wait_done(uint32_t cache_level, cache_type_t type) WINK_SLA_ERROR("Wink SLA Violation: cache_hal_preload_wait_done out of Core 8 scope.");
#else
void cache_hal_preload_wait_done(uint32_t cache_level, cache_type_t type);
#endif

#if defined(__WINK_SIM__)
void cache_hal_resume(uint32_t cache_level, cache_type_t type) WINK_SLA_ERROR("Wink SLA Violation: cache_hal_resume out of Core 8 scope.");
#else
void cache_hal_resume(uint32_t cache_level, cache_type_t type);
#endif

#if defined(__WINK_SIM__)
void cache_hal_suspend(uint32_t cache_level, cache_type_t type) WINK_SLA_ERROR("Wink SLA Violation: cache_hal_suspend out of Core 8 scope.");
#else
void cache_hal_suspend(uint32_t cache_level, cache_type_t type);
#endif

#if defined(__WINK_SIM__)
bool cache_hal_vaddr_to_cache_level_id(uint32_t vaddr_start, uint32_t len, uint32_t *out_level, uint32_t *out_id) WINK_SLA_ERROR("Wink SLA Violation: cache_hal_vaddr_to_cache_level_id out of Core 8 scope.");
#else
bool cache_hal_vaddr_to_cache_level_id(uint32_t vaddr_start, uint32_t len, uint32_t *out_level, uint32_t *out_id);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_CACHE_HAL_H__ */
#endif /* WINK_H_GUARD_HAL_CACHE_HAL_H */
