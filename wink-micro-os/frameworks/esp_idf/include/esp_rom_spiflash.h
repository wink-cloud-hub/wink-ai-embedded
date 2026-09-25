/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_ROM_SPIFLASH_H
#define WINK_H_GUARD_ESP_ROM_SPIFLASH_H
#ifndef __WINK_HARVESTED_ESP_ROM_SPIFLASH_H__
#define __WINK_HARVESTED_ESP_ROM_SPIFLASH_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stdint.h>

#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef g_rom_flashchip
#define g_rom_flashchip (rom_spiflash_legacy_data->chip)
#endif
#ifndef g_rom_spiflash_dummy_len_plus
#define g_rom_spiflash_dummy_len_plus (rom_spiflash_legacy_data->dummy_len_plus)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    ESP_ROM_SPIFLASH_QIO_MODE = 0,
    ESP_ROM_SPIFLASH_QOUT_MODE = 1,
    ESP_ROM_SPIFLASH_DIO_MODE = 2,
    ESP_ROM_SPIFLASH_DOUT_MODE = 3,
    ESP_ROM_SPIFLASH_FASTRD_MODE = 4,
    ESP_ROM_SPIFLASH_SLOWRD_MODE = 5,
    ESP_ROM_SPIFLASH_OPI_STR_MODE = 6,
    ESP_ROM_SPIFLASH_OPI_DTR_MODE = 7,
    ESP_ROM_SPIFLASH_OOUT_MODE = 8,
    ESP_ROM_SPIFLASH_OIO_STR_MODE = 9,
    ESP_ROM_SPIFLASH_OIO_DTR_MODE = 10,
    ESP_ROM_SPIFLASH_QPI_MODE = 11,
    ESP_ROM_SPIFLASH_OPI_HEX_DTR_MODE = 12,
} esp_rom_spiflash_read_mode_t;
typedef enum {
    ESP_ROM_SPIFLASH_RESULT_OK = 0,
    ESP_ROM_SPIFLASH_RESULT_ERR = 1,
    ESP_ROM_SPIFLASH_RESULT_TIMEOUT = 2,
} esp_rom_spiflash_result_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
    uint32_t device_id;
    uint32_t chip_size;
    uint32_t block_size;
    uint32_t sector_size;
    uint32_t page_size;
    uint32_t status_mask;
} esp_rom_spiflash_chip_t;
typedef struct {
    esp_rom_spiflash_chip_t chip;
    uint8_t dummy_len_plus[3];
    uint8_t sig_matrix;
} esp_rom_spiflash_legacy_data_t;



#if defined(__WINK_SIM__)
void esp_rom_spiflash_attach(uint32_t ishspi, bool legacy) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_spiflash_attach out of Core 8 scope.");
#else
void esp_rom_spiflash_attach(uint32_t ishspi, bool legacy);
#endif

#if defined(__WINK_SIM__)
esp_rom_spiflash_result_t esp_rom_spiflash_config_clk(uint8_t freqdiv, uint8_t spi) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_spiflash_config_clk out of Core 8 scope.");
#else
esp_rom_spiflash_result_t esp_rom_spiflash_config_clk(uint8_t freqdiv, uint8_t spi);
#endif

#if defined(__WINK_SIM__)
esp_rom_spiflash_result_t esp_rom_spiflash_config_param(uint32_t deviceId, uint32_t chip_size, uint32_t block_size,
                                                        uint32_t sector_size, uint32_t page_size, uint32_t status_mask) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_spiflash_config_param out of Core 8 scope.");
#else
esp_rom_spiflash_result_t esp_rom_spiflash_config_param(uint32_t deviceId, uint32_t chip_size, uint32_t block_size,
                                                        uint32_t sector_size, uint32_t page_size, uint32_t status_mask);
#endif

#if defined(__WINK_SIM__)
esp_rom_spiflash_result_t esp_rom_spiflash_config_readmode(esp_rom_spiflash_read_mode_t mode) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_spiflash_config_readmode out of Core 8 scope.");
#else
esp_rom_spiflash_result_t esp_rom_spiflash_config_readmode(esp_rom_spiflash_read_mode_t mode);
#endif

#if defined(__WINK_SIM__)
esp_rom_spiflash_result_t esp_rom_spiflash_erase_area(uint32_t start_addr, uint32_t area_len) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_spiflash_erase_area out of Core 8 scope.");
#else
esp_rom_spiflash_result_t esp_rom_spiflash_erase_area(uint32_t start_addr, uint32_t area_len);
#endif

#if defined(__WINK_SIM__)
esp_rom_spiflash_result_t esp_rom_spiflash_erase_block(uint32_t block_num) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_spiflash_erase_block out of Core 8 scope.");
#else
esp_rom_spiflash_result_t esp_rom_spiflash_erase_block(uint32_t block_num);
#endif

#if defined(__WINK_SIM__)
esp_rom_spiflash_result_t esp_rom_spiflash_erase_chip(void) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_spiflash_erase_chip out of Core 8 scope.");
#else
esp_rom_spiflash_result_t esp_rom_spiflash_erase_chip(void);
#endif

#if defined(__WINK_SIM__)
esp_rom_spiflash_result_t esp_rom_spiflash_erase_sector(uint32_t sector_num) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_spiflash_erase_sector out of Core 8 scope.");
#else
esp_rom_spiflash_result_t esp_rom_spiflash_erase_sector(uint32_t sector_num);
#endif

#if defined(__WINK_SIM__)
esp_rom_spiflash_result_t esp_rom_spiflash_prepare_encrypted_data(uint32_t flash_addr, uint32_t *data) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_spiflash_prepare_encrypted_data out of Core 8 scope.");
#else
esp_rom_spiflash_result_t esp_rom_spiflash_prepare_encrypted_data(uint32_t flash_addr, uint32_t *data);
#endif

#if defined(__WINK_SIM__)
esp_rom_spiflash_result_t esp_rom_spiflash_read(uint32_t src_addr, uint32_t *dest, int32_t len) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_spiflash_read out of Core 8 scope.");
#else
esp_rom_spiflash_result_t esp_rom_spiflash_read(uint32_t src_addr, uint32_t *dest, int32_t len);
#endif

#if defined(__WINK_SIM__)
esp_rom_spiflash_result_t esp_rom_spiflash_read_status(esp_rom_spiflash_chip_t *spi, uint32_t *status) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_spiflash_read_status out of Core 8 scope.");
#else
esp_rom_spiflash_result_t esp_rom_spiflash_read_status(esp_rom_spiflash_chip_t *spi, uint32_t *status);
#endif

#if defined(__WINK_SIM__)
esp_rom_spiflash_result_t esp_rom_spiflash_read_statushigh(esp_rom_spiflash_chip_t *spi, uint32_t *status) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_spiflash_read_statushigh out of Core 8 scope.");
#else
esp_rom_spiflash_result_t esp_rom_spiflash_read_statushigh(esp_rom_spiflash_chip_t *spi, uint32_t *status);
#endif

#if defined(__WINK_SIM__)
esp_rom_spiflash_result_t esp_rom_spiflash_read_user_cmd(uint32_t *status, uint8_t cmd) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_spiflash_read_user_cmd out of Core 8 scope.");
#else
esp_rom_spiflash_result_t esp_rom_spiflash_read_user_cmd(uint32_t *status, uint8_t cmd);
#endif

#if defined(__WINK_SIM__)
void esp_rom_spiflash_select_qio_pins(uint8_t wp_gpio_num, uint32_t spiconfig) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_spiflash_select_qio_pins out of Core 8 scope.");
#else
void esp_rom_spiflash_select_qio_pins(uint8_t wp_gpio_num, uint32_t spiconfig);
#endif

#if defined(__WINK_SIM__)
esp_rom_spiflash_result_t esp_rom_spiflash_wait_idle(esp_rom_spiflash_chip_t *spi) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_spiflash_wait_idle out of Core 8 scope.");
#else
esp_rom_spiflash_result_t esp_rom_spiflash_wait_idle(esp_rom_spiflash_chip_t *spi);
#endif

#if defined(__WINK_SIM__)
esp_rom_spiflash_result_t esp_rom_spiflash_write(uint32_t dest_addr, const uint32_t *src, int32_t len) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_spiflash_write out of Core 8 scope.");
#else
esp_rom_spiflash_result_t esp_rom_spiflash_write(uint32_t dest_addr, const uint32_t *src, int32_t len);
#endif

#if defined(__WINK_SIM__)
esp_rom_spiflash_result_t esp_rom_spiflash_write_disable(void) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_spiflash_write_disable out of Core 8 scope.");
#else
esp_rom_spiflash_result_t esp_rom_spiflash_write_disable(void);
#endif

#if defined(__WINK_SIM__)
esp_rom_spiflash_result_t esp_rom_spiflash_write_enable(esp_rom_spiflash_chip_t *spi) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_spiflash_write_enable out of Core 8 scope.");
#else
esp_rom_spiflash_result_t esp_rom_spiflash_write_enable(esp_rom_spiflash_chip_t *spi);
#endif

#if defined(__WINK_SIM__)
esp_rom_spiflash_result_t esp_rom_spiflash_write_encrypted(uint32_t flash_addr, uint32_t *data, uint32_t len) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_spiflash_write_encrypted out of Core 8 scope.");
#else
esp_rom_spiflash_result_t esp_rom_spiflash_write_encrypted(uint32_t flash_addr, uint32_t *data, uint32_t len);
#endif

#if defined(__WINK_SIM__)
void esp_rom_spiflash_write_encrypted_disable(void) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_spiflash_write_encrypted_disable out of Core 8 scope.");
#else
void esp_rom_spiflash_write_encrypted_disable(void);
#endif

#if defined(__WINK_SIM__)
void esp_rom_spiflash_write_encrypted_enable(void) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_spiflash_write_encrypted_enable out of Core 8 scope.");
#else
void esp_rom_spiflash_write_encrypted_enable(void);
#endif

#if defined(__WINK_SIM__)
esp_rom_spiflash_result_t esp_rom_spiflash_write_status(esp_rom_spiflash_chip_t *spi, uint32_t status_value) WINK_SLA_ERROR("Wink SLA Violation: esp_rom_spiflash_write_status out of Core 8 scope.");
#else
esp_rom_spiflash_result_t esp_rom_spiflash_write_status(esp_rom_spiflash_chip_t *spi, uint32_t status_value);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_ROM_SPIFLASH_H__ */
#endif /* WINK_H_GUARD_ESP_ROM_SPIFLASH_H */
