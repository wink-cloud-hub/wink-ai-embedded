/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_MEMPROT_ERR_H
#define WINK_H_GUARD_ESP_MEMPROT_ERR_H
#ifndef __WINK_HARVESTED_ESP_MEMPROT_ERR_H__
#define __WINK_HARVESTED_ESP_MEMPROT_ERR_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef ESP_ERR_MEMPROT_AREA_INVALID
#define ESP_ERR_MEMPROT_AREA_INVALID (ESP_ERR_MEMPROT_BASE + 7)
#endif
#ifndef ESP_ERR_MEMPROT_CPUID_INVALID
#define ESP_ERR_MEMPROT_CPUID_INVALID (ESP_ERR_MEMPROT_BASE + 8)
#endif
#ifndef ESP_ERR_MEMPROT_MEMORY_TYPE_INVALID
#define ESP_ERR_MEMPROT_MEMORY_TYPE_INVALID (ESP_ERR_MEMPROT_BASE + 1)
#endif
#ifndef ESP_ERR_MEMPROT_SPLIT_ADDR_INVALID
#define ESP_ERR_MEMPROT_SPLIT_ADDR_INVALID (ESP_ERR_MEMPROT_BASE + 2)
#endif
#ifndef ESP_ERR_MEMPROT_SPLIT_ADDR_OUT_OF_RANGE
#define ESP_ERR_MEMPROT_SPLIT_ADDR_OUT_OF_RANGE (ESP_ERR_MEMPROT_BASE + 3)
#endif
#ifndef ESP_ERR_MEMPROT_SPLIT_ADDR_UNALIGNED
#define ESP_ERR_MEMPROT_SPLIT_ADDR_UNALIGNED (ESP_ERR_MEMPROT_BASE + 4)
#endif
#ifndef ESP_ERR_MEMPROT_UNIMGMT_BLOCK_INVALID
#define ESP_ERR_MEMPROT_UNIMGMT_BLOCK_INVALID (ESP_ERR_MEMPROT_BASE + 5)
#endif
#ifndef ESP_ERR_MEMPROT_WORLD_INVALID
#define ESP_ERR_MEMPROT_WORLD_INVALID (ESP_ERR_MEMPROT_BASE + 6)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_MEMPROT_ERR_H__ */
#endif /* WINK_H_GUARD_ESP_MEMPROT_ERR_H */
