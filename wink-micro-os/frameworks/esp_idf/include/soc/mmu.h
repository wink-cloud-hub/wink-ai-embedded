/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_SOC_MMU_H
#define WINK_H_GUARD_SOC_MMU_H
#ifndef __WINK_HARVESTED_SOC_MMU_H__
#define __WINK_HARVESTED_SOC_MMU_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef SOC_MMU_ADDR_MASK
#define SOC_MMU_ADDR_MASK DPORT_MMU_ADDRESS_MASK
#endif
#ifndef SOC_MMU_DROM0_PAGES_END
#define SOC_MMU_DROM0_PAGES_END 64
#endif
#ifndef SOC_MMU_DROM0_PAGES_START
#define SOC_MMU_DROM0_PAGES_START 0
#endif
#ifndef SOC_MMU_IROM0_PAGES_END
#define SOC_MMU_IROM0_PAGES_END 256
#endif
#ifndef SOC_MMU_IROM0_PAGES_START
#define SOC_MMU_IROM0_PAGES_START 64
#endif
#ifndef SOC_MMU_PAGES_PER_REGION
#define SOC_MMU_PAGES_PER_REGION 64
#endif
#ifndef SOC_MMU_PAGE_IN_FLASH
#define SOC_MMU_PAGE_IN_FLASH(page) (page)
#endif
#ifndef SOC_MMU_PRO_IRAM0_FIRST_USABLE_PAGE
#define SOC_MMU_PRO_IRAM0_FIRST_USABLE_PAGE ((SOC_MMU_VADDR1_FIRST_USABLE_ADDR - SOC_MMU_VADDR1_START_ADDR) / SPI_FLASH_MMU_PAGE_SIZE + SOC_MMU_IROM0_PAGES_START)
#endif
#ifndef SOC_MMU_REGIONS_COUNT
#define SOC_MMU_REGIONS_COUNT 4
#endif
#ifndef SOC_MMU_VADDR0_START_ADDR
#define SOC_MMU_VADDR0_START_ADDR SOC_DROM_LOW
#endif
#ifndef SOC_MMU_VADDR1_FIRST_USABLE_ADDR
#define SOC_MMU_VADDR1_FIRST_USABLE_ADDR SOC_IROM_LOW
#endif
#ifndef SOC_MMU_VADDR1_START_ADDR
#define SOC_MMU_VADDR1_START_ADDR SOC_IROM_MASK_LOW
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_SOC_MMU_H__ */
#endif /* WINK_H_GUARD_SOC_MMU_H */
