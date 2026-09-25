/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_MMU_TYPES_H
#define WINK_H_GUARD_HAL_MMU_TYPES_H
#ifndef __WINK_HARVESTED_HAL_MMU_TYPES_H__
#define __WINK_HARVESTED_HAL_MMU_TYPES_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"

#include "esp_bit_defs.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    MMU_MEM_CAP_EXEC = 1,
    MMU_MEM_CAP_READ = 2,
    MMU_MEM_CAP_WRITE = 4,
    MMU_MEM_CAP_32BIT = 8,
    MMU_MEM_CAP_8BIT = 16,
} mmu_mem_caps_t;
typedef enum {
    MMU_PAGE_8KB = 8192,
    MMU_PAGE_16KB = 16384,
    MMU_PAGE_32KB = 32768,
    MMU_PAGE_64KB = 65536,
    MMU_PAGE_128KB = 131072,
    MMU_PAGE_256KB = 262144,
} mmu_page_size_t;
typedef enum {
    MMU_VADDR_DATA = 1,
    MMU_VADDR_INSTRUCTION = 2,
} mmu_vaddr_t;
typedef enum {
    MMU_TARGET_FLASH0 = 1,
    MMU_TARGET_PSRAM0 = 2,
} mmu_target_t;
typedef enum {
    MMU_TABLE_CORE0 = 0,
    MMU_TABLE_CORE1 = 1,
} mmu_table_id_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_MMU_TYPES_H__ */
#endif /* WINK_H_GUARD_HAL_MMU_TYPES_H */
