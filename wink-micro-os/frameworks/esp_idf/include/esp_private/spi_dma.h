/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_SPI_DMA_H
#define WINK_H_GUARD_ESP_PRIVATE_SPI_DMA_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_SPI_DMA_H__
#define __WINK_HARVESTED_ESP_PRIVATE_SPI_DMA_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stddef.h>

#include "hal/spi_types.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    DMA_CHANNEL_DIRECTION_TX = 0,
    DMA_CHANNEL_DIRECTION_RX = 1,
} spi_dma_chan_dir_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
    spi_host_device_t host_id;
    spi_dma_chan_dir_t dir;
    int chan_id;
} spi_dma_chan_handle_t;



#if defined(__WINK_SIM__)
void spi_dma_append(spi_dma_chan_handle_t chan_handle) WINK_SLA_ERROR("Wink SLA Violation: spi_dma_append out of Core 8 scope.");
#else
void spi_dma_append(spi_dma_chan_handle_t chan_handle);
#endif

#if defined(__WINK_SIM__)
void spi_dma_enable_burst(spi_dma_chan_handle_t chan_handle, bool data_burst, bool desc_burst) WINK_SLA_ERROR("Wink SLA Violation: spi_dma_enable_burst out of Core 8 scope.");
#else
void spi_dma_enable_burst(spi_dma_chan_handle_t chan_handle, bool data_burst, bool desc_burst);
#endif

#if defined(__WINK_SIM__)
void spi_dma_get_alignment_constraints(spi_dma_chan_handle_t chan_handle, size_t *internal_size, size_t *external_size) WINK_SLA_ERROR("Wink SLA Violation: spi_dma_get_alignment_constraints out of Core 8 scope.");
#else
void spi_dma_get_alignment_constraints(spi_dma_chan_handle_t chan_handle, size_t *internal_size, size_t *external_size);
#endif

#if defined(__WINK_SIM__)
uint32_t spi_dma_get_eof_desc(spi_dma_chan_handle_t chan_handle) WINK_SLA_ERROR("Wink SLA Violation: spi_dma_get_eof_desc out of Core 8 scope.");
#else
uint32_t spi_dma_get_eof_desc(spi_dma_chan_handle_t chan_handle);
#endif

#if defined(__WINK_SIM__)
void spi_dma_reset(spi_dma_chan_handle_t chan_handle) WINK_SLA_ERROR("Wink SLA Violation: spi_dma_reset out of Core 8 scope.");
#else
void spi_dma_reset(spi_dma_chan_handle_t chan_handle);
#endif

#if defined(__WINK_SIM__)
void spi_dma_start(spi_dma_chan_handle_t chan_handle, void *addr) WINK_SLA_ERROR("Wink SLA Violation: spi_dma_start out of Core 8 scope.");
#else
void spi_dma_start(spi_dma_chan_handle_t chan_handle, void *addr);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_SPI_DMA_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_SPI_DMA_H */
