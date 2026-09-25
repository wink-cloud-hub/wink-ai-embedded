/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_ROM_REGI2C_H
#define WINK_H_GUARD_ESP_ROM_REGI2C_H
#ifndef __WINK_HARVESTED_ESP_ROM_REGI2C_H__
#define __WINK_HARVESTED_ESP_ROM_REGI2C_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
uint8_t esp_rom_regi2c_read(uint8_t block, uint8_t host_id, uint8_t reg_add) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_regi2c_read out of Core 8 scope.");
#else
uint8_t esp_rom_regi2c_read(uint8_t block, uint8_t host_id, uint8_t reg_add);
#endif

#if defined(__WINK_SIM__)
uint8_t esp_rom_regi2c_read_mask(uint8_t block, uint8_t host_id, uint8_t reg_add, uint8_t msb, uint8_t lsb) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_regi2c_read_mask out of Core 8 scope.");
#else
uint8_t esp_rom_regi2c_read_mask(uint8_t block, uint8_t host_id, uint8_t reg_add, uint8_t msb, uint8_t lsb);
#endif

#if defined(__WINK_SIM__)
void esp_rom_regi2c_write(uint8_t block, uint8_t host_id, uint8_t reg_add, uint8_t data) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_regi2c_write out of Core 8 scope.");
#else
void esp_rom_regi2c_write(uint8_t block, uint8_t host_id, uint8_t reg_add, uint8_t data);
#endif

#if defined(__WINK_SIM__)
void esp_rom_regi2c_write_mask(uint8_t block, uint8_t host_id, uint8_t reg_add, uint8_t msb, uint8_t lsb, uint8_t data) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_regi2c_write_mask out of Core 8 scope.");
#else
void esp_rom_regi2c_write_mask(uint8_t block, uint8_t host_id, uint8_t reg_add, uint8_t msb, uint8_t lsb, uint8_t data);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_ROM_REGI2C_H__ */
#endif /* WINK_H_GUARD_ESP_ROM_REGI2C_H */
