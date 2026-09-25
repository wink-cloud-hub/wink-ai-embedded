/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_ESP_CLK_TREE_COMMON_H
#define WINK_H_GUARD_ESP_PRIVATE_ESP_CLK_TREE_COMMON_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_ESP_CLK_TREE_COMMON_H__
#define __WINK_HARVESTED_ESP_PRIVATE_ESP_CLK_TREE_COMMON_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stdint.h>

#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
    soc_module_clk_t clk_upstream;
    uint32_t divider;
} esp_clk_tree_src_config_t;



#if defined(__WINK_SIM__)
void esp_clk_tree_apll_acquire(void) WINK_SLA_ERROR("Wink SLA Violation: esp_clk_tree_apll_acquire out of Core 8 scope.");
#else
void esp_clk_tree_apll_acquire(void);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_clk_tree_apll_freq_set(uint32_t expt_freq_hz, uint32_t *real_freq_hz) WINK_SLA_ERROR("Wink SLA Violation: esp_clk_tree_apll_freq_set out of Core 8 scope.");
#else
esp_err_t esp_clk_tree_apll_freq_set(uint32_t expt_freq_hz, uint32_t *real_freq_hz);
#endif

#if defined(__WINK_SIM__)
void esp_clk_tree_apll_release(void) WINK_SLA_ERROR("Wink SLA Violation: esp_clk_tree_apll_release out of Core 8 scope.");
#else
void esp_clk_tree_apll_release(void);
#endif

#if defined(__WINK_SIM__)
bool esp_clk_tree_enable_power(soc_root_clk_circuit_t clk_circuit, bool enable) WINK_SLA_ERROR("Wink SLA Violation: esp_clk_tree_enable_power out of Core 8 scope.");
#else
bool esp_clk_tree_enable_power(soc_root_clk_circuit_t clk_circuit, bool enable);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_clk_tree_enable_src(soc_module_clk_t clk_src, bool enable) WINK_SLA_ERROR("Wink SLA Violation: esp_clk_tree_enable_src out of Core 8 scope.");
#else
esp_err_t esp_clk_tree_enable_src(soc_module_clk_t clk_src, bool enable);
#endif

#if defined(__WINK_SIM__)
void esp_clk_tree_initialize(void) WINK_SLA_ERROR("Wink SLA Violation: esp_clk_tree_initialize out of Core 8 scope.");
#else
void esp_clk_tree_initialize(void);
#endif

#if defined(__WINK_SIM__)
bool esp_clk_tree_is_power_on(soc_root_clk_circuit_t clk_circuit) WINK_SLA_ERROR("Wink SLA Violation: esp_clk_tree_is_power_on out of Core 8 scope.");
#else
bool esp_clk_tree_is_power_on(soc_root_clk_circuit_t clk_circuit);
#endif

#if defined(__WINK_SIM__)
uint32_t esp_clk_tree_lp_fast_get_freq_hz(esp_clk_tree_src_freq_precision_t precision) WINK_SLA_ERROR("Wink SLA Violation: esp_clk_tree_lp_fast_get_freq_hz out of Core 8 scope.");
#else
uint32_t esp_clk_tree_lp_fast_get_freq_hz(esp_clk_tree_src_freq_precision_t precision);
#endif

#if defined(__WINK_SIM__)
uint32_t esp_clk_tree_lp_slow_get_freq_hz(esp_clk_tree_src_freq_precision_t precision) WINK_SLA_ERROR("Wink SLA Violation: esp_clk_tree_lp_slow_get_freq_hz out of Core 8 scope.");
#else
uint32_t esp_clk_tree_lp_slow_get_freq_hz(esp_clk_tree_src_freq_precision_t precision);
#endif

#if defined(__WINK_SIM__)
uint32_t esp_clk_tree_rc_fast_d256_get_freq_hz(esp_clk_tree_src_freq_precision_t precision) WINK_SLA_ERROR("Wink SLA Violation: esp_clk_tree_rc_fast_d256_get_freq_hz out of Core 8 scope.");
#else
uint32_t esp_clk_tree_rc_fast_d256_get_freq_hz(esp_clk_tree_src_freq_precision_t precision);
#endif

#if defined(__WINK_SIM__)
uint32_t esp_clk_tree_rc_fast_get_freq_hz(esp_clk_tree_src_freq_precision_t precision) WINK_SLA_ERROR("Wink SLA Violation: esp_clk_tree_rc_fast_get_freq_hz out of Core 8 scope.");
#else
uint32_t esp_clk_tree_rc_fast_get_freq_hz(esp_clk_tree_src_freq_precision_t precision);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_clk_tree_src_select_upstream(soc_module_clk_t clk_src,
                                           soc_module_clk_t upstream) WINK_SLA_ERROR("Wink SLA Violation: esp_clk_tree_src_select_upstream out of Core 8 scope.");
#else
esp_err_t esp_clk_tree_src_select_upstream(soc_module_clk_t clk_src,
                                           soc_module_clk_t upstream);
#endif

#if defined(__WINK_SIM__)
uint32_t esp_clk_tree_xtal32k_get_freq_hz(esp_clk_tree_src_freq_precision_t precision) WINK_SLA_ERROR("Wink SLA Violation: esp_clk_tree_xtal32k_get_freq_hz out of Core 8 scope.");
#else
uint32_t esp_clk_tree_xtal32k_get_freq_hz(esp_clk_tree_src_freq_precision_t precision);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_ESP_CLK_TREE_COMMON_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_ESP_CLK_TREE_COMMON_H */
