/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_EFUSE_HAL_H
#define WINK_H_GUARD_HAL_EFUSE_HAL_H
#ifndef __WINK_HARVESTED_HAL_EFUSE_HAL_H__
#define __WINK_HARVESTED_HAL_EFUSE_HAL_H__
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



#if defined(__WINK_SIM__)
uint32_t efuse_hal_blk_version(void) WINK_SLA_ERROR("Wink SLA Violation: efuse_hal_blk_version out of Core 8 scope.");
#else
uint32_t efuse_hal_blk_version(void);
#endif

#if defined(__WINK_SIM__)
uint32_t efuse_hal_chip_revision(void) WINK_SLA_ERROR("Wink SLA Violation: efuse_hal_chip_revision out of Core 8 scope.");
#else
uint32_t efuse_hal_chip_revision(void);
#endif

#if defined(__WINK_SIM__)
bool efuse_hal_flash_encryption_enabled(void) WINK_SLA_ERROR("Wink SLA Violation: efuse_hal_flash_encryption_enabled out of Core 8 scope.");
#else
bool efuse_hal_flash_encryption_enabled(void);
#endif

#if defined(__WINK_SIM__)
uint32_t efuse_hal_get_chip_ver_pkg(void) WINK_SLA_ERROR("Wink SLA Violation: efuse_hal_get_chip_ver_pkg out of Core 8 scope.");
#else
uint32_t efuse_hal_get_chip_ver_pkg(void);
#endif

#if defined(__WINK_SIM__)
bool efuse_hal_get_disable_blk_version_major(void) WINK_SLA_ERROR("Wink SLA Violation: efuse_hal_get_disable_blk_version_major out of Core 8 scope.");
#else
bool efuse_hal_get_disable_blk_version_major(void);
#endif

#if defined(__WINK_SIM__)
bool efuse_hal_get_disable_wafer_version_major(void) WINK_SLA_ERROR("Wink SLA Violation: efuse_hal_get_disable_wafer_version_major out of Core 8 scope.");
#else
bool efuse_hal_get_disable_wafer_version_major(void);
#endif

#if defined(__WINK_SIM__)
void efuse_hal_get_mac(uint8_t *mac) WINK_SLA_ERROR("Wink SLA Violation: efuse_hal_get_mac out of Core 8 scope.");
#else
void efuse_hal_get_mac(uint8_t *mac);
#endif

#if defined(__WINK_SIM__)
uint32_t efuse_hal_get_major_chip_version(void) WINK_SLA_ERROR("Wink SLA Violation: efuse_hal_get_major_chip_version out of Core 8 scope.");
#else
uint32_t efuse_hal_get_major_chip_version(void);
#endif

#if defined(__WINK_SIM__)
uint32_t efuse_hal_get_minor_chip_version(void) WINK_SLA_ERROR("Wink SLA Violation: efuse_hal_get_minor_chip_version out of Core 8 scope.");
#else
uint32_t efuse_hal_get_minor_chip_version(void);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_EFUSE_HAL_H__ */
#endif /* WINK_H_GUARD_HAL_EFUSE_HAL_H */
