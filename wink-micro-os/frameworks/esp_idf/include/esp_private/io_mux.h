/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_IO_MUX_H
#define WINK_H_GUARD_ESP_PRIVATE_IO_MUX_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_IO_MUX_H__
#define __WINK_HARVESTED_ESP_PRIVATE_IO_MUX_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>

#include "esp_err.h"
#include "soc/clk_tree_defs.h"
#include "soc/gpio_num.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
esp_err_t io_mux_acquire_clock_source(soc_module_clk_t clk_src) WINK_SLA_ERROR("Wink SLA Violation: io_mux_acquire_clock_source out of Core 8 scope.");
#else
esp_err_t io_mux_acquire_clock_source(soc_module_clk_t clk_src);
#endif

#if defined(__WINK_SIM__)
bool io_mux_is_lp_io_in_use(gpio_num_t gpio_num) WINK_SLA_ERROR("Wink SLA Violation: io_mux_is_lp_io_in_use out of Core 8 scope.");
#else
bool io_mux_is_lp_io_in_use(gpio_num_t gpio_num);
#endif

#if defined(__WINK_SIM__)
esp_err_t io_mux_release_clock_source(soc_module_clk_t clk_src) WINK_SLA_ERROR("Wink SLA Violation: io_mux_release_clock_source out of Core 8 scope.");
#else
esp_err_t io_mux_release_clock_source(soc_module_clk_t clk_src);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_IO_MUX_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_IO_MUX_H */
