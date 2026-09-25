/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_DRIVER_SPI_COMMON_H
#define WINK_H_GUARD_DRIVER_SPI_COMMON_H
#ifndef __WINK_HARVESTED_DRIVER_SPI_COMMON_H__
#define __WINK_HARVESTED_DRIVER_SPI_COMMON_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_intr_types.h"
#include "esp_ipc.h"
#include "hal/spi_types.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef SPICOMMON_BUSFLAG_DUAL
#define SPICOMMON_BUSFLAG_DUAL (1<<6)
#endif
#ifndef SPICOMMON_BUSFLAG_GPIO_PINS
#define SPICOMMON_BUSFLAG_GPIO_PINS (1<<2)
#endif
#ifndef SPICOMMON_BUSFLAG_IO4_IO7
#define SPICOMMON_BUSFLAG_IO4_IO7 (1<<8)
#endif
#ifndef SPICOMMON_BUSFLAG_IOMUX_PINS
#define SPICOMMON_BUSFLAG_IOMUX_PINS (1<<1)
#endif
#ifndef SPICOMMON_BUSFLAG_MASTER
#define SPICOMMON_BUSFLAG_MASTER (1<<0)
#endif
#ifndef SPICOMMON_BUSFLAG_MISO
#define SPICOMMON_BUSFLAG_MISO (1<<4)
#endif
#ifndef SPICOMMON_BUSFLAG_MOSI
#define SPICOMMON_BUSFLAG_MOSI (1<<5)
#endif
#ifndef SPICOMMON_BUSFLAG_NATIVE_PINS
#define SPICOMMON_BUSFLAG_NATIVE_PINS SPICOMMON_BUSFLAG_IOMUX_PINS
#endif
#ifndef SPICOMMON_BUSFLAG_OCTAL
#define SPICOMMON_BUSFLAG_OCTAL (SPICOMMON_BUSFLAG_QUAD|SPICOMMON_BUSFLAG_IO4_IO7)
#endif
#ifndef SPICOMMON_BUSFLAG_QUAD
#define SPICOMMON_BUSFLAG_QUAD (SPICOMMON_BUSFLAG_DUAL|SPICOMMON_BUSFLAG_WPHD)
#endif
#ifndef SPICOMMON_BUSFLAG_SCLK
#define SPICOMMON_BUSFLAG_SCLK (1<<3)
#endif
#ifndef SPICOMMON_BUSFLAG_SLAVE
#define SPICOMMON_BUSFLAG_SLAVE 0
#endif
#ifndef SPICOMMON_BUSFLAG_SLP_ALLOW_PD
#define SPICOMMON_BUSFLAG_SLP_ALLOW_PD (1<<9)
#endif
#ifndef SPICOMMON_BUSFLAG_WPHD
#define SPICOMMON_BUSFLAG_WPHD (1<<7)
#endif
#ifndef SPI_MAX_DMA_LEN
#define SPI_MAX_DMA_LEN (4096-4)
#endif
#ifndef SPI_SWAP_DATA_RX
#define SPI_SWAP_DATA_RX(DATA, LEN) (__builtin_bswap32(DATA)>>(32-(LEN)))
#endif
#ifndef SPI_SWAP_DATA_TX
#define SPI_SWAP_DATA_TX(DATA, LEN) __builtin_bswap32((uint32_t)(DATA)<<(32-(LEN)))
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
union {
        struct {
            union {
                int mosi_io_num;    
                int data0_io_num;   
            };
            union {
                int miso_io_num;    
                int data1_io_num;   
            };
            int sclk_io_num;        
            union {
                int quadwp_io_num;  
                int data2_io_num;   
            };
            union {
                int quadhd_io_num;  
                int data3_io_num;   
            };
            int data4_io_num;       
            int data5_io_num;       
            int data6_io_num;       
            int data7_io_num;       
        };
        int iocfg[9];               
    };
    bool data_io_default_level; 
    int max_transfer_sz;  
    uint32_t dma_burst_size; 
    uint32_t flags;       
    esp_intr_cpu_affinity_t  isr_cpu_id;    
    int intr_flags;
} spi_bus_config_t;

esp_err_t spi_bus_free(spi_host_device_t host_id);
esp_err_t spi_bus_initialize(spi_host_device_t host_id, const spi_bus_config_t *bus_config, spi_dma_chan_t dma_chan);


#if defined(__WINK_SIM__)
void * spi_bus_dma_memory_alloc(spi_host_device_t host_id, size_t size, uint32_t extra_heap_caps) WINK_SLA_ERROR("Wink SLA Violation: spi_bus_dma_memory_alloc out of Core 8 scope.");
#else
void * spi_bus_dma_memory_alloc(spi_host_device_t host_id, size_t size, uint32_t extra_heap_caps);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_DRIVER_SPI_COMMON_H__ */
#endif /* WINK_H_GUARD_DRIVER_SPI_COMMON_H */
