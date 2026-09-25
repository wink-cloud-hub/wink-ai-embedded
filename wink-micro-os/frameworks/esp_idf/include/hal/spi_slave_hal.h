/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_SPI_SLAVE_HAL_H
#define WINK_H_GUARD_HAL_SPI_SLAVE_HAL_H
#ifndef __WINK_HARVESTED_HAL_SPI_SLAVE_HAL_H__
#define __WINK_HARVESTED_HAL_SPI_SLAVE_HAL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"

#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef dma_descriptor_align4_t spi_dma_desc_t;
typedef struct {
spi_dev_t     *hw;              
    
    spi_dma_desc_t *dmadesc_rx;     



    spi_dma_desc_t *dmadesc_tx;     



    int           dmadesc_n;        

    



    struct {
        uint32_t rx_lsbfirst : 1;
        uint32_t tx_lsbfirst : 1;
        uint32_t use_dma     : 1;
    };
    int mode;

    



    uint32_t tx_bitlen;             
    uint32_t rx_bitlen;             
    const void *tx_buffer;          
    void *rx_buffer;                

    
    uint32_t rcv_bitlen;
} spi_slave_hal_context_t;
typedef struct {
    uint32_t host_id;
} spi_slave_hal_config_t;



#if defined(__WINK_SIM__)
void spi_slave_hal_clear_intr_status(spi_slave_hal_context_t *hal, uint32_t mask) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hal_clear_intr_status out of Core 8 scope.");
#else
void spi_slave_hal_clear_intr_status(spi_slave_hal_context_t *hal, uint32_t mask);
#endif

#if defined(__WINK_SIM__)
void spi_slave_hal_deinit(spi_slave_hal_context_t *hal) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hal_deinit out of Core 8 scope.");
#else
void spi_slave_hal_deinit(spi_slave_hal_context_t *hal);
#endif

#if defined(__WINK_SIM__)
bool spi_slave_hal_dma_need_reset(const spi_slave_hal_context_t *hal) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hal_dma_need_reset out of Core 8 scope.");
#else
bool spi_slave_hal_dma_need_reset(const spi_slave_hal_context_t *hal);
#endif

#if defined(__WINK_SIM__)
void spi_slave_hal_enable_data_line(spi_slave_hal_context_t *hal) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hal_enable_data_line out of Core 8 scope.");
#else
void spi_slave_hal_enable_data_line(spi_slave_hal_context_t *hal);
#endif

#if defined(__WINK_SIM__)
bool spi_slave_hal_get_intr_status(spi_slave_hal_context_t *hal, uint32_t mask) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hal_get_intr_status out of Core 8 scope.");
#else
bool spi_slave_hal_get_intr_status(spi_slave_hal_context_t *hal, uint32_t mask);
#endif

#if defined(__WINK_SIM__)
uint32_t spi_slave_hal_get_rcv_bitlen(spi_slave_hal_context_t *hal) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hal_get_rcv_bitlen out of Core 8 scope.");
#else
uint32_t spi_slave_hal_get_rcv_bitlen(spi_slave_hal_context_t *hal);
#endif

#if defined(__WINK_SIM__)
void spi_slave_hal_hw_fifo_reset(spi_slave_hal_context_t *hal, bool tx_rst, bool rx_rst) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hal_hw_fifo_reset out of Core 8 scope.");
#else
void spi_slave_hal_hw_fifo_reset(spi_slave_hal_context_t *hal, bool tx_rst, bool rx_rst);
#endif

#if defined(__WINK_SIM__)
void spi_slave_hal_hw_prepare_rx(spi_dev_t *hw) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hal_hw_prepare_rx out of Core 8 scope.");
#else
void spi_slave_hal_hw_prepare_rx(spi_dev_t *hw);
#endif

#if defined(__WINK_SIM__)
void spi_slave_hal_hw_prepare_tx(spi_dev_t *hw) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hal_hw_prepare_tx out of Core 8 scope.");
#else
void spi_slave_hal_hw_prepare_tx(spi_dev_t *hw);
#endif

#if defined(__WINK_SIM__)
void spi_slave_hal_hw_reset(spi_slave_hal_context_t *hal) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hal_hw_reset out of Core 8 scope.");
#else
void spi_slave_hal_hw_reset(spi_slave_hal_context_t *hal);
#endif

#if defined(__WINK_SIM__)
void spi_slave_hal_init(spi_slave_hal_context_t *hal, const spi_slave_hal_config_t *hal_config) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hal_init out of Core 8 scope.");
#else
void spi_slave_hal_init(spi_slave_hal_context_t *hal, const spi_slave_hal_config_t *hal_config);
#endif

#if defined(__WINK_SIM__)
void spi_slave_hal_push_tx_buffer(spi_slave_hal_context_t *hal) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hal_push_tx_buffer out of Core 8 scope.");
#else
void spi_slave_hal_push_tx_buffer(spi_slave_hal_context_t *hal);
#endif

#if defined(__WINK_SIM__)
void spi_slave_hal_set_trans_bitlen(spi_slave_hal_context_t *hal) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hal_set_trans_bitlen out of Core 8 scope.");
#else
void spi_slave_hal_set_trans_bitlen(spi_slave_hal_context_t *hal);
#endif

#if defined(__WINK_SIM__)
void spi_slave_hal_setup_device(const spi_slave_hal_context_t *hal) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hal_setup_device out of Core 8 scope.");
#else
void spi_slave_hal_setup_device(const spi_slave_hal_context_t *hal);
#endif

#if defined(__WINK_SIM__)
void spi_slave_hal_store_result(spi_slave_hal_context_t *hal) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hal_store_result out of Core 8 scope.");
#else
void spi_slave_hal_store_result(spi_slave_hal_context_t *hal);
#endif

#if defined(__WINK_SIM__)
void spi_slave_hal_user_start(const spi_slave_hal_context_t *hal) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hal_user_start out of Core 8 scope.");
#else
void spi_slave_hal_user_start(const spi_slave_hal_context_t *hal);
#endif

#if defined(__WINK_SIM__)
bool spi_slave_hal_usr_is_done(spi_slave_hal_context_t* hal) WINK_SLA_ERROR("Wink SLA Violation: spi_slave_hal_usr_is_done out of Core 8 scope.");
#else
bool spi_slave_hal_usr_is_done(spi_slave_hal_context_t* hal);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_SPI_SLAVE_HAL_H__ */
#endif /* WINK_H_GUARD_HAL_SPI_SLAVE_HAL_H */
