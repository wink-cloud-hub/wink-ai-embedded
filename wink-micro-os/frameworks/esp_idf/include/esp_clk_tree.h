/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_CLK_TREE_H
#define WINK_H_GUARD_ESP_CLK_TREE_H
#ifndef __WINK_HARVESTED_ESP_CLK_TREE_H__
#define __WINK_HARVESTED_ESP_CLK_TREE_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>

#include "esp_err.h"
#include "soc/clk_tree_defs.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED = 0,
    ESP_CLK_TREE_SRC_FREQ_PRECISION_APPROX = 1,
    ESP_CLK_TREE_SRC_FREQ_PRECISION_EXACT = 2,
    ESP_CLK_TREE_SRC_FREQ_PRECISION_INVALID = 3,
} esp_clk_tree_src_freq_precision_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
esp_err_t esp_clk_tree_src_get_freq_hz(soc_module_clk_t clk_src, esp_clk_tree_src_freq_precision_t precision,
uint32_t *freq_value) WINK_SLA_ERROR("Wink SLA Violation: esp_clk_tree_src_get_freq_hz out of Core 8 scope.");
#else
esp_err_t esp_clk_tree_src_get_freq_hz(soc_module_clk_t clk_src, esp_clk_tree_src_freq_precision_t precision,
uint32_t *freq_value);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_clk_tree_src_set_freq_hz(soc_module_clk_t clk_src, uint32_t expt_freq_value, uint32_t *ret_freq_value) WINK_SLA_ERROR("Wink SLA Violation: esp_clk_tree_src_set_freq_hz out of Core 8 scope.");
#else
esp_err_t esp_clk_tree_src_set_freq_hz(soc_module_clk_t clk_src, uint32_t expt_freq_value, uint32_t *ret_freq_value);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_CLK_TREE_H__ */
#endif /* WINK_H_GUARD_ESP_CLK_TREE_H */
