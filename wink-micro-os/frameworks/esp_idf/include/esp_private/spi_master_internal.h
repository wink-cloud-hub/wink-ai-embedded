/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_SPI_MASTER_INTERNAL_H
#define WINK_H_GUARD_ESP_PRIVATE_SPI_MASTER_INTERNAL_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_SPI_MASTER_INTERNAL_H__
#define __WINK_HARVESTED_ESP_PRIVATE_SPI_MASTER_INTERNAL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef SPI_MULTI_TRANS_ADDR_LEN_UPDATED
#define SPI_MULTI_TRANS_ADDR_LEN_UPDATED (1<<2)
#endif
#ifndef SPI_MULTI_TRANS_CMD_LEN_UPDATED
#define SPI_MULTI_TRANS_CMD_LEN_UPDATED (1<<1)
#endif
#ifndef SPI_MULTI_TRANS_DONE_LEN_UPDATED
#define SPI_MULTI_TRANS_DONE_LEN_UPDATED (1<<4)
#endif
#ifndef SPI_MULTI_TRANS_DUMMY_LEN_UPDATED
#define SPI_MULTI_TRANS_DUMMY_LEN_UPDATED (1<<3)
#endif
#ifndef SPI_MULTI_TRANS_PREP_LEN_UPDATED
#define SPI_MULTI_TRANS_PREP_LEN_UPDATED (1<<0)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
    struct spi_transaction_t base;
    uint8_t cs_ena_pretrans;
    uint8_t cs_ena_posttrans;
    uint8_t command_bits;
    uint8_t address_bits;
    uint8_t dummy_bits;
    uint32_t sct_gap_len;
    uint32_t seg_trans_flags;
} spi_multi_transaction_t;



#if defined(__WINK_SIM__)
esp_err_t spi_bus_multi_trans_mode_enable(spi_device_handle_t handle, bool enable) WINK_SLA_ERROR("Wink SLA Violation: spi_bus_multi_trans_mode_enable out of Core 8 scope.");
#else
esp_err_t spi_bus_multi_trans_mode_enable(spi_device_handle_t handle, bool enable);
#endif

#if defined(__WINK_SIM__)
esp_err_t spi_device_get_multi_trans_result(spi_device_handle_t handle, spi_multi_transaction_t **seg_trans_desc, uint32_t ticks_to_wait) WINK_SLA_ERROR("Wink SLA Violation: spi_device_get_multi_trans_result out of Core 8 scope.");
#else
esp_err_t spi_device_get_multi_trans_result(spi_device_handle_t handle, spi_multi_transaction_t **seg_trans_desc, uint32_t ticks_to_wait);
#endif

#if defined(__WINK_SIM__)
esp_err_t spi_device_queue_multi_trans(spi_device_handle_t handle, spi_multi_transaction_t *seg_trans_desc, uint32_t trans_num, uint32_t ticks_to_wait) WINK_SLA_ERROR("Wink SLA Violation: spi_device_queue_multi_trans out of Core 8 scope.");
#else
esp_err_t spi_device_queue_multi_trans(spi_device_handle_t handle, spi_multi_transaction_t *seg_trans_desc, uint32_t trans_num, uint32_t ticks_to_wait);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_SPI_MASTER_INTERNAL_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_SPI_MASTER_INTERNAL_H */
