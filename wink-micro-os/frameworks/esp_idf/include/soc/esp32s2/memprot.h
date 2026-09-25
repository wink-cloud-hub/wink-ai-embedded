/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_SOC_ESP32S2_MEMPROT_H
#define WINK_H_GUARD_SOC_ESP32S2_MEMPROT_H
#ifndef __WINK_HARVESTED_SOC_ESP32S2_MEMPROT_H__
#define __WINK_HARVESTED_SOC_ESP32S2_MEMPROT_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stdint.h>

#include "esp_attr.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef DEF_SPLIT_LINE
#define DEF_SPLIT_LINE NULL
#endif
#ifndef EX_DIS
#define EX_DIS false
#endif
#ifndef EX_ENA
#define EX_ENA true
#endif
#ifndef EX_HIGH_DIS
#define EX_HIGH_DIS false
#endif
#ifndef EX_HIGH_ENA
#define EX_HIGH_ENA true
#endif
#ifndef EX_LOW_DIS
#define EX_LOW_DIS false
#endif
#ifndef EX_LOW_ENA
#define EX_LOW_ENA true
#endif
#ifndef MEMPROT_INVALID_ADDRESS
#define MEMPROT_INVALID_ADDRESS -1
#endif
#ifndef MEMPROT_LOCK
#define MEMPROT_LOCK true
#endif
#ifndef MEMPROT_UNLOCK
#define MEMPROT_UNLOCK false
#endif
#ifndef PANIC_HNDL_OFF
#define PANIC_HNDL_OFF false
#endif
#ifndef PANIC_HNDL_ON
#define PANIC_HNDL_ON true
#endif
#ifndef RD_DIS
#define RD_DIS false
#endif
#ifndef RD_ENA
#define RD_ENA true
#endif
#ifndef RD_HIGH_DIS
#define RD_HIGH_DIS false
#endif
#ifndef RD_HIGH_ENA
#define RD_HIGH_ENA true
#endif
#ifndef RD_LOW_DIS
#define RD_LOW_DIS false
#endif
#ifndef RD_LOW_ENA
#define RD_LOW_ENA true
#endif
#ifndef WR_DIS
#define WR_DIS false
#endif
#ifndef WR_ENA
#define WR_ENA true
#endif
#ifndef WR_HIGH_DIS
#define WR_HIGH_DIS false
#endif
#ifndef WR_HIGH_ENA
#define WR_HIGH_ENA true
#endif
#ifndef WR_LOW_DIS
#define WR_LOW_DIS false
#endif
#ifndef WR_LOW_ENA
#define WR_LOW_ENA true
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    MEMPROT_NONE = 0,
    MEMPROT_IRAM0_SRAM = 1,
    MEMPROT_DRAM0_SRAM = 2,
    MEMPROT_IRAM0_RTCFAST = 4,
    MEMPROT_DRAM0_RTCFAST = 8,
    MEMPROT_PERI1_RTCSLOW = 16,
    MEMPROT_PERI2_RTCSLOW_0 = 32,
    MEMPROT_PERI2_RTCSLOW_1 = 64,
    MEMPROT_ALL = 4294967295,
} mem_type_prot_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
esp_err_t esp_memprot_clear_intr(mem_type_prot_t mem_type) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_clear_intr out of Core 8 scope.");
#else
esp_err_t esp_memprot_clear_intr(mem_type_prot_t mem_type);
#endif

#if defined(__WINK_SIM__)
mem_type_prot_t esp_memprot_get_active_intr_memtype(void) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_get_active_intr_memtype out of Core 8 scope.");
#else
mem_type_prot_t esp_memprot_get_active_intr_memtype(void);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_memprot_get_conf_reg(mem_type_prot_t mem_type, uint32_t *conf_reg_val) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_get_conf_reg out of Core 8 scope.");
#else
esp_err_t esp_memprot_get_conf_reg(mem_type_prot_t mem_type, uint32_t *conf_reg_val);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_memprot_get_fault_reg(mem_type_prot_t mem_type, uint32_t *fault_reg_val) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_get_fault_reg out of Core 8 scope.");
#else
esp_err_t esp_memprot_get_fault_reg(mem_type_prot_t mem_type, uint32_t *fault_reg_val);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_memprot_get_fault_status(mem_type_prot_t mem_type, uint32_t **faulting_address, uint32_t *op_type, uint32_t *op_subtype) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_get_fault_status out of Core 8 scope.");
#else
esp_err_t esp_memprot_get_fault_status(mem_type_prot_t mem_type, uint32_t **faulting_address, uint32_t *op_type, uint32_t *op_subtype);
#endif

#if defined(__WINK_SIM__)
uint32_t esp_memprot_get_high_limit(mem_type_prot_t mem_type) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_get_high_limit out of Core 8 scope.");
#else
uint32_t esp_memprot_get_high_limit(mem_type_prot_t mem_type);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_memprot_get_intr_clr_bit(mem_type_prot_t mem_type, uint32_t *clear_bit) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_get_intr_clr_bit out of Core 8 scope.");
#else
esp_err_t esp_memprot_get_intr_clr_bit(mem_type_prot_t mem_type, uint32_t *clear_bit);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_memprot_get_intr_ena_bit(mem_type_prot_t mem_type, uint32_t *enable_bit) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_get_intr_ena_bit out of Core 8 scope.");
#else
esp_err_t esp_memprot_get_intr_ena_bit(mem_type_prot_t mem_type, uint32_t *enable_bit);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_memprot_get_intr_on_bit(mem_type_prot_t mem_type, uint32_t *intr_on_bit) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_get_intr_on_bit out of Core 8 scope.");
#else
esp_err_t esp_memprot_get_intr_on_bit(mem_type_prot_t mem_type, uint32_t *intr_on_bit);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_memprot_get_lock(mem_type_prot_t mem_type, bool *locked) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_get_lock out of Core 8 scope.");
#else
esp_err_t esp_memprot_get_lock(mem_type_prot_t mem_type, bool *locked);
#endif

#if defined(__WINK_SIM__)
uint32_t esp_memprot_get_low_limit(mem_type_prot_t mem_type) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_get_low_limit out of Core 8 scope.");
#else
uint32_t esp_memprot_get_low_limit(mem_type_prot_t mem_type);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_memprot_get_perm_exec(mem_type_prot_t mem_type, bool *lx, bool *hx) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_get_perm_exec out of Core 8 scope.");
#else
esp_err_t esp_memprot_get_perm_exec(mem_type_prot_t mem_type, bool *lx, bool *hx);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_memprot_get_perm_read(mem_type_prot_t mem_type, bool *lr, bool *hr) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_get_perm_read out of Core 8 scope.");
#else
esp_err_t esp_memprot_get_perm_read(mem_type_prot_t mem_type, bool *lr, bool *hr);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_memprot_get_perm_split_bits_dram(mem_type_prot_t mem_type, bool *lw, bool *lr, bool *hw, bool *hr) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_get_perm_split_bits_dram out of Core 8 scope.");
#else
esp_err_t esp_memprot_get_perm_split_bits_dram(mem_type_prot_t mem_type, bool *lw, bool *lr, bool *hw, bool *hr);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_memprot_get_perm_split_bits_iram(mem_type_prot_t mem_type, bool *lw, bool *lr, bool *lx, bool *hw, bool *hr, bool *hx) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_get_perm_split_bits_iram out of Core 8 scope.");
#else
esp_err_t esp_memprot_get_perm_split_bits_iram(mem_type_prot_t mem_type, bool *lw, bool *lr, bool *lx, bool *hw, bool *hr, bool *hx);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_memprot_get_perm_split_bits_peri1(mem_type_prot_t mem_type, bool *lw, bool *lr, bool *hw, bool *hr) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_get_perm_split_bits_peri1 out of Core 8 scope.");
#else
esp_err_t esp_memprot_get_perm_split_bits_peri1(mem_type_prot_t mem_type, bool *lw, bool *lr, bool *hw, bool *hr);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_memprot_get_perm_split_bits_peri2(mem_type_prot_t mem_type, bool *lw, bool *lr, bool *lx, bool *hw, bool *hr, bool *hx) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_get_perm_split_bits_peri2 out of Core 8 scope.");
#else
esp_err_t esp_memprot_get_perm_split_bits_peri2(mem_type_prot_t mem_type, bool *lw, bool *lr, bool *lx, bool *hw, bool *hr, bool *hx);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_memprot_get_perm_split_reg(mem_type_prot_t mem_type, uint32_t *split_reg) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_get_perm_split_reg out of Core 8 scope.");
#else
esp_err_t esp_memprot_get_perm_split_reg(mem_type_prot_t mem_type, uint32_t *split_reg);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_memprot_get_perm_uni_reg(mem_type_prot_t mem_type, uint32_t *perm_reg) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_get_perm_uni_reg out of Core 8 scope.");
#else
esp_err_t esp_memprot_get_perm_uni_reg(mem_type_prot_t mem_type, uint32_t *perm_reg);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_memprot_get_perm_write(mem_type_prot_t mem_type, bool *lw, bool *hw) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_get_perm_write out of Core 8 scope.");
#else
esp_err_t esp_memprot_get_perm_write(mem_type_prot_t mem_type, bool *lw, bool *hw);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_memprot_get_permissions(mem_type_prot_t mem_type, bool *lw, bool *lr, bool *lx, bool *hw, bool *hr, bool *hx) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_get_permissions out of Core 8 scope.");
#else
esp_err_t esp_memprot_get_permissions(mem_type_prot_t mem_type, bool *lw, bool *lr, bool *lx, bool *hw, bool *hr, bool *hx);
#endif

#if defined(__WINK_SIM__)
uint32_t * esp_memprot_get_split_addr(mem_type_prot_t mem_type) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_get_split_addr out of Core 8 scope.");
#else
uint32_t * esp_memprot_get_split_addr(mem_type_prot_t mem_type);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_memprot_get_uni_block_exec_bit(mem_type_prot_t mem_type, uint32_t block, uint32_t *exec_bit) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_get_uni_block_exec_bit out of Core 8 scope.");
#else
esp_err_t esp_memprot_get_uni_block_exec_bit(mem_type_prot_t mem_type, uint32_t block, uint32_t *exec_bit);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_memprot_get_uni_block_read_bit(mem_type_prot_t mem_type, uint32_t block, uint32_t *read_bit) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_get_uni_block_read_bit out of Core 8 scope.");
#else
esp_err_t esp_memprot_get_uni_block_read_bit(mem_type_prot_t mem_type, uint32_t block, uint32_t *read_bit);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_memprot_get_uni_block_write_bit(mem_type_prot_t mem_type, uint32_t block, uint32_t *write_bit) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_get_uni_block_write_bit out of Core 8 scope.");
#else
esp_err_t esp_memprot_get_uni_block_write_bit(mem_type_prot_t mem_type, uint32_t block, uint32_t *write_bit);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_memprot_intr_ena(mem_type_prot_t mem_type, bool enable) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_intr_ena out of Core 8 scope.");
#else
esp_err_t esp_memprot_intr_ena(mem_type_prot_t mem_type, bool enable);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_memprot_intr_init(mem_type_prot_t mem_type) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_intr_init out of Core 8 scope.");
#else
esp_err_t esp_memprot_intr_init(mem_type_prot_t mem_type);
#endif

#if defined(__WINK_SIM__)
bool esp_memprot_is_intr_ena_any(void) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_is_intr_ena_any out of Core 8 scope.");
#else
bool esp_memprot_is_intr_ena_any(void);
#endif

#if defined(__WINK_SIM__)
bool esp_memprot_is_locked_any(void) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_is_locked_any out of Core 8 scope.");
#else
bool esp_memprot_is_locked_any(void);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_memprot_set_exec_perm(mem_type_prot_t mem_type, bool lx, bool hx) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_set_exec_perm out of Core 8 scope.");
#else
esp_err_t esp_memprot_set_exec_perm(mem_type_prot_t mem_type, bool lx, bool hx);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_memprot_set_lock(mem_type_prot_t mem_type) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_set_lock out of Core 8 scope.");
#else
esp_err_t esp_memprot_set_lock(mem_type_prot_t mem_type);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_memprot_set_prot(bool invoke_panic_handler, bool lock_feature, uint32_t *mem_type_mask) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_set_prot out of Core 8 scope.");
#else
esp_err_t esp_memprot_set_prot(bool invoke_panic_handler, bool lock_feature, uint32_t *mem_type_mask);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_memprot_set_prot_dram(mem_type_prot_t mem_type, uint32_t *split_addr, bool lw, bool lr, bool hw, bool hr) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_set_prot_dram out of Core 8 scope.");
#else
esp_err_t esp_memprot_set_prot_dram(mem_type_prot_t mem_type, uint32_t *split_addr, bool lw, bool lr, bool hw, bool hr);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_memprot_set_prot_iram(mem_type_prot_t mem_type, uint32_t *split_addr, bool lw, bool lr, bool lx, bool hw, bool hr, bool hx) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_set_prot_iram out of Core 8 scope.");
#else
esp_err_t esp_memprot_set_prot_iram(mem_type_prot_t mem_type, uint32_t *split_addr, bool lw, bool lr, bool lx, bool hw, bool hr, bool hx);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_memprot_set_prot_peri1(mem_type_prot_t mem_type, uint32_t *split_addr, bool lw, bool lr, bool hw, bool hr) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_set_prot_peri1 out of Core 8 scope.");
#else
esp_err_t esp_memprot_set_prot_peri1(mem_type_prot_t mem_type, uint32_t *split_addr, bool lw, bool lr, bool hw, bool hr);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_memprot_set_prot_peri2(mem_type_prot_t mem_type, uint32_t *split_addr, bool lw, bool lr, bool lx, bool hw, bool hr, bool hx) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_set_prot_peri2 out of Core 8 scope.");
#else
esp_err_t esp_memprot_set_prot_peri2(mem_type_prot_t mem_type, uint32_t *split_addr, bool lw, bool lr, bool lx, bool hw, bool hr, bool hx);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_memprot_set_read_perm(mem_type_prot_t mem_type, bool lr, bool hr) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_set_read_perm out of Core 8 scope.");
#else
esp_err_t esp_memprot_set_read_perm(mem_type_prot_t mem_type, bool lr, bool hr);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_memprot_set_uni_block_perm_dram(mem_type_prot_t mem_type, uint32_t block, bool write_perm, bool read_perm) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_set_uni_block_perm_dram out of Core 8 scope.");
#else
esp_err_t esp_memprot_set_uni_block_perm_dram(mem_type_prot_t mem_type, uint32_t block, bool write_perm, bool read_perm);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_memprot_set_uni_block_perm_iram(mem_type_prot_t mem_type, uint32_t block, bool write_perm, bool read_perm, bool exec_perm) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_set_uni_block_perm_iram out of Core 8 scope.");
#else
esp_err_t esp_memprot_set_uni_block_perm_iram(mem_type_prot_t mem_type, uint32_t block, bool write_perm, bool read_perm, bool exec_perm);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_memprot_set_write_perm(mem_type_prot_t mem_type, bool lw, bool hw) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_set_write_perm out of Core 8 scope.");
#else
esp_err_t esp_memprot_set_write_perm(mem_type_prot_t mem_type, bool lw, bool hw);
#endif

#if defined(__WINK_SIM__)
const char * esp_memprot_type_to_str(mem_type_prot_t mem_type) WINK_SLA_ERROR("Wink SLA Violation: esp_memprot_type_to_str out of Core 8 scope.");
#else
const char * esp_memprot_type_to_str(mem_type_prot_t mem_type);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_SOC_ESP32S2_MEMPROT_H__ */
#endif /* WINK_H_GUARD_SOC_ESP32S2_MEMPROT_H */
