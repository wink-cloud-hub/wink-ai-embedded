/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_ROM_EFUSE_H
#define WINK_H_GUARD_ESP_ROM_EFUSE_H
#ifndef __WINK_HARVESTED_ESP_ROM_EFUSE_H__
#define __WINK_HARVESTED_ESP_ROM_EFUSE_H__
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
#ifndef ESP_ROM_EFUSE_FLASH_DEFAULT_HSPI
#define ESP_ROM_EFUSE_FLASH_DEFAULT_HSPI (1)
#endif
#ifndef ESP_ROM_EFUSE_FLASH_DEFAULT_SPI
#define ESP_ROM_EFUSE_FLASH_DEFAULT_SPI (0)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */

uint32_t esp_rom_efuse_get_flash_gpio_info(void);


#if defined(__WINK_SIM__)
uint32_t esp_rom_efuse_get_flash_wp_gpio(void) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_efuse_get_flash_wp_gpio out of Core 8 scope.");
#else
uint32_t esp_rom_efuse_get_flash_wp_gpio(void);
#endif

#if defined(__WINK_SIM__)
bool esp_rom_efuse_is_secure_boot_enabled(void) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_efuse_is_secure_boot_enabled out of Core 8 scope.");
#else
bool esp_rom_efuse_is_secure_boot_enabled(void);
#endif

#if defined(__WINK_SIM__)
uint8_t esp_rom_efuse_mac_address_crc8(const uint8_t *data, uint32_t len) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_efuse_mac_address_crc8 out of Core 8 scope.");
#else
uint8_t esp_rom_efuse_mac_address_crc8(const uint8_t *data, uint32_t len);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_ROM_EFUSE_H__ */
#endif /* WINK_H_GUARD_ESP_ROM_EFUSE_H */
