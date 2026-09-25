/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_DRIVER_SPI_SLAVE_HD_H
#define WINK_H_GUARD_DRIVER_SPI_SLAVE_HD_H
#ifndef __WINK_HARVESTED_DRIVER_SPI_SLAVE_HD_H__
#define __WINK_HARVESTED_DRIVER_SPI_SLAVE_HD_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"

#include "driver/spi_common.h"
#include "hal/spi_types.h"
#include "sdkconfig.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef SPI_SLAVE_HD_3WIRE_MODE
#define SPI_SLAVE_HD_3WIRE_MODE (1<<3)
#endif
#ifndef SPI_SLAVE_HD_APPEND_MODE
#define SPI_SLAVE_HD_APPEND_MODE (1<<2)
#endif
#ifndef SPI_SLAVE_HD_BIT_LSBFIRST
#define SPI_SLAVE_HD_BIT_LSBFIRST (SPI_SLAVE_HD_TXBIT_LSBFIRST|SPI_SLAVE_HD_RXBIT_LSBFIRST)
#endif
#ifndef SPI_SLAVE_HD_RXBIT_LSBFIRST
#define SPI_SLAVE_HD_RXBIT_LSBFIRST (1<<1)
#endif
#ifndef SPI_SLAVE_HD_TRANS_DMA_BUFFER_ALIGN_AUTO
#define SPI_SLAVE_HD_TRANS_DMA_BUFFER_ALIGN_AUTO (1<<0)
#endif
#ifndef SPI_SLAVE_HD_TXBIT_LSBFIRST
#define SPI_SLAVE_HD_TXBIT_LSBFIRST (1<<0)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    SPI_SLAVE_CHAN_TX = 0,
    SPI_SLAVE_CHAN_RX = 1,
} spi_slave_chan_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
    uint8_t* data;
    size_t len;
    size_t trans_len;
    uint32_t flags;
    void* arg;
} spi_slave_hd_data_t;
typedef struct {
    spi_event_t event;
    spi_slave_hd_data_t* trans;
} spi_slave_hd_event_t;
typedef bool (*slave_cb_t)(void* arg, spi_slave_hd_event_t* event, int* awoken);
typedef struct {
    slave_cb_t cb_buffer_tx;
    slave_cb_t cb_buffer_rx;
    slave_cb_t cb_send_dma_ready;
    slave_cb_t cb_sent;
    slave_cb_t cb_recv_dma_ready;
    slave_cb_t cb_recv;
    slave_cb_t cb_cmd9;
    slave_cb_t cb_cmdA;
    void* arg;
} spi_slave_hd_callback_config_t;
typedef struct {
    uint8_t mode;
    uint32_t spics_io_num;
    uint32_t flags;
    uint32_t command_bits;
    uint32_t address_bits;
    uint32_t dummy_bits;
    uint32_t queue_size;
    spi_dma_chan_t dma_chan;
    spi_slave_hd_callback_config_t cb_config;
} spi_slave_hd_slot_config_t;



#if defined(__WINK_SIM__)
typedef bool(*slave_cb_t)(void* arg, spi_slave_hd_event_t* event, int* awoken) WINK_SLA_ERROR("Wink SLA Violation: bool out of Core 8 scope.");
#else
typedef bool(*slave_cb_t)(void* arg, spi_slave_hd_event_t* event, int* awoken);
#endif

#if defined(__WINK_SIM__)
esp_err_t spi_slave_hd_append_trans(spi_host_device_t host_id, spi_slave_chan_t chan, spi_slave_hd_data_t *trans, uint32_t timeout) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hd_append_trans out of Core 8 scope.");
#else
esp_err_t spi_slave_hd_append_trans(spi_host_device_t host_id, spi_slave_chan_t chan, spi_slave_hd_data_t *trans, uint32_t timeout);
#endif

#if defined(__WINK_SIM__)
esp_err_t spi_slave_hd_deinit(spi_host_device_t host_id) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hd_deinit out of Core 8 scope.");
#else
esp_err_t spi_slave_hd_deinit(spi_host_device_t host_id);
#endif

#if defined(__WINK_SIM__)
esp_err_t spi_slave_hd_disable(spi_host_device_t host_id) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hd_disable out of Core 8 scope.");
#else
esp_err_t spi_slave_hd_disable(spi_host_device_t host_id);
#endif

#if defined(__WINK_SIM__)
esp_err_t spi_slave_hd_enable(spi_host_device_t host_id) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hd_enable out of Core 8 scope.");
#else
esp_err_t spi_slave_hd_enable(spi_host_device_t host_id);
#endif

#if defined(__WINK_SIM__)
esp_err_t spi_slave_hd_get_append_trans_res(spi_host_device_t host_id, spi_slave_chan_t chan, spi_slave_hd_data_t **out_trans, uint32_t timeout) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hd_get_append_trans_res out of Core 8 scope.");
#else
esp_err_t spi_slave_hd_get_append_trans_res(spi_host_device_t host_id, spi_slave_chan_t chan, spi_slave_hd_data_t **out_trans, uint32_t timeout);
#endif

#if defined(__WINK_SIM__)
esp_err_t spi_slave_hd_get_trans_res(spi_host_device_t host_id, spi_slave_chan_t chan, spi_slave_hd_data_t **out_trans, uint32_t timeout) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hd_get_trans_res out of Core 8 scope.");
#else
esp_err_t spi_slave_hd_get_trans_res(spi_host_device_t host_id, spi_slave_chan_t chan, spi_slave_hd_data_t **out_trans, uint32_t timeout);
#endif

#if defined(__WINK_SIM__)
esp_err_t spi_slave_hd_init(spi_host_device_t host_id, const spi_bus_config_t *bus_config,
                            const spi_slave_hd_slot_config_t *config) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hd_init out of Core 8 scope.");
#else
esp_err_t spi_slave_hd_init(spi_host_device_t host_id, const spi_bus_config_t *bus_config,
                            const spi_slave_hd_slot_config_t *config);
#endif

#if defined(__WINK_SIM__)
esp_err_t spi_slave_hd_queue_trans(spi_host_device_t host_id, spi_slave_chan_t chan, spi_slave_hd_data_t* trans, uint32_t timeout) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hd_queue_trans out of Core 8 scope.");
#else
esp_err_t spi_slave_hd_queue_trans(spi_host_device_t host_id, spi_slave_chan_t chan, spi_slave_hd_data_t* trans, uint32_t timeout);
#endif

#if defined(__WINK_SIM__)
void spi_slave_hd_read_buffer(spi_host_device_t host_id, int addr, uint8_t *out_data, size_t len) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hd_read_buffer out of Core 8 scope.");
#else
void spi_slave_hd_read_buffer(spi_host_device_t host_id, int addr, uint8_t *out_data, size_t len);
#endif

#if defined(__WINK_SIM__)
void spi_slave_hd_write_buffer(spi_host_device_t host_id, int addr, uint8_t *data, size_t len) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hd_write_buffer out of Core 8 scope.");
#else
void spi_slave_hd_write_buffer(spi_host_device_t host_id, int addr, uint8_t *data, size_t len);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_DRIVER_SPI_SLAVE_HD_H__ */
#endif /* WINK_H_GUARD_DRIVER_SPI_SLAVE_HD_H */
