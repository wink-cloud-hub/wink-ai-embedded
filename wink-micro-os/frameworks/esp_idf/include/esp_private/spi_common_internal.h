/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_SPI_COMMON_INTERNAL_H
#define WINK_H_GUARD_ESP_PRIVATE_SPI_COMMON_INTERNAL_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_SPI_COMMON_INTERNAL_H__
#define __WINK_HARVESTED_ESP_PRIVATE_SPI_COMMON_INTERNAL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <esp_intr_alloc.h>

#include "driver/spi_common.h"
#include "hal/spi_types.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef ADDR_CPU_2_DMA
#define ADDR_CPU_2_DMA(addr) ((typeof(addr))CACHE_LL_L2MEM_CACHE_ADDR(addr))
#endif
#ifndef ADDR_DMA_2_CPU
#define ADDR_DMA_2_CPU(addr) ((typeof(addr))CACHE_LL_L2MEM_NON_CACHE_ADDR(addr))
#endif
#ifndef DMA_DESC_MEM_ALIGN_SIZE
#define DMA_DESC_MEM_ALIGN_SIZE 8
#endif
#ifndef SPI_ALIGN_UP
#define SPI_ALIGN_UP(num, align) (((num) + ((align) - 1)) & ~((align) - 1))
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    SPI_BUS_FSM_DISABLED = 0,
    SPI_BUS_FSM_ENABLED = 1,
} spi_bus_fsm_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef dma_descriptor_align8_t spi_dma_desc_t;
typedef struct {
    spi_bus_config_t bus_cfg;
    uint64_t gpio_reserve;
    uint32_t flags;
    int max_transfer_sz;
    bool dma_enabled;
    size_t cache_align_int;
    size_t cache_align_ext;
    spi_bus_lock_handle_t lock;
    esp_pm_lock_handle_t pm_lock;
} spi_bus_attr_t;
typedef struct {
    gdma_channel_handle_t tx_dma_chan;
    gdma_channel_handle_t rx_dma_chan;
    spi_dma_chan_handle_t tx_dma_chan;
    spi_dma_chan_handle_t rx_dma_chan;
    size_t dma_align_tx_int;
    size_t dma_align_tx_ext;
    size_t dma_align_rx_int;
    size_t dma_align_rx_ext;
    int dma_desc_num;
    spi_dma_desc_t * dmadesc_tx;
    spi_dma_desc_t * dmadesc_rx;
} spi_dma_ctx_t;
typedef esp_err_t (*spi_destroy_func_t)(void*);
typedef void(*dmaworkaround_cb_t)(void *arg);



#if defined(__WINK_SIM__)
typedef esp_err_t(*spi_destroy_func_t)(void*) WINK_SLA_ERROR("Wink SLA Violation: esp_err_t out of Core 8 scope.");
#else
typedef esp_err_t(*spi_destroy_func_t)(void*);
#endif

#if defined(__WINK_SIM__)
spi_bus_attr_t* spi_bus_get_attr(spi_host_device_t host_id) WINK_SLA_ERROR("Wink SLA Violation: spi_bus_get_attr out of Core 8 scope.");
#else
spi_bus_attr_t* spi_bus_get_attr(spi_host_device_t host_id);
#endif

#if defined(__WINK_SIM__)
spi_dma_ctx_t* spi_bus_get_dma_ctx(spi_host_device_t host_id) WINK_SLA_ERROR("Wink SLA Violation: spi_bus_get_dma_ctx out of Core 8 scope.");
#else
spi_dma_ctx_t* spi_bus_get_dma_ctx(spi_host_device_t host_id);
#endif

#if defined(__WINK_SIM__)
esp_err_t spi_bus_register_destroy_func(spi_host_device_t host_id, spi_destroy_func_t f, void *arg) WINK_SLA_ERROR("Wink SLA Violation: spi_bus_register_destroy_func out of Core 8 scope.");
#else
esp_err_t spi_bus_register_destroy_func(spi_host_device_t host_id, spi_destroy_func_t f, void *arg);
#endif

#if defined(__WINK_SIM__)
esp_err_t spicommon_bus_alloc(spi_host_device_t host_id, const char *name) WINK_SLA_ERROR("Wink SLA Violation: spicommon_bus_alloc out of Core 8 scope.");
#else
esp_err_t spicommon_bus_alloc(spi_host_device_t host_id, const char *name);
#endif

#if defined(__WINK_SIM__)
esp_err_t spicommon_bus_free(spi_host_device_t host_id) WINK_SLA_ERROR("Wink SLA Violation: spicommon_bus_free out of Core 8 scope.");
#else
esp_err_t spicommon_bus_free(spi_host_device_t host_id);
#endif

#if defined(__WINK_SIM__)
esp_err_t spicommon_bus_free_io_cfg(spi_host_device_t host) WINK_SLA_ERROR("Wink SLA Violation: spicommon_bus_free_io_cfg out of Core 8 scope.");
#else
esp_err_t spicommon_bus_free_io_cfg(spi_host_device_t host);
#endif

#if defined(__WINK_SIM__)
esp_err_t spicommon_bus_initialize_io(spi_host_device_t host, const spi_bus_config_t *bus_config, uint32_t flags, uint32_t *flags_o) WINK_SLA_ERROR("Wink SLA Violation: spicommon_bus_initialize_io out of Core 8 scope.");
#else
esp_err_t spicommon_bus_initialize_io(spi_host_device_t host, const spi_bus_config_t *bus_config, uint32_t flags, uint32_t *flags_o);
#endif

#if defined(__WINK_SIM__)
void spicommon_cs_free_io(int cs_gpio_num, uint64_t *io_reserved) WINK_SLA_ERROR("Wink SLA Violation: spicommon_cs_free_io out of Core 8 scope.");
#else
void spicommon_cs_free_io(int cs_gpio_num, uint64_t *io_reserved);
#endif

#if defined(__WINK_SIM__)
void spicommon_cs_initialize(spi_host_device_t host, int cs_io_num, int cs_id, int force_gpio_matrix, uint64_t *io_reserved) WINK_SLA_ERROR("Wink SLA Violation: spicommon_cs_initialize out of Core 8 scope.");
#else
void spicommon_cs_initialize(spi_host_device_t host, int cs_io_num, int cs_id, int force_gpio_matrix, uint64_t *io_reserved);
#endif

#if defined(__WINK_SIM__)
esp_err_t spicommon_dma_chan_alloc(spi_host_device_t host_id, spi_dma_chan_t dma_chan, uint32_t dma_burst_size) WINK_SLA_ERROR("Wink SLA Violation: spicommon_dma_chan_alloc out of Core 8 scope.");
#else
esp_err_t spicommon_dma_chan_alloc(spi_host_device_t host_id, spi_dma_chan_t dma_chan, uint32_t dma_burst_size);
#endif

#if defined(__WINK_SIM__)
esp_err_t spicommon_dma_chan_free(spi_host_device_t host_id) WINK_SLA_ERROR("Wink SLA Violation: spicommon_dma_chan_free out of Core 8 scope.");
#else
esp_err_t spicommon_dma_chan_free(spi_host_device_t host_id);
#endif

#if defined(__WINK_SIM__)
esp_err_t spicommon_dma_desc_alloc(spi_host_device_t host_id, int cfg_max_sz, int *actual_max_sz) WINK_SLA_ERROR("Wink SLA Violation: spicommon_dma_desc_alloc out of Core 8 scope.");
#else
esp_err_t spicommon_dma_desc_alloc(spi_host_device_t host_id, int cfg_max_sz, int *actual_max_sz);
#endif

#if defined(__WINK_SIM__)
void spicommon_dma_desc_setup_link(spi_dma_desc_t *dmadesc, const void *data, int len, bool is_rx) WINK_SLA_ERROR("Wink SLA Violation: spicommon_dma_desc_setup_link out of Core 8 scope.");
#else
void spicommon_dma_desc_setup_link(spi_dma_desc_t *dmadesc, const void *data, int len, bool is_rx);
#endif

#if defined(__WINK_SIM__)
void spicommon_dma_rx_mb(spi_host_device_t host_id, void *rx_buffer) WINK_SLA_ERROR("Wink SLA Violation: spicommon_dma_rx_mb out of Core 8 scope.");
#else
void spicommon_dma_rx_mb(spi_host_device_t host_id, void *rx_buffer);
#endif

#if defined(__WINK_SIM__)
esp_err_t spicommon_dma_setup_priv_buffer(spi_host_device_t host_id, uint32_t *buffer, uint32_t len, bool is_tx, bool psram_prefer, bool auto_malloc, uint32_t **ret_buffer) WINK_SLA_ERROR("Wink SLA Violation: spicommon_dma_setup_priv_buffer out of Core 8 scope.");
#else
esp_err_t spicommon_dma_setup_priv_buffer(spi_host_device_t host_id, uint32_t *buffer, uint32_t len, bool is_tx, bool psram_prefer, bool auto_malloc, uint32_t **ret_buffer);
#endif

#if defined(__WINK_SIM__)
void spicommon_dmaworkaround_idle(int dmachan) WINK_SLA_ERROR("Wink SLA Violation: spicommon_dmaworkaround_idle out of Core 8 scope.");
#else
void spicommon_dmaworkaround_idle(int dmachan);
#endif

#if defined(__WINK_SIM__)
bool spicommon_dmaworkaround_req_reset(int dmachan, dmaworkaround_cb_t cb, void *arg) WINK_SLA_ERROR("Wink SLA Violation: spicommon_dmaworkaround_req_reset out of Core 8 scope.");
#else
bool spicommon_dmaworkaround_req_reset(int dmachan, dmaworkaround_cb_t cb, void *arg);
#endif

#if defined(__WINK_SIM__)
bool spicommon_dmaworkaround_reset_in_progress(void) WINK_SLA_ERROR("Wink SLA Violation: spicommon_dmaworkaround_reset_in_progress out of Core 8 scope.");
#else
bool spicommon_dmaworkaround_reset_in_progress(void);
#endif

#if defined(__WINK_SIM__)
void spicommon_dmaworkaround_transfer_active(int dmachan) WINK_SLA_ERROR("Wink SLA Violation: spicommon_dmaworkaround_transfer_active out of Core 8 scope.");
#else
void spicommon_dmaworkaround_transfer_active(int dmachan);
#endif

#if defined(__WINK_SIM__)
int spicommon_irqdma_source_for_host(spi_host_device_t host) WINK_SLA_ERROR("Wink SLA Violation: spicommon_irqdma_source_for_host out of Core 8 scope.");
#else
int spicommon_irqdma_source_for_host(spi_host_device_t host);
#endif

#if defined(__WINK_SIM__)
int spicommon_irqsource_for_host(spi_host_device_t host) WINK_SLA_ERROR("Wink SLA Violation: spicommon_irqsource_for_host out of Core 8 scope.");
#else
int spicommon_irqsource_for_host(spi_host_device_t host);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_SPI_COMMON_INTERNAL_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_SPI_COMMON_INTERNAL_H */
