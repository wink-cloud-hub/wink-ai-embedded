/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_REGI2C_CTRL_H
#define WINK_H_GUARD_ESP_PRIVATE_REGI2C_CTRL_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_REGI2C_CTRL_H__
#define __WINK_HARVESTED_ESP_PRIVATE_REGI2C_CTRL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stdint.h>

#include "sdkconfig.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef ANALOG_CLOCK_DISABLE
#define ANALOG_CLOCK_DISABLE() {  PERIPH_RCC_RELEASE_ATOMIC(PERIPH_ANA_I2C_MASTER_MODULE, ref_count) {  if (ref_count == 0) {  regi2c_ctrl_ll_master_enable_clock(false);  }  }  ANA_I2C_SRC_CLOCK_ENABLE(false);  }
#endif
#ifndef ANALOG_CLOCK_ENABLE
#define ANALOG_CLOCK_ENABLE() {  ANA_I2C_SRC_CLOCK_ENABLE(true);  PERIPH_RCC_ACQUIRE_ATOMIC(PERIPH_ANA_I2C_MASTER_MODULE, ref_count) {  if (ref_count == 0) {  regi2c_ctrl_ll_master_enable_clock(true);  }  }  }
#endif
#ifndef ANALOG_CLOCK_IS_ENABLED
#define ANALOG_CLOCK_IS_ENABLED() regi2c_ctrl_ll_master_is_clock_enabled()
#endif
#ifndef REGI2C_CLOCK_DISABLE
#define REGI2C_CLOCK_DISABLE() ANALOG_CLOCK_DISABLE()
#endif
#ifndef REGI2C_CLOCK_ENABLE
#define REGI2C_CLOCK_ENABLE() ANALOG_CLOCK_ENABLE()
#endif
#ifndef REGI2C_ENTER_CRITICAL
#define REGI2C_ENTER_CRITICAL() int __DECLARE_REGI2C_ATOMIC_ENV                        ; regi2c_enter_critical()
#endif
#ifndef REGI2C_EXIT_CRITICAL
#define REGI2C_EXIT_CRITICAL() regi2c_exit_critical()
#endif
#ifndef REGI2C_READ
#define REGI2C_READ(block, reg_add) regi2c_ctrl_read_reg(block, block##_HOSTID,  reg_add)
#endif
#ifndef REGI2C_READ_MASK
#define REGI2C_READ_MASK(block, reg_add) regi2c_ctrl_read_reg_mask(block, block##_HOSTID,  reg_add,  reg_add##_MSB,  reg_add##_LSB)
#endif
#ifndef REGI2C_WRITE
#define REGI2C_WRITE(block, reg_add, indata) regi2c_ctrl_write_reg(block, block##_HOSTID,  reg_add, indata)
#endif
#ifndef REGI2C_WRITE_MASK
#define REGI2C_WRITE_MASK(block, reg_add, indata) regi2c_ctrl_write_reg_mask(block, block##_HOSTID,  reg_add,  reg_add##_MSB,  reg_add##_LSB,  indata)
#endif
#ifndef regi2c_ctrl_read_reg
#define regi2c_ctrl_read_reg regi2c_impl_read
#endif
#ifndef regi2c_ctrl_read_reg_mask
#define regi2c_ctrl_read_reg_mask regi2c_impl_read_mask
#endif
#ifndef regi2c_ctrl_write_reg
#define regi2c_ctrl_write_reg regi2c_impl_write
#endif
#ifndef regi2c_ctrl_write_reg_mask
#define regi2c_ctrl_write_reg_mask regi2c_impl_write_mask
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
void ANA_I2C_SRC_CLOCK_ENABLE(bool enable) WINK_SLA_ERROR("Wink SLA Violation: ANA_I2C_SRC_CLOCK_ENABLE out of Core 8 scope.");
#else
void ANA_I2C_SRC_CLOCK_ENABLE(bool enable);
#endif

#if defined(__WINK_SIM__)
uint8_t regi2c_ctrl_read_reg(uint8_t block, uint8_t host_id, uint8_t reg_add) WINK_SLA_ERROR("Wink SLA Violation: regi2c_ctrl_read_reg out of Core 8 scope.");
#else
uint8_t regi2c_ctrl_read_reg(uint8_t block, uint8_t host_id, uint8_t reg_add);
#endif

#if defined(__WINK_SIM__)
uint8_t regi2c_ctrl_read_reg_mask(uint8_t block, uint8_t host_id, uint8_t reg_add, uint8_t msb, uint8_t lsb) WINK_SLA_ERROR("Wink SLA Violation: regi2c_ctrl_read_reg_mask out of Core 8 scope.");
#else
uint8_t regi2c_ctrl_read_reg_mask(uint8_t block, uint8_t host_id, uint8_t reg_add, uint8_t msb, uint8_t lsb);
#endif

#if defined(__WINK_SIM__)
void regi2c_ctrl_write_reg(uint8_t block, uint8_t host_id, uint8_t reg_add, uint8_t data) WINK_SLA_ERROR("Wink SLA Violation: regi2c_ctrl_write_reg out of Core 8 scope.");
#else
void regi2c_ctrl_write_reg(uint8_t block, uint8_t host_id, uint8_t reg_add, uint8_t data);
#endif

#if defined(__WINK_SIM__)
void regi2c_ctrl_write_reg_mask(uint8_t block, uint8_t host_id, uint8_t reg_add, uint8_t msb, uint8_t lsb, uint8_t data) WINK_SLA_ERROR("Wink SLA Violation: regi2c_ctrl_write_reg_mask out of Core 8 scope.");
#else
void regi2c_ctrl_write_reg_mask(uint8_t block, uint8_t host_id, uint8_t reg_add, uint8_t msb, uint8_t lsb, uint8_t data);
#endif

#if defined(__WINK_SIM__)
void regi2c_enter_critical(void) WINK_SLA_ERROR("Wink SLA Violation: regi2c_enter_critical out of Core 8 scope.");
#else
void regi2c_enter_critical(void);
#endif

#if defined(__WINK_SIM__)
void regi2c_exit_critical(void) WINK_SLA_ERROR("Wink SLA Violation: regi2c_exit_critical out of Core 8 scope.");
#else
void regi2c_exit_critical(void);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_REGI2C_CTRL_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_REGI2C_CTRL_H */
