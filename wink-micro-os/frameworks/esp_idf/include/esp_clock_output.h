/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_CLOCK_OUTPUT_H
#define WINK_H_GUARD_ESP_CLOCK_OUTPUT_H
#ifndef __WINK_HARVESTED_ESP_CLOCK_OUTPUT_H__
#define __WINK_HARVESTED_ESP_CLOCK_OUTPUT_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdint.h>

#include "esp_err.h"
#include "soc/clk_tree_defs.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct esp_clock_output_mapping * esp_clock_output_mapping_handle_t;



#if defined(__WINK_SIM__)
esp_err_t esp_clock_output_start(soc_clkout_sig_id_t clk_sig, gpio_num_t gpio_num, esp_clock_output_mapping_handle_t *clkout_mapping_ret_hdl) WINK_SLA_ERROR("Wink SLA Violation: esp_clock_output_start out of Core 8 scope.");
#else
esp_err_t esp_clock_output_start(soc_clkout_sig_id_t clk_sig, gpio_num_t gpio_num, esp_clock_output_mapping_handle_t *clkout_mapping_ret_hdl);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_clock_output_stop(esp_clock_output_mapping_handle_t clkout_mapping_hdl) WINK_SLA_ERROR("Wink SLA Violation: esp_clock_output_stop out of Core 8 scope.");
#else
esp_err_t esp_clock_output_stop(esp_clock_output_mapping_handle_t clkout_mapping_hdl);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_CLOCK_OUTPUT_H__ */
#endif /* WINK_H_GUARD_ESP_CLOCK_OUTPUT_H */
