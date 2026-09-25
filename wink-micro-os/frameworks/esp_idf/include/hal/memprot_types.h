/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_MEMPROT_TYPES_H
#define WINK_H_GUARD_HAL_MEMPROT_TYPES_H
#ifndef __WINK_HARVESTED_HAL_MEMPROT_TYPES_H__
#define __WINK_HARVESTED_HAL_MEMPROT_TYPES_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"

#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef MEMP_HAL_CHECK_DRAM_ADDR_IN_RANGE
#define MEMP_HAL_CHECK_DRAM_ADDR_IN_RANGE(x) if (x < SOC_DIRAM_DRAM_LOW || x >= SOC_DIRAM_DRAM_HIGH) { return MEMP_HAL_ERR_SPLIT_ADDR_OUT_OF_RANGE; }
#endif
#ifndef MEMP_HAL_CHECK_IRAM_ADDR_IN_RANGE
#define MEMP_HAL_CHECK_IRAM_ADDR_IN_RANGE(x) if (x < SOC_DIRAM_IRAM_LOW || x >= SOC_DIRAM_IRAM_HIGH) { return MEMP_HAL_ERR_SPLIT_ADDR_OUT_OF_RANGE; }
#endif
#ifndef MEMP_HAL_CHECK_SPLIT_ADDR_ALIGNED
#define MEMP_HAL_CHECK_SPLIT_ADDR_ALIGNED(x) if (x % I_D_SPLIT_LINE_ALIGN != 0) { return MEMP_HAL_ERR_SPLIT_ADDR_UNALIGNED; }
#endif
#ifndef MEMP_HAL_CORE_X_IRAM0_DRAM0_DMA_SRAM_CATEGORY_BITS_ABOVE_SA
#define MEMP_HAL_CORE_X_IRAM0_DRAM0_DMA_SRAM_CATEGORY_BITS_ABOVE_SA 0x3
#endif
#ifndef MEMP_HAL_CORE_X_IRAM0_DRAM0_DMA_SRAM_CATEGORY_BITS_BELOW_SA
#define MEMP_HAL_CORE_X_IRAM0_DRAM0_DMA_SRAM_CATEGORY_BITS_BELOW_SA 0x0
#endif
#ifndef MEMP_HAL_CORE_X_IRAM0_DRAM0_DMA_SRAM_CATEGORY_BITS_EQUAL_SA
#define MEMP_HAL_CORE_X_IRAM0_DRAM0_DMA_SRAM_CATEGORY_BITS_EQUAL_SA 0x2
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    MEMP_HAL_OK = 0,
    MEMP_HAL_ERR_SPLIT_ADDR_OUT_OF_RANGE = 2,
    MEMP_HAL_ERR_SPLIT_ADDR_INVALID = 2,
    MEMP_HAL_ERR_SPLIT_ADDR_UNALIGNED = 3,
    MEMP_HAL_ERR_UNI_BLOCK_INVALID = 4,
    MEMP_HAL_ERR_AREA_INVALID = 5,
    MEMP_HAL_ERR_WORLD_INVALID = 6,
    MEMP_HAL_ERR_CORE_INVALID = 7,
    MEMP_HAL_FAIL = -1,
} memprot_hal_err_t;
typedef enum {
    MEMP_HAL_WORLD_NONE = 0x00,
    MEMP_HAL_WORLD_0 = 0x01,
    MEMP_HAL_WORLD_1 = 0x10,
} memprot_hal_world_t;
typedef enum {
    MEMP_HAL_AREA_NONE = 0,
    MEMP_HAL_AREA_LOW = 1,
    MEMP_HAL_AREA_HIGH = 2,
} memprot_hal_area_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_MEMPROT_TYPES_H__ */
#endif /* WINK_H_GUARD_HAL_MEMPROT_TYPES_H */
