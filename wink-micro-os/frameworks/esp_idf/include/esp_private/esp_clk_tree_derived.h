/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_ESP_CLK_TREE_DERIVED_H
#define WINK_H_GUARD_ESP_PRIVATE_ESP_CLK_TREE_DERIVED_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_ESP_CLK_TREE_DERIVED_H__
#define __WINK_HARVESTED_ESP_PRIVATE_ESP_CLK_TREE_DERIVED_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "soc/clk_tree_defs.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
    int ref_cnt;
    soc_module_clk_t cur_upstream;
    uint32_t cur_divider;
} esp_clk_tree_derived_clk_state_t;
typedef struct {
    soc_module_clk_t upstream;
    uint8_t mux_sel;
} esp_clk_tree_derived_upstream_t;
typedef struct {
    soc_module_clk_t clk_id;
    void (*set_src)(uint8_t mux_sel);
    void (*set_divider)(uint32_t divider);
    void (*set_gate)(bool enable);
    const esp_clk_tree_derived_upstream_t * upstreams;
    size_t upstream_count;
    esp_clk_tree_derived_clk_state_t * state;
} esp_clk_tree_derived_clk_desc_t;



#if defined(__WINK_SIM__)
esp_err_t esp_clk_tree_derived_clk_acquire(soc_module_clk_t clk_src) WINK_SLA_ERROR("Wink SLA Violation: esp_clk_tree_derived_clk_acquire out of Core 8 scope.");
#else
esp_err_t esp_clk_tree_derived_clk_acquire(soc_module_clk_t clk_src);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_clk_tree_derived_clk_freq_set(soc_module_clk_t clk_src,
                                            uint32_t expt_freq_hz,
                                            uint32_t *real_freq_hz) WINK_SLA_ERROR("Wink SLA Violation: esp_clk_tree_derived_clk_freq_set out of Core 8 scope.");
#else
esp_err_t esp_clk_tree_derived_clk_freq_set(soc_module_clk_t clk_src,
                                            uint32_t expt_freq_hz,
                                            uint32_t *real_freq_hz);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_clk_tree_derived_clk_release(soc_module_clk_t clk_src) WINK_SLA_ERROR("Wink SLA Violation: esp_clk_tree_derived_clk_release out of Core 8 scope.");
#else
esp_err_t esp_clk_tree_derived_clk_release(soc_module_clk_t clk_src);
#endif

#if defined(__WINK_SIM__)
const esp_clk_tree_derived_clk_desc_t * esp_clk_tree_get_derived_clk_desc(soc_module_clk_t clk_src) WINK_SLA_ERROR("Wink SLA Violation: esp_clk_tree_get_derived_clk_desc out of Core 8 scope.");
#else
const esp_clk_tree_derived_clk_desc_t * esp_clk_tree_get_derived_clk_desc(soc_module_clk_t clk_src);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_ESP_CLK_TREE_DERIVED_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_ESP_CLK_TREE_DERIVED_H */
