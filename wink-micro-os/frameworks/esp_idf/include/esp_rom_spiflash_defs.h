/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_ROM_SPIFLASH_DEFS_H
#define WINK_H_GUARD_ESP_ROM_SPIFLASH_DEFS_H
#ifndef __WINK_HARVESTED_ESP_ROM_SPIFLASH_DEFS_H__
#define __WINK_HARVESTED_ESP_ROM_SPIFLASH_DEFS_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef ESP_ROM_SPIFLASH_BP0
#define ESP_ROM_SPIFLASH_BP0 BIT2
#endif
#ifndef ESP_ROM_SPIFLASH_BP1
#define ESP_ROM_SPIFLASH_BP1 BIT3
#endif
#ifndef ESP_ROM_SPIFLASH_BP2
#define ESP_ROM_SPIFLASH_BP2 BIT4
#endif
#ifndef ESP_ROM_SPIFLASH_BP_MASK_ISSI
#define ESP_ROM_SPIFLASH_BP_MASK_ISSI (BIT7 | BIT5 | BIT4 | BIT3 | BIT2)
#endif
#ifndef ESP_ROM_SPIFLASH_BUSY_FLAG
#define ESP_ROM_SPIFLASH_BUSY_FLAG BIT0
#endif
#ifndef ESP_ROM_SPIFLASH_QE
#define ESP_ROM_SPIFLASH_QE BIT9
#endif
#ifndef ESP_ROM_SPIFLASH_WRENABLE_FLAG
#define ESP_ROM_SPIFLASH_WRENABLE_FLAG BIT1
#endif
#ifndef ESP_ROM_SPIFLASH_WR_PROTECT
#define ESP_ROM_SPIFLASH_WR_PROTECT (ESP_ROM_SPIFLASH_BP0|ESP_ROM_SPIFLASH_BP1|ESP_ROM_SPIFLASH_BP2)
#endif
#ifndef FLASH_ID_GD25LQ32C
#define FLASH_ID_GD25LQ32C 0xC86016
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_ROM_SPIFLASH_DEFS_H__ */
#endif /* WINK_H_GUARD_ESP_ROM_SPIFLASH_DEFS_H */
