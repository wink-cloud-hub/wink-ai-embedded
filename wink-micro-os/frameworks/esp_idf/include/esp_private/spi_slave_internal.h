/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_SPI_SLAVE_INTERNAL_H
#define WINK_H_GUARD_ESP_PRIVATE_SPI_SLAVE_INTERNAL_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_SPI_SLAVE_INTERNAL_H__
#define __WINK_HARVESTED_ESP_PRIVATE_SPI_SLAVE_INTERNAL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"

#include "esp_err.h"
#include "hal/spi_types.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
esp_err_t spi_slave_queue_reset(spi_host_device_t host) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_queue_reset out of Core 8 scope.");
#else
esp_err_t spi_slave_queue_reset(spi_host_device_t host);
#endif

#if defined(__WINK_SIM__)
esp_err_t spi_slave_queue_reset_isr(spi_host_device_t host) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_queue_reset_isr out of Core 8 scope.");
#else
esp_err_t spi_slave_queue_reset_isr(spi_host_device_t host);
#endif

#if defined(__WINK_SIM__)
esp_err_t spi_slave_queue_trans_isr(spi_host_device_t host, const spi_slave_transaction_t *trans_desc) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_queue_trans_isr out of Core 8 scope.");
#else
esp_err_t spi_slave_queue_trans_isr(spi_host_device_t host, const spi_slave_transaction_t *trans_desc);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_SPI_SLAVE_INTERNAL_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_SPI_SLAVE_INTERNAL_H */
