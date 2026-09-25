/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef _DRIVER_SPI_SLAVE_H_
#define _DRIVER_SPI_SLAVE_H_
#ifndef __WINK_HARVESTED_DRIVER_SPI_SLAVE_H__
#define __WINK_HARVESTED_DRIVER_SPI_SLAVE_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"

#include "driver/spi_common.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef SPI_SLAVE_BIT_LSBFIRST
#define SPI_SLAVE_BIT_LSBFIRST (SPI_SLAVE_TXBIT_LSBFIRST|SPI_SLAVE_RXBIT_LSBFIRST)
#endif
#ifndef SPI_SLAVE_NO_RETURN_RESULT
#define SPI_SLAVE_NO_RETURN_RESULT (1<<2)
#endif
#ifndef SPI_SLAVE_RXBIT_LSBFIRST
#define SPI_SLAVE_RXBIT_LSBFIRST (1<<1)
#endif
#ifndef SPI_SLAVE_TRANS_DMA_BUFFER_ALIGN_AUTO
#define SPI_SLAVE_TRANS_DMA_BUFFER_ALIGN_AUTO (1<<0)
#endif
#ifndef SPI_SLAVE_TXBIT_LSBFIRST
#define SPI_SLAVE_TXBIT_LSBFIRST (1<<0)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct spi_slave_transaction_t spi_slave_transaction_t;
typedef void(*slave_transaction_cb_t)(spi_slave_transaction_t *trans);
typedef struct {
    int spics_io_num;
    uint32_t flags;
    int queue_size;
    uint8_t mode;
    slave_transaction_cb_t post_setup_cb;
    slave_transaction_cb_t post_trans_cb;
} spi_slave_interface_config_t;
struct spi_slave_transaction_t {
    uint32_t flags;
    size_t length;
    size_t tx_length;
    size_t rx_length;
    size_t trans_len;
    const void * tx_buffer;
    void * rx_buffer;
    void * user;
};



#if defined(__WINK_SIM__)
esp_err_t spi_slave_disable(spi_host_device_t host) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_disable out of Core 8 scope.");
#else
esp_err_t spi_slave_disable(spi_host_device_t host);
#endif

#if defined(__WINK_SIM__)
esp_err_t spi_slave_enable(spi_host_device_t host) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_enable out of Core 8 scope.");
#else
esp_err_t spi_slave_enable(spi_host_device_t host);
#endif

#if defined(__WINK_SIM__)
esp_err_t spi_slave_free(spi_host_device_t host) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_free out of Core 8 scope.");
#else
esp_err_t spi_slave_free(spi_host_device_t host);
#endif

#if defined(__WINK_SIM__)
esp_err_t spi_slave_get_trans_result(spi_host_device_t host, spi_slave_transaction_t **trans_desc, uint32_t ticks_to_wait) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_get_trans_result out of Core 8 scope.");
#else
esp_err_t spi_slave_get_trans_result(spi_host_device_t host, spi_slave_transaction_t **trans_desc, uint32_t ticks_to_wait);
#endif

#if defined(__WINK_SIM__)
esp_err_t spi_slave_initialize(spi_host_device_t host, const spi_bus_config_t *bus_config, const spi_slave_interface_config_t *slave_config, spi_dma_chan_t dma_chan) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_initialize out of Core 8 scope.");
#else
esp_err_t spi_slave_initialize(spi_host_device_t host, const spi_bus_config_t *bus_config, const spi_slave_interface_config_t *slave_config, spi_dma_chan_t dma_chan);
#endif

#if defined(__WINK_SIM__)
esp_err_t spi_slave_queue_trans(spi_host_device_t host, const spi_slave_transaction_t *trans_desc, uint32_t ticks_to_wait) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_queue_trans out of Core 8 scope.");
#else
esp_err_t spi_slave_queue_trans(spi_host_device_t host, const spi_slave_transaction_t *trans_desc, uint32_t ticks_to_wait);
#endif

#if defined(__WINK_SIM__)
esp_err_t spi_slave_transmit(spi_host_device_t host, spi_slave_transaction_t *trans_desc, uint32_t ticks_to_wait) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_transmit out of Core 8 scope.");
#else
esp_err_t spi_slave_transmit(spi_host_device_t host, spi_slave_transaction_t *trans_desc, uint32_t ticks_to_wait);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_DRIVER_SPI_SLAVE_H__ */
#endif /* _DRIVER_SPI_SLAVE_H_ */
