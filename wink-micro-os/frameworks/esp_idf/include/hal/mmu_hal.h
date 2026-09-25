/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_MMU_HAL_H
#define WINK_H_GUARD_HAL_MMU_HAL_H
#ifndef __WINK_HARVESTED_HAL_MMU_HAL_H__
#define __WINK_HARVESTED_HAL_MMU_HAL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <esp_types.h>

#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
    uint8_t core_nums;
    uint32_t mmu_page_size;
} mmu_hal_config_t;



#if defined(__WINK_SIM__)
uint32_t mmu_hal_bytes_to_pages(uint32_t mmu_id, uint32_t bytes) WINK_SLA_ERROR("Wink SLA Violation: mmu_hal_bytes_to_pages out of Core 8 scope.");
#else
uint32_t mmu_hal_bytes_to_pages(uint32_t mmu_id, uint32_t bytes);
#endif

#if defined(__WINK_SIM__)
bool mmu_hal_check_valid_ext_vaddr_region(uint32_t mmu_id, uint32_t vaddr_start, uint32_t len, mmu_vaddr_t type) WINK_SLA_ERROR("Wink SLA Violation: mmu_hal_check_valid_ext_vaddr_region out of Core 8 scope.");
#else
bool mmu_hal_check_valid_ext_vaddr_region(uint32_t mmu_id, uint32_t vaddr_start, uint32_t len, mmu_vaddr_t type);
#endif

#if defined(__WINK_SIM__)
bool mmu_hal_check_valid_paddr_region(uint32_t mmu_id, uint32_t paddr_start, uint32_t len) WINK_SLA_ERROR("Wink SLA Violation: mmu_hal_check_valid_paddr_region out of Core 8 scope.");
#else
bool mmu_hal_check_valid_paddr_region(uint32_t mmu_id, uint32_t paddr_start, uint32_t len);
#endif

#if defined(__WINK_SIM__)
void mmu_hal_ctx_init(const mmu_hal_config_t *config) WINK_SLA_ERROR("Wink SLA Violation: mmu_hal_ctx_init out of Core 8 scope.");
#else
void mmu_hal_ctx_init(const mmu_hal_config_t *config);
#endif

#if defined(__WINK_SIM__)
uint32_t mmu_hal_get_id_from_target(mmu_target_t target) WINK_SLA_ERROR("Wink SLA Violation: mmu_hal_get_id_from_target out of Core 8 scope.");
#else
uint32_t mmu_hal_get_id_from_target(mmu_target_t target);
#endif

#if defined(__WINK_SIM__)
uint32_t mmu_hal_get_id_from_vaddr(uint32_t vaddr) WINK_SLA_ERROR("Wink SLA Violation: mmu_hal_get_id_from_vaddr out of Core 8 scope.");
#else
uint32_t mmu_hal_get_id_from_vaddr(uint32_t vaddr);
#endif

#if defined(__WINK_SIM__)
void mmu_hal_init(const mmu_hal_config_t *config) WINK_SLA_ERROR("Wink SLA Violation: mmu_hal_init out of Core 8 scope.");
#else
void mmu_hal_init(const mmu_hal_config_t *config);
#endif

#if defined(__WINK_SIM__)
void mmu_hal_map_region(uint32_t mmu_id, mmu_target_t mem_type, uint32_t vaddr, uint32_t paddr, uint32_t len, uint32_t *out_len) WINK_SLA_ERROR("Wink SLA Violation: mmu_hal_map_region out of Core 8 scope.");
#else
void mmu_hal_map_region(uint32_t mmu_id, mmu_target_t mem_type, uint32_t vaddr, uint32_t paddr, uint32_t len, uint32_t *out_len);
#endif

#if defined(__WINK_SIM__)
void mmu_hal_map_region_no_enc(uint32_t vaddr, uint32_t paddr, uint32_t len) WINK_SLA_ERROR("Wink SLA Violation: mmu_hal_map_region_no_enc out of Core 8 scope.");
#else
void mmu_hal_map_region_no_enc(uint32_t vaddr, uint32_t paddr, uint32_t len);
#endif

#if defined(__WINK_SIM__)
bool mmu_hal_paddr_to_vaddr(uint32_t mmu_id, uint32_t paddr, mmu_target_t target, mmu_vaddr_t type, uint32_t *out_vaddr) WINK_SLA_ERROR("Wink SLA Violation: mmu_hal_paddr_to_vaddr out of Core 8 scope.");
#else
bool mmu_hal_paddr_to_vaddr(uint32_t mmu_id, uint32_t paddr, mmu_target_t target, mmu_vaddr_t type, uint32_t *out_vaddr);
#endif

#if defined(__WINK_SIM__)
uint32_t mmu_hal_pages_to_bytes(uint32_t mmu_id, uint32_t page_num) WINK_SLA_ERROR("Wink SLA Violation: mmu_hal_pages_to_bytes out of Core 8 scope.");
#else
uint32_t mmu_hal_pages_to_bytes(uint32_t mmu_id, uint32_t page_num);
#endif

#if defined(__WINK_SIM__)
void mmu_hal_unmap_all(void) WINK_SLA_ERROR("Wink SLA Violation: mmu_hal_unmap_all out of Core 8 scope.");
#else
void mmu_hal_unmap_all(void);
#endif

#if defined(__WINK_SIM__)
void mmu_hal_unmap_region(uint32_t mmu_id, uint32_t vaddr, uint32_t len) WINK_SLA_ERROR("Wink SLA Violation: mmu_hal_unmap_region out of Core 8 scope.");
#else
void mmu_hal_unmap_region(uint32_t mmu_id, uint32_t vaddr, uint32_t len);
#endif

#if defined(__WINK_SIM__)
bool mmu_hal_vaddr_to_paddr(uint32_t mmu_id, uint32_t vaddr, uint32_t *out_paddr, mmu_target_t *out_target) WINK_SLA_ERROR("Wink SLA Violation: mmu_hal_vaddr_to_paddr out of Core 8 scope.");
#else
bool mmu_hal_vaddr_to_paddr(uint32_t mmu_id, uint32_t vaddr, uint32_t *out_paddr, mmu_target_t *out_target);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_MMU_HAL_H__ */
#endif /* WINK_H_GUARD_HAL_MMU_HAL_H */
