/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_I2C_HAL_H
#define WINK_H_GUARD_HAL_I2C_HAL_H
#ifndef __WINK_HARVESTED_HAL_I2C_HAL_H__
#define __WINK_HARVESTED_HAL_I2C_HAL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"

#include "hal/i2c_types.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef i2c_hal_deinit
#define i2c_hal_deinit(...) do {(void)__DECLARE_RCC_ATOMIC_ENV; _i2c_hal_deinit(__VA_ARGS__);} while(0)
#endif
#ifndef i2c_hal_init
#define i2c_hal_init(...) do {(void)__DECLARE_RCC_ATOMIC_ENV; _i2c_hal_init(__VA_ARGS__);} while(0)
#endif
#ifndef i2c_hal_set_bus_timing
#define i2c_hal_set_bus_timing(...) do {(void)__DECLARE_RCC_ATOMIC_ENV; _i2c_hal_set_bus_timing(__VA_ARGS__);} while(0)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
    i2c_dev_t * dev;
} i2c_hal_context_t;
typedef struct {
    uint8_t clk_sel;
    uint8_t clk_active;
    hal_utils_clk_div_t clk_div;
} i2c_hal_sclk_info_t;
typedef struct {
    int high_period;
    int low_period;
    int wait_high_period;
    int rstart_setup;
    int start_hold;
    int stop_setup;
    int stop_hold;
    int sda_sample;
    int sda_hold;
    int timeout;
    i2c_hal_sclk_info_t clk_cfg;
} i2c_hal_timing_config_t;



#if defined(__WINK_SIM__)
void _i2c_hal_deinit(i2c_hal_context_t *hal) WINK_SLA_ERROR("Wink SLA Violation: _i2c_hal_deinit out of Core 8 scope.");
#else
void _i2c_hal_deinit(i2c_hal_context_t *hal);
#endif

#if defined(__WINK_SIM__)
void _i2c_hal_init(i2c_hal_context_t *hal, int i2c_port) WINK_SLA_ERROR("Wink SLA Violation: _i2c_hal_init out of Core 8 scope.");
#else
void _i2c_hal_init(i2c_hal_context_t *hal, int i2c_port);
#endif

#if defined(__WINK_SIM__)
void _i2c_hal_set_bus_timing(i2c_hal_context_t *hal, int scl_freq, i2c_clock_source_t src_clk, int source_freq) WINK_SLA_ERROR("Wink SLA Violation: _i2c_hal_set_bus_timing out of Core 8 scope.");
#else
void _i2c_hal_set_bus_timing(i2c_hal_context_t *hal, int scl_freq, i2c_clock_source_t src_clk, int source_freq);
#endif

#if defined(__WINK_SIM__)
void i2c_hal_get_timing_config(i2c_hal_context_t *hal, i2c_hal_timing_config_t *timing_config) WINK_SLA_ERROR("Wink SLA Violation: i2c_hal_get_timing_config out of Core 8 scope.");
#else
void i2c_hal_get_timing_config(i2c_hal_context_t *hal, i2c_hal_timing_config_t *timing_config);
#endif

#if defined(__WINK_SIM__)
void i2c_hal_master_fsm_rst(i2c_hal_context_t *hal) WINK_SLA_ERROR("Wink SLA Violation: i2c_hal_master_fsm_rst out of Core 8 scope.");
#else
void i2c_hal_master_fsm_rst(i2c_hal_context_t *hal);
#endif

#if defined(__WINK_SIM__)
void i2c_hal_master_handle_rx_event(i2c_hal_context_t *hal, i2c_intr_event_t *event) WINK_SLA_ERROR("Wink SLA Violation: i2c_hal_master_handle_rx_event out of Core 8 scope.");
#else
void i2c_hal_master_handle_rx_event(i2c_hal_context_t *hal, i2c_intr_event_t *event);
#endif

#if defined(__WINK_SIM__)
void i2c_hal_master_handle_tx_event(i2c_hal_context_t *hal, i2c_intr_event_t *event) WINK_SLA_ERROR("Wink SLA Violation: i2c_hal_master_handle_tx_event out of Core 8 scope.");
#else
void i2c_hal_master_handle_tx_event(i2c_hal_context_t *hal, i2c_intr_event_t *event);
#endif

#if defined(__WINK_SIM__)
void i2c_hal_master_init(i2c_hal_context_t *hal) WINK_SLA_ERROR("Wink SLA Violation: i2c_hal_master_init out of Core 8 scope.");
#else
void i2c_hal_master_init(i2c_hal_context_t *hal);
#endif

#if defined(__WINK_SIM__)
void i2c_hal_master_set_scl_timeout_val(i2c_hal_context_t *hal, uint32_t timeout_us, uint32_t sclk_clock_hz) WINK_SLA_ERROR("Wink SLA Violation: i2c_hal_master_set_scl_timeout_val out of Core 8 scope.");
#else
void i2c_hal_master_set_scl_timeout_val(i2c_hal_context_t *hal, uint32_t timeout_us, uint32_t sclk_clock_hz);
#endif

#if defined(__WINK_SIM__)
void i2c_hal_master_trans_start(i2c_hal_context_t *hal) WINK_SLA_ERROR("Wink SLA Violation: i2c_hal_master_trans_start out of Core 8 scope.");
#else
void i2c_hal_master_trans_start(i2c_hal_context_t *hal);
#endif

#if defined(__WINK_SIM__)
void i2c_hal_set_timing_config(i2c_hal_context_t *hal, i2c_hal_timing_config_t *timing_config) WINK_SLA_ERROR("Wink SLA Violation: i2c_hal_set_timing_config out of Core 8 scope.");
#else
void i2c_hal_set_timing_config(i2c_hal_context_t *hal, i2c_hal_timing_config_t *timing_config);
#endif

#if defined(__WINK_SIM__)
void i2c_hal_slave_init(i2c_hal_context_t *hal) WINK_SLA_ERROR("Wink SLA Violation: i2c_hal_slave_init out of Core 8 scope.");
#else
void i2c_hal_slave_init(i2c_hal_context_t *hal);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_I2C_HAL_H__ */
#endif /* WINK_H_GUARD_HAL_I2C_HAL_H */
