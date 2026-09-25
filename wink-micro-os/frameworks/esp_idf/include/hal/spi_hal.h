/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_SPI_HAL_H
#define WINK_H_GUARD_HAL_SPI_HAL_H
#ifndef __WINK_HARVESTED_HAL_SPI_HAL_H__
#define __WINK_HARVESTED_HAL_SPI_HAL_H__
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
#ifndef spi_hal_sct_setup_conf_base
#define spi_hal_sct_setup_conf_base(hal, conf_base) spi_ll_set_conf_base_bitslen((hal)->hw, conf_base)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
    uint32_t clk_src_hz;
    uint32_t half_duplex;
    uint32_t no_compensate;
    uint32_t expected_freq;
    uint32_t duty_cycle;
    uint32_t input_delay_ns;
    bool use_gpio;
} spi_hal_timing_param_t;
typedef struct {
    spi_ll_clock_val_t clock_reg;
    spi_clock_source_t clock_source;
    uint32_t source_pre_div;
    uint32_t source_real_freq;
    int expect_freq;
    int real_freq;
    int timing_dummy;
    int timing_miso_delay;
    spi_sampling_point_t rx_sample_point;
} spi_hal_timing_conf_t;
typedef struct {
    uint16_t cmd;
    int cmd_bits;
    int addr_bits;
    int dummy_bits;
    int tx_bitlen;
    int rx_bitlen;
    uint64_t addr;
    uint8_t * send_buffer;
    uint8_t * rcv_buffer;
    spi_line_mode_t line_mode;
    int cs_keep_active;
} spi_hal_trans_config_t;
typedef struct {
    spi_dev_t * hw;
    bool dma_enabled;
    spi_hal_trans_config_t trans_config;
} spi_hal_context_t;
typedef struct {
int mode;                           
    int cs_setup;                       
    int cs_hold;                        
    int cs_pin_id;                      
    spi_hal_timing_conf_t timing_conf;  


    struct {
        uint32_t sio : 1;               
        uint32_t half_duplex : 1;       
        uint32_t tx_lsbfirst : 1;       
        uint32_t rx_lsbfirst : 1;       
        uint32_t no_compensate : 1;     

        uint32_t as_cs  : 1;            

        uint32_t positive_cs : 1;       
    };
} spi_hal_dev_config_t;
typedef struct {
    bool seg_end;
    uint32_t seg_gap_len;
    int cs_setup;
    uint16_t cmd;
    int cmd_bits;
    uint64_t addr;
    int addr_bits;
    int dummy_bits;
    int tx_bitlen;
    int rx_bitlen;
    int cs_hold;
} spi_hal_seg_config_t;



#if defined(__WINK_SIM__)
esp_err_t spi_hal_cal_clock_conf(const spi_hal_timing_param_t *timing_param, spi_hal_timing_conf_t *timing_conf) WINK_SLA_ERROR("Wink SLA Violation: spi_hal_cal_clock_conf out of Core 8 scope.");
#else
esp_err_t spi_hal_cal_clock_conf(const spi_hal_timing_param_t *timing_param, spi_hal_timing_conf_t *timing_conf);
#endif

#if defined(__WINK_SIM__)
void spi_hal_cal_timing(int source_freq_hz, int eff_clk, bool gpio_is_used, int input_delay_ns, int *dummy_n, int *miso_delay_n) WINK_SLA_ERROR("Wink SLA Violation: spi_hal_cal_timing out of Core 8 scope.");
#else
void spi_hal_cal_timing(int source_freq_hz, int eff_clk, bool gpio_is_used, int input_delay_ns, int *dummy_n, int *miso_delay_n);
#endif

#if defined(__WINK_SIM__)
void spi_hal_clear_intr_mask(spi_hal_context_t *hal, uint32_t mask) WINK_SLA_ERROR("Wink SLA Violation: spi_hal_clear_intr_mask out of Core 8 scope.");
#else
void spi_hal_clear_intr_mask(spi_hal_context_t *hal, uint32_t mask);
#endif

#if defined(__WINK_SIM__)
void spi_hal_deinit(spi_hal_context_t *hal) WINK_SLA_ERROR("Wink SLA Violation: spi_hal_deinit out of Core 8 scope.");
#else
void spi_hal_deinit(spi_hal_context_t *hal);
#endif

#if defined(__WINK_SIM__)
void spi_hal_enable_data_line(spi_dev_t *hw, bool mosi_ena, bool miso_ena) WINK_SLA_ERROR("Wink SLA Violation: spi_hal_enable_data_line out of Core 8 scope.");
#else
void spi_hal_enable_data_line(spi_dev_t *hw, bool mosi_ena, bool miso_ena);
#endif

#if defined(__WINK_SIM__)
void spi_hal_fetch_result(const spi_hal_context_t *hal) WINK_SLA_ERROR("Wink SLA Violation: spi_hal_fetch_result out of Core 8 scope.");
#else
void spi_hal_fetch_result(const spi_hal_context_t *hal);
#endif

#if defined(__WINK_SIM__)
int spi_hal_get_freq_limit(bool gpio_is_used, int input_delay_ns) WINK_SLA_ERROR("Wink SLA Violation: spi_hal_get_freq_limit out of Core 8 scope.");
#else
int spi_hal_get_freq_limit(bool gpio_is_used, int input_delay_ns);
#endif

#if defined(__WINK_SIM__)
bool spi_hal_get_intr_mask(spi_hal_context_t *hal, uint32_t mask) WINK_SLA_ERROR("Wink SLA Violation: spi_hal_get_intr_mask out of Core 8 scope.");
#else
bool spi_hal_get_intr_mask(spi_hal_context_t *hal, uint32_t mask);
#endif

#if defined(__WINK_SIM__)
void spi_hal_hw_prepare_rx(spi_dev_t *hw) WINK_SLA_ERROR("Wink SLA Violation: spi_hal_hw_prepare_rx out of Core 8 scope.");
#else
void spi_hal_hw_prepare_rx(spi_dev_t *hw);
#endif

#if defined(__WINK_SIM__)
void spi_hal_hw_prepare_tx(spi_dev_t *hw) WINK_SLA_ERROR("Wink SLA Violation: spi_hal_hw_prepare_tx out of Core 8 scope.");
#else
void spi_hal_hw_prepare_tx(spi_dev_t *hw);
#endif

#if defined(__WINK_SIM__)
void spi_hal_init(spi_hal_context_t *hal, uint32_t host_id) WINK_SLA_ERROR("Wink SLA Violation: spi_hal_init out of Core 8 scope.");
#else
void spi_hal_init(spi_hal_context_t *hal, uint32_t host_id);
#endif

#if defined(__WINK_SIM__)
int spi_hal_master_cal_clock(int fapb, int hz, int duty_cycle) WINK_SLA_ERROR("Wink SLA Violation: spi_hal_master_cal_clock out of Core 8 scope.");
#else
int spi_hal_master_cal_clock(int fapb, int hz, int duty_cycle);
#endif

#if defined(__WINK_SIM__)
void spi_hal_push_tx_buffer(const spi_hal_context_t *hal, const spi_hal_trans_config_t *hal_trans) WINK_SLA_ERROR("Wink SLA Violation: spi_hal_push_tx_buffer out of Core 8 scope.");
#else
void spi_hal_push_tx_buffer(const spi_hal_context_t *hal, const spi_hal_trans_config_t *hal_trans);
#endif

#if defined(__WINK_SIM__)
void spi_hal_sct_deinit(spi_hal_context_t *hal) WINK_SLA_ERROR("Wink SLA Violation: spi_hal_sct_deinit out of Core 8 scope.");
#else
void spi_hal_sct_deinit(spi_hal_context_t *hal);
#endif

#if defined(__WINK_SIM__)
void spi_hal_sct_format_conf_buffer(spi_hal_context_t *hal, const spi_hal_seg_config_t *config, const spi_hal_dev_config_t *dev, uint32_t *conf_buffer) WINK_SLA_ERROR("Wink SLA Violation: spi_hal_sct_format_conf_buffer out of Core 8 scope.");
#else
void spi_hal_sct_format_conf_buffer(spi_hal_context_t *hal, const spi_hal_seg_config_t *config, const spi_hal_dev_config_t *dev, uint32_t *conf_buffer);
#endif

#if defined(__WINK_SIM__)
void spi_hal_sct_init(spi_hal_context_t *hal) WINK_SLA_ERROR("Wink SLA Violation: spi_hal_sct_init out of Core 8 scope.");
#else
void spi_hal_sct_init(spi_hal_context_t *hal);
#endif

#if defined(__WINK_SIM__)
void spi_hal_sct_init_conf_buffer(spi_hal_context_t *hal, uint32_t *conf_buffer) WINK_SLA_ERROR("Wink SLA Violation: spi_hal_sct_init_conf_buffer out of Core 8 scope.");
#else
void spi_hal_sct_init_conf_buffer(spi_hal_context_t *hal, uint32_t *conf_buffer);
#endif

#if defined(__WINK_SIM__)
void spi_hal_sct_set_conf_bits_len(spi_hal_context_t *hal, uint32_t conf_len) WINK_SLA_ERROR("Wink SLA Violation: spi_hal_sct_set_conf_bits_len out of Core 8 scope.");
#else
void spi_hal_sct_set_conf_bits_len(spi_hal_context_t *hal, uint32_t conf_len);
#endif

#if defined(__WINK_SIM__)
void spi_hal_set_data_pin_idle_level(spi_hal_context_t *hal, bool level) WINK_SLA_ERROR("Wink SLA Violation: spi_hal_set_data_pin_idle_level out of Core 8 scope.");
#else
void spi_hal_set_data_pin_idle_level(spi_hal_context_t *hal, bool level);
#endif

#if defined(__WINK_SIM__)
void spi_hal_setup_device(spi_hal_context_t *hal, const spi_hal_dev_config_t *hal_dev) WINK_SLA_ERROR("Wink SLA Violation: spi_hal_setup_device out of Core 8 scope.");
#else
void spi_hal_setup_device(spi_hal_context_t *hal, const spi_hal_dev_config_t *hal_dev);
#endif

#if defined(__WINK_SIM__)
void spi_hal_setup_trans(spi_hal_context_t *hal, const spi_hal_dev_config_t *hal_dev, const spi_hal_trans_config_t *hal_trans) WINK_SLA_ERROR("Wink SLA Violation: spi_hal_setup_trans out of Core 8 scope.");
#else
void spi_hal_setup_trans(spi_hal_context_t *hal, const spi_hal_dev_config_t *hal_dev, const spi_hal_trans_config_t *hal_trans);
#endif

#if defined(__WINK_SIM__)
void spi_hal_user_start(const spi_hal_context_t *hal) WINK_SLA_ERROR("Wink SLA Violation: spi_hal_user_start out of Core 8 scope.");
#else
void spi_hal_user_start(const spi_hal_context_t *hal);
#endif

#if defined(__WINK_SIM__)
bool spi_hal_usr_is_done(const spi_hal_context_t *hal) WINK_SLA_ERROR("Wink SLA Violation: spi_hal_usr_is_done out of Core 8 scope.");
#else
bool spi_hal_usr_is_done(const spi_hal_context_t *hal);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_SPI_HAL_H__ */
#endif /* WINK_H_GUARD_HAL_SPI_HAL_H */
