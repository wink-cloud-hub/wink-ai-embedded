/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_SPI_LL_H
#define WINK_H_GUARD_HAL_SPI_LL_H
#ifndef __WINK_HARVESTED_HAL_SPI_LL_H__
#define __WINK_HARVESTED_HAL_SPI_LL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdlib.h>
#include <string.h>

#include "hal/spi_types.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef HAL_SPI_SWAP_DATA_TX
#define HAL_SPI_SWAP_DATA_TX(data, len) HAL_SWAP32((uint32_t)(data) << (32 - len))
#endif
#ifndef SPI_LL_CPU_MAX_BIT_LEN
#define SPI_LL_CPU_MAX_BIT_LEN (16 * 32)
#endif
#ifndef SPI_LL_DMA_CHANNEL_NUM
#define SPI_LL_DMA_CHANNEL_NUM (2)
#endif
#ifndef SPI_LL_DMA_FIFO_RST_MASK
#define SPI_LL_DMA_FIFO_RST_MASK (SPI_AHBM_RST | SPI_AHBM_FIFO_RST)
#endif
#ifndef SPI_LL_DMA_MAX_BIT_LEN
#define SPI_LL_DMA_MAX_BIT_LEN (1 << 24)
#endif
#ifndef SPI_LL_GET_HW
#define SPI_LL_GET_HW(ID) ((ID)==SPI1_HOST ? &SPI1:((ID)==SPI2_HOST ? &SPI2 : &SPI3))
#endif
#ifndef SPI_LL_MAX_PRE_DIV_NUM
#define SPI_LL_MAX_PRE_DIV_NUM (8192)
#endif
#ifndef SPI_LL_MOSI_FREE_LEVEL
#define SPI_LL_MOSI_FREE_LEVEL 0
#endif
#ifndef SPI_LL_ONE_LINE_CTRL_MASK
#define SPI_LL_ONE_LINE_CTRL_MASK (SPI_FREAD_DUAL | SPI_FREAD_QUAD | SPI_FREAD_DIO | SPI_FREAD_QIO)
#endif
#ifndef SPI_LL_ONE_LINE_USER_MASK
#define SPI_LL_ONE_LINE_USER_MASK (SPI_FWRITE_DUAL | SPI_FWRITE_QUAD | SPI_FWRITE_DIO | SPI_FWRITE_QIO)
#endif
#ifndef SPI_LL_PERIPH_BITWIDTH
#define SPI_LL_PERIPH_BITWIDTH(host) (4)
#endif
#ifndef SPI_LL_PERIPH_CS_NUM
#define SPI_LL_PERIPH_CS_NUM(i) 3
#endif
#ifndef SPI_LL_RX_MINI_EXTRA_BITS
#define SPI_LL_RX_MINI_EXTRA_BITS 1
#endif
#ifndef SPI_LL_SLAVE_NEEDS_CS_WORKAROUND
#define SPI_LL_SLAVE_NEEDS_CS_WORKAROUND 1
#endif
#ifndef SPI_LL_SLAVE_NEEDS_RESET_WORKAROUND
#define SPI_LL_SLAVE_NEEDS_RESET_WORKAROUND 1
#endif
#ifndef SPI_LL_SUPPORT_CLK_AS_CS
#define SPI_LL_SUPPORT_CLK_AS_CS 1
#endif
#ifndef SPI_LL_SUPPORT_TIME_TUNING
#define SPI_LL_SUPPORT_TIME_TUNING 1
#endif
#ifndef SPI_LL_TX_MINI_EXTRA_BITS
#define SPI_LL_TX_MINI_EXTRA_BITS 1
#endif
#ifndef SPI_LL_UNUSED_INT_MASK
#define SPI_LL_UNUSED_INT_MASK (SPI_INT_EN | SPI_SLV_WR_STA_DONE | SPI_SLV_RD_STA_DONE | SPI_SLV_WR_BUF_DONE | SPI_SLV_RD_BUF_DONE)
#endif
#ifndef spi_dma_ll_enable_bus_clock
#define spi_dma_ll_enable_bus_clock(...) do {  (void)__DECLARE_RCC_ATOMIC_ENV;  spi_dma_ll_enable_bus_clock(__VA_ARGS__);  } while(0)
#endif
#ifndef spi_dma_ll_reset_register
#define spi_dma_ll_reset_register(...) do {  (void)__DECLARE_RCC_ATOMIC_ENV;  spi_dma_ll_reset_register(__VA_ARGS__);  } while(0)
#endif
#ifndef spi_ll_enable_bus_clock
#define spi_ll_enable_bus_clock(...) do {  (void)__DECLARE_RCC_ATOMIC_ENV;  spi_ll_enable_bus_clock(__VA_ARGS__);  } while(0)
#endif
#ifndef spi_ll_reset_register
#define spi_ll_reset_register(...) do {  (void)__DECLARE_RCC_ATOMIC_ENV;  spi_ll_reset_register(__VA_ARGS__);  } while(0)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef uint32_t spi_ll_clock_val_t;
typedef spi_dev_t spi_dma_dev_t;



#if defined(__WINK_SIM__)
void spi_dma_ll_enable_bus_clock(spi_host_device_t host_id, bool enable) WINK_SLA_ERROR("Wink SLA Violation: spi_dma_ll_enable_bus_clock out of Core 8 scope.");
#else
void spi_dma_ll_enable_bus_clock(spi_host_device_t host_id, bool enable);
#endif

#if defined(__WINK_SIM__)
void spi_dma_ll_enable_out_auto_wrback(spi_dma_dev_t *dma_out, uint32_t channel, bool enable) WINK_SLA_ERROR("Wink SLA Violation: spi_dma_ll_enable_out_auto_wrback out of Core 8 scope.");
#else
void spi_dma_ll_enable_out_auto_wrback(spi_dma_dev_t *dma_out, uint32_t channel, bool enable);
#endif

#if defined(__WINK_SIM__)
void spi_dma_ll_get_rx_alignment_require(spi_dma_dev_t *dma_dev, uint32_t *internal_size, uint32_t *external_size) WINK_SLA_ERROR("Wink SLA Violation: spi_dma_ll_get_rx_alignment_require out of Core 8 scope.");
#else
void spi_dma_ll_get_rx_alignment_require(spi_dma_dev_t *dma_dev, uint32_t *internal_size, uint32_t *external_size);
#endif

#if defined(__WINK_SIM__)
void spi_dma_ll_reset_register(spi_host_device_t host_id) WINK_SLA_ERROR("Wink SLA Violation: spi_dma_ll_reset_register out of Core 8 scope.");
#else
void spi_dma_ll_reset_register(spi_host_device_t host_id);
#endif

#if defined(__WINK_SIM__)
void spi_dma_ll_rx_enable_burst_data(spi_dma_dev_t *dma_in, uint32_t channel, bool enable) WINK_SLA_ERROR("Wink SLA Violation: spi_dma_ll_rx_enable_burst_data out of Core 8 scope.");
#else
void spi_dma_ll_rx_enable_burst_data(spi_dma_dev_t *dma_in, uint32_t channel, bool enable);
#endif

#if defined(__WINK_SIM__)
void spi_dma_ll_rx_enable_burst_desc(spi_dma_dev_t *dma_in, uint32_t channel, bool enable) WINK_SLA_ERROR("Wink SLA Violation: spi_dma_ll_rx_enable_burst_desc out of Core 8 scope.");
#else
void spi_dma_ll_rx_enable_burst_desc(spi_dma_dev_t *dma_in, uint32_t channel, bool enable);
#endif

#if defined(__WINK_SIM__)
void spi_dma_ll_rx_reset(spi_dma_dev_t *dma_in, uint32_t channel) WINK_SLA_ERROR("Wink SLA Violation: spi_dma_ll_rx_reset out of Core 8 scope.");
#else
void spi_dma_ll_rx_reset(spi_dma_dev_t *dma_in, uint32_t channel);
#endif

#if defined(__WINK_SIM__)
void spi_dma_ll_rx_start(spi_dma_dev_t *dma_in, uint32_t channel, lldesc_t *addr) WINK_SLA_ERROR("Wink SLA Violation: spi_dma_ll_rx_start out of Core 8 scope.");
#else
void spi_dma_ll_rx_start(spi_dma_dev_t *dma_in, uint32_t channel, lldesc_t *addr);
#endif

#if defined(__WINK_SIM__)
void spi_dma_ll_set_out_eof_generation(spi_dma_dev_t *dma_out, uint32_t channel, bool enable) WINK_SLA_ERROR("Wink SLA Violation: spi_dma_ll_set_out_eof_generation out of Core 8 scope.");
#else
void spi_dma_ll_set_out_eof_generation(spi_dma_dev_t *dma_out, uint32_t channel, bool enable);
#endif

#if defined(__WINK_SIM__)
void spi_dma_ll_tx_enable_burst_data(spi_dma_dev_t *dma_out, uint32_t channel, bool enable) WINK_SLA_ERROR("Wink SLA Violation: spi_dma_ll_tx_enable_burst_data out of Core 8 scope.");
#else
void spi_dma_ll_tx_enable_burst_data(spi_dma_dev_t *dma_out, uint32_t channel, bool enable);
#endif

#if defined(__WINK_SIM__)
void spi_dma_ll_tx_enable_burst_desc(spi_dma_dev_t *dma_out, uint32_t channel, bool enable) WINK_SLA_ERROR("Wink SLA Violation: spi_dma_ll_tx_enable_burst_desc out of Core 8 scope.");
#else
void spi_dma_ll_tx_enable_burst_desc(spi_dma_dev_t *dma_out, uint32_t channel, bool enable);
#endif

#if defined(__WINK_SIM__)
void spi_dma_ll_tx_reset(spi_dma_dev_t *dma_out, uint32_t channel) WINK_SLA_ERROR("Wink SLA Violation: spi_dma_ll_tx_reset out of Core 8 scope.");
#else
void spi_dma_ll_tx_reset(spi_dma_dev_t *dma_out, uint32_t channel);
#endif

#if defined(__WINK_SIM__)
void spi_dma_ll_tx_start(spi_dma_dev_t *dma_out, uint32_t channel, lldesc_t *addr) WINK_SLA_ERROR("Wink SLA Violation: spi_dma_ll_tx_start out of Core 8 scope.");
#else
void spi_dma_ll_tx_start(spi_dma_dev_t *dma_out, uint32_t channel, lldesc_t *addr);
#endif

#if defined(__WINK_SIM__)
void spi_ll_apply_config(spi_dev_t *hw) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_apply_config out of Core 8 scope.");
#else
void spi_ll_apply_config(spi_dev_t *hw);
#endif

#if defined(__WINK_SIM__)
void spi_ll_clear_int_stat(spi_dev_t *hw) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_clear_int_stat out of Core 8 scope.");
#else
void spi_ll_clear_int_stat(spi_dev_t *hw);
#endif

#if defined(__WINK_SIM__)
void spi_ll_cpu_rx_fifo_reset(spi_dev_t *hw) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_cpu_rx_fifo_reset out of Core 8 scope.");
#else
void spi_ll_cpu_rx_fifo_reset(spi_dev_t *hw);
#endif

#if defined(__WINK_SIM__)
void spi_ll_cpu_tx_fifo_reset(spi_dev_t *hw) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_cpu_tx_fifo_reset out of Core 8 scope.");
#else
void spi_ll_cpu_tx_fifo_reset(spi_dev_t *hw);
#endif

#if defined(__WINK_SIM__)
void spi_ll_disable_int(spi_dev_t *hw) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_disable_int out of Core 8 scope.");
#else
void spi_ll_disable_int(spi_dev_t *hw);
#endif

#if defined(__WINK_SIM__)
void spi_ll_dma_rx_enable(spi_dev_t *hw, bool enable) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_dma_rx_enable out of Core 8 scope.");
#else
void spi_ll_dma_rx_enable(spi_dev_t *hw, bool enable);
#endif

#if defined(__WINK_SIM__)
void spi_ll_dma_rx_fifo_reset(spi_dev_t *hw) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_dma_rx_fifo_reset out of Core 8 scope.");
#else
void spi_ll_dma_rx_fifo_reset(spi_dev_t *hw);
#endif

#if defined(__WINK_SIM__)
void spi_ll_dma_set_rx_eof_generation(spi_dev_t *hw, bool enable) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_dma_set_rx_eof_generation out of Core 8 scope.");
#else
void spi_ll_dma_set_rx_eof_generation(spi_dev_t *hw, bool enable);
#endif

#if defined(__WINK_SIM__)
void spi_ll_dma_tx_enable(spi_dev_t *hw, bool enable) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_dma_tx_enable out of Core 8 scope.");
#else
void spi_ll_dma_tx_enable(spi_dev_t *hw, bool enable);
#endif

#if defined(__WINK_SIM__)
void spi_ll_dma_tx_fifo_reset(spi_dev_t *hw) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_dma_tx_fifo_reset out of Core 8 scope.");
#else
void spi_ll_dma_tx_fifo_reset(spi_dev_t *hw);
#endif

#if defined(__WINK_SIM__)
void spi_ll_enable_bus_clock(spi_host_device_t host_id, bool enable) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_enable_bus_clock out of Core 8 scope.");
#else
void spi_ll_enable_bus_clock(spi_host_device_t host_id, bool enable);
#endif

#if defined(__WINK_SIM__)
void spi_ll_enable_clock(spi_host_device_t host_id, bool enable) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_enable_clock out of Core 8 scope.");
#else
void spi_ll_enable_clock(spi_host_device_t host_id, bool enable);
#endif

#if defined(__WINK_SIM__)
void spi_ll_enable_int(spi_dev_t *hw) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_enable_int out of Core 8 scope.");
#else
void spi_ll_enable_int(spi_dev_t *hw);
#endif

#if defined(__WINK_SIM__)
void spi_ll_enable_miso(spi_dev_t *hw, int enable) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_enable_miso out of Core 8 scope.");
#else
void spi_ll_enable_miso(spi_dev_t *hw, int enable);
#endif

#if defined(__WINK_SIM__)
void spi_ll_enable_mosi(spi_dev_t *hw, int enable) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_enable_mosi out of Core 8 scope.");
#else
void spi_ll_enable_mosi(spi_dev_t *hw, int enable);
#endif

#if defined(__WINK_SIM__)
int spi_ll_freq_for_pre_n(int fapb, int pre, int n) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_freq_for_pre_n out of Core 8 scope.");
#else
int spi_ll_freq_for_pre_n(int fapb, int pre, int n);
#endif

#if defined(__WINK_SIM__)
uint32_t spi_ll_get_running_cmd(spi_dev_t *hw) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_get_running_cmd out of Core 8 scope.");
#else
uint32_t spi_ll_get_running_cmd(spi_dev_t *hw);
#endif

#if defined(__WINK_SIM__)
uint16_t spi_ll_get_slave_hd_command(spi_command_t cmd_t, spi_line_mode_t line_mode) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_get_slave_hd_command out of Core 8 scope.");
#else
uint16_t spi_ll_get_slave_hd_command(spi_command_t cmd_t, spi_line_mode_t line_mode);
#endif

#if defined(__WINK_SIM__)
int spi_ll_get_slave_hd_dummy_bits(spi_line_mode_t line_mode) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_get_slave_hd_dummy_bits out of Core 8 scope.");
#else
int spi_ll_get_slave_hd_dummy_bits(spi_line_mode_t line_mode);
#endif

#if defined(__WINK_SIM__)
void spi_ll_infifo_full_clr(spi_dev_t *hw) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_infifo_full_clr out of Core 8 scope.");
#else
void spi_ll_infifo_full_clr(spi_dev_t *hw);
#endif

#if defined(__WINK_SIM__)
int spi_ll_master_cal_clock(int fapb, int hz, int duty_cycle, spi_ll_clock_val_t *out_reg) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_master_cal_clock out of Core 8 scope.");
#else
int spi_ll_master_cal_clock(int fapb, int hz, int duty_cycle, spi_ll_clock_val_t *out_reg);
#endif

#if defined(__WINK_SIM__)
void spi_ll_master_init(spi_dev_t *hw) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_master_init out of Core 8 scope.");
#else
void spi_ll_master_init(spi_dev_t *hw);
#endif

#if defined(__WINK_SIM__)
bool spi_ll_master_is_rx_std_sample_supported(void) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_master_is_rx_std_sample_supported out of Core 8 scope.");
#else
bool spi_ll_master_is_rx_std_sample_supported(void);
#endif

#if defined(__WINK_SIM__)
void spi_ll_master_keep_cs(spi_dev_t *hw, int keep_active) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_master_keep_cs out of Core 8 scope.");
#else
void spi_ll_master_keep_cs(spi_dev_t *hw, int keep_active);
#endif

#if defined(__WINK_SIM__)
void spi_ll_master_select_cs(spi_dev_t *hw, int cs_id) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_master_select_cs out of Core 8 scope.");
#else
void spi_ll_master_select_cs(spi_dev_t *hw, int cs_id);
#endif

#if defined(__WINK_SIM__)
void spi_ll_master_set_cksel(spi_dev_t *hw, int cs, uint32_t cksel) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_master_set_cksel out of Core 8 scope.");
#else
void spi_ll_master_set_cksel(spi_dev_t *hw, int cs, uint32_t cksel);
#endif

#if defined(__WINK_SIM__)
int spi_ll_master_set_clock(spi_dev_t *hw, int fapb, int hz, int duty_cycle) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_master_set_clock out of Core 8 scope.");
#else
int spi_ll_master_set_clock(spi_dev_t *hw, int fapb, int hz, int duty_cycle);
#endif

#if defined(__WINK_SIM__)
void spi_ll_master_set_clock_by_reg(spi_dev_t *hw, const spi_ll_clock_val_t *val) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_master_set_clock_by_reg out of Core 8 scope.");
#else
void spi_ll_master_set_clock_by_reg(spi_dev_t *hw, const spi_ll_clock_val_t *val);
#endif

#if defined(__WINK_SIM__)
void spi_ll_master_set_cs_hold(spi_dev_t *hw, int hold) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_master_set_cs_hold out of Core 8 scope.");
#else
void spi_ll_master_set_cs_hold(spi_dev_t *hw, int hold);
#endif

#if defined(__WINK_SIM__)
void spi_ll_master_set_cs_setup(spi_dev_t *hw, uint8_t setup) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_master_set_cs_setup out of Core 8 scope.");
#else
void spi_ll_master_set_cs_setup(spi_dev_t *hw, uint8_t setup);
#endif

#if defined(__WINK_SIM__)
void spi_ll_master_set_line_mode(spi_dev_t *hw, spi_line_mode_t line_mode) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_master_set_line_mode out of Core 8 scope.");
#else
void spi_ll_master_set_line_mode(spi_dev_t *hw, spi_line_mode_t line_mode);
#endif

#if defined(__WINK_SIM__)
void spi_ll_master_set_mode(spi_dev_t *hw, uint8_t mode) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_master_set_mode out of Core 8 scope.");
#else
void spi_ll_master_set_mode(spi_dev_t *hw, uint8_t mode);
#endif

#if defined(__WINK_SIM__)
void spi_ll_master_set_pos_cs(spi_dev_t *hw, int cs, uint32_t pos_cs) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_master_set_pos_cs out of Core 8 scope.");
#else
void spi_ll_master_set_pos_cs(spi_dev_t *hw, int cs, uint32_t pos_cs);
#endif

#if defined(__WINK_SIM__)
void spi_ll_master_set_rx_timing_mode(spi_dev_t *hw, spi_sampling_point_t sample_point) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_master_set_rx_timing_mode out of Core 8 scope.");
#else
void spi_ll_master_set_rx_timing_mode(spi_dev_t *hw, spi_sampling_point_t sample_point);
#endif

#if defined(__WINK_SIM__)
void spi_ll_outfifo_empty_clr(spi_dev_t *hw) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_outfifo_empty_clr out of Core 8 scope.");
#else
void spi_ll_outfifo_empty_clr(spi_dev_t *hw);
#endif

#if defined(__WINK_SIM__)
void spi_ll_read_buffer(spi_dev_t *hw, uint8_t *buffer_to_rcv, size_t bitlen) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_read_buffer out of Core 8 scope.");
#else
void spi_ll_read_buffer(spi_dev_t *hw, uint8_t *buffer_to_rcv, size_t bitlen);
#endif

#if defined(__WINK_SIM__)
void spi_ll_reset_register(spi_host_device_t host_id) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_reset_register out of Core 8 scope.");
#else
void spi_ll_reset_register(spi_host_device_t host_id);
#endif

#if defined(__WINK_SIM__)
void spi_ll_set_addr_bitlen(spi_dev_t *hw, int bitlen) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_set_addr_bitlen out of Core 8 scope.");
#else
void spi_ll_set_addr_bitlen(spi_dev_t *hw, int bitlen);
#endif

#if defined(__WINK_SIM__)
void spi_ll_set_address(spi_dev_t *hw, uint64_t addr, int addrlen, uint32_t lsbfirst) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_set_address out of Core 8 scope.");
#else
void spi_ll_set_address(spi_dev_t *hw, uint64_t addr, int addrlen, uint32_t lsbfirst);
#endif

#if defined(__WINK_SIM__)
void spi_ll_set_clk_source(spi_dev_t *hw, spi_clock_source_t clk_source) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_set_clk_source out of Core 8 scope.");
#else
void spi_ll_set_clk_source(spi_dev_t *hw, spi_clock_source_t clk_source);
#endif

#if defined(__WINK_SIM__)
void spi_ll_set_command(spi_dev_t *hw, uint16_t cmd, int cmdlen, bool lsbfirst) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_set_command out of Core 8 scope.");
#else
void spi_ll_set_command(spi_dev_t *hw, uint16_t cmd, int cmdlen, bool lsbfirst);
#endif

#if defined(__WINK_SIM__)
void spi_ll_set_command_bitlen(spi_dev_t *hw, int bitlen) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_set_command_bitlen out of Core 8 scope.");
#else
void spi_ll_set_command_bitlen(spi_dev_t *hw, int bitlen);
#endif

#if defined(__WINK_SIM__)
void spi_ll_set_dummy(spi_dev_t *hw, int dummy_n) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_set_dummy out of Core 8 scope.");
#else
void spi_ll_set_dummy(spi_dev_t *hw, int dummy_n);
#endif

#if defined(__WINK_SIM__)
void spi_ll_set_half_duplex(spi_dev_t *hw, bool half_duplex) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_set_half_duplex out of Core 8 scope.");
#else
void spi_ll_set_half_duplex(spi_dev_t *hw, bool half_duplex);
#endif

#if defined(__WINK_SIM__)
void spi_ll_set_int_stat(spi_dev_t *hw) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_set_int_stat out of Core 8 scope.");
#else
void spi_ll_set_int_stat(spi_dev_t *hw);
#endif

#if defined(__WINK_SIM__)
void spi_ll_set_miso_bitlen(spi_dev_t *hw, size_t bitlen) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_set_miso_bitlen out of Core 8 scope.");
#else
void spi_ll_set_miso_bitlen(spi_dev_t *hw, size_t bitlen);
#endif

#if defined(__WINK_SIM__)
void spi_ll_set_miso_delay(spi_dev_t *hw, int delay_mode, int delay_num) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_set_miso_delay out of Core 8 scope.");
#else
void spi_ll_set_miso_delay(spi_dev_t *hw, int delay_mode, int delay_num);
#endif

#if defined(__WINK_SIM__)
void spi_ll_set_mosi_bitlen(spi_dev_t *hw, size_t bitlen) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_set_mosi_bitlen out of Core 8 scope.");
#else
void spi_ll_set_mosi_bitlen(spi_dev_t *hw, size_t bitlen);
#endif

#if defined(__WINK_SIM__)
void spi_ll_set_mosi_delay(spi_dev_t *hw, int delay_mode, int delay_num) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_set_mosi_delay out of Core 8 scope.");
#else
void spi_ll_set_mosi_delay(spi_dev_t *hw, int delay_mode, int delay_num);
#endif

#if defined(__WINK_SIM__)
void spi_ll_set_rx_lsbfirst(spi_dev_t *hw, bool lsbfirst) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_set_rx_lsbfirst out of Core 8 scope.");
#else
void spi_ll_set_rx_lsbfirst(spi_dev_t *hw, bool lsbfirst);
#endif

#if defined(__WINK_SIM__)
void spi_ll_set_sio_mode(spi_dev_t *hw, int sio_mode) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_set_sio_mode out of Core 8 scope.");
#else
void spi_ll_set_sio_mode(spi_dev_t *hw, int sio_mode);
#endif

#if defined(__WINK_SIM__)
void spi_ll_set_tx_lsbfirst(spi_dev_t *hw, bool lsbfirst) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_set_tx_lsbfirst out of Core 8 scope.");
#else
void spi_ll_set_tx_lsbfirst(spi_dev_t *hw, bool lsbfirst);
#endif

#if defined(__WINK_SIM__)
uint32_t spi_ll_slave_get_rcv_bitlen(spi_dev_t *hw) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_slave_get_rcv_bitlen out of Core 8 scope.");
#else
uint32_t spi_ll_slave_get_rcv_bitlen(spi_dev_t *hw);
#endif

#if defined(__WINK_SIM__)
void spi_ll_slave_init(spi_dev_t *hw) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_slave_init out of Core 8 scope.");
#else
void spi_ll_slave_init(spi_dev_t *hw);
#endif

#if defined(__WINK_SIM__)
void spi_ll_slave_reset(spi_dev_t *hw) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_slave_reset out of Core 8 scope.");
#else
void spi_ll_slave_reset(spi_dev_t *hw);
#endif

#if defined(__WINK_SIM__)
void spi_ll_slave_set_mode(spi_dev_t *hw, const int mode, bool dma_used) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_slave_set_mode out of Core 8 scope.");
#else
void spi_ll_slave_set_mode(spi_dev_t *hw, const int mode, bool dma_used);
#endif

#if defined(__WINK_SIM__)
void spi_ll_slave_set_rx_bitlen(spi_dev_t *hw, size_t bitlen) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_slave_set_rx_bitlen out of Core 8 scope.");
#else
void spi_ll_slave_set_rx_bitlen(spi_dev_t *hw, size_t bitlen);
#endif

#if defined(__WINK_SIM__)
void spi_ll_slave_set_tx_bitlen(spi_dev_t *hw, size_t bitlen) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_slave_set_tx_bitlen out of Core 8 scope.");
#else
void spi_ll_slave_set_tx_bitlen(spi_dev_t *hw, size_t bitlen);
#endif

#if defined(__WINK_SIM__)
void spi_ll_user_start(spi_dev_t *hw) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_user_start out of Core 8 scope.");
#else
void spi_ll_user_start(spi_dev_t *hw);
#endif

#if defined(__WINK_SIM__)
bool spi_ll_usr_is_done(spi_dev_t *hw) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_usr_is_done out of Core 8 scope.");
#else
bool spi_ll_usr_is_done(spi_dev_t *hw);
#endif

#if defined(__WINK_SIM__)
void spi_ll_write_buffer(spi_dev_t *hw, const uint8_t *buffer_to_send, size_t bitlen) WINK_SLA_ERROR("Wink SLA Violation: spi_ll_write_buffer out of Core 8 scope.");
#else
void spi_ll_write_buffer(spi_dev_t *hw, const uint8_t *buffer_to_send, size_t bitlen);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_SPI_LL_H__ */
#endif /* WINK_H_GUARD_HAL_SPI_LL_H */
