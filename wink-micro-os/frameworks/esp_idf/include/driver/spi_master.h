/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_DRIVER_SPI_MASTER_H
#define WINK_H_GUARD_DRIVER_SPI_MASTER_H
#ifndef __WINK_HARVESTED_DRIVER_SPI_MASTER_H__
#define __WINK_HARVESTED_DRIVER_SPI_MASTER_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"

#include "driver/spi_common.h"
#include "esp_err.h"
#include "hal/spi_types.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef SPI_DEVICE_3WIRE
#define SPI_DEVICE_3WIRE (1<<2)
#endif
#ifndef SPI_DEVICE_BIT_LSBFIRST
#define SPI_DEVICE_BIT_LSBFIRST (SPI_DEVICE_TXBIT_LSBFIRST|SPI_DEVICE_RXBIT_LSBFIRST)
#endif
#ifndef SPI_DEVICE_CLK_AS_CS
#define SPI_DEVICE_CLK_AS_CS (1<<5)
#endif
#ifndef SPI_DEVICE_DDRCLK
#define SPI_DEVICE_DDRCLK (1<<7)
#endif
#ifndef SPI_DEVICE_HALFDUPLEX
#define SPI_DEVICE_HALFDUPLEX (1<<4)
#endif
#ifndef SPI_DEVICE_NO_DUMMY
#define SPI_DEVICE_NO_DUMMY (1<<6)
#endif
#ifndef SPI_DEVICE_NO_RETURN_RESULT
#define SPI_DEVICE_NO_RETURN_RESULT (1<<8)
#endif
#ifndef SPI_DEVICE_POSITIVE_CS
#define SPI_DEVICE_POSITIVE_CS (1<<3)
#endif
#ifndef SPI_DEVICE_RXBIT_LSBFIRST
#define SPI_DEVICE_RXBIT_LSBFIRST (1<<1)
#endif
#ifndef SPI_DEVICE_TXBIT_LSBFIRST
#define SPI_DEVICE_TXBIT_LSBFIRST (1<<0)
#endif
#ifndef SPI_MASTER_FREQ_10M
#define SPI_MASTER_FREQ_10M (80 * 1000 * 1000 / 8)
#endif
#ifndef SPI_MASTER_FREQ_11M
#define SPI_MASTER_FREQ_11M (80 * 1000 * 1000 / 7)
#endif
#ifndef SPI_MASTER_FREQ_13M
#define SPI_MASTER_FREQ_13M (80 * 1000 * 1000 / 6)
#endif
#ifndef SPI_MASTER_FREQ_16M
#define SPI_MASTER_FREQ_16M (80 * 1000 * 1000 / 5)
#endif
#ifndef SPI_MASTER_FREQ_20M
#define SPI_MASTER_FREQ_20M (80 * 1000 * 1000 / 4)
#endif
#ifndef SPI_MASTER_FREQ_26M
#define SPI_MASTER_FREQ_26M (80 * 1000 * 1000 / 3)
#endif
#ifndef SPI_MASTER_FREQ_40M
#define SPI_MASTER_FREQ_40M (80 * 1000 * 1000 / 2)
#endif
#ifndef SPI_MASTER_FREQ_80M
#define SPI_MASTER_FREQ_80M (80 * 1000 * 1000 / 1)
#endif
#ifndef SPI_MASTER_FREQ_8M
#define SPI_MASTER_FREQ_8M (80 * 1000 * 1000 / 10)
#endif
#ifndef SPI_MASTER_FREQ_9M
#define SPI_MASTER_FREQ_9M (80 * 1000 * 1000 / 9)
#endif
#ifndef SPI_TRANS_CS_KEEP_ACTIVE
#define SPI_TRANS_CS_KEEP_ACTIVE (1<<8)
#endif
#ifndef SPI_TRANS_DMA_BUFFER_ALIGN_MANUAL
#define SPI_TRANS_DMA_BUFFER_ALIGN_MANUAL (1<<11)
#endif
#ifndef SPI_TRANS_DMA_RX_FAIL
#define SPI_TRANS_DMA_RX_FAIL (1<<30)
#endif
#ifndef SPI_TRANS_DMA_TX_FAIL
#define SPI_TRANS_DMA_TX_FAIL (1<<31)
#endif
#ifndef SPI_TRANS_DMA_USE_PSRAM
#define SPI_TRANS_DMA_USE_PSRAM (1<<12)
#endif
#ifndef SPI_TRANS_MODE_DIO
#define SPI_TRANS_MODE_DIO (1<<0)
#endif
#ifndef SPI_TRANS_MODE_DIOQIO_ADDR
#define SPI_TRANS_MODE_DIOQIO_ADDR (1<<4)
#endif
#ifndef SPI_TRANS_MODE_OCT
#define SPI_TRANS_MODE_OCT (1<<10)
#endif
#ifndef SPI_TRANS_MODE_QIO
#define SPI_TRANS_MODE_QIO (1<<1)
#endif
#ifndef SPI_TRANS_MULTILINE_ADDR
#define SPI_TRANS_MULTILINE_ADDR SPI_TRANS_MODE_DIOQIO_ADDR
#endif
#ifndef SPI_TRANS_MULTILINE_CMD
#define SPI_TRANS_MULTILINE_CMD (1<<9)
#endif
#ifndef SPI_TRANS_USE_RXDATA
#define SPI_TRANS_USE_RXDATA (1<<2)
#endif
#ifndef SPI_TRANS_USE_TXDATA
#define SPI_TRANS_USE_TXDATA (1<<3)
#endif
#ifndef SPI_TRANS_VARIABLE_ADDR
#define SPI_TRANS_VARIABLE_ADDR (1<<6)
#endif
#ifndef SPI_TRANS_VARIABLE_CMD
#define SPI_TRANS_VARIABLE_CMD (1<<5)
#endif
#ifndef SPI_TRANS_VARIABLE_DUMMY
#define SPI_TRANS_VARIABLE_DUMMY (1<<7)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct spi_transaction_t spi_transaction_t;
typedef void(*transaction_cb_t)(spi_transaction_t *trans);
typedef struct {
    uint8_t command_bits;
    uint8_t address_bits;
    uint8_t dummy_bits;
    uint8_t mode;
    spi_clock_source_t clock_source;
    uint16_t duty_cycle_pos;
    uint16_t cs_ena_pretrans;
    uint8_t cs_ena_posttrans;
    int clock_speed_hz;
    int input_delay_ns;
    spi_sampling_point_t sample_point;
    int spics_io_num;
    uint32_t flags;
    int queue_size;
    transaction_cb_t pre_cb;
    transaction_cb_t post_cb;
} spi_device_interface_config_t;
struct spi_transaction_t {
uint32_t flags;                 
    uint16_t cmd;                   





    uint64_t addr;                  





    size_t length;                  
    size_t rxlength;                
    uint32_t override_freq_hz;      
    void *user;                     
    union {
        const void *tx_buffer;      
        uint8_t tx_data[4];         
    };
    union {
        void *rx_buffer;            
        uint8_t rx_data[4];         
    };
};
typedef struct {
    struct spi_transaction_t base;
    uint8_t command_bits;
    uint8_t address_bits;
    uint8_t dummy_bits;
} spi_transaction_ext_t;
typedef struct spi_device_t * spi_device_handle_t;

esp_err_t spi_bus_add_device(spi_host_device_t host_id, const spi_device_interface_config_t *dev_config, spi_device_handle_t *handle);
esp_err_t spi_bus_remove_device(spi_device_handle_t handle);
esp_err_t spi_device_transmit(spi_device_handle_t handle, spi_transaction_t *trans_desc);


#if defined(__WINK_SIM__)
esp_err_t spi_bus_get_max_transaction_len(spi_host_device_t host_id, size_t *max_bytes) WINK_SLA_ERROR("Wink SLA Violation: spi_bus_get_max_transaction_len out of Core 8 scope.");
#else
esp_err_t spi_bus_get_max_transaction_len(spi_host_device_t host_id, size_t *max_bytes);
#endif

#if defined(__WINK_SIM__)
esp_err_t spi_device_acquire_bus(spi_device_handle_t device, uint32_t wait) WINK_SLA_ERROR("Wink SLA Violation: spi_device_acquire_bus out of Core 8 scope.");
#else
esp_err_t spi_device_acquire_bus(spi_device_handle_t device, uint32_t wait);
#endif

#if defined(__WINK_SIM__)
esp_err_t spi_device_get_actual_freq(spi_device_handle_t handle, int *freq_khz) WINK_SLA_ERROR("Wink SLA Violation: spi_device_get_actual_freq out of Core 8 scope.");
#else
esp_err_t spi_device_get_actual_freq(spi_device_handle_t handle, int *freq_khz);
#endif

#if defined(__WINK_SIM__)
esp_err_t spi_device_get_trans_result(spi_device_handle_t handle, spi_transaction_t **trans_desc, uint32_t ticks_to_wait) WINK_SLA_ERROR("Wink SLA Violation: spi_device_get_trans_result out of Core 8 scope.");
#else
esp_err_t spi_device_get_trans_result(spi_device_handle_t handle, spi_transaction_t **trans_desc, uint32_t ticks_to_wait);
#endif

#if defined(__WINK_SIM__)
esp_err_t spi_device_polling_end(spi_device_handle_t handle, uint32_t ticks_to_wait) WINK_SLA_ERROR("Wink SLA Violation: spi_device_polling_end out of Core 8 scope.");
#else
esp_err_t spi_device_polling_end(spi_device_handle_t handle, uint32_t ticks_to_wait);
#endif

#if defined(__WINK_SIM__)
esp_err_t spi_device_polling_start(spi_device_handle_t handle, spi_transaction_t *trans_desc, uint32_t ticks_to_wait) WINK_SLA_ERROR("Wink SLA Violation: spi_device_polling_start out of Core 8 scope.");
#else
esp_err_t spi_device_polling_start(spi_device_handle_t handle, spi_transaction_t *trans_desc, uint32_t ticks_to_wait);
#endif

#if defined(__WINK_SIM__)
esp_err_t spi_device_polling_transmit(spi_device_handle_t handle, spi_transaction_t *trans_desc) WINK_SLA_ERROR("Wink SLA Violation: spi_device_polling_transmit out of Core 8 scope.");
#else
esp_err_t spi_device_polling_transmit(spi_device_handle_t handle, spi_transaction_t *trans_desc);
#endif

#if defined(__WINK_SIM__)
esp_err_t spi_device_queue_trans(spi_device_handle_t handle, spi_transaction_t *trans_desc, uint32_t ticks_to_wait) WINK_SLA_ERROR("Wink SLA Violation: spi_device_queue_trans out of Core 8 scope.");
#else
esp_err_t spi_device_queue_trans(spi_device_handle_t handle, spi_transaction_t *trans_desc, uint32_t ticks_to_wait);
#endif

#if defined(__WINK_SIM__)
void spi_device_release_bus(spi_device_handle_t dev) WINK_SLA_ERROR("Wink SLA Violation: spi_device_release_bus out of Core 8 scope.");
#else
void spi_device_release_bus(spi_device_handle_t dev);
#endif

#if defined(__WINK_SIM__)
int spi_get_actual_clock(int fapb, int hz, int duty_cycle) WINK_SLA_ERROR("Wink SLA Violation: spi_get_actual_clock out of Core 8 scope.");
#else
int spi_get_actual_clock(int fapb, int hz, int duty_cycle);
#endif

#if defined(__WINK_SIM__)
int spi_get_freq_limit(bool gpio_is_used, int input_delay_ns) WINK_SLA_ERROR("Wink SLA Violation: spi_get_freq_limit out of Core 8 scope.");
#else
int spi_get_freq_limit(bool gpio_is_used, int input_delay_ns);
#endif

#if defined(__WINK_SIM__)
void spi_get_timing(bool gpio_is_used, int input_delay_ns, int eff_clk, int *dummy_o, int *cycles_remain_o) WINK_SLA_ERROR("Wink SLA Violation: spi_get_timing out of Core 8 scope.");
#else
void spi_get_timing(bool gpio_is_used, int input_delay_ns, int eff_clk, int *dummy_o, int *cycles_remain_o);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_DRIVER_SPI_MASTER_H__ */
#endif /* WINK_H_GUARD_DRIVER_SPI_MASTER_H */
