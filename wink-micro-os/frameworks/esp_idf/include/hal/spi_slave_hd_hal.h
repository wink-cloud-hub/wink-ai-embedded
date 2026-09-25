/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_SPI_SLAVE_HD_HAL_H
#define WINK_H_GUARD_HAL_SPI_SLAVE_HD_HAL_H
#ifndef __WINK_HARVESTED_HAL_SPI_SLAVE_HD_HAL_H__
#define __WINK_HARVESTED_HAL_SPI_SLAVE_HD_HAL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"

#include "esp_err.h"
#include "hal/spi_types.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef dma_descriptor_align4_t spi_dma_desc_t;
typedef struct {
    spi_dma_desc_t * desc;
    void * arg;
} spi_slave_hd_hal_desc_append_t;
typedef struct {
uint32_t      host_id;                          
    bool          dma_enabled;                      
    bool          append_mode;                      
    bool          three_wire_mode;                  
    uint32_t      spics_io_num;                     
    uint8_t       mode;                             
    uint32_t      command_bits;                     
    uint32_t      address_bits;                     
    uint32_t      dummy_bits;                       

    struct {
        uint32_t  tx_lsbfirst : 1;                  
        uint32_t  rx_lsbfirst : 1;                  
    };
} spi_slave_hd_hal_config_t;
typedef struct {
    spi_slave_hd_hal_desc_append_t * dmadesc_tx;
    spi_slave_hd_hal_desc_append_t * dmadesc_rx;
    spi_dev_t * dev;
    bool dma_enabled;
    bool append_mode;
    uint32_t dma_desc_num;
    uint32_t current_eof_addr;
    spi_slave_hd_hal_desc_append_t * tx_cur_desc;
    spi_slave_hd_hal_desc_append_t * tx_dma_head;
    spi_slave_hd_hal_desc_append_t * tx_dma_tail;
    uint32_t tx_used_desc_cnt;
    spi_slave_hd_hal_desc_append_t * rx_cur_desc;
    spi_slave_hd_hal_desc_append_t * rx_dma_head;
    spi_slave_hd_hal_desc_append_t * rx_dma_tail;
    uint32_t rx_used_desc_cnt;
    uint32_t intr_not_triggered;
} spi_slave_hd_hal_context_t;



#if defined(__WINK_SIM__)
bool spi_slave_hd_hal_check_clear_event(spi_slave_hd_hal_context_t* hal, spi_event_t ev) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hd_hal_check_clear_event out of Core 8 scope.");
#else
bool spi_slave_hd_hal_check_clear_event(spi_slave_hd_hal_context_t* hal, spi_event_t ev);
#endif

#if defined(__WINK_SIM__)
bool spi_slave_hd_hal_check_clear_intr(spi_slave_hd_hal_context_t *hal, uint32_t mask) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hd_hal_check_clear_intr out of Core 8 scope.");
#else
bool spi_slave_hd_hal_check_clear_intr(spi_slave_hd_hal_context_t *hal, uint32_t mask);
#endif

#if defined(__WINK_SIM__)
bool spi_slave_hd_hal_check_disable_event(spi_slave_hd_hal_context_t* hal, spi_event_t ev) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hd_hal_check_disable_event out of Core 8 scope.");
#else
bool spi_slave_hd_hal_check_disable_event(spi_slave_hd_hal_context_t* hal, spi_event_t ev);
#endif

#if defined(__WINK_SIM__)
void spi_slave_hd_hal_enable_event_intr(spi_slave_hd_hal_context_t* hal, spi_event_t ev) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hd_hal_enable_event_intr out of Core 8 scope.");
#else
void spi_slave_hd_hal_enable_event_intr(spi_slave_hd_hal_context_t* hal, spi_event_t ev);
#endif

#if defined(__WINK_SIM__)
int spi_slave_hd_hal_get_last_addr(spi_slave_hd_hal_context_t *hal) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hd_hal_get_last_addr out of Core 8 scope.");
#else
int spi_slave_hd_hal_get_last_addr(spi_slave_hd_hal_context_t *hal);
#endif

#if defined(__WINK_SIM__)
bool spi_slave_hd_hal_get_rx_finished_trans(spi_slave_hd_hal_context_t *hal, void **out_trans, void **real_buff_addr, size_t *out_len) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hd_hal_get_rx_finished_trans out of Core 8 scope.");
#else
bool spi_slave_hd_hal_get_rx_finished_trans(spi_slave_hd_hal_context_t *hal, void **out_trans, void **real_buff_addr, size_t *out_len);
#endif

#if defined(__WINK_SIM__)
int spi_slave_hd_hal_get_rxlen(spi_slave_hd_hal_context_t *hal) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hd_hal_get_rxlen out of Core 8 scope.");
#else
int spi_slave_hd_hal_get_rxlen(spi_slave_hd_hal_context_t *hal);
#endif

#if defined(__WINK_SIM__)
bool spi_slave_hd_hal_get_tx_finished_trans(spi_slave_hd_hal_context_t *hal, void **out_trans, void **real_buff_addr) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hd_hal_get_tx_finished_trans out of Core 8 scope.");
#else
bool spi_slave_hd_hal_get_tx_finished_trans(spi_slave_hd_hal_context_t *hal, void **out_trans, void **real_buff_addr);
#endif

#if defined(__WINK_SIM__)
void spi_slave_hd_hal_hw_prepare_rx(spi_slave_hd_hal_context_t *hal) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hd_hal_hw_prepare_rx out of Core 8 scope.");
#else
void spi_slave_hd_hal_hw_prepare_rx(spi_slave_hd_hal_context_t *hal);
#endif

#if defined(__WINK_SIM__)
void spi_slave_hd_hal_hw_prepare_tx(spi_slave_hd_hal_context_t *hal) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hd_hal_hw_prepare_tx out of Core 8 scope.");
#else
void spi_slave_hd_hal_hw_prepare_tx(spi_slave_hd_hal_context_t *hal);
#endif

#if defined(__WINK_SIM__)
void spi_slave_hd_hal_init(spi_slave_hd_hal_context_t *hal, const spi_slave_hd_hal_config_t *hal_config) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hd_hal_init out of Core 8 scope.");
#else
void spi_slave_hd_hal_init(spi_slave_hd_hal_context_t *hal, const spi_slave_hd_hal_config_t *hal_config);
#endif

#if defined(__WINK_SIM__)
void spi_slave_hd_hal_invoke_event_intr(spi_slave_hd_hal_context_t* hal, spi_event_t ev) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hd_hal_invoke_event_intr out of Core 8 scope.");
#else
void spi_slave_hd_hal_invoke_event_intr(spi_slave_hd_hal_context_t* hal, spi_event_t ev);
#endif

#if defined(__WINK_SIM__)
void spi_slave_hd_hal_read_buffer(spi_slave_hd_hal_context_t *hal, int addr, uint8_t *out_data, size_t len) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hd_hal_read_buffer out of Core 8 scope.");
#else
void spi_slave_hd_hal_read_buffer(spi_slave_hd_hal_context_t *hal, int addr, uint8_t *out_data, size_t len);
#endif

#if defined(__WINK_SIM__)
void spi_slave_hd_hal_rxdma(spi_slave_hd_hal_context_t *hal) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hd_hal_rxdma out of Core 8 scope.");
#else
void spi_slave_hd_hal_rxdma(spi_slave_hd_hal_context_t *hal);
#endif

#if defined(__WINK_SIM__)
esp_err_t spi_slave_hd_hal_rxdma_append(spi_slave_hd_hal_context_t *hal, uint8_t *data, size_t len, void *arg) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hd_hal_rxdma_append out of Core 8 scope.");
#else
esp_err_t spi_slave_hd_hal_rxdma_append(spi_slave_hd_hal_context_t *hal, uint8_t *data, size_t len, void *arg);
#endif

#if defined(__WINK_SIM__)
int spi_slave_hd_hal_rxdma_seg_get_len(spi_slave_hd_hal_context_t *hal) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hd_hal_rxdma_seg_get_len out of Core 8 scope.");
#else
int spi_slave_hd_hal_rxdma_seg_get_len(spi_slave_hd_hal_context_t *hal);
#endif

#if defined(__WINK_SIM__)
void spi_slave_hd_hal_txdma(spi_slave_hd_hal_context_t *hal) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hd_hal_txdma out of Core 8 scope.");
#else
void spi_slave_hd_hal_txdma(spi_slave_hd_hal_context_t *hal);
#endif

#if defined(__WINK_SIM__)
esp_err_t spi_slave_hd_hal_txdma_append(spi_slave_hd_hal_context_t *hal, uint8_t *data, size_t len, void *arg) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hd_hal_txdma_append out of Core 8 scope.");
#else
esp_err_t spi_slave_hd_hal_txdma_append(spi_slave_hd_hal_context_t *hal, uint8_t *data, size_t len, void *arg);
#endif

#if defined(__WINK_SIM__)
void spi_slave_hd_hal_write_buffer(spi_slave_hd_hal_context_t *hal, int addr, uint8_t *data, size_t len) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hd_hal_write_buffer out of Core 8 scope.");
#else
void spi_slave_hd_hal_write_buffer(spi_slave_hd_hal_context_t *hal, int addr, uint8_t *data, size_t len);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_SPI_SLAVE_HD_HAL_H__ */
#endif /* WINK_H_GUARD_HAL_SPI_SLAVE_HD_HAL_H */
