/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_SOC_FLASH_ENCRYPTION_REG_H
#define WINK_H_GUARD_SOC_FLASH_ENCRYPTION_REG_H
#ifndef __WINK_HARVESTED_SOC_FLASH_ENCRYPTION_REG_H__
#define __WINK_HARVESTED_SOC_FLASH_ENCRYPTION_REG_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef FLASH_ENCRYPTION_ADDRESS_REG
#define FLASH_ENCRYPTION_ADDRESS_REG (PERIPHS_SPI_ENCRYPT_BASEADDR + 0x24)
#endif
#ifndef FLASH_ENCRYPTION_BUFFER_REG
#define FLASH_ENCRYPTION_BUFFER_REG (PERIPHS_SPI_ENCRYPT_BASEADDR)
#endif
#ifndef FLASH_ENCRYPTION_DONE_REG
#define FLASH_ENCRYPTION_DONE_REG (PERIPHS_SPI_ENCRYPT_BASEADDR + 0x28)
#endif
#ifndef FLASH_ENCRYPTION_START_REG
#define FLASH_ENCRYPTION_START_REG (PERIPHS_SPI_ENCRYPT_BASEADDR + 0x20)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_SOC_FLASH_ENCRYPTION_REG_H__ */
#endif /* WINK_H_GUARD_SOC_FLASH_ENCRYPTION_REG_H */
